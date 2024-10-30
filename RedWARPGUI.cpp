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
#include <FL/fl_ask.H> // Include this for fl_alert()

using namespace std;
namespace fs = std::filesystem;

// Struct to hold pointers to the input widgets
struct UserData {
    Fl_Input* input_endpoint;
    Fl_Input* input_mtu;
    Fl_Choice* ipv6_choice;
};

// Global variables for user inputs
string custom_endpoint, custom_mtu;
char ipv6_enabled = 'y';

// Function to run external command and check its exit code
bool run_command(const array<string, 3>& command) {
    if (command.empty()) {
        return false;
    }

    // Create process
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
    // Set the path to the wgcf binary depending on the architecture
    string wgcf_path = "./bin/wgcf_amd64";

    // Check that the selected wgcf binary exists
    if (!fs::exists(wgcf_path)) {
        fl_alert("Binary file %s not found. Make sure it exists and is accessible.", wgcf_path.c_str());
        return;
    }

    // Generating a configuration file using wgcf
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

    // Check if the generated wgcf-profile.conf file exists
    if (!fs::exists("wgcf-profile.conf")) {
        fl_alert("Could not find generated configuration file wgcf-profile.conf");
        return;
    }

    // Modify the configuration file
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

        // Remove ipv6
        if (ipv6_enabled == 'n') {
            size_t pos;
            while ((pos = line.find(", 2606:4700")) != string::npos) {
                line.erase(pos, 50); // default 50
            }
            while ((pos = line.find(", ::/0")) != string::npos) {
                line.erase(pos, 8); // default 8
            }
        }

        if (interface_section && line.find("PrivateKey =") == 0) {
            outfile << line << endl;
            outfile << "S1 = 0\nS2 = 0\nJc = 120\nJmin = 23\nJmax = 911\nH1 = 1\nH2 = 2\nH3 = 3\nH4 = 4\n";
        } else if (line.find("MTU = ") == 0) {
            outfile << "MTU = " << custom_mtu << endl;
        } else if (line.find("Endpoint = ") == 0) {
            outfile << "Endpoint = " << custom_endpoint << endl;
        } else if (line.find("DNS = ") == 0 && ipv6_enabled == 'n') {
            outfile << "DNS = 208.67.222.222, 208.67.220.220\n";
        } else if (line.find("DNS = ") == 0 && ipv6_enabled == 'y') {
            outfile << "DNS = 208.67.222.222, 208.67.220.220, 2620:119:35::35, 2620:119:53::53\n";
        } else {
            outfile << line << endl;
        }
    }
    infile.close();
    outfile.close();
    fs::remove("wgcf-profile.conf");
    fs::rename("wgcf-profile.conf.new", "wgcf-profile.conf");

    // Renaming to RedWARP.conf
    fs::rename("wgcf-profile.conf", "RedWARP.conf");

    // Checking for successful modification
    ifstream checkfile("RedWARP.conf");
    string content((istreambuf_iterator<char>(checkfile)), istreambuf_iterator<char>());
    checkfile.close();

    if (content.find("S1 = 0") != string::npos &&
        content.find("MTU = " + custom_mtu) != string::npos &&
        content.find("Endpoint = " + custom_endpoint) != string::npos &&
        (content.find("DNS = 208.67.222.222, 208.67.220.220") != string::npos ||
        content.find("DNS = 208.67.222.222, 208.67.220.220, 2620:119:35::35, 2620:119:53::53") != string::npos)) {
        fl_alert("Configuration successfully updated and saved to RedWARP.conf!");
    } else {
        fl_alert("An error occurred while updating the configuration.");
    }
}

// Callback function for the "Generate" button
void generate_cb(Fl_Widget*, void* data) {
    UserData* ud = (UserData*)data;

    custom_endpoint = ud->input_endpoint->value();
    custom_mtu = ud->input_mtu->value();

    if (ud->ipv6_choice->value() == 0) {
        ipv6_enabled = 'y';
    } else {
        ipv6_enabled = 'n';
    }

    generate_config();
}

int main() {
    // Create the main window
    Fl_Window window(300, 200, "RedWARP Config Generator");

    // Create input fields
    Fl_Box label_endpoint(10, 20, 100, 25, "Endpoint:");
    Fl_Input input_endpoint(120, 20, 170, 25);
    input_endpoint.value("162.159.193.5:4500");

    Fl_Box label_mtu(10, 60, 100, 25, "MTU:");
    Fl_Input input_mtu(120, 60, 170, 25);
    input_mtu.value("1420");

    Fl_Box label_ipv6(10, 100, 100, 25, "IPv6:");
    Fl_Choice ipv6_choice(120, 100, 100, 25);
    ipv6_choice.add("Yes");
    ipv6_choice.add("No");
    ipv6_choice.value(0); // Default to "Yes"

    // Create the "Generate" button
    Fl_Button button_generate(100, 150, 100, 25, "Generate");

    // Create UserData instance and pass it to the callback
    UserData ud;
    ud.input_endpoint = &input_endpoint;
    ud.input_mtu = &input_mtu;
    ud.ipv6_choice = &ipv6_choice;
    button_generate.callback(generate_cb, &ud);

    window.end();
    window.show();

    return Fl::run();
}
