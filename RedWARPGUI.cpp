#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <system_error>
#include <array>
#include <string_view>

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
    Fl_Choice* dns_ipv4_choice;
    Fl_Choice* dns_ipv6_choice;
    Fl_Input* input_custom_dns_ipv4;
    Fl_Input* input_custom_dns_ipv6;
};

// Global variables for user inputs
string custom_endpoint, custom_mtu;
char ipv6_enabled = 'y';
bool amnezia_enabled = true;
string selected_dns_ipv4, selected_dns_ipv6;

// Function to run external command and check its exit code
bool run_command(const array<string, 3>& command) {
    if (command.empty()) {
        return false;
    }

    string command_str = command[0];
    for (size_t i = 1; i < command.size(); ++i) {
        command_str += " " + command[i];
    }
    if (int exit_code = std::system(command_str.c_str()); exit_code == 0) {
        return true;
    } else {
        return false;
    }
}

// Function to generate and modify the configuration file
void generate_config() {
    string wgcf_path = "./bin/wgcf_amd64"; // Adjust path if needed

    // Check for and remove existing config files
    if (fs::exists("RedWARP.conf")) {
        fs::remove("RedWARP.conf");
    }
    if (fs::exists("wgcf-account.toml")) {
        fs::remove("wgcf-account.toml");
    }

    if (!fs::exists(wgcf_path)) {
        fl_alert("Binary file %s not found. Make sure it exists and is accessible.", wgcf_path.c_str());
        return;
    }

    array<string, 3> register_command = {wgcf_path, "register", "--accept-tos"};
    if (!run_command(register_command)) {
        fl_alert("Error running wgcf register command");
        return;
    }

    array<string, 3> generate_command = {wgcf_path, "generate", ""};
    if (!run_command(generate_command)) {
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
        if (line.find("[Interface]") == 0) {
            interface_section = true;
        } else if (line.find("[") == 0) {
            interface_section = false;
        }

        if (ipv6_enabled == 'n') {
            size_t pos;
            while ((pos = line.find(", 2606:4700")) != string::npos) {
                line.erase(pos, 50);
            }
            while ((pos = line.find(", ::/0")) != string::npos) {
                line.erase(pos, 8);
            }
        }

        if (interface_section && line.find("PrivateKey =") == 0 && amnezia_enabled) {
            outfile << line << endl;
            outfile << "S1 = 0\nS2 = 0\nJc = 120\nJmin = 23\nJmax = 911\nH1 = 1\nH2 = 2\nH3 = 3\nH4 = 4\n";
        } else if (line.find("MTU = ") == 0) {
            outfile << "MTU = " << custom_mtu << endl;
        } else if (line.find("Endpoint = ") == 0) {
            outfile << "Endpoint = " << custom_endpoint << endl;
        } else if (line.find("DNS = ") == 0) {
            outfile << "DNS = " << selected_dns_ipv4;
            if (ipv6_enabled == 'y') {
                outfile << ", " << selected_dns_ipv6;
            }
            outfile << endl;
        } else {
            outfile << line << endl;
        }
    }
    infile.close();
    outfile.close();
    fs::remove("wgcf-profile.conf");
    fs::rename("wgcf-profile.conf.new", "wgcf-profile.conf");
    fs::rename("wgcf-profile.conf", "RedWARP.conf");

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

    if (ud->ipv6_choice->value() == 0) {
        ipv6_enabled = 'y';
    } else {
        ipv6_enabled = 'n';
    }

    amnezia_enabled = (ud->amnezia_choice->value() == 0);

    // Set DNS selection based on user choice
    static const string dns_options[] = {
        "208.67.222.222, 208.67.220.220", // OpenDNS IPv4
        "1.1.1.1, 1.0.0.1",               // Cloudflare IPv4
        "8.8.8.8, 8.8.4.4",               // Google IPv4
        "9.9.9.9, 149.112.112.112",       // Quad9 IPv4
    };
    selected_dns_ipv4 = dns_options[ud->dns_ipv4_choice->value()];

    static const string dns_ipv6_options[] = {
        "2620:119:35::35, 2620:119:53::53", // OpenDNS IPv6
        "2606:4700:4700::1111, 2606:4700:4700::1001", // Cloudflare IPv6
        "2001:4860:4860::8888, 2001:4860:4860::8844", // Google IPv6
        "2620:fe::fe, 2620:fe::9",         // Quad9 IPv6
    };
    selected_dns_ipv6 = dns_ipv6_options[ud->dns_ipv6_choice->value()];

    // Use custom DNS if selected
    if (ud->dns_ipv4_choice->value() == 4) {
        selected_dns_ipv4 = ud->input_custom_dns_ipv4->value();
    }
    if (ud->dns_ipv6_choice->value() == 4) {
        selected_dns_ipv6 = ud->input_custom_dns_ipv6->value();
    }

    generate_config();
}

void ipv6_toggle_cb(Fl_Widget*, void* data) {
    UserData* ud = (UserData*)data;

    if (ud->ipv6_choice->value() == 1) { // "No" selected
        ud->dns_ipv6_choice->deactivate();
        ud->input_custom_dns_ipv6->deactivate();
    } else { // "Yes" selected
        ud->dns_ipv6_choice->activate();
        if (ud->dns_ipv6_choice->value() == 4) { // Custom selected
            ud->input_custom_dns_ipv6->activate();
        }
    }
}

void dns_ipv4_choice_cb(Fl_Widget* widget, void* data) {
    auto* choice = static_cast<Fl_Choice*>(widget);
    auto* ud = static_cast<UserData*>(data);

    if (choice->value() == 4) { // "Custom" selected
        ud->input_custom_dns_ipv4->activate();
    } else {
        ud->input_custom_dns_ipv4->deactivate();
    }
}

void dns_ipv6_choice_cb(Fl_Widget* widget, void* data) {
    auto* choice = static_cast<Fl_Choice*>(widget);
    auto* ud = static_cast<UserData*>(data);

    if (choice->value() == 4 && ipv6_enabled == 'y') { // "Custom" selected and IPv6 enabled
        ud->input_custom_dns_ipv6->activate();
    } else {
        ud->input_custom_dns_ipv6->deactivate();
    }
}

int main() {
    Fl_Window window(400, 350, "RedWARP Config Generator");

    Fl_Box label_endpoint(10, 20, 100, 25, "Endpoint:");
    Fl_Input input_endpoint(120, 20, 270, 25);
    input_endpoint.value("162.159.193.5:4500");

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

    Fl_Box label_dns_ipv4(10, 180, 100, 25, "DNS IPv4:");
    Fl_Choice dns_ipv4_choice(120, 180, 150, 25);
    dns_ipv4_choice.add("OpenDNS");
    dns_ipv4_choice.add("Cloudflare");
    dns_ipv4_choice.add("Google");
    dns_ipv4_choice.add("Quad9");
    dns_ipv4_choice.add("Custom");
    dns_ipv4_choice.value(0);

    Fl_Input input_custom_dns_ipv4(280, 180, 110, 25);
    input_custom_dns_ipv4.deactivate();

    Fl_Box label_dns_ipv6(10, 220, 100, 25, "DNS IPv6:");
    Fl_Choice dns_ipv6_choice(120, 220, 150, 25);
    dns_ipv6_choice.add("OpenDNS");
    dns_ipv6_choice.add("Cloudflare");
    dns_ipv6_choice.add("Google");
    dns_ipv6_choice.add("Quad9");
    dns_ipv6_choice.add("Custom");
    dns_ipv6_choice.value(0);

    Fl_Input input_custom_dns_ipv6(280, 220, 110, 25);
    input_custom_dns_ipv6.deactivate();

    UserData ud{
        &input_endpoint, &input_mtu, &ipv6_choice, &amnezia_choice,
        &dns_ipv4_choice, &dns_ipv6_choice,
        &input_custom_dns_ipv4, &input_custom_dns_ipv6
    };

    dns_ipv4_choice.callback(dns_ipv4_choice_cb, &ud);
    dns_ipv6_choice.callback(dns_ipv6_choice_cb, &ud);
    ipv6_choice.callback(ipv6_toggle_cb, &ud);

    Fl_Button button_generate(150, 270, 100, 25, "Generate");
    button_generate.callback(generate_cb, &ud);

    window.end();
    window.show();
    return Fl::run();
}
