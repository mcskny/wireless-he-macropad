



# Custom Wireless Hall Effect (HE) & Digital Macropad

[![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v6.0.2%2B-blue)](https://github.com/espressif/esp-idf)
[![Docker](https://img.shields.io/badge/Docker-Supported-2496ED?logo=docker&logoColor=white)](https://www.docker.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Status](https://img.shields.io/badge/Hardware-4--Layer%20PCB-orange)](#-hardware-specifications)

A high-performance, open-source **wireless mechanical macropad** that solves the battery drain issue of wireless Hall Effect keyboards. Featuring a **Hybrid Dual-Sensor Architecture (Analog HE + Digital Switches)**, MOSFET power gating, esports-grade algorithms (Rapid Trigger, SnapTap/SOCD), 4 hardware layers, and a high-efficiency **TPS6300x Buck-Boost power system**.

---


## Gallery & Preview

<!-- Project photos and renders -->
<table>
  <tr>
    <td align="center" width="50%">
      <img src="docs/images/macropad.png" style="border-radius: 10px;" height="350" alt="3D Render">
      <br>
      <b>3D Enclosure Render</b>
    </td>
    <td align="center" width="50%">
      <img src="docs/images/pcb.png" style="border-radius: 10px;" height="350" alt="PCB Design">
      <br>
      <b>4-Layer PCB Design</b>
    </td>
  </tr>
</table>

---

## The Core Innovation

Wireless Hall Effect (HE) keyboards traditionally suffer from severe battery drain due to the constant current consumption of linear magnetic Hall ICs (~3-5mA per key). 

This project solves the battery crisis by implementing a **MOSFET-gated Hybrid Power System**:
- When idle, power to energy-hungry analog Hall sensors is completely cut via MOSFET switches.
- Keypresses are detected instantly via ultra-low-power digital switches, which trigger the MOSFET to power up the analog sensors on demand.

---

## Key Features

###  Smart Power System & Operation Modes
- **MOSFET Power Gating:** Hardware-level power cutoff for analog Hall Effect ICs.
- **3 Dynamic Power Modes:**
  - 🟢 **Digital Mode:** Operates exclusively on digital switches for ultra-low power consumption and maximum battery longevity.
  - 🟡 **Hybrid Mode (Smart Wake):** Digital switches remain active; pressing any key instantly powers on the Analog HE sensors via MOSFETs for a configurable duration.
  - 🔴 **Aggressive Mode:** Analog HE sensors remain continuously powered for zero-latency competitive esports performance.

### Esports Performance Algorithms
- **Rapid Trigger:** Dynamic actuation and reset points based on real-time key travel distance.
- **SnapTap (SOCD):** Simultaneous Opposing Cardinal Directions prioritization for counter-strafing advantage in tactical shooters (CS2, Valorant).
- **ActivePoint:** Per-key customizable actuation depth via software.

### Deep Customization & 4 Hardware Layers
- **4 Hardware Layers:** Dedicated hardware layer button to toggle through 4 independent configurations.
- **Per-Layer Remapping:** Full customization over Rotary Encoder functions, Encoder Push Button, Switch keycodes, and Addressable RGB (Neopixel) LED animations per layer.
- **Web AP Portal (`config_ap`):** Onboard Wi-Fi Access Point portal for web-based configuration.

### Hardware & Power Circuitry
- **TPS6300x Buck-Boost Converter:** High-efficiency power regulation supplying a rock-solid 3.3V rail across the entire discharge curve of an 18650 Li-Ion battery (4.2V down to 3.0V).
- **18650 Li-Ion Power:** Powered by a high-capacity rechargeable 18650 cell.
- **Fast Charging:** Integrated Li-Ion charger supporting up to 1A charge current (scalable to 2A on future PCB revisions).
- **4-Layer PCB Stackup:** Optimized trace routing, power planes, and EMI/EMC shielding.

### Dual Microcontroller Architecture
- **📡 Transmitter / Keyboard (ESP32-C6):**
  - Powered by the ultra-low-power **ESP32-C6 (RISC-V 32-bit single-core CPU)**.
  - Supports Wi-Fi 6 (802.11ax), Bluetooth 5.3 (LE), and 802.15.4 for ultra-efficient wireless data transmission.
  - Optimized sleep modes for long battery life.

- **🔌 Receiver / Dongle (ESP32-S3):**
  - Powered by the high-performance **ESP32-S3 (Dual-core Xtensa LX7 CPU)**.
  - Features **Native USB OTG / Full-Speed USB Controller** to act as a hardware-level USB HID Keyboard to the host PC without needing USB-to-UART conversion chips.

---

## Repository & Component Architecture

This repository uses a **modular C architecture** built on top of the ESP-IDF framework:

```text
wireless-he-macropad/
├── docs/                           # Documentation & Images
│   └── images/                     # Photos, PCB diagrams, 3D renders
├── firmware/                       # ESP-IDF Firmware Codebase
│   ├── common/                     # Shared protocol definitions (keyboard_protocol.h)
│   ├── macropad-sender/            # Keyboard (Transmitter) Firmware
│   └── dongle-receiver/            # USB Dongle (Receiver) Firmware
├── components/                     # Modular C Custom Engines
│   ├── hall_effect_engine/        # Magnetic switch sensing & Rapid Trigger algorithms
│   ├── layer_manager/             # 4-Layer switching logic (Keys, Encoder, RGB)
│   ├── macro_engine/              # Custom macro execution engine
│   ├── espnow_rx/                 # Low-latency 2.4GHz ESP-NOW wireless module
│   ├── config_ap/                 # Web-based Access Point configuration portal
│   ├── usb_hid/                   # USB HID Keyboard class implementation
│   ├── usb_power/                 # Power gating, battery monitoring, sleep logic
│   └── watchdog/                  # System safety & task watchdog monitors
├── hardware/                       # KiCad PCB Files & Gerber Exports
│   ├── macropad.kicad_pro
│   ├── macropad.kicad_sch
│   ├── macropad.kicad_pcb
│   └── gerbers/                    # Production Gerber zip files
├── build_docker.bat                # Windows Docker build script
└── build_docker.sh                 # Linux/macOS Docker build script
```

---

## How to Build Firmware (Using Docker)

No local ESP-IDF toolchain installation is required. Firmware for both the **Transmitter** and **Receiver** can be built inside a clean, containerized Docker environment.

### Prerequisites
- [Docker Desktop](https://www.docker.com/products/docker-desktop/) installed and running.

### 1. Build Both Firmwares
Run the automated build script for your operating system:

**Windows (CMD / PowerShell):**
```cmd
build_docker.bat
```

**Linux / macOS:**
```bash
chmod +x build_docker.sh
./build_docker.sh
```

### 2. Clean Build Artifacts
To clear temporary CMake build caches:
```cmd
build_docker.bat clean
```

The compiled binary files (`.bin`) will be generated inside:
- **Transmitter:** `firmware/macropad-sender/build/`
- **Receiver:** `firmware/dongle-receiver/build/`

---

## How to Flash Firmware to Hardware

You can flash the generated binary files (`.bin`) to your ESP32 boards using either of the following methods:

### Method 1: Web-Based Flasher (Recommended - No Install Needed)
1. Open **[Espressif Web Serial Flasher](https://espressif.github.io/esptool-js/)** in Google Chrome or Microsoft Edge.
2. Connect your ESP32 board via USB and click **Connect**.
3. Select the generated binary files from your `build/` folder:
   - `0x1000` -> `bootloader.bin`
   - `0x8000` -> `partition-table.bin`
   - `0x10000` -> `macropad-sender.bin` (or `dongle-receiver.bin`)
4. Click **Program**.

### Method 2: Command Line (`esptool.py`)
If you have Python and `esptool` installed:
```bash
pip install esptool

# Flash Transmitter (Replace COM3 with your port)
esptool.py -p COM3 -b 921600 write_flash 0x10000 firmware/macropad-sender/build/macropad-sender.bin

# Flash Receiver
esptool.py -p COM4 -b 921600 write_flash 0x10000 firmware/dongle-receiver/build/dongle-receiver.bin
```

---

## 🤝 Contributing & License

Contributions, issues, and feature requests are welcome! Feel free to check the [issues page](../../issues).

Distributed under the **MIT License**. See `LICENSE` for more information.

---
