

# ESP32 RFID Access Control System with PIR Wake-Up

## Project Overview

This project is an energy-efficient, localized security access control system built on the ESP32 microcontroller. To minimize power consumption in remote or battery-powered deployments, the system utilizes a Passive Infrared (PIR) motion sensor as a hardware interrupt.

The ESP32 remains in **Deep Sleep mode** ($I_{sleep} \approx 10\mu\text{A}-150\mu\text{A}$) until motion is detected. Upon waking, it energizes the RC522 RFID module, processes the security credentials, operates a locking mechanism, and immediately returns to deep sleep, significantly extending battery operational life.

---

## Key Features & Power Methodology

* **PIR Hardware Interrupt:** The PIR sensor acts as an external wake-up trigger connected to a designated RTC GPIO pin, keeping the ESP32 in deep sleep until a human approaches.
* **Low-Power State Management:** The RC522 RFID reader is explicitly powered down or kept un-energized during sleep states to prevent idle current draw.
* **Rapid Authentication:** Optimized setup routines ensure credential validation and peripheral triggering happen in under 2 seconds upon wake-up.
* **Local Access Control:** Hardware-level security checks without dependency on cloud latency.

---

## Hardware Architecture

### Components Used

* **Microcontroller:** ESP32 (NodeMCU or similar development board)
* **RFID Reader:** MFRC522 (SPI Interface)
* **Motion Sensor:** HC-SR501 or AM312 PIR Sensor
* **Actuator:** 12V Solenoid Door Lock via Optocoupler/Relay or N-Channel MOSFET
* **Indicators:** 5V Active Buzzer, Green and Red LEDs

### System Block Diagram

The system's operational flow from detection to authentication is outlined below:

```markdown
### System Block Diagram
![System Block Diagram](images/block_diagram.png)

```

### Circuit Schematics

The hardware connections, emphasizing the SPI communication lines for the RFID reader and the RTC-capable GPIO pin for the PIR interrupt, are detailed below:

```markdown
### Circuit Schematics
![Circuit Schematic](images/schematic.png)

```

---

## Pin Mapping Guide

| Component | Component Pin | ESP32 GPIO Pin | Notes |
| --- | --- | --- | --- |
| **HC-SR501 PIR** | VCC / GND | 5V / GND | Power |
|  | OUT | **GPIO 33** | Must be an RTC-capable GPIO for Deep Sleep wake-up |
| **MFRC522 RFID** | VCC (3.3V) / GND | 3V3 / GND | Power |
|  | RST | GPIO 22 | Reset |
|  | SDA (SS) | GPIO 5 | SPI SS |
|  | MOSI | GPIO 23 | SPI MOSI |
|  | MISO | GPIO 19 | SPI MISO |
|  | SCK | GPIO 18 | SPI SCK |
| **Relay / Lock** | IN | GPIO 25 | Actuator Signal |
| **Buzzer** | I/O | GPIO 26 | Audio Feedback |

---

## Software Execution Flow

1. **Sleep State:** ESP32 is in deep sleep; MFRC522 is unpowered/idle.
2. **Wake-up:** PIR detects motion $\rightarrow$ Sends HIGH signal to GPIO 33 $\rightarrow$ Wakes ESP32.
3. **Initialization:** ESP32 initializes SPI bus and configures MFRC522.
4. **Polling:** System waits for a specified window (e.g., 10 seconds) for an RFID card scan.
* **Authorized Card Found:** Relay pulses HIGH (unlocks door), Green LED turns ON, Buzzer beeps once.
* **Unauthorized Card Found:** Red LED blinks, Buzzer sounds continuous error tone.
* **Timeout (No Card):** System prepares for shutdown.


5. **Re-entering Sleep:** System prints deep sleep statistics to serial and invokes `esp_deep_sleep_start()`.

---

## How to Deploy the Code
1. **Prerequisites:** Install the Arduino IDE or PlatformIO extension in VS Code.
2. **Library Dependencies:** Install the following libraries via the Library Manager:
* `MFRC522` by GitHubCommunity


3. **Configuration:** Clone this repository, open the `.ino` file, and update the `allowed_uid_tags` array with your specific RFID card UIDs.
4. **Upload:** Connect your ESP32 via USB, select the correct COM port and board type, and flash the software.

