# V-Bot Climbing Gondola

A custom C++ ESP32 firmware for a V-Bot (Wall Plotter / Polargraph) climbing gondola. This project turns an ESP32-S3 SuperMini into a standalone wireless CNC controller for drawing large formats on walls or whiteboards.

## Features

- **Wall Plotter Kinematics:** Converts standard cartesian Cartesian G-code (X/Y) into left/right string lengths.
- **Standalone WiFi AP:** Creates its own `V-Bot` WiFi network, no external router needed.
- **Web Interface:** Built-in web server for uploading G-Code, jogging motors, viewing logs, and sending commands.
- **Accurate Motion Control:** Uses `AccelStepper` for smooth, accelerated motor movements.
- **G-Code parsing:** Supports standard G-code commands (G0, G1, G4, M3, M5, etc.).
- **Servo Pen Lift:** Automatic Z-axis mapping to a PWM servo for lifting and lowering the pen.
- **Persistent Settings:** Configurable anchor widths, feed rates, and machine params saved to flash.
- **Over-The-Air (OTA) Updates:** Update firmware directly over WiFi using ElegantOTA.
- **Status LED:** NeoPixel integration for visual machine state (Idle, Running, Error).

## Hardware Requirements

- **ESP32-S3 SuperMini** (or similar ESP32-S3 development board)
- Two Stepper Motors with drivers (e.g., A4988 / TMC2209)
- One standard RC Servo for the pen lift mechanism
- Power Supply (appropriate for your stepper motors)

### Pinout (Default Configuration)
*Refer to `config.yaml` or `src/config.h` to change these default pins.*

- **Left Motor (X)**: Step PIN: `gpio.1`, Dir PIN: `gpio.2`
- **Right Motor (Y)**: Step PIN: `gpio.4`, Dir PIN: `gpio.5`
- **Servo (Z / Pen)**: PWM PIN: `gpio.6`
- **Stepper Enable**: PIN `gpio.7` (Active Low)

## Installation & Flashing

This project is built using [PlatformIO](https://platformio.org/).

1. Clone or download the repository.
2. Open the project folder in VSCode with the PlatformIO extension installed.
3. Connect your ESP32-S3 SuperMini via USB.
4. Click the upload button (➔) in PlatformIO. 
**Note:** It will automatically install dependent libraries (AccelStepper, ArduinoJson, ESPAsyncWebServer, etc.).

## Usage

1. **Power On:** Turn on the V-Bot. Wait a few seconds for it to boot.
2. **Connect to WiFi:** From your phone or computer, connect to the V-Bot network:
   - SSID: `V-Bot`
   - Password: `vbot1234`
3. **Access WebUI:** Open a web browser and navigate to `http://192.168.4.1`.
4. **Calibration:** Set your gondola on the wall. Use the Web UI to define the proper distance between your two top motor anchors.
5. **Draw:** Upload a `.gcode` file through the Web Interface to start plotting!

## G-Code Generator

A standalone utility is provided in the repository to generate shapes and text:
- Open `gcode_generator.html` in any modern web browser.
- Adjust dimensions, center points, and settings.
- Export as standard `.gcode` and upload directly to your V-Bot.

Included sample files: `square.gcode`, `circle.gcode`.

## FluidNC Similarity

While this firmware aims to be lightweight and specific to the V-Bot application, the configuration file structure (`config.yaml`) is modeled after the excellent [FluidNC](https://github.com/bdring/FluidNC) project to make it familiar for CNC enthusiasts.

## License

Open Source / MIT License
