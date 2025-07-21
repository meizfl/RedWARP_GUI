#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <system_error>
#include <array>
#include <string_view>
#include <random>

// Include necessary headers for GUI
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>

using namespace std;
namespace fs = std::filesystem;

// Struct to hold pointers to the input widgets
struct UserData {
    Fl_Input* input_endpoint;
    Fl_Input* input_mtu;
    Fl_Choice* ipv6_choice;
    Fl_Choice* amnezia_choice;
    Fl_Choice* randomize_amnezia_choice;
    Fl_Choice* dns_ipv4_choice;
    Fl_Choice* dns_ipv6_choice;
    Fl_Input* input_custom_dns_ipv4;
    Fl_Input* input_custom_dns_ipv6;
};

// Global variables for user inputs
string custom_endpoint, custom_mtu;
char ipv6_enabled = 'y';
bool amnezia_enabled = true;
bool randomize_amnezia = false;
string selected_dns_ipv4, selected_dns_ipv6;

// Generate random int in range [min, max]
int random_int(int min, int max) {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<> dis(min, max);
    return dis(gen);
}

// Generate random 32-bit unsigned int from 1 to 0xFFFFFFFF
uint32_t random_uint32() {
    static random_device rd;
    static mt19937 gen(rd());
    uniform_int_distribution<uint32_t> dis(1, 0xFFFFFFFF);
    return dis(gen);
}

// Function to run external command and check its exit code
bool run_command(const array<string, 3>& command) {
    if (command.empty()) {
        return false;
    }

    string command_str = command[0];
    for (size_t i = 1; i < command.size(); ++i) {
        command_str += " " + command[i];
    }
    return std::system(command_str.c_str()) == 0;
}

// Function to generate and modify the configuration file
void generate_config() {
    string wgcf_path = "./bin/wgcf_amd64.exe";

    if (fs::exists("RedWARP.conf")) fs::remove("RedWARP.conf");
    if (fs::exists("wgcf-account.toml")) fs::remove("wgcf-account.toml");

    if (!fs::exists(wgcf_path)) {
        fl_alert("Binary file %s not found. Make sure it exists and is accessible.", wgcf_path.c_str());
        return;
    }

    if (!run_command({wgcf_path, "register", "--accept-tos"})) {
        fl_alert("Error running wgcf register command");
        return;
    }

    if (!run_command({wgcf_path, "generate", ""})) {
        fl_alert("Error running wgcf generate command");
        return;
    }

    if (!fs::exists("wgcf-profile.conf")) {
        fl_alert("Could not find generated configuration file wgcf-profile.conf");
        return;
    }

    ifstream infile("wgcf-profile.conf");
    ofstream outfile("wgcf-profile.conf.new");

    string line;
    bool interface_section = false;
    while (getline(infile, line)) {
        if (line.find("[Interface]") == 0)
            interface_section = true;
        else if (line.find("[") == 0)
            interface_section = false;

        if (ipv6_enabled == 'n') {
            size_t pos;
            while ((pos = line.find(", 2606:4700")) != string::npos)
                line.erase(pos, 50);
            while ((pos = line.find(", ::/0")) != string::npos)
                line.erase(pos, 8);
        }

        if (interface_section && line.find("PrivateKey =") == 0 && amnezia_enabled) {
            outfile << line << endl;

            int Jc = randomize_amnezia ? random_int(1, 128) : 120;
            int Jmin = randomize_amnezia ? random_int(1, 400) : 23;
            int Jmax = randomize_amnezia ? random_int(Jmin + 1, 1280) : 911;

            outfile << "S1 = 0\nS2 = 0\n";
            outfile << "Jc = " << Jc << "\n";
            outfile << "Jmin = " << Jmin << "\n";
            outfile << "Jmax = " << Jmax << "\n";

            if (randomize_amnezia) {
                outfile << "H1 = " << (random_uint32() % 4 + 1) << "\n";
                outfile << "H2 = " << (random_uint32() % 4 + 1) << "\n";
                outfile << "H3 = " << (random_uint32() % 4 + 1) << "\n";
                outfile << "H4 = " << (random_uint32() % 4 + 1) << "\n";
            } else {
                outfile << "H1 = 1\nH2 = 2\nH3 = 3\nH4 = 4\n";
            }
        } else if (line.find("MTU = ") == 0) {
            outfile << "MTU = " << custom_mtu << endl;
        } else if (line.find("Endpoint = ") == 0) {
            outfile << "Endpoint = " << custom_endpoint << endl;
        } else if (line.find("DNS = ") == 0) {
            outfile << "DNS = " << selected_dns_ipv4;
            if (ipv6_enabled == 'y')
                outfile << ", " << selected_dns_ipv6;
            outfile << endl;
        } else {
            outfile << line << endl;
        }
    }
    infile.close();
    outfile.close();

    fs::remove("wgcf-profile.conf");
    fs::rename("wgcf-profile.conf.new", "RedWARP.conf");

    ifstream checkfile("RedWARP.conf");
    string content((istreambuf_iterator<char>(checkfile)), istreambuf_iterator<char>());
    checkfile.close();

    if (content.find("MTU = " + custom_mtu) != string::npos &&
        content.find("Endpoint = " + custom_endpoint) != string::npos &&
        content.find("DNS = " + selected_dns_ipv4) != string::npos) {
        fl_alert("Configuration successfully updated and saved to RedWARP.conf!");
        } else {
            fl_alert("An error occurred while updating the configuration.");
        }
}

void generate_cb(Fl_Widget*, void* data) {
    UserData* ud = (UserData*)data;

    custom_endpoint = ud->input_endpoint->value();
    custom_mtu = ud->input_mtu->value();

    ipv6_enabled = ud->ipv6_choice->value() == 0 ? 'y' : 'n';
    amnezia_enabled = (ud->amnezia_choice->value() == 0);
    randomize_amnezia = (ud->randomize_amnezia_choice->value() == 0);

    static const string dns_options[] = {
        "208.67.222.222, 208.67.220.220",
        "1.1.1.1, 1.0.0.1",
        "8.8.8.8, 8.8.4.4",
        "9.9.9.9, 149.112.112.112",
    };
    selected_dns_ipv4 = dns_options[ud->dns_ipv4_choice->value()];

    static const string dns_ipv6_options[] = {
        "2620:119:35::35, 2620:119:53::53",
        "2606:4700:4700::1111, 2606:4700:4700::1001",
        "2001:4860:4860::8888, 2001:4860:4860::8844",
        "2620:fe::fe, 2620:fe::9",
    };
    selected_dns_ipv6 = dns_ipv6_options[ud->dns_ipv6_choice->value()];

    if (ud->dns_ipv4_choice->value() == 4)
        selected_dns_ipv4 = ud->input_custom_dns_ipv4->value();
    if (ud->dns_ipv6_choice->value() == 4)
        selected_dns_ipv6 = ud->input_custom_dns_ipv6->value();

    generate_config();
}

void ipv6_toggle_cb(Fl_Widget*, void* data) {
    UserData* ud = (UserData*)data;

    if (ud->ipv6_choice->value() == 1) {
        ud->dns_ipv6_choice->deactivate();
        ud->input_custom_dns_ipv6->deactivate();
    } else {
        ud->dns_ipv6_choice->activate();
        if (ud->dns_ipv6_choice->value() == 4)
            ud->input_custom_dns_ipv6->activate();
    }
}

void dns_ipv4_choice_cb(Fl_Widget* widget, void* data) {
    auto* choice = static_cast<Fl_Choice*>(widget);
    auto* ud = static_cast<UserData*>(data);

    if (choice->value() == 4)
        ud->input_custom_dns_ipv4->activate();
    else
        ud->input_custom_dns_ipv4->deactivate();
}

void dns_ipv6_choice_cb(Fl_Widget* widget, void* data) {
    auto* choice = static_cast<Fl_Choice*>(widget);
    auto* ud = static_cast<UserData*>(data);

    if (choice->value() == 4 && ipv6_enabled == 'y')
        ud->input_custom_dns_ipv6->activate();
    else
        ud->input_custom_dns_ipv6->deactivate();
}

int main() {
    Fl_Window window(400, 345, "RedWARP Config Generator");

    Fl_Box label_endpoint(10, 20, 100, 25, "Endpoint:");
    Fl_Input input_endpoint(120, 20, 270, 25);
    input_endpoint.value("188.114.97.14:4500");

    Fl_Box label_mtu(10, 60, 100, 25, "MTU:");
    Fl_Input input_mtu(120, 60, 270, 25);
    input_mtu.value("1420");

    Fl_Box label_ipv6(10, 100, 100, 25, "IPv6:");
    Fl_Choice ipv6_choice(120, 100, 150, 25);
    ipv6_choice.add("Yes");
    ipv6_choice.add("No");
    ipv6_choice.value(0);

    Fl_Box label_amnezia(10, 140, 100, 25, "AmneziaWG:");
    Fl_Choice amnezia_choice(120, 140, 150, 25);
    amnezia_choice.add("Yes");
    amnezia_choice.add("No");
    amnezia_choice.value(0);

    Fl_Box label_randomize(10, 180, 120, 25, "Randomize:");
    Fl_Choice randomize_amnezia_choice(120, 180, 120, 25);
    randomize_amnezia_choice.add("Yes");
    randomize_amnezia_choice.add("No");
    randomize_amnezia_choice.value(1);

    Fl_Box label_dns_ipv4(10, 220, 100, 25, "DNS IPv4:");
    Fl_Choice dns_ipv4_choice(120, 220, 150, 25);
    dns_ipv4_choice.add("OpenDNS");
    dns_ipv4_choice.add("Cloudflare");
    dns_ipv4_choice.add("Google");
    dns_ipv4_choice.add("Quad9");
    dns_ipv4_choice.add("Custom");
    dns_ipv4_choice.value(0);

    Fl_Input input_custom_dns_ipv4(280, 220, 110, 25);
    input_custom_dns_ipv4.deactivate();

    Fl_Box label_dns_ipv6(10, 260, 100, 25, "DNS IPv6:");
    Fl_Choice dns_ipv6_choice(120, 260, 150, 25);
    dns_ipv6_choice.add("OpenDNS");
    dns_ipv6_choice.add("Cloudflare");
    dns_ipv6_choice.add("Google");
    dns_ipv6_choice.add("Quad9");
    dns_ipv6_choice.add("Custom");
    dns_ipv6_choice.value(0);

    Fl_Input input_custom_dns_ipv6(280, 260, 110, 25);
    input_custom_dns_ipv6.deactivate();

    UserData ud{
        &input_endpoint, &input_mtu,
        &ipv6_choice, &amnezia_choice, &randomize_amnezia_choice,
        &dns_ipv4_choice, &dns_ipv6_choice,
        &input_custom_dns_ipv4, &input_custom_dns_ipv6
    };

    dns_ipv4_choice.callback(dns_ipv4_choice_cb, &ud);
    dns_ipv6_choice.callback(dns_ipv6_choice_cb, &ud);
    ipv6_choice.callback(ipv6_toggle_cb, &ud);

    Fl_Button button_generate(150, 300, 100, 30, "Generate");
    button_generate.callback(generate_cb, &ud);

    window.end();
    window.show();
    return Fl::run();
}
