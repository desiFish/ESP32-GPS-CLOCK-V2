<div align="center">
  <h1>🌟 ESP32-GPS-Clock-V2 ⏰</h1>
  <p><i>A smart GPS Clock with temperature, humidity, and light sensing capabilities</i></p>
  
  [![GitHub license](https://img.shields.io/github/license/desiFish/ESP32-GPS-Clock-V2)](https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/LICENSE)
  [![GitHub release (latest by date)](https://img.shields.io/github/v/release/desiFish/ESP32-GPS-Clock-V2)](https://github.com/desiFish/ESP32-GPS-Clock-V2/releases)
  [![GitHub issues](https://img.shields.io/github/issues/desiFish/ESP32-GPS-Clock-V2)](https://github.com/desiFish/ESP32-GPS-Clock-V2/issues)
  [![GitHub stars](https://img.shields.io/github/stars/desiFish/ESP32-GPS-Clock-V2)](https://github.com/desiFish/ESP32-GPS-Clock-V2/stargazers)
  [![GitHub forks](https://img.shields.io/github/forks/desiFish/ESP32-GPS-Clock-V2)](https://github.com/desiFish/ESP32-GPS-Clock-V2/network)
  [![GitHub last commit](https://img.shields.io/github/last-commit/desiFish/ESP32-GPS-Clock-V2)](https://github.com/desiFish/ESP32-GPS-Clock-V2/commits/main)
  [![GitHub repo size](https://img.shields.io/github/repo-size/desiFish/ESP32-GPS-Clock-V2)](https://github.com/desiFish/ESP32-GPS-Clock-V2)
</div>

> Read ⚠️ Important Notice below gallery section

## 🛠️ Hardware Components

### 📡 Core Components
1. **ESP32 Development Board** <img src="https://raw.githubusercontent.com/espressif/esp-idf/master/docs/_static/espressif-logo.svg" width="20" height="20" style="vertical-align: middle;">
   - Dual-core processor up to 240MHz
   - Integrated Wi-Fi and Bluetooth
   - Operating voltage: 3.3V
   - Tested on: ESP32-Devkit-V1

2. **BH1750 Light Sensor** 🌞
   - 16-bit digital output
   - Range: 1-65535 lux
   - Power: 2.4-3.6V
   - I²C interface

3. **GPS Module (NEO-6M)** 🛰️
   - Update rate: 1-5 Hz
   - Position accuracy: 2.5m
   - Cold start: 27s typical
   - Hot start: 1s typical
   - Operating voltage: 3.0-3.6V

4. **ST7920 LCD Display** 📺
   - Resolution: 128x64 pixels
   - 5V logic level
   - Parallel/Serial interface

5. **Buzzer Module** 🔊
   - Active buzzer
   - Operating voltage: 3.3-5V
   - Frequency: ~2300Hz
   - Sound output: >85dB

6. **Push Buttons** x3 ⚡
   - Tactile momentary switches
   - Life cycle: 100,000 clicks
   - With caps for better feel
   - Two for Menu navigation, one for reset

7. **10K Potentiometer**
   - For contrast adjustment

### 🔋 Optional Components (Battery Backup)

#### Power Management
1. **LiFePO4 Battery** 
   - Capacity: 80mAh (AAA size)
   - Nominal voltage: 3.2V
   - Cycle life: >2000 cycles
   - Temperature range: -20°C to 60°C
   - Perfect for GPS backup

2. **TP5000 Charging Circuit** ⚡
   - Input voltage: 4.5-28V
   - Charging current: 0.5-2A
   - Efficiency: >90%
   - Built-in protection features
   - Auto-detect battery type

3. **Battery Protection** 🛡️
   - BMS for single cell
   - Using for Over-discharge protection

4. **Additional Components** 🔧
   - IN4007 diode (1A, 1000V)
   - JST connectors
   - Prototyping board
   - Silicone wires (better) or PVC Wires
   - Heat shrink tubing

### 💡 Compatibility Notes
- All I²C devices operate at 3.3V logic
- Power supply should provide at least 500mA
- USB connection recommended for programming
- External antenna optional for GPS

### 🏗️ Construction Guide

#### 📦 Enclosure Preparation
* Used a standard PVC electrical junction box (IP55 rated for weather resistance)
  - Size: Approximately 150mm x 100mm x 70mm
  - Cost-effective alternative to 3D printing
  - Naturally resistant to moisture and dust
  - Available at most hardware stores

#### 🛠️ Tools Required
* Drill machine with metal cutting bits (for holes)
* Drill machine with metal cutting attachment that looks like CD (for rectangular cuts)
* Hot glue gun
* Wire strippers
* Soldering iron
* Basic hand tools (screwdrivers, pliers)
* Safety equipment (goggles, gloves)

#### 🔨 Assembly Steps
1. **Display Window Creation** 🪟
   * Mark display dimensions on box
   * Drill corner pilot holes
   * Use metal cutting bits for rough cut
   * File edges smooth for perfect fit
   * Pro tip: Use masking tape to prevent scratches

2. **Light Sensor Window** 💡
   * Create small 10mm opening for BH1750
   * Use clear epoxy or hot glue to seal
   * Ensure sensor faces directly outward
   * Keep sealed but transparent for accuracy

3. **Component Mounting** 🔌
   * Mount display using hot glue at corners
   * Create standoffs for ESP32 board
   * Position GPS antenna near top for best reception

#### 💡 Pro Tips
* Ensure proper ventilation while cutting/drilling PVC
* Double-check measurements before cutting
* Test all components before final assembly
* Label all wires for future maintenance
* Add small ventilation holes for the sensor
* Consider magnetic mount options
* Keep spare wire lengths for modifications
* Document your build with photos

## 🚀 Upcoming Features
Check [Issues](https://github.com/desiFish/ESP32-GPS-Clock-V2/issues)

## 📷 Pictures, Schematics and other stuff 
<table>
  <tr>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/schematics/Schematic_ESP32-GPS-CLOCK-V2_2024-11-20.png" alt="Schematics"></td>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x10.jpg" alt="front-view"></td>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x1.jpg" alt="front-view"></td>
  </tr>
  <tr>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x11.jpg" alt="inside"></td>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x2.jpg" alt="display-connections"></td>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x3.jpg" alt="bme280"></td>
  </tr>
  <tr>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x4.jpg" alt="testing-phase"></td>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x5.jpg" alt="esp32_with_connections"></td>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x6.jpg" alt="circuit-connections"></td>
  </tr>
  <tr>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x7.jpg" alt="cutting-display-window"></td>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x8.jpg" alt="display-in-plastic-frame"></td>
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x9.jpg" alt="fullview"></td>
  </tr>
</table>

<h2>⚠️ Important Notice</h2>
<div style="background-color: #fff3cd; padding: 10px; border-radius: 5px; border-left: 5px solid #ffeeba;">
  <strong>❌ DO NOT USE AHT25 SENSOR!</strong><br>
  Due to significant accuracy issues, we recommend using BME280/BMP280/TMP117 instead.
</div> <br>
<strong>🔋 GPS Battery Modification</strong>
<div style="background-color: #f8f9fa; padding: 15px; border-radius: 5px; margin-top: 10px;">
  <h4>⚠️ Known Issue with GPS Module's Internal Battery</h4>
  <p>
    The NEO-6M GPS modules often come with problematic internal rechargeable batteries that:
    <ul>
      <li>Are frequently dead on arrival</li>
      <li>Fail to hold charge properly</li>
      <li>Only last 15-20 minutes when disconnected</li>
      <li>Cannot be reliably recharged</li>
    </ul>
  </p>

  <h4>🛠️ Solution Implemented</h4>
  <p>
    To resolve this, I've made the following modifications:
    <ul>
      <li>Removed the internal battery and charging diode</li>
      <li>Installed a LiFePO4 battery (AAA size)</li>
      <li>Added TP5000 charging circuit for reliable charging</li>
      <li>Implemented BMS for deep discharge protection</li>
      <li>Added diode to drop voltage to 3V for GPS backup pin</li>
    </ul>
  </p>

  <h4>💡 User Options</h4>
  <div style="background-color: #e2e3e5; padding: 10px; border-radius: 5px;">
    <strong>You have two choices:</strong>
    <ol>
      <li><strong>Keep Original Battery:</strong> 
        <ul>
          <li>Suitable if clock remains powered most of the time</li>
          <li>No modifications needed</li>
        </ul>
      </li>
      <li><strong>Modify Battery (Recommended):</strong>
        <ul>
          <li>Better for frequent power cycles</li>
          <li>Eliminates 5-10 minute GPS lock delay on cold starts</li>
          <li>More reliable long-term solution</li>
        </ul>
      </li>
    </ol>
  </div>
</div>


## 📜 License

<details>
<summary>GNU General Public License v3.0</summary>

This project is protected under the GNU General Public License v3.0. This means:

- ✅ Commercial use is permitted
- ✅ Modification is permitted
- ✅ Distribution is permitted
- ✅ Private use is permitted
- ❗ License and copyright notice must be included
- ❗ Source code must be made available when distributing
- ❗ Changes must be documented
- ❗ Same license must be used

[View full license](https://www.gnu.org/licenses/gpl-3.0.en.html)
</details>

---
<div align="center">
  <p>Made with ❤️ for the IoT Community</p>
  <img src="https://img.shields.io/badge/ESP32-Ready-blue?logo=espressif&logoColor=white" alt="ESP32 Ready">
  <img src="https://img.shields.io/badge/GPS-Enabled-green?logo=googlemaps&logoColor=white" alt="GPS Enabled">
  <img src="https://img.shields.io/badge/Open-Source-orange?logo=opensourceinitiative&logoColor=white" alt="Open Source">
  <br>
  <img src="https://img.shields.io/github/watchers/desiFish/ESP32-GPS-Clock-V2?style=social" alt="Watchers">
  <a href="https://github.com/desiFish/ESP32-GPS-Clock-V2/network/members"><img src="https://img.shields.io/github/forks/desiFish/ESP32-GPS-Clock-V2?style=social"></a>
  <a href="https://github.com/desiFish/ESP32-GPS-Clock-V2/stargazers"><img src="https://img.shields.io/github/stars/desiFish/ESP32-GPS-Clock-V2?style=social"></a>
</div>
