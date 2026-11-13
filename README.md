# HygieSense — IoT Restroom Hygiene Feedback & Monitoring System

A modern, responsive, real-time web dashboard and ESP32 IoT system built for monitoring restroom hygiene feedback devices connected directly to **Firebase Realtime Database**.

---

## 🚀 Features

- **⚡ Real-time Firebase Sync**: Direct WebSocket synchronization with Firebase Realtime Database SDK v10 (modular ESM). Any button press on an ESP32 unit updates the dashboard immediately with zero page refresh.
- **🛡️ Executive KPI Metrics**:
  - **Hygiene Satisfaction Index (HSI %)**: Live dynamic percentage with radial gauge and condition status (*Optimal*, *Fair*, *Needs Attention*).
  - **Clean Feedbacks (Green Button)**: Total positive votes and proportion.
  - **Dirty Feedbacks (Red Button)**: Dissatisfied customer alert counts.
  - **Monitored Restroom Nodes**: Active ESP32 stations online with total logged interactions.
- **🏢 Restroom Zone Cards**:
  - Live radial score dials and proportional progress bars for Clean vs. Dirty votes.
  - 3-segment threshold visual progress meter showing progression toward dirty alert limits.
  - Direct simulation buttons (`+ Clean`, `+ Dirty`) on every card.
  - One-click `Reset to 0` / `Verify Clean` action to reset dirty counts once serviced.
- **🚨 Dynamic Alert & Janitorial Dispatch Banner**:
  - Pops up an urgent priority banner **only when a restroom reaches or exceeds 3 dirty votes** (`RedCount >= 3`).
  - Below threshold (1 or 2 dirty votes), the restroom is tracked normally without intrusive popups.
  - One-click **"Dispatch Janitorial Staff"** button updates status to `dispatched` in real-time across all screens.
  - When janitorial staff (or a customer) presses the **Clean (Green)** button, the dirty count resets back to **0** and the alert clears automatically!
- **📊 Real-time Visual Analytics**:
  - **Chart.js Comparison**: Live grouped bar chart comparing Clean vs. Dirty votes across all restroom nodes.
  - **Live Telemetry Event Feed**: Chronological log of real-time button events with color-coded tags and timestamps.
- **🔊 Native Web Audio Synthesizer**:
  - Generates pleasant harmonic two-tone chimes for Clean presses, warning tones for Dirty reports, and dispatch completion sounds without external audio files.
  - Easily toggle sound on or off with a single click.
- **🛠️ Integrated Hardware Simulator & Demo Mode**:
  - Test button presses right from your browser without needing physical ESP32 hardware connected.
  - Continuous **Auto-Simulation Demo** mode that emulates realistic customer traffic every ~3 seconds for presentations and testing.
  - Modal form to register and initialize new restroom nodes (e.g., `Restroom_2`, `Floor_3_East`) directly into Firebase RTDB.
- **📑 Facility Audit & CSV Export**:
  - Full audit table detailing hygiene scores, vote tallies, and compliance status.
  - One-click CSV export of all facility hygiene records.
- **⚡ Standalone Ready-to-Flash ESP32 Firmware**:
  - Found in [`firmware/esp32_hygiesense.ino`](firmware/esp32_hygiesense.ino) using ESP32's native HTTPS REST API with **zero external libraries required**!

---

## 📁 Repository Structure

```
Hygisense/
├── index.html                   # Complete single-file real-time web dashboard
├── firmware/
│   └── esp32_hygiesense.ino     # Ready-to-flash Arduino C++ firmware for ESP32
└── README.md                    # Project documentation & setup instructions
```

---

## ⚙️ Quick Start

### Option 1: Direct File Launch
Double click `index.html` or open it directly in any modern web browser (Google Chrome, Microsoft Edge, Brave, Firefox, Safari).

### Option 2: Local HTTP Server (Recommended)
Using Python:
```bash
python -m http.server 8080
```
Then navigate to:
```
http://localhost:8080
```

---

## 🗄️ Firebase Realtime Database Structure

The system expects root keys for each restroom node (e.g., `Restroom_1`, `Restroom_2`):

```json
{
  "Restroom_1": {
    "GreenCount": 4,
    "RedCount": 0,
    "status": "optimal",
    "lastCleaned": 1793382730835,
    "lastUpdated": 1793382730835
  }
}
```

### Key Normalization:
The web dashboard automatically normalizes key variations like `greenCount`, `clean`, `redCount`, and `dirty`.

### Recommended Firebase Security Rules:
In your Firebase Console -> Realtime Database -> Rules tab:
```json
{
  "rules": {
    ".read": true,
    ".write": true
  }
}
```

---

## 🔌 ESP32 Hardware Wiring & Pinout

| Component | ESP32 Pin | Wiring Description |
| :--- | :--- | :--- |
| **Clean Button (Green)** | **GPIO 12** | One terminal to GPIO 12, other terminal to **GND** (Uses internal `INPUT_PULLUP`). |
| **Dirty Button (Red)** | **GPIO 14** | One terminal to GPIO 14, other terminal to **GND** (Uses internal `INPUT_PULLUP`). |
| **Status LED** | **GPIO 2** | Built-in onboard LED (Blinks on WiFi connection and button events). |

### ESP32 Flashing Guide:
1. Open [`firmware/esp32_hygiesense.ino`](firmware/esp32_hygiesense.ino) in Arduino IDE or VS Code with Arduino / PlatformIO.
2. In Arduino IDE, select **Tools -> Board -> ESP32 Arduino -> ESP32 Dev Module** (or your specific ESP32 board).
3. Update `WIFI_SSID` and `WIFI_PASSWORD` with your local Wi-Fi details.
4. Verify `FIREBASE_HOST` points to:
   `https://hygiesense-default-rtdb.asia-southeast1.firebasedatabase.app`
5. Click **Upload** (ensure you press and hold the `BOOT` button on the ESP32 if required during upload).
6. Open **Serial Monitor** at **115200 baud** to see real-time button events and WiFi connection logs.

---

## 🎯 Threshold & Sanitation Logic

1. **Clean Rating / Staff Verification**:
   - Customer or staff presses the **Green Button** (GPIO 12).
   - `GreenCount` increments by 1.
   - `RedCount` resets back to **0**.
   - `status` updates to `"optimal"`.
   - Alert banners are dismissed.
2. **Dirty Rating**:
   - Customer presses the **Red Button** (GPIO 14).
   - `RedCount` increments by 1.
   - If `RedCount < 3`: Restroom is tracked in monitoring mode without intrusive alerts.
   - If `RedCount >= 3`: Restroom triggers a priority **Hygiene Alert**, updating `status` to `"needs_attention"`, popping up the janitorial dispatch banner, and sounding an alert chime.
3. **Dispatch & Resolution**:
   - Supervisor clicks **"Dispatch Janitorial Staff"** on the dashboard -> status switches to `dispatched` ("Cleaning In Progress").
   - Once cleaned, pressing the physical **Green Button** on the ESP32 or clicking **"Verify Cleaned"** on the dashboard resets the dirty counter back to 0 and restores the status to optimal.
