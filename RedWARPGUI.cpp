#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <vector>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>

// Cross-platform process / filesystem
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <io.h>       // _chmod
#  include <sys/stat.h> // _S_IEXEC
#else
#  include <unistd.h>
#  include <sys/wait.h>
#  include <sys/stat.h>
#endif

// FLTK
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Input.H>
#include <FL/Fl_Choice.H>
#include <FL/Fl_Box.H>
#include <FL/fl_ask.H>

using namespace std;
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Platform detection helpers
// ---------------------------------------------------------------------------
#ifdef _WIN32
#  define PLATFORM_WINDOWS 1
#  define EXE_EXT ".exe"
#else
#  define PLATFORM_WINDOWS 0
#  define EXE_EXT ""
#endif

#if defined(__x86_64__) || defined(_M_X64)
#  define ARCH_STR "amd64"
#elif defined(__aarch64__) || defined(_M_ARM64)
#  define ARCH_STR "arm64"
#elif defined(__arm__) || defined(_M_ARM)
#  define ARCH_STR "armv7"
#else
#  define ARCH_STR "386"
#endif

#if PLATFORM_WINDOWS
#  define OS_STR "windows"
#elif defined(__APPLE__)
#  define OS_STR "darwin"
#else
#  define OS_STR "linux"
#endif

// ---------------------------------------------------------------------------
// Widget struct
// ---------------------------------------------------------------------------
struct UserData {
    Fl_Input*  input_endpoint;
    Fl_Input*  input_mtu;
    Fl_Choice* ipv6_choice;
    Fl_Choice* amnezia_choice;
    Fl_Choice* randomize_amnezia_choice;
    Fl_Choice* dns_ipv4_choice;
    Fl_Choice* dns_ipv6_choice;
    Fl_Input*  input_custom_dns_ipv4;
    Fl_Input*  input_custom_dns_ipv6;
};

// Global state
string custom_endpoint, custom_mtu;
char   ipv6_enabled      = 'y';
bool   amnezia_enabled   = true;
bool   randomize_amnezia = false;
string selected_dns_ipv4, selected_dns_ipv6;

// ---------------------------------------------------------------------------
// Random helpers
// ---------------------------------------------------------------------------
static mt19937& rng() {
    static random_device rd;
    static mt19937 gen(rd());
    return gen;
}

int random_int(int lo, int hi) {
    uniform_int_distribution<int> d(lo, hi);
    return d(rng());
}

uint32_t random_uint32(uint32_t lo = 1u, uint32_t hi = 0xFFFFFFFFu) {
    uniform_int_distribution<uint32_t> d(lo, hi);
    return d(rng());
}

vector<uint8_t> rand_bytes(size_t n) {
    uniform_int_distribution<unsigned> d(0, 255);
    vector<uint8_t> out(n);
    for (auto& b : out) b = static_cast<uint8_t>(d(rng()));
    return out;
}

string to_hex(const vector<uint8_t>& bytes) {
    ostringstream ss;
    ss << hex << setfill('0');
    for (uint8_t b : bytes) ss << setw(2) << (unsigned)b;
    return ss.str();
}

string rand_hex(size_t n)  { return to_hex(rand_bytes(n)); }

string rand_ip() {
    return to_string(random_int(10,239)) + "." +
           to_string(random_int(1,254))  + "." +
           to_string(random_int(1,254))  + "." +
           to_string(random_int(1,254));
}

int rand_port() { return random_int(1024, 65534); }

template<typename T>
const T& pick(const vector<T>& v) { return v[random_int(0,(int)v.size()-1)]; }

// ---------------------------------------------------------------------------
// run_command – shell-free on both platforms
// Windows: CreateProcess   Linux/macOS: fork+execv
// ---------------------------------------------------------------------------
bool run_command(const string& exe, const vector<string>& args) {
#if PLATFORM_WINDOWS
    // Build a properly-quoted command line for CreateProcess
    // Each token is wrapped in double-quotes; internal quotes are escaped.
    auto quote_arg = [](const string& s) -> string {
        string out = "\"";
        for (char c : s) {
            if (c == '"') out += "\\\"";
            else          out += c;
        }
        out += '"';
        return out;
    };

    string cmdline = quote_arg(exe);
    for (const auto& a : args) cmdline += " " + quote_arg(a);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(
            nullptr,
            cmdline.data(),   // mutable copy
            nullptr, nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr, nullptr,
            &si, &pi))
        return false;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return exit_code == 0;

#else
    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        vector<const char*> argv;
        argv.push_back(exe.c_str());
        for (const auto& a : args) argv.push_back(a.c_str());
        argv.push_back(nullptr);
        execv(exe.c_str(), const_cast<char* const*>(argv.data()));
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

// ---------------------------------------------------------------------------
// find_curl – returns full path to curl or empty string
// ---------------------------------------------------------------------------
string find_curl() {
#if PLATFORM_WINDOWS
    // Try PATH via where.exe, fall back to common locations
    static const vector<string> CANDIDATES = {
        "C:\\Windows\\System32\\curl.exe",
        "C:\\Program Files\\Git\\mingw64\\bin\\curl.exe",
        "C:\\ProgramData\\chocolatey\\bin\\curl.exe"
    };
    for (const auto& p : CANDIDATES)
        if (fs::exists(p)) return p;
    // Last resort: rely on PATH (may work on Win10 1803+)
    return "curl.exe";
#else
    static const vector<string> CANDIDATES = {
        "/usr/bin/curl",
        "/usr/local/bin/curl",
        "/opt/homebrew/bin/curl"  // macOS Homebrew arm64
    };
    for (const auto& p : CANDIDATES)
        if (fs::exists(p)) return p;
    return "curl";  // hope it's in PATH
#endif
}

// ---------------------------------------------------------------------------
// make_executable – chmod +x (no-op on Windows; Explorer handles .exe)
// ---------------------------------------------------------------------------
void make_executable(const string& path) {
#if PLATFORM_WINDOWS
    // Windows executability is determined by extension – nothing to do
    (void)path;
#else
    struct stat st{};
    if (stat(path.c_str(), &st) == 0)
        chmod(path.c_str(), st.st_mode | S_IXUSR | S_IXGRP | S_IXOTH);
#endif
}

// ---------------------------------------------------------------------------
// download_latest_wgcf (ported from F# downloadLatestWgcf)
// ---------------------------------------------------------------------------
string download_latest_wgcf(const string& bin_dir) {
    const string curl     = find_curl();
    const string os_name  = OS_STR;
    const string arch     = ARCH_STR;
    const string filename = string("wgcf_") + os_name + "_" + arch + EXE_EXT;
    const string target   = bin_dir + "/" + filename;
    const string json_tmp = bin_dir + "/wgcf_release.json";

    const string api_url =
        "https://api.github.com/repos/ViRb3/wgcf/releases/latest";

    if (!run_command(curl,
            {"-fsSL", "-A", "RedWARP-Generator", "-o", json_tmp, api_url})) {
        return {};
    }

    ifstream jf(json_tmp);
    string json((istreambuf_iterator<char>(jf)),
                 istreambuf_iterator<char>());
    jf.close();
    fs::remove(json_tmp);

    // Find browser_download_url matching our os+arch
    string download_url;
    size_t pos = 0;
    while ((pos = json.find("browser_download_url", pos)) != string::npos) {
        size_t s = json.find('"', pos + 22);
        if (s == string::npos) break;
        size_t e = json.find('"', s + 1);
        if (e == string::npos) break;
        string url = json.substr(s + 1, e - s - 1);
        if (url.find(os_name) != string::npos &&
            url.find(arch)    != string::npos) {
            download_url = url;
            break;
        }
        pos = e;
    }

    if (download_url.empty()) return {};

    if (!run_command(curl,
            {"-fsSL", "-A", "RedWARP-Generator", "-o", target, download_url}))
        return {};

    make_executable(target);
    return target;
}

// ---------------------------------------------------------------------------
// ensure_wgcf_exists (ported from F# ensureWgcfExists)
// ---------------------------------------------------------------------------
string ensure_wgcf_exists() {
    const string bin_dir = "./bin";
    fs::create_directories(bin_dir);

    for (const auto& entry : fs::directory_iterator(bin_dir)) {
        const string name = entry.path().filename().string();
        if (name.rfind("wgcf", 0) == 0) {
            make_executable(entry.path().string());
            return entry.path().string();
        }
    }

    return download_latest_wgcf(bin_dir);
}

// ---------------------------------------------------------------------------
// Junk-packet generators (ported from F# Program.fs)
//   I1 = SIP REGISTER   I2 = TLS ClientHello  I3 = TLS ServerHello
//   I4 = TLS AppData    I5 = HTTP GET
// ---------------------------------------------------------------------------

static const vector<string> POPULAR_DOMAINS = {
    "google.com","youtube.com","cloudflare.com","apple.com","microsoft.com",
    "facebook.com","instagram.com","whatsapp.com","wikipedia.org","amazon.com",
    "bing.com","reddit.com","chatgpt.com","netflix.com","tiktok.com",
    "akamai.com","fastly.com","yandex.ru","vk.com","mail.ru",
    "dzen.ru","ozon.ru","wildberries.ru","avito.ru","gosuslugi.ru",
    "sber.ru","vkontakte.ru","ok.ru","rambler.ru","ria.ru",
    "baidu.com","qq.com","taobao.com","weibo.com","163.com",
    "alibaba.com","tmall.com","jd.com","douyin.com","sina.com.cn",
    "tencent.com","pinduoduo.com","ximalaya.com","yahoo.com","linkedin.com",
    "twitch.tv","spotify.com","adobe.com","ebay.com","paypal.com",
    "booking.com","airbnb.com","aliexpress.com","huawei.com","samsung.com",
    "sony.com","nvidia.com","intel.com","oracle.com","ibm.com",
    "zoom.us","discord.com","telegram.org","github.com","stackoverflow.com",
    "medium.com","quora.com","bbc.com","cnn.com","nytimes.com",
    "washingtonpost.com","naver.com","daum.net","line.me"
};

string wrap_packet(const vector<uint8_t>& data) {
    return "<b 0x" + to_hex(data) + ">";
}

// I1: SIP REGISTER
string make_sip_register() {
    auto ip      = rand_ip();
    int  srcPort = rand_port();
    auto branch  = rand_hex(26);
    auto callId  = rand_hex(16);
    auto fromTag = rand_hex(8);
    auto domain  = pick(POPULAR_DOMAINS);
    int  expires = random_int(3600, 7200);
    int  cseq    = random_int(1, 9);

    ostringstream body;
    body << "REGISTER sip:" << domain << " SIP/2.0\r\n"
         << "Via: SIP/2.0/UDP " << ip << ":" << srcPort
             << ";branch=z9hG4bK" << branch << "\r\n"
         << "Max-Forwards: 70\r\n"
         << "To: <sip:user@" << domain << ">\r\n"
         << "From: <sip:user@" << domain << ">;tag=" << fromTag << "\r\n"
         << "Call-ID: " << callId << "\r\n"
         << "CSeq: " << cseq << " REGISTER\r\n"
         << "Contact: <sip:user@" << ip << ":" << srcPort << ">\r\n"
         << "User-Agent: Bria 5.0.0\r\n"
         << "Expires: " << expires << "\r\n"
         << "Content-Length: 0\r\n\r\n";

    string s = body.str();
    return wrap_packet(vector<uint8_t>(s.begin(), s.end()));
}

// I2: TLS ClientHello
string make_tls_client_hello() {
    auto sni      = pick(POPULAR_DOMAINS);
    auto sniBytes = vector<uint8_t>(sni.begin(), sni.end());
    size_t sniLen = sniBytes.size();
    auto clientRandom = rand_bytes(32);

    static const vector<uint16_t> ALL_CIPHERS = {
        0xC02B,0xC02C,0xCCA8,0xCCA9,0xC013,0xC014,0x009C,0x009D
    };
    int numCiphers = random_int(2, 4);
    vector<uint16_t> ciphers(numCiphers);
    for (auto& c : ciphers) c = pick(ALL_CIPHERS);

    vector<uint8_t> sniExt = {
        0x00, 0x00,
        0x00, uint8_t(sniLen + 5),
        0x00, uint8_t(sniLen + 3),
        0x00,
        0x00, uint8_t(sniLen)
    };
    sniExt.insert(sniExt.end(), sniBytes.begin(), sniBytes.end());

    vector<uint8_t> sgExt = {0x00,0x0A,0x00,0x0A,0x00,0x08,
                              0x7B,0x88,0x65,0x2C,0xE4,0x6B,0x47,0xAB};
    vector<uint8_t> epExt = {0x00,0x0B,0x00,0x04,0x03,0x00,0x01,0x02};

    vector<uint8_t> exts;
    for (auto& e : {sniExt, sgExt, epExt})
        exts.insert(exts.end(), e.begin(), e.end());

    vector<uint8_t> cipherBytes;
    for (auto c : ciphers) {
        cipherBytes.push_back(uint8_t(c >> 8));
        cipherBytes.push_back(uint8_t(c));
    }

    vector<uint8_t> helloBody;
    helloBody.insert(helloBody.end(), clientRandom.begin(), clientRandom.end());
    helloBody.push_back(0x00);
    helloBody.push_back(0x00);
    helloBody.push_back(uint8_t(cipherBytes.size()));
    helloBody.insert(helloBody.end(), cipherBytes.begin(), cipherBytes.end());
    helloBody.push_back(0x01); helloBody.push_back(0x00);
    helloBody.push_back(0x00); helloBody.push_back(uint8_t(exts.size()));
    helloBody.insert(helloBody.end(), exts.begin(), exts.end());

    uint16_t helloLen = uint16_t(helloBody.size());
    vector<uint8_t> handshake = {
        0x01, 0x00, 0x00, uint8_t(helloLen), 0x03, 0x03
    };
    handshake.insert(handshake.end(), helloBody.begin(), helloBody.end());

    uint16_t hsLen = uint16_t(handshake.size());
    vector<uint8_t> record = {
        0x16, 0x03, 0x03, uint8_t(hsLen >> 8), uint8_t(hsLen)
    };
    record.insert(record.end(), handshake.begin(), handshake.end());
    return wrap_packet(record);
}

// I3: TLS ServerHello
string make_tls_server_hello() {
    auto serverRandom = rand_bytes(32);
    static const vector<uint16_t> CIPHERS = {
        0xC02F,0xC030,0xCCA8,0x009C,0x009D,0xC013,0xC014
    };
    uint16_t cipher = pick(CIPHERS);

    vector<uint8_t> handshake = {
        0x02, 0x00, 0x00, uint8_t(serverRandom.size() + 4),
        0x03, 0x03
    };
    handshake.insert(handshake.end(), serverRandom.begin(), serverRandom.end());
    handshake.push_back(0x00);
    handshake.push_back(uint8_t(cipher >> 8));
    handshake.push_back(uint8_t(cipher));
    handshake.push_back(0x00);

    uint16_t hsLen = uint16_t(handshake.size());
    vector<uint8_t> record = {
        0x16, 0x03, 0x03, uint8_t(hsLen >> 8), uint8_t(hsLen)
    };
    record.insert(record.end(), handshake.begin(), handshake.end());
    return wrap_packet(record);
}

// I4: TLS AppData – DHE KeyExchange + ChangeCipherSpec + Finished
string make_tls_appdata() {
    auto dhPart = rand_bytes(128);
    vector<uint8_t> keyExBody = {0x10, 0x00, 0x00, 0x80};
    keyExBody.insert(keyExBody.end(), dhPart.begin(), dhPart.end());

    vector<uint8_t> keyExRec = {
        0x16, 0x03, 0x03, 0x00, uint8_t(keyExBody.size())
    };
    keyExRec.insert(keyExRec.end(), keyExBody.begin(), keyExBody.end());

    vector<uint8_t> ccsRec = {0x14, 0x03, 0x03, 0x00, 0x01, 0x01};

    auto finData = rand_bytes(52);
    vector<uint8_t> finRec = {
        0x16, 0x03, 0x03, 0x00, uint8_t(finData.size())
    };
    finRec.insert(finRec.end(), finData.begin(), finData.end());

    vector<uint8_t> full;
    for (auto& part : {keyExRec, ccsRec, finRec})
        full.insert(full.end(), part.begin(), part.end());

    return wrap_packet(full);
}

// I5: HTTP GET
string make_http_get() {
    static const vector<string> PATHS = {
        "/mail","/search","/index.html","/api/v1/status","/favicon.ico","/"
    };
    static const vector<string> UAS = {
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:149.0) Gecko/20100101 Firefox/149.0",
        "Mozilla/5.0 (iPhone; CPU iPhone OS 18_7_7 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/26.0 Mobile/15E148 Safari/604.1",
        "Mozilla/5.0 (Linux; Android 15; SM-S931B) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.7680.178 Mobile Safari/537.36",
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36 Edg/146.0.0.0",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/146.0.0.0 Safari/537.36"
    };

    string host = pick(POPULAR_DOMAINS);
    string path = pick(PATHS);
    string ua   = pick(UAS);

    ostringstream body;
    body << "GET " << path << " HTTP/1.1\r\n"
         << "Host: " << host << "\r\n"
         << "User-Agent: " << ua << "\r\n"
         << "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"
         << "Accept-Language: en-US,en;q=0.5\r\n"
         << "Accept-Encoding: gzip, deflate, br\r\n"
         << "Connection: keep-alive\r\n\r\n";

    string s = body.str();
    return wrap_packet(vector<uint8_t>(s.begin(), s.end()));
}

struct JunkPackets { string i1, i2, i3, i4, i5; };

JunkPackets generate_junk_packets() {
    return {
        make_sip_register(),
        make_tls_client_hello(),
        make_tls_server_hello(),
        make_tls_appdata(),
        make_http_get()
    };
}

// ---------------------------------------------------------------------------
// generate_config
// ---------------------------------------------------------------------------
void generate_config() {
    string wgcf_path = ensure_wgcf_exists();
    if (wgcf_path.empty()) {
        fl_alert("wgcf binary not found and could not be downloaded.\n"
                 "Place wgcf" EXE_EXT " in ./bin/ or check your internet connection.");
        return;
    }

    if (fs::exists("RedWARP.conf"))      fs::remove("RedWARP.conf");
    if (fs::exists("wgcf-account.toml")) fs::remove("wgcf-account.toml");

    if (!run_command(wgcf_path, {"register", "--accept-tos"})) {
        fl_alert("Error running: wgcf register --accept-tos");
        return;
    }
    if (!run_command(wgcf_path, {"generate"})) {
        fl_alert("Error running: wgcf generate");
        return;
    }
    if (!fs::exists("wgcf-profile.conf")) {
        fl_alert("wgcf-profile.conf not found after generate.");
        return;
    }

    ifstream infile("wgcf-profile.conf");
    ofstream outfile("wgcf-profile.conf.new");

    auto junk = generate_junk_packets();

    string line;
    bool interface_section = false;

    while (getline(infile, line)) {
        if (line.find("[Interface]") == 0)
            interface_section = true;
        else if (!line.empty() && line[0] == '[')
            interface_section = false;

        if (ipv6_enabled == 'n') {
            size_t pos;
            if (line.find("Address = ") == 0) {
                pos = line.find(',');
                if (pos != string::npos) line = line.substr(0, pos);
            }
            while ((pos = line.find(", ::/0")) != string::npos) line.erase(pos, 6);
            while ((pos = line.find(",::/0"))  != string::npos) line.erase(pos, 5);
            while ((pos = line.find(", 2606:4700")) != string::npos) line.erase(pos, 50);
        }

        if (interface_section && line.find("PrivateKey =") == 0 && amnezia_enabled) {
            outfile << line << "\n";

            int Jc   = randomize_amnezia ? random_int(1, 128) : 4;
            int Jmin = randomize_amnezia ? random_int(1, 400) : 40;
            int Jmax = randomize_amnezia ? random_int(Jmin + 1, 1280) : 70;
            uint32_t h3 = random_uint32(2073986817u, 2147128181u);

            outfile << "S1 = 0\n"
                    << "S2 = 0\n"
                    << "Jc = "   << Jc   << "\n"
                    << "Jmin = " << Jmin << "\n"
                    << "Jmax = " << Jmax << "\n";

            if (randomize_amnezia) {
                outfile << "H1 = " << (random_uint32() % 4 + 1) << "\n"
                        << "H2 = " << (random_uint32() % 4 + 1) << "\n"
                        << "H3 = " << h3                         << "\n"
                        << "H4 = " << (random_uint32() % 4 + 1) << "\n";
            } else {
                outfile << "H1 = 1\nH2 = 2\nH3 = " << h3 << "\nH4 = 4\n";
            }

            outfile << "I1 = " << junk.i1 << "\n"
                    << "I2 = " << junk.i2 << "\n"
                    << "I3 = " << junk.i3 << "\n"
                    << "I4 = " << junk.i4 << "\n"
                    << "I5 = " << junk.i5 << "\n";

        } else if (line.find("MTU = ") == 0) {
            outfile << "MTU = " << custom_mtu << "\n";
        } else if (line.find("Endpoint = ") == 0) {
            outfile << "Endpoint = " << custom_endpoint << "\n";
        } else if (line.find("DNS = ") == 0) {
            outfile << "DNS = " << selected_dns_ipv4;
            if (ipv6_enabled == 'y')
                outfile << ", " << selected_dns_ipv6;
            outfile << "\n";
        } else {
            outfile << line << "\n";
        }
    }

    infile.close();
    outfile.close();

    fs::remove("wgcf-profile.conf");
    fs::rename("wgcf-profile.conf.new", "RedWARP.conf");

    ifstream checkfile("RedWARP.conf");
    string content((istreambuf_iterator<char>(checkfile)),
                    istreambuf_iterator<char>());
    checkfile.close();

    if (content.find("MTU = " + custom_mtu)          != string::npos &&
        content.find("Endpoint = " + custom_endpoint) != string::npos &&
        content.find("DNS = " + selected_dns_ipv4)    != string::npos) {
        fl_alert("Configuration successfully updated and saved to RedWARP.conf!");
    } else {
        fl_alert("An error occurred while updating the configuration.");
    }
}

// ---------------------------------------------------------------------------
// FLTK callbacks
// ---------------------------------------------------------------------------
void generate_cb(Fl_Widget*, void* data) {
    UserData* ud = (UserData*)data;

    custom_endpoint   = ud->input_endpoint->value();
    custom_mtu        = ud->input_mtu->value();
    ipv6_enabled      = ud->ipv6_choice->value() == 0 ? 'y' : 'n';
    amnezia_enabled   = (ud->amnezia_choice->value() == 0);
    randomize_amnezia = (ud->randomize_amnezia_choice->value() == 0);

    static const string dns_ipv4_opts[] = {
        "208.67.222.222, 208.67.220.220",
        "1.1.1.1, 1.0.0.1",
        "8.8.8.8, 8.8.4.4",
        "9.9.9.9, 149.112.112.112",
    };
    static const string dns_ipv6_opts[] = {
        "2620:119:35::35, 2620:119:53::53",
        "2606:4700:4700::1111, 2606:4700:4700::1001",
        "2001:4860:4860::8888, 2001:4860:4860::8844",
        "2620:fe::fe, 2620:fe::9",
    };

    int v4idx = ud->dns_ipv4_choice->value();
    int v6idx = ud->dns_ipv6_choice->value();

    selected_dns_ipv4 = (v4idx == 4) ? ud->input_custom_dns_ipv4->value()
                                      : dns_ipv4_opts[v4idx];
    selected_dns_ipv6 = (v6idx == 4) ? ud->input_custom_dns_ipv6->value()
                                      : dns_ipv6_opts[v6idx];

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

void dns_ipv4_choice_cb(Fl_Widget* w, void* data) {
    auto* ud = static_cast<UserData*>(data);
    if (static_cast<Fl_Choice*>(w)->value() == 4)
        ud->input_custom_dns_ipv4->activate();
    else
        ud->input_custom_dns_ipv4->deactivate();
}

void dns_ipv6_choice_cb(Fl_Widget* w, void* data) {
    auto* ud = static_cast<UserData*>(data);
    if (static_cast<Fl_Choice*>(w)->value() == 4 && ipv6_enabled == 'y')
        ud->input_custom_dns_ipv6->activate();
    else
        ud->input_custom_dns_ipv6->deactivate();
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main() {
    Fl_Window window(400, 345, "RedWARP Config Generator");

    Fl_Box   label_endpoint(10, 20, 100, 25, "Endpoint:");
    Fl_Input input_endpoint(120, 20, 270, 25);
    input_endpoint.value("162.159.192.1:4500");

    Fl_Box   label_mtu(10, 60, 100, 25, "MTU:");
    Fl_Input input_mtu(120, 60, 270, 25);
    input_mtu.value("1420");

    Fl_Box    label_ipv6(10, 100, 100, 25, "IPv6:");
    Fl_Choice ipv6_choice(120, 100, 150, 25);
    ipv6_choice.add("Yes"); ipv6_choice.add("No");
    ipv6_choice.value(0);

    Fl_Box    label_amnezia(10, 140, 100, 25, "AmneziaWG:");
    Fl_Choice amnezia_choice(120, 140, 150, 25);
    amnezia_choice.add("Yes"); amnezia_choice.add("No");
    amnezia_choice.value(0);

    Fl_Box    label_randomize(10, 180, 120, 25, "Randomize:");
    Fl_Choice randomize_amnezia_choice(120, 180, 120, 25);
    randomize_amnezia_choice.add("Yes"); randomize_amnezia_choice.add("No");
    randomize_amnezia_choice.value(1);

    Fl_Box    label_dns_ipv4(10, 220, 100, 25, "DNS IPv4:");
    Fl_Choice dns_ipv4_choice(120, 220, 150, 25);
    dns_ipv4_choice.add("OpenDNS"); dns_ipv4_choice.add("Cloudflare");
    dns_ipv4_choice.add("Google");  dns_ipv4_choice.add("Quad9");
    dns_ipv4_choice.add("Custom");
    dns_ipv4_choice.value(0);

    Fl_Input input_custom_dns_ipv4(280, 220, 110, 25);
    input_custom_dns_ipv4.deactivate();

    Fl_Box    label_dns_ipv6(10, 260, 100, 25, "DNS IPv6:");
    Fl_Choice dns_ipv6_choice(120, 260, 150, 25);
    dns_ipv6_choice.add("OpenDNS"); dns_ipv6_choice.add("Cloudflare");
    dns_ipv6_choice.add("Google");  dns_ipv6_choice.add("Quad9");
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
