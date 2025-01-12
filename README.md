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

## 🛠️ Hardware
1. ESP32
2. BH1750 (Light Sensor)
3. AHT25 (Temperature and Humidity)
4. GPS Neo 6m 
5. ST7920 128X64 LCD Display
6. Buzzer 
7. 3x Buttons

### 🔋 Optional
8. LiFePO4 AAA 80mAh cell (for longer GPS memory backup)
9. TP5000 Charging circuit
10. BMS (for the cell) and diode (IN4007)
11. Wires and other stuff (like Prototyping board, connectors, switch etc.. as needed)

### 🏗️ Construction
* Used a PVC box (used for electrical fixtures) as a frame for the clock (I do not have a 3d printer)
* Used metal cutters in the drilling machine for cutting out holes.
* Fixed the display using hot glue
* make a transparent yet sealed window for the light sensor using hot glue
  
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
    <td><img src="https://github.com/desiFish/ESP32-GPS-Clock-V2/blob/main/resources/x3.jpg" alt="aht25"></td>
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
