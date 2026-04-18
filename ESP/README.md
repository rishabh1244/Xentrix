# Web-Controlled ESP8266 Water Pump System

This project allows you to command a physical water pump directly from a web browser interface. It uses Google Chrome's Web Serial API to communicate directly with a NodeMCU (ESP8266) over USB, which then triggers a 5V relay module powered by an external battery.

## 🛠️ Hardware Requirements
- **NodeMCU (ESP8266)**
- **5V Relay Module** (Active-Low)
- **DC Water Pump** (3V to 5V Submersible)
- **External Battery** (3.7V or 5V for the motor)
- **Jumper Wires**

---

## ⚡ The Wiring Guide

By using a physical relay, the "Control Circuit" (NodeMCU) is safely isolated from the heavy current of the "Power Circuit" (Pump + Battery).

### 1. Control Circuit (NodeMCU to Relay)
*Note: We power the 5V relay from the 3.3V pin to solve logic-level leakage that keeps the relay permanently stuck on when driven by a 3.3V GPIO pin.*
- **Relay VCC** ➔ NodeMCU **3V3**
- **Relay GND** ➔ NodeMCU **GND**
- **Relay IN** ➔ NodeMCU **D1 (GPIO 5)**

### 2. Power Circuit (Battery to Pump)
- **Battery Negative (-)** ➔ **Pump Negative (-)**
- **Battery Positive (+)** ➔ **Relay COM** (Center Terminal)
- **Pump Positive (+)** ➔ **Relay NO** (Normally Open Terminal)

---

## 💻 Software Setup

### 1. Flashing the NodeMCU
1. Open the Arduino IDE. 
2. Go to **File > Preferences** and add this to the Additional Boards Manager URLs:
   `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
3. Go to **Tools > Board > Boards Manager**, search for `esp8266`, and install it.
4. Select **NodeMCU 1.0 (ESP-12E Module)** under Tools > Board.
5. Open `servo_pump/servo_pump.ino` and click **Upload**.

> **Baud Rate Note:** 
> The code explicitly uses `Serial.begin(74880);`. Because the ESP8266 hardware bootloader outputs diagnostic text at exactly 74880 baud during startup, matching this speed prevents the Web Serial API from throwing a `Framing Error` crashing the connection!

### 2. The Web Dashboard
The `dashboard.html` file acts as the user interface. It utilizes raw Javascript and the `navigator.serial` object to send real-time byte strings (`PUMP_ON`, `PUMP_OFF`) over the USB cable.

---

## 🚀 How to Run
1. Plug your NodeMCU into your computer via USB.
2. Hook up your external battery to the motor/relay circuit.
3. Open `dashboard.html` using a local web server (e.g., **VS Code Live Server** extension) to ensure standard browser security allows Web Serial access.
4. Click the **Connect Master (Serial)** button at the top of the webpage.
5. Select the COM port corresponding to your NodeMCU.
6. Click **Turn ON** and **Turn OFF** to operate the pump!
