# mesh-beacon-ESP32

An ESP32-based beacon/client that connects to a Meshtastic unit (over BLE), listens for simple text commands coming from the mesh, and optionally responds back to the sender.  
This repository contains the firmware, PlatformIO configuration, and helper files used for development and deployment.

---

## What this code does

- Scans for and connects to a Meshtastic radio/peer over BLE using the configured **RADIO MAC**.
- Receives mesh messages, parses incoming text commands, and decides whether to respond.
- Sends responses back to the originating node when a command has a configured reply.
- Command handling and response formatting are implemented in `src/command_handler.cpp`.

**Runtime flow (high level):**
1. `setup()` initializes the serial port and calls `MeshInterface::setupBLE(RADIO_MAC_STR)`.
2. `loop()` calls `MeshInterface::loop()`, checks for incoming messages, decodes them, and passes the text to `CommandHandler::respondToCommand(...)`.
3. If the command handler returns a response, the firmware sends it back to the source address via `MeshInterface::sendMessage(...)`.

**Files of interest:**
- `src/main.cpp` – main loop and orchestration  
- `src/mesh_interface.*` – BLE / Meshtastic interaction layer  
- `src/command_handler.*` – command table and response logic

---

## Development environment

- Tested with **PlatformIO** (VS Code + PlatformIO extension).  
- Framework: **esp32/esp-idf Arduino** (configured via `platformio.ini`).  
- Developed primarily on **Windows**, verified on **Linux** and **macOS**.

> 🖥️ **Cross-platform note:**  
> All commands work on Windows, Linux, and macOS.  
> Replace Windows-style paths (`C:\Users\<user>\...`) with POSIX paths (`~/Documents/...`), and PowerShell commands with `bash` equivalents shown below.

---

## Quick reference – platform differences

| Platform | Typical Serial Port | Example Path | Python Command |
|-----------|--------------------|---------------|----------------|
| Windows   | `COM3`, `COM4`, …  | `C:\Users\<user>\Documents\GitHub\mesh-beacon-ESP32` | `python -m ...` |
| Linux     | `/dev/ttyUSB0` or `/dev/ttyACM0` | `~/Documents/GitHub/mesh-beacon-ESP32` | `python3 -m ...` |
| macOS     | `/dev/cu.usbserial-*` | `~/Documents/GitHub/mesh-beacon-ESP32` | `python3 -m ...` |

---

## Setting up the environment

1. **Install VS Code** and the **PlatformIO extension**, or install **PlatformIO Core CLI**.

```powershell
# Windows (PowerShell)
python -m pip install -U platformio
```

```bash
# Linux / macOS (bash)
python3 -m pip install -U platformio
```

2. **Open this repository folder** in VS Code (or `cd` into it from your terminal):

```powershell
cd "C:\Users\<user>\Documents\GitHub\mesh-beacon-ESP32"
```

```bash
cd ~/Documents/GitHub/mesh-beacon-ESP32
```

3. **Build the firmware:**

```powershell
# Windows
platformio run -e esp32-s3-devkitc-1-n16r8v
```

```bash
# Linux / macOS
pio run -e esp32-s3-devkitc-1-n16r8v
```

4. **Upload to a connected device:**

```powershell
platformio run -t upload -e esp32-s3-devkitc-1-n16r8v
```

```bash
pio run -t upload -e esp32-s3-devkitc-1-n16r8v
```

5. **Open the serial monitor (baud 115200):**

```powershell
# Windows
platformio device monitor -e esp32-s3-devkitc-1-n16r8v -b 115200
```

```bash
# Linux / macOS
pio device monitor -e esp32-s3-devkitc-1-n16r8v -b 115200 --port /dev/ttyUSB0
```

> **Note:**  
> - Windows uses `COM3`, `COM4`, etc.  
> - Linux uses `/dev/ttyUSB0` or `/dev/ttyACM0`.  
> - macOS uses `/dev/cu.usbserial-*`.  
> - On Linux, if you get a *permission denied* error, add your user to the `dialout` group:  
>   ```bash
>   sudo usermod -a -G dialout $USER
>   # then log out and back in
>   ```

---

## ESP32 board used (and what to update if different)

This repo’s `platformio.ini` includes an environment for an **ESP32-S3 DevKitC-1 (N16R8V)** board.  
If you use a different ESP32 variant, update your environment section in `platformio.ini`:

- Change `board = ...` to match your board (e.g., `esp32dev`, `esp32-s2-saola-1`).
- Adjust flash size, partition scheme, or PSRAM flags as needed.

Example change:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
```

The `RADIO_MAC_STR` define remains valid across boards — only board-specific BLE quirks may require adjustment.

---

## Setting your Meshtastic radio MAC address

The firmware expects your Meshtastic node’s **Bluetooth MAC address** defined as `RADIO_MAC_STR`.  
To avoid sharing your MAC on GitHub, set it in `MyConfig.ini` (not committed to the repo).

1. Find your Meshtastic radio’s Bluetooth MAC (via your phone or a BLE scanner app).  
2. Open `platformio.ini` and edit:

```ini
; Replace with your radio's MAC address
-D RADIO_MAC_STR="B0:82:84:9B:71:59"
```

3. Rebuild and upload the firmware.

---

## Meshtastic protobufs (nanopb setup)

This project depends on Meshtastic `.proto` files converted to compact **nanopb** C sources.  
You must generate these locally and place them in `src/meshtastic/`.

### Steps

1. **Get the `.proto` files** from the official [Meshtastic repository](https://github.com/meshtastic/protobufs), located in the `proto/` folder.  
2. **Install nanopb and protobuf:**

```powershell
# Windows
git clone https://github.com/nanopb/nanopb.git
python -m pip install --upgrade pip protobuf
```

```bash
# Linux / macOS
git clone https://github.com/nanopb/nanopb.git
python3 -m pip install --upgrade pip protobuf
```

3. **Run the generator:**

```powershell
# Windows / Linux / macOS (bash)
python -m nanopb.generator.nanopb_generator {check parameters and directories}
```

4. **Copy the outputs into the project:**

```powershell
Copy-Item .\out\*.pb.* .\src\meshtastic\
```

> The generated `.pb.c` and `.pb.h` files will compile alongside your `.cpp` files in PlatformIO.

---

## Configuring / updating responses

Responses are defined in `src/command_handler.cpp` inside a static `commandTable[]`. Example:

```cpp
static const CommandEntry commandTable[] = {
    {"/info",  "Welcome to jkpg-mesh.se", false},
    {"/help",  "Available commands: /info, /help, /about, /status, /signal", false},
    {"/about", "jkpg-mesh community project using Meshtastic and LoRa", false},
    {"/status","Node running normally", false},
    {"/signal","You were received Snr: %.2f and Rssi: %.2f", true}
};
```

- For static responses → edit the text string.  
- For dynamic responses (`isDynamic == true`) → the format string uses `rxSnr` and `rxRssi`.  
- Keep messages < 200 characters to avoid buffer overflow.

To add new commands:
1. Add an entry to `commandTable`.  
2. Rebuild and upload.

For more complex logic (parameters, multiple replies), extend `CommandHandler::respondToCommand(...)`.

---

## Local testing

1. **Upload firmware:**

```powershell
platformio run -t upload -e esp32-s3-devkitc-1-n16r8v
```

2. **Open serial monitor:**

```bash
pio device monitor -e esp32-s3-devkitc-1-n16r8v -b 115200
```

3. **Send a command** from another Meshtastic node (e.g., `/info`) and observe the response.

---

## Troubleshooting

- No BLE connection → verify `RADIO_MAC_STR` and ensure the target node is advertising.  
- Serial monitor shows nothing → check correct COM or `/dev` port and baud rate 115200.  
- BLE scanning issues → ensure Bluetooth permissions are enabled on your host.
- Meshtastic unit must have the bluetooth pairing mode set to "No Pin".

Logs use `Serial0` at 115200 baud.

---

## License

See the repository’s `LICENSE` file.
