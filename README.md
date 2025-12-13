
# RedWARP GUI 🚀

[![C++](https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![FLTK](https://img.shields.io/badge/FLTK-Lightweight_GUI-4B8BBE?style=for-the-badge)](https://www.fltk.org/)
[![Cloudflare WARP](https://img.shields.io/badge/Cloudflare-WARP-FF6A00?style=for-the-badge&logo=cloudflare)](https://www.cloudflare.com/products/zero-trust/warp-client/)

**A lightweight graphical interface for Cloudflare WARP with AmnesiaWG support.**

RedWARP_GUI is a fast and minimal GUI client for Cloudflare WARP, rewritten in C++ using the FLTK toolkit. It integrates AmnesiaWG (an anonymity-focused WireGuard fork) for enhanced privacy.

Perfect for users who want a simple, resource-efficient way to connect to WARP without heavy dependencies.

## ✨ Features

- **One-click connection**: Connect/disconnect with ease.
- **AmnesiaWG integration**: Better anonymity than standard WireGuard.
- **Ultra-lightweight**: Built with C++ and FLTK — minimal CPU/RAM usage.
- **Cross-platform potential**: Primarily tested on Linux, easily portable.
- **Open source**: Fully customizable and transparent.

## 🛠 Installation

### Prerequisites
- Linux distribution (Ubuntu/Debian recommended)
- Cloudflare WARP CLI installed: Follow the [official guide](https://developers.cloudflare.com/warp-client/setting-up/linux/)
- Build tools: `g++`, `make`, and FLTK development libraries

```bash
sudo apt update
sudo apt install libfltk1.3-dev g++ make
```

### Build & Run
```bash
git clone https://github.com/meizfl/RedWARP_GUI.git
cd RedWARP_GUI
make          # Compiles the binary
./RedWARP_GUI # Launch the app
```

## 🚀 Usage

1. Launch the application.
2. Click **Connect** to start WARP via AmnesiaWG.
3. Click **Disconnect** to stop.
4. Status indicators show connection state in real-time.

> Tip: Run with sudo if you encounter permission issues with WARP interfaces.

*(Add screenshots here for extra wow-factor!)*

## 🤝 Contributing

Found a bug? Have an idea? Fork it, hack it, send a pull request!  
Contributions are very welcome — let's make the best WARP GUI out there 🔥

## 📄 License

This project is licensed under the GNU General Public License v3.0 — see the LICENSE file for details.

## ⭐ Star this project!

If you like it, hit that star button — it means the world! 🌟

---

Made with ❤️ and a bit of anonymity by [@meizfl](https://github.com/meizfl)
