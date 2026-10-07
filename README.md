# 7-Segment Fitness Display & RFID Access Control

This project combines a two-digit 7-segment display with a lightweight RFID gate-control system. The Python application provides a graphical controller for sending fitness-related display values to the Arduino display, while the RFID components validate access cards and record entry and exit events.

> **Important:** This repository connects two separate systems. The display path uses UDP and the RFID path uses TCP. The current project does not include a dedicated fitness application; the Python controller is the fitness-facing interface for sending values from 0 to 99.

## Features

- Display values from 0 through 99 on two seven-segment LED strips.
- Send values, colors, all-on, and all-off commands to one display or all displays.
- Receive heartbeat data from one or more Arduino displays.
- Track display online/offline status from the last received heartbeat.
- Read MFRC522 RFID cards using an ESP32 and send gate events over TCP.
- Validate RFID tags against an Excel database.
- Record access events as entries or exits and calculate exit duration.
- Provide visual feedback with an LED buzzer, success/failure tones, and a connected-device list.

## Project structure

```text
.
├── display_controller.py
├── arduino_udp_display/
│   └── arduino_udp_display.ino
├── RFID/
│   ├── testRFID.py
│   ├── rfid_database.xlsx
│   └── RFID_ESP/
│       └── RFID_ESP.ino
└── README.md
```

## Architecture

### 1. Fitness display controller

The Python GUI in `display_controller.py` uses the standard library only and runs with Tkinter.

- Listens for Arduino heartbeats on UDP port `5000`.
- Sends commands to Arduino devices on UDP port `5001`.
- Accepts a display ID from `0` to `255`; `0` targets every display.
- Accepts a value from `0` to `99`.
- Supports RGB color commands and all-on/all-off commands.
- Tracks the most recently reported value and online status.

Example message:

```text
1,42
```

This means: send value `42` to display ID `1`.

The Arduino sketch `arduino_udp_display/arduino_udp_display.ino` interprets commands as follows:

```text
<number>   Display a number from 0 to 99
A          Turn all segments on
S          Turn all segments off
R/G/B/W/Y/C/M/O/P/K/L/Q   Change color
```

The Arduino sketch sends heartbeats in this format:

```text
HB,<display-id>,<shown-value>
```

For example:

```text
HB,1,42
```

### 2. RFID access control

The Python application in `RFID/testRFID.py` is the TCP server for gate events.

- Listens on TCP port `5005` by default.
- Accepts ESP32 RFID gate connections.
- Reads a fixed-size nine-byte packet containing a gate ID and RFID value.
- Uses a checksum to detect corrupted packets.
- Checks the RFID value against the `ValidIDs` worksheet in `rfid_database.xlsx`.
- Returns status `1` for an accepted card and status `2` for an unknown card.
- Records entry, exit, and rejected-event logs.

The ESP32 sketch `RFID/RFID_ESP/RFID_ESP.ino`:

- Connects to the configured Wi-Fi network.
- Reads RFID cards with the MFRC522 module.
- Sends gate ID and RFID value packets to the Python server.
- Sends a heartbeat every five seconds.
- Produces success, failure, or connection-error buzzer tones.

The server maps:

- Gate `1` to an entry.
- Gate `2` to an exit.
- RFID value `0` to a heartbeat.

## Prerequisites

### Python

- Python 3
- `openpyxl`
- Tkinter included with most Python installations on Windows

Install the dependency:

```powershell
python -m pip install openpyxl
```

### Arduino / ESP32

- Arduino IDE with the required libraries:
  - `EtherCard`
  - `Adafruit_NeoPixel`
  - `WiFi`
  - `SPI`
  - `MFRC522`
- An Arduino Ethernet board for the UDP display or an ESP32 for the RFID gate.
- A network that can reach the configured PC and controller addresses.

## Configuration

### Display controller

Edit the network addresses in `arduino_udp_display/arduino_udp_display.ino`:

```cpp
static byte myip[] = { 192, 168, 1, 177 };
static byte pcip[] = { 192, 168, 1, 100 };
```

Update the Python GUI default IP in `display_controller.py` if needed:

```python
DEFAULT_IP = "192.168.1.177"
```

The Python application uses the address discovered from the Arduino heartbeat when available; otherwise, the configured IP is used.

### RFID system

Update these values in `RFID/RFID_ESP/RFID_ESP.ino`:

```cpp
const char* ssid = "MO";
const char* password = "10000000@#";

IPAddress local_IP(192, 168, 0, 101);
IPAddress serverIP(192, 168, 0, 100);
const uint16_t serverPort = 5005;
```

Change `GATE_ID` to `1` or `2` for the two gate devices.

The Python server defaults to listening on `0.0.0.0:5005`. The current GUI lets the user select another IP and port.

RFID tags are stored in `RFID/rfid_database.xlsx`. The `ValidIDs` worksheet contains tag IDs and display names. The `Log` worksheet contains access history.

## Running the project

### Run the display controller

```powershell
python display_controller.py
```

The GUI opens and listens for Arduino heartbeats.

### Run the RFID server

```powershell
python RFID/testRFID.py
```

Use the GUI to connect, enter the server address and TCP port, and start the listener.

### Run the firmware

1. Upload `arduino_udp_display/arduino_udp_display.ino` to the Ethernet-connected Arduino.
2. Upload `RFID/RFID_ESP/RFID_ESP.ino` to the ESP32 board.
3. Confirm that the network addresses, ports, gate IDs, and RFID database match.
4. Start the Python server before bringing the ESP32 gateway online.

## Fitness usage

The Python controller can be used as a fitness display for operations such as:

- Showing a workout or session count.
- Displaying current repetitions or completed sets.
- Showing a progress value from 0 to 99.
- Changing the display color for visual status.
- Showing an all-segments indicator or clearing the display.

The application does not currently implement a fitness API, workout database, authentication, or live telemetry. It only sends display values to the attached hardware.

## Protocol summary

| System | Transport | Port | Direction | Message |
|---|---|---:|---|---|
| Display | UDP | 5000 | Arduino -> Python | `HB,<id>,<value>` |
| Display | UDP | 5001 | Python -> Arduino | `<id>,<command>` |
| RFID | TCP | 5005 | ESP32 -> Python | 9-byte gate/RFID packet |
| RFID response | TCP | 5005 | Python -> ESP32 | 9-byte status/RFID packet |

## Safety and configuration notes

- Replace all hard-coded Wi-Fi credentials, IP addresses, and test RFID records before using this project outside a lab.
- The RFID database is a local file and is modified by the server as events are recorded.
- Keep the controller and RFID server on the same local network and restrict outgoing network access where possible.
- The current firmware uses a broadcast heartbeat for Arduino displays; a different network configuration may require changing `HB_BROADCAST` and the destination address.
- The source code contains example credentials and test card IDs. Do not publish sensitive deployment settings without reviewing and replacing them.
- The Python server currently accepts a single 9-byte packet per TCP connection. A production deployment should add connection handling, packet fragmentation, timeout controls, and secure transport if required.

## License

This project is shared as an open-source demonstration. Review the source and replace the example network configuration before deploying it on a real fitness or access-control installation.
