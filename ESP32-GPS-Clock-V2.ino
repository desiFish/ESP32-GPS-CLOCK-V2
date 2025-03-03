/*
  ESP32-GPS-CLOCK-V2.ino

  This is an enhanced version of https://github.com/desiFish/GPS-CLOCK-V2
  with added buttons and menu system for improved user interaction and control.

  Copyright (C) 2024 desiFish

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

/**
 * Software version number
 * Format: major.minor.patch
 */
#define SWVersion "1.1.2"

//========== Library Includes ==========//
/**
 * Network and OTA Libraries
 * AsyncTCP: Enables asynchronous TCP operations
 * ESPAsyncWebServer: Provides web server functionality
 * ElegantOTA: Handles Over-The-Air updates
 */
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
// for display
#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
// I2C related
#include <Wire.h>
#include <BH1750.h> //Light Sensor BH1750
#include <AHTxx.h>

// GPS and Timing
#include <TinyGPSPlus.h>
#include <TimeLib.h>

// Data Storage
#include <Preferences.h>

//========== Hardware Configuration ==========//
/**
 * Display Configuration
 * Using ST7920 128x64 LCD with Software SPI
 * clock=18: Clock signal pin
 * data=23: Data signal pin
 * No CS or Reset pins used (U8X8_PIN_NONE)
 */
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=*/18, /* data=*/23, /* cs=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);

// Preference library object or instance
Preferences pref;
// Light Sensor Object
BH1750 lightMeter;

float ahtValue; // to store Temp result

AHTxx aht20(AHTXX_ADDRESS_X38, AHT2x_SENSOR); // sensor address, sensor type

//========== Pin Definitions ==========//
/**
 * GPS Module Pins
 * RXPin = 16: Connect to GPS TX
 * TXPin = 17: Connect to GPS RX
 */
static const int RXPin = 16, TXPin = 17;

/**
 * Control Pins
 * LCD_LIGHT = 4: LCD backlight control (PWM)
 * BUZZER_PIN = 33: Active buzzer control
 * NEXT_BUTTON = 36: Menu navigation (ADC input)
 * SELECT_BUTTON = 39: Menu selection (ADC input)
 */
#define LCD_LIGHT 4
#define BUZZER_PIN 33
#define NEXT_BUTTON 36
#define SELECT_BUTTON 39

// GPS instance
TinyGPSPlus gps;

//========== Global Variables ==========//
/**
 * Time/Date Arrays
 * week[]: Day names for display
 * monthChar[]: Month names for display
 */
String week[7] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
String monthChar[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

// used for blinking ":" in time (for display)
byte pulse = 0;
// variables for holding time data globally
byte days = 0, months = 0, hours = 0, minutes = 0, seconds = 0;
int years = 0;

// LUX (BH1750) update frequency
unsigned long lastTime1 = 0;
const long timerDelay1 = 2000; // LUX delay

// AHT25 update frequency
unsigned long lastTime2 = 0;
const long timerDelay2 = 12000; // aht update delay

bool isDark; // Tracks ambient light state
float ahtTemp = 0.0;

// features config, saved in preference library
int LCD_BRIGHTNESS, buzzVol;
bool autoBright, hourlyAlarm, halfHourlyAlarm, useWifi, muteDark; // Auto brightness status indicator

// your wifi name and password (saved in preference library)
String ssid;
String password;

// for various server related stuff
AsyncWebServer server(80);

// Wifi Manager HTML Code
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Wi-Fi Manager</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
html {
  font-family: Arial, Helvetica, sans-serif; 
  display: inline-block; 
  text-align: center;
}

h1 {
  font-size: 1.8rem; 
  color: white;
}

p { 
  font-size: 1.4rem;
}

.topnav { 
  overflow: hidden; 
  background-color: #0A1128;
}

body {  
  margin: 0;
}

.content { 
  padding: 5%;
}

.card-grid { 
  max-width: 800px; 
  margin: 0 auto; 
  display: grid; 
  grid-gap: 2rem; 
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
}

.card { 
  background-color: white; 
  box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5);
}

.card-title { 
  font-size: 1.2rem;
  font-weight: bold;
  color: #034078
}

input[type=submit] {
  border: none;
  color: #FEFCFB;
  background-color: #034078;
  padding: 15px 15px;
  text-align: center;
  text-decoration: none;
  display: inline-block;
  font-size: 16px;
  width: 100px;
  margin-right: 10px;
  border-radius: 4px;
  transition-duration: 0.4s;
  }

input[type=submit]:hover {
  background-color: #1282A2;
}

input[type=text], input[type=number], select {
  width: 50%;
  padding: 12px 20px;
  margin: 18px;
  display: inline-block;
  border: 1px solid #ccc;
  border-radius: 4px;
  box-sizing: border-box;
}

label {
  font-size: 1.2rem; 
}
.value{
  font-size: 1.2rem;
  color: #1282A2;  
}
.state {
  font-size: 1.2rem;
  color: #1282A2;
}
button {
  border: none;
  color: #FEFCFB;
  padding: 15px 32px;
  text-align: center;
  font-size: 16px;
  width: 100px;
  border-radius: 4px;
  transition-duration: 0.4s;
}
.button-on {
  background-color: #034078;
}
.button-on:hover {
  background-color: #1282A2;
}
.button-off {
  background-color: #858585;
}
.button-off:hover {
  background-color: #252524;
} 
  </style>
</head>
<body>
  <div class="topnav">
    <h1>Wi-Fi Manager</h1>
  </div>
  <div class="content">
    <div class="card-grid">
      <div class="card">
        <form action="/wifi" method="POST">
          <p>
            <label for="ssid">SSID</label>
            <input type="text" id ="ssid" name="ssid"><br>
            <label for="pass">Password</label>
            <input type="text" id ="pass" name="pass"><br>
            <input type ="submit" value ="Submit">
          </p>
        </form>
      </div>
    </div>
  </div>
</body>
</html>
)rawliteral";

// Search for parameter in HTTP POST request
const char *PARAM_INPUT_1 = "ssid";
const char *PARAM_INPUT_2 = "pass";

bool updateInProgress = false;

// Elegant OTA related task
unsigned long ota_progress_millis = 0;

/**
 * @brief Handle OTA update start event
 */
void onOTAStart()
{
  // Log when OTA has started
  updateInProgress = true;
  Serial.println("OTA update started!");
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_luRS08_tr);
  u8g2.setCursor(1, 20);
  u8g2.print("OTA UPDATE");
  u8g2.setCursor(1, 32);
  u8g2.print("HAVE STARTED");
  u8g2.sendBuffer();
  delay(1000);
}

/**
 * @brief Handle OTA update progress
 * @param current Current bytes transferred
 * @param final Total bytes to transfer
 */
void onOTAProgress(size_t current, size_t final)
{
  // Log
  if (millis() - ota_progress_millis > 500)
  {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 20);
    u8g2.print("OTA UPDATE");
    u8g2.setCursor(1, 32);
    u8g2.print("UNDER PROGRESS");
    u8g2.setCursor(1, 44);
    u8g2.print("Done: ");
    u8g2.print(current);
    u8g2.print(" bytes");
    u8g2.setCursor(1, 56);
    u8g2.print("Total: ");
    u8g2.print(final);
    u8g2.print(" bytes");
    u8g2.sendBuffer();
  }
}

/**
 * @brief Handle OTA update completion
 * @param success Boolean indicating if update was successful
 */
void onOTAEnd(bool success)
{
  // Log when OTA has finished
  if (success)
  {
    Serial.println("OTA update finished successfully!");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 20);
    u8g2.print("OTA UPDATE");
    u8g2.setCursor(1, 32);
    u8g2.print("COMPLETED!");
    u8g2.sendBuffer();
  }
  else
  {
    Serial.println("There was an error during OTA update!");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 20);
    u8g2.print("OTA UPDATE");
    u8g2.setCursor(1, 32);
    u8g2.print("HAVE FAILED");
    u8g2.sendBuffer();
  }
  // <Add your own code here>
  updateInProgress = false;
  delay(1000);
}

time_t prevDisplay = 0; // when the digital clock was displayed

// for creating task attached to CORE 0 of CPU
TaskHandle_t loop1Task;
byte currentBrightness = 250; // Track current brightness level

/**
 * @brief Initialize all hardware and configurations
 *
 * Setup sequence:
 * 1. Initialize Serial communications
 * 2. Configure GPIO pins
 * 3. Load saved preferences
 * 4. Initialize I2C devices
 * 5. Setup display
 * 6. Configure WiFi if enabled
 * 7. Start secondary core task
 */
void setup(void)
{
  if (getCpuFrequencyMhz() != 160)
    setCpuFrequencyMhz(160); // if not 160MHz, set to 160MHz
  Serial.begin(115200);
  Serial1.begin(9600, SERIAL_8N1, RXPin, TXPin); // for GPS running on Hardware Serial
  pinMode(LCD_LIGHT, OUTPUT);
  analogWrite(LCD_LIGHT, 250);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(100);
  digitalWrite(BUZZER_PIN, LOW);

  if (!pref.begin("database", false)) // open database
    errorMsgPrint("DATABASE", "ERROR INITIALIZE");

  bool checkVal = pref.isKey("lcd_bright");
  if (!checkVal)
  {
    pref.putInt("lcd_bright", 250);
  }
  LCD_BRIGHTNESS = pref.getInt("lcd_bright", 250);

  checkVal = pref.isKey("autoBright");
  if (!checkVal)
  {
    pref.putBool("autoBright", false);
  }
  autoBright = pref.getBool("autoBright", false);

  checkVal = pref.isKey("hourlyAlarm");
  if (!checkVal)
  {
    pref.putBool("hourlyAlarm", false);
  }
  hourlyAlarm = pref.getBool("hourlyAlarm", false);

  checkVal = pref.isKey("halfHourlyAlarm");
  if (!checkVal)
  {
    pref.putBool("halfHourlyAlarm", false);
  }
  halfHourlyAlarm = pref.getBool("halfHourlyAlarm", false);

  checkVal = pref.isKey("useWifi");
  if (!checkVal)
  {
    pref.putBool("useWifi", false);
  }
  useWifi = pref.getBool("useWifi", false);

  checkVal = pref.isKey("muteDark");
  if (!checkVal)
  {
    pref.putBool("muteDark", false);
  }
  muteDark = pref.getBool("muteDark", false);

  checkVal = pref.isKey("buzzVol");
  if (!checkVal)
  {
    pref.putInt("buzzVol", 50);
  }
  buzzVol = pref.getInt("buzzVol", 50);

  analogWrite(LCD_LIGHT, LCD_BRIGHTNESS);
  Wire.begin();

  if (!lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE))
    errorMsgPrint("BH1750", "CANNOT FIND");

  if (aht20.begin() != true)
  {
    errorMsgPrint("AHT25", "CANNOT FIND");
  }
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_mr);
  u8g2.setCursor(12, 15);
  u8g2.print("RETRO");
  u8g2.setCursor(12, 30);
  u8g2.print("GPS CLOCK");
  u8g2.sendBuffer();
  xTaskCreatePinnedToCore(
      loop1,       /* Task function. */
      "loop1Task", /* name of task. */
      10000,       /* Stack size of task */
      NULL,        /* parameter of the task */
      1,           /* priority of the task */
      &loop1Task,  /* Task handle to keep track of created task */
      0);          /* pin task to core 0 */

  delay(2500);

  // wifi manager
  if (useWifi)
  {
    if (!(pref.isKey("ssid")))
    {
      pref.putString("ssid", "");
      pref.putString("password", "");
    }

    ssid = pref.getString("ssid", "");
    password = pref.getString("password", "");

    if (ssid == "" || password == "")
    {
      Serial.println("No values saved for ssid or password");
      // Connect to Wi-Fi network with SSID and password
      Serial.println("Setting AP (Access Point)");
      // NULL sets an open Access Point
      WiFi.softAP("WIFI_MANAGER", "WIFImanager");

      IPAddress IP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(IP);
      wifiManagerInfoPrint();

      // Web Server Root URL
      server.on("/wifi", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send(200, "text/html", index_html); });

      server.on("/wifi", HTTP_POST, [](AsyncWebServerRequest *request)
                {
        int params = request->params();
        for (int i = 0; i < params; i++) {
          const AsyncWebParameter* p = request->getParam(i);
          if (p->isPost()) {
            // HTTP POST ssid value
            if (p->name() == PARAM_INPUT_1) {
              ssid = p->value();
              Serial.print("SSID set to: ");
              Serial.println(ssid);
              ssid.trim();
              pref.putString("ssid", ssid);
            }
            // HTTP POST pass value
            if (p->name() == PARAM_INPUT_2) {
              password = p->value();
              Serial.print("Password set to: ");
              Serial.println(password);
              password.trim();
              pref.putString("password", password);
            }
            //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
          }
        }
        request->send(200, "text/plain", "Done. Device will now restart.");
        delay(3000);
        ESP.restart(); });
      server.begin();
      WiFi.onEvent(WiFiEvent);
      while (true)
        ;
    }

    WiFi.mode(WIFI_STA);
    WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    WiFi.setHostname("MiniGPSClock");
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.println("");

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 20);
    u8g2.print("WAITING FOR WIFI");
    u8g2.setCursor(1, 32);
    u8g2.print("TO CONNECT");
    u8g2.sendBuffer();

    // count variable stores the status of WiFi connection. 0 means NOT CONNECTED. 1 means CONNECTED

    bool count = true;
    while (WiFi.waitForConnectResult() != WL_CONNECTED)
    {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_luRS08_tr);
      u8g2.setCursor(1, 20);
      u8g2.print("COULD NOT CONNECT");
      u8g2.setCursor(1, 32);
      u8g2.print("CHECK CONNECTION");
      u8g2.setCursor(1, 44);
      u8g2.print("OR, RESET AND");
      u8g2.setCursor(1, 56);
      u8g2.print("TRY AGAIN");
      u8g2.sendBuffer();
      Serial.println("Connection Failed");
      delay(6000);
      count = false;
      break;
    }
    if (count)
    { // if wifi is connected
      Serial.println(ssid);
      Serial.println(WiFi.localIP());
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_luRS08_tr);
      u8g2.setCursor(1, 20);
      u8g2.print("WIFI CONNECTED");
      u8g2.setCursor(1, 42);
      u8g2.print(WiFi.localIP());
      u8g2.sendBuffer();
      delay(4000);

      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                { request->send(200, "text/plain", "Hi! Please add "
                                                   "/update"
                                                   " on the above address."); });

      ElegantOTA.begin(&server); // Start ElegantOTA
      // ElegantOTA callbacks
      ElegantOTA.onStart(onOTAStart);
      ElegantOTA.onProgress(onOTAProgress);
      ElegantOTA.onEnd(onOTAEnd);

      server.begin();
      Serial.println("HTTP server started");
    }
  }
  // Wifi related stuff END

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_mr);
  u8g2.setCursor(44, 30);
  u8g2.print("HELLO!");
  u8g2.setCursor(36, 52);
  u8g2.print("MINI");
  u8g2.setFont(u8g2_font_streamline_food_drink_t);
  u8g2.drawUTF8(80, 54, "U+4"); // birthday cake icon
  u8g2.sendBuffer();
  delay(2500);

  pref.end();
  ahtTemp = (aht20.readTemperature() - 3); // NEED TO CHANGE THE SENSOR, it shows +3 degrees extra
}

/**
 * @brief Secondary loop running on Core 0
 *
 * Handles:
 * - Light sensor readings
 * - Auto brightness adjustment
 * - Temperature sensor updates
 * - Alarm timing and control
 *
 * @param pvParameters Required by FreeRTOS
 */
void loop1(void *pvParameters)
{
  for (;;)
  {
    if ((millis() - lastTime1) > timerDelay1)
    { // light sensor based power saving operations
      lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
      float lux;
      while (!lightMeter.measurementReady(true))
      {
        yield();
      }
      lux = lightMeter.readLightLevel();

      Serial.println("LUXRaw: ");
      Serial.println(lux);

      isDark = muteDark && (lux <= 1); // Check if it's dark only if muteDark is enabled

      // Brightness control
      if (autoBright)
      {
        byte targetBrightness;
        if (lux == 0)
        {
          targetBrightness = 5;
        }
        else
        {
          byte val1 = constrain(lux, 1, 120);
          targetBrightness = map(val1, 1, 120, 40, 255);
        }

        // Improved smooth transition with dynamic step size
        if (currentBrightness != targetBrightness)
        {
          int diff = targetBrightness - currentBrightness;
          // Calculate step size based on difference
          // Larger differences = larger steps, smaller differences = smaller steps
          byte stepSize = max(1, min(abs(diff) / 4, 15));

          if (diff > 0)
          {
            currentBrightness = min(255, currentBrightness + stepSize);
          }
          else
          {
            currentBrightness = max(5, currentBrightness - stepSize);
          }
        }

        Serial.println("Brightness: ");
        Serial.println(currentBrightness);
        analogWrite(LCD_LIGHT, currentBrightness); // set brightness
      }

      lastTime1 = millis();
    }

    if ((millis() - lastTime2) > timerDelay2)
    {
      ahtTemp = (aht20.readTemperature()) - 3;
      lastTime2 = millis();
    }

    if (!isDark && seconds == 0 && (hourlyAlarm || halfHourlyAlarm)) // Only check when seconds is 0
    {
      switch (minutes)
      {
      case 0:
        if (hourlyAlarm)
          buzzer(600, 1);
        break;
      case 30:
        if (halfHourlyAlarm)
          buzzer(400, 2);
        break;
      }
    }
    delay(100);
  }
}

/**
 * @brief Main program loop running on Core 1
 *
 * Handles:
 * - GPS data processing
 * - Time synchronization
 * - Display updates
 * - Menu system
 * - OTA updates
 */
void loop(void)
{
  if (useWifi)
    ElegantOTA.loop();

  while (gps.hdop.hdop() > 100 && gps.satellites.value() < 2)
  {
    gpsInfo("Waiting for GPS...");
  }

  while (Serial1.available())
  {
    if (gps.encode(Serial1.read()))
    { // process gps messages
      // when TinyGPSPlus reports new data...
      unsigned long age = gps.time.age();
      if (age < 500)
      {
        // set the Time according to the latest GPS reading
        setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
        adjustTime(19800);
      }
    }
  }

  days = day();
  months = month();
  years = year();
  hours = hourFormat12();
  minutes = minute();
  seconds = second();

  if (!updateInProgress)
  {
    if (timeStatus() != timeNotSet)
    {
      if (now() != prevDisplay)
      { // update the display only if the time has changed
        prevDisplay = now();
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_etl16thai_t);
        u8g2.setCursor(2, 15);
        u8g2.print(int(ahtTemp));
        u8g2.setFont(u8g2_font_threepix_tr);
        u8g2.setCursor(20, 8);
        u8g2.print("o");
        u8g2.setFont(u8g2_font_etl16thai_t);
        u8g2.setCursor(26, 15);
        u8g2.print("C ");
        String tempWeek = week[weekday() - 1];
        byte stringLen = tempWeek.length();
        byte newStartPos = 128 - (stringLen * 8); // keeping it at right side
        u8g2.setCursor(newStartPos - 2, 15);
        u8g2.print(tempWeek);

        u8g2.drawLine(0, 17, 127, 17);
        u8g2.setFont(u8g2_font_samim_12_t_all); // u8g2_font_t0_11_mf  u8g2_font_t0_16_mr
        u8g2.setCursor(4, 29);
        if (days < 10)
          u8g2.print("0");
        u8g2.print(days);
        byte x = days % 10;
        u8g2.setFont(u8g2_font_tiny_simon_tr); //  u8g2_font_profont10_mr
        u8g2.setCursor(18, 25);
        if (days == 11 || days == 12 || days == 13)
          u8g2.print("th");
        else if (x == 1)
          u8g2.print("st");
        else if (x == 2)
          u8g2.print("nd");
        else if (x == 3)
          u8g2.print("rd");
        else
          u8g2.print("th");

        u8g2.setFont(u8g2_font_samim_12_t_all);
        String tempMonth = monthChar[months - 1];
        stringLen = tempMonth.length();
        newStartPos = 64 - (stringLen * 4); // keeping it at center
        u8g2.setCursor(newStartPos, 29);
        u8g2.print(tempMonth);
        u8g2.setCursor(97, 29);
        u8g2.print(years);

        u8g2.drawLine(0, 31, 127, 31);

        if (days == 14 && months == 11) // set these to ZERO to disable birthday message
        {                               // special message on birthday
          u8g2.setFont(u8g2_font_6x13_tr);
          u8g2.setCursor(5, 43);
          u8g2.print("HAPPY BIRTHDAY MINI!");

          u8g2.setFont(u8g2_font_logisoso16_tr);
          u8g2.setCursor(15, 63);
          if (hours < 10)
            u8g2.print("0");
          u8g2.print(hours);
          if (pulse == 0)
            u8g2.print(":");
          else
            u8g2.print("");

          u8g2.setCursor(41, 63);
          if (minutes < 10)
            u8g2.print("0");
          u8g2.print(minutes);

          if (pulse == 0)
            u8g2.print(":");
          else
            u8g2.print("");

          u8g2.setCursor(67, 63);
          if (seconds < 10)
            u8g2.print("0");
          u8g2.print(seconds);

          u8g2.setCursor(95, 63);
          u8g2.print(isAM() ? "AM" : "PM");

          u8g2.setFont(u8g2_font_waffle_t_all);
          if (!isDark)
          { // if mute on dark is not active (or false)
            if (hourlyAlarm || halfHourlyAlarm)
              u8g2.drawUTF8(5, 54, "\ue271"); // symbol for hourly/half-hourly alarm
          }
          if (useWifi)
            u8g2.drawUTF8(5, 64, "\ue2b5"); // wifi-active symbol
        }
        else
        {
          u8g2.setFont(u8g2_font_logisoso30_tn);
          u8g2.setCursor(15, 63);
          if (hours < 10)
            u8g2.print("0");
          u8g2.print(hours);
          if (pulse == 0)
            u8g2.print(":");
          else
            u8g2.print("");

          u8g2.setCursor(63, 63);
          if (minutes < 10)
            u8g2.print("0");
          u8g2.print(minutes);

          u8g2.setFont(u8g2_font_tenthinnerguys_tu);
          u8g2.setCursor(105, 42);
          if (seconds < 10)
            u8g2.print("0");
          u8g2.print(seconds);

          u8g2.setCursor(105, 63);
          u8g2.print(isAM() ? "AM" : "PM");

          u8g2.setFont(u8g2_font_waffle_t_all);
          if (!isDark)
          { // if mute on dark is not active (or false)
            if (hourlyAlarm || halfHourlyAlarm)
              u8g2.drawUTF8(103, 52, "\ue271"); // symbol for hourly/half-hourly alarm
          }

          if (useWifi)
            u8g2.drawUTF8(112, 52, "\ue2b5"); // wifi-active symbol
        }
        u8g2.sendBuffer();
        pulse = !pulse;
      }
    }

    // check if the touchValue is below the threshold
    // if it is, set ledPin to HIGH
    if (analogRead(NEXT_BUTTON) > 1000)
    {
      menu();
    }
  }
}

/**
 * @brief Main menu handling function
 */
void menu()
{
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso22_tr);
  u8g2.setCursor(5, 43);
  u8g2.print("SETTINGS");
  u8g2.sendBuffer();
  delay(500);
  byte count = 0;
  while (true)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("SETTINGS");
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("DISPLAY");
    u8g2.setCursor(5, 32);
    u8g2.print("ALARMS");
    u8g2.setCursor(5, 42);
    u8g2.print("WIFI");
    u8g2.setCursor(5, 52);
    u8g2.print("GPS DATA");
    u8g2.setCursor(5, 62);
    u8g2.print("ABOUT");
    u8g2.setCursor(80, 52);
    u8g2.print("RESET");
    u8g2.setCursor(80, 62);
    u8g2.print("EXIT");

    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(48, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(42, 32, "\ue14e");
    else if (count == 2)
      u8g2.drawUTF8(30, 42, "\ue14e");
    else if (count == 3)
      u8g2.drawUTF8(54, 52, "\ue14e");
    else if (count == 4)
      u8g2.drawUTF8(36, 62, "\ue14e");
    else if (count == 5)
      u8g2.drawUTF8(111, 52, "\ue14e");
    else if (count == 6)
      u8g2.drawUTF8(104, 62, "\ue14e");

    if (analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      count++;
      if (count > 6)
        count = 0;
    }

    u8g2.sendBuffer();
    if (analogRead(SELECT_BUTTON) > 1000)
    {
      delay(100);
      if (count == 0)
      {
        adjustBrightness();
        count = 6;
      }
      else if (count == 1)
      {
        editAlarms();
        count = 6;
      }
      else if (count == 2)
      {
        wifiConfig();
        count = 6;
      }
      else if (count == 3)
      {
        aboutGPS();
        count = 6;
      }
      else if (count == 4)
      {
        displayInfo();
        count = 6;
      }
      else if (count == 5)
      {
        resetAll();
        count = 6;
      }
      else if (count == 6)
        return;
    }
  }
}

/**
 * @brief Configure display brightness settings
 */
void adjustBrightness()
{
  pref.begin("database", false);
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(5, 43);
  u8g2.print("DISPLAY");
  u8g2.sendBuffer();
  delay(500);
  byte count = 0;
  while (true)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("DISPLAY: BRIGHTNESS");
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("MANUAL CONTROL");
    u8g2.setCursor(5, 32);
    u8g2.print("AUTO BRIGHTNESS");
    u8g2.setCursor(5, 42);
    u8g2.print("EXIT");

    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(90, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(96, 32, "\ue14e");
    else if (count == 2)
      u8g2.drawUTF8(30, 42, "\ue14e");

    if (analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      count++;
      if (count > 2)
        count = 0;
    }

    u8g2.sendBuffer();
    if (analogRead(SELECT_BUTTON) > 1000)
    {
      delay(100);

      if (count == 0)
      {
        if (autoBright == true)
        {
          byte i = 3;
          while (i > 0)
          {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_t0_11_mf);
            u8g2.setCursor(5, 22);
            u8g2.print("PLEASE DISABLE");
            u8g2.setCursor(5, 32);
            u8g2.print("AUTO BRIGHTNESS");
            u8g2.setCursor(60, 51);
            u8g2.print(i);
            u8g2.sendBuffer();
            delay(1000);
            i--;
          }
          count = 1;
        }
        else
        {
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_logisoso20_tr);
          u8g2.setCursor(5, 43);
          u8g2.print("MANUAL");
          u8g2.sendBuffer();
          delay(500);
          while (true)
          {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_t0_11_mf);
            u8g2.setCursor(5, 10);
            u8g2.print("BRIGHTNESS: MANUAL");
            u8g2.drawLine(0, 11, 127, 11);
            u8g2.setCursor(5, 32);
            u8g2.print("-");
            if (LCD_BRIGHTNESS < 100)
              u8g2.setCursor(20, 32);
            else if (LCD_BRIGHTNESS >= 100)
              u8g2.setCursor(17, 32);
            u8g2.print(LCD_BRIGHTNESS);
            u8g2.setCursor(40, 32);
            u8g2.print("+");
            u8g2.setCursor(5, 62);
            u8g2.print("EXIT");

            u8g2.setFont(u8g2_font_waffle_t_all);
            if (count == 0)
              u8g2.drawUTF8(3, 38, "\ue15a");
            else if (count == 1)
              u8g2.drawUTF8(38, 38, "\ue15a");
            else if (count == 2)
              u8g2.drawUTF8(32, 62, "\ue14e");

            if (analogRead(NEXT_BUTTON) > 1000)
            {
              delay(100);
              count++;
              if (count > 2)
                count = 0;
            }

            analogWrite(LCD_LIGHT, LCD_BRIGHTNESS);
            u8g2.sendBuffer();
            if (analogRead(SELECT_BUTTON) > 1000)
            {
              delay(100);

              if (count == 0)
              {
                LCD_BRIGHTNESS -= 5;
                if (LCD_BRIGHTNESS < 5)
                  LCD_BRIGHTNESS = 250;
              }
              if (count == 1)
              {
                LCD_BRIGHTNESS += 5;
                if (LCD_BRIGHTNESS > 250)
                  LCD_BRIGHTNESS = 5;
              }

              if (count == 2)
              {
                pref.putInt("lcd_bright", LCD_BRIGHTNESS); // SAVE THE VALUE
                pref.end();
                count = 2;
                break;
              }
            }
          }
        }
      }
      else if (count == 1)
      {
        delay(100);
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_logisoso20_tr);
        u8g2.setCursor(0, 43);
        u8g2.print("AUTO");
        u8g2.sendBuffer();
        delay(500);
        if (autoBright == true)
          count = 0;
        else
          count = 1;
        while (true)
        {
          delay(100);
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_t0_11_mf);
          u8g2.setCursor(5, 10);
          u8g2.print("BRIGHTNESS: AUTO");
          u8g2.drawLine(0, 11, 127, 11);
          u8g2.setCursor(5, 22);
          u8g2.print("ON");
          u8g2.setCursor(5, 32);
          u8g2.print("OFF");

          u8g2.setFont(u8g2_font_waffle_t_all);
          if (count == 0)
            u8g2.drawUTF8(24, 22, "\ue14e");
          else if (count == 1)
            u8g2.drawUTF8(24, 32, "\ue14e");

          u8g2.sendBuffer();
          if (analogRead(NEXT_BUTTON) > 1000)
          {
            delay(100);
            count++;
            if (count > 1)
              count = 0;
          }

          if (analogRead(SELECT_BUTTON) > 1000)
          {
            delay(100);
            if (count == 0)
            {
              pref.putBool("autoBright", true);
              autoBright = pref.getBool("autoBright", false);
              pref.end();
              break;
            }
            else if (count == 1)
            {
              pref.putBool("autoBright", false);
              autoBright = pref.getBool("autoBright", false);
              pref.end();
              break;
            }
          }
        }
        count = 2;
      }
      else if (count == 2)
      {
        delay(100);
        pref.end();
        return;
      }
    }
  }
}

/**
 * @brief Configure alarm settings
 */
void editAlarms()
{
  pref.begin("database", false);
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(0, 43);
  u8g2.print("ALARMS");
  u8g2.sendBuffer();
  delay(500);
  byte count = 0;
  while (true)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("SETTINGS: ALARMS");
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("HOURLY");
    u8g2.setCursor(5, 32);
    u8g2.print("HALF-HOURLY");
    u8g2.setCursor(5, 42);
    u8g2.print("MUTE ALARMS IN DARK");
    u8g2.setCursor(5, 52);
    u8g2.print("BUZZER VOLUME");
    u8g2.setCursor(5, 62);
    u8g2.print("EXIT");

    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(42, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(72, 32, "\ue14e");
    else if (count == 2)
      u8g2.drawUTF8(119, 42, "\ue14e");
    else if (count == 3)
      u8g2.drawUTF8(85, 52, "\ue14e");
    else if (count == 4)
      u8g2.drawUTF8(30, 62, "\ue14e");

    if (analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      count++;
      if (count > 4)
        count = 0;
    }
    u8g2.sendBuffer();
    if (analogRead(SELECT_BUTTON) > 1000)
    {
      delay(100);

      if (count == 0)
      {
        delay(100);
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_logisoso20_tr);
        u8g2.setCursor(0, 43);
        u8g2.print("HOURLY");
        u8g2.sendBuffer();
        delay(500);
        if (hourlyAlarm == true)
          count = 0;
        else
          count = 1;
        while (true)
        {
          delay(100);
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_t0_11_mf);
          u8g2.setCursor(5, 10);
          u8g2.print("ALARMS: HOURLY");
          u8g2.drawLine(0, 11, 127, 11);
          u8g2.setCursor(5, 22);
          u8g2.print("ON");
          u8g2.setCursor(5, 32);
          u8g2.print("OFF");

          u8g2.setFont(u8g2_font_waffle_t_all);
          if (count == 0)
            u8g2.drawUTF8(24, 22, "\ue14e");
          else if (count == 1)
            u8g2.drawUTF8(24, 32, "\ue14e");

          u8g2.sendBuffer();
          if (analogRead(NEXT_BUTTON) > 1000)
          {
            delay(100);
            count++;
            if (count > 1)
              count = 0;
          }

          if (analogRead(SELECT_BUTTON) > 1000)
          {
            delay(100);
            if (count == 0)
            {
              pref.putBool("hourlyAlarm", true);
              hourlyAlarm = pref.getBool("hourlyAlarm", false);
              pref.end();
              break;
            }
            else if (count == 1)
            {
              pref.putBool("hourlyAlarm", false);
              hourlyAlarm = pref.getBool("hourlyAlarm", false);
              pref.end();
              break;
            }
          }
        }
        count = 4;
      }
      else if (count == 1)
      {
        delay(100);
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_logisoso20_tr);
        u8g2.setCursor(0, 43);
        u8g2.print("HALF-HOUR");
        u8g2.sendBuffer();
        delay(500);
        if (halfHourlyAlarm == true)
          count = 0;
        else
          count = 1;
        while (true)
        {
          delay(100);
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_t0_11_mf);
          u8g2.setCursor(5, 10);
          u8g2.print("ALARMS: HALF-HOURLY");
          u8g2.drawLine(0, 11, 127, 11);
          u8g2.setCursor(5, 22);
          u8g2.print("ON");
          u8g2.setCursor(5, 32);
          u8g2.print("OFF");
          u8g2.setFont(u8g2_font_waffle_t_all);
          if (count == 0)
            u8g2.drawUTF8(24, 22, "\ue14e");
          else if (count == 1)
            u8g2.drawUTF8(24, 32, "\ue14e");
          u8g2.sendBuffer();

          if (analogRead(NEXT_BUTTON) > 1000)
          {
            delay(100);
            count++;
            if (count > 1)
              count = 0;
          }

          if (analogRead(SELECT_BUTTON) > 1000)
          {
            delay(100);
            if (count == 0)
            {
              pref.putBool("halfHourlyAlarm", true);
              halfHourlyAlarm = pref.getBool("halfHourlyAlarm", false);
              pref.end();
              break;
            }
            else if (count == 1)
            {
              pref.putBool("halfHourlyAlarm", false);
              halfHourlyAlarm = pref.getBool("halfHourlyAlarm", false);
              pref.end();
              break;
            }
          }
        }
        count = 4;
      }
      else if (count == 2)
      {
        delay(100);
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_logisoso20_tr);
        u8g2.setCursor(0, 43);
        u8g2.print("MUTE DARK");
        u8g2.sendBuffer();
        delay(500);
        if (muteDark == true)
          count = 0;
        else
          count = 1;
        while (true)
        {
          delay(100);
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_t0_11_mf);
          u8g2.setCursor(5, 10);
          u8g2.print("ALARMS: MUTE IN DARK");
          u8g2.drawLine(0, 11, 127, 11);
          u8g2.setCursor(5, 22);
          u8g2.print("ON");
          u8g2.setCursor(5, 32);
          u8g2.print("OFF");
          u8g2.setFont(u8g2_font_waffle_t_all);
          if (count == 0)
            u8g2.drawUTF8(24, 22, "\ue14e");
          else if (count == 1)
            u8g2.drawUTF8(24, 32, "\ue14e");

          u8g2.sendBuffer();
          if (analogRead(NEXT_BUTTON) > 1000)
          {
            delay(100);
            count++;
            if (count > 1)
              count = 0;
          }
          if (analogRead(SELECT_BUTTON) > 1000)
          {
            delay(100);
            if (count == 0)
            {
              pref.putBool("muteDark", true);
              muteDark = pref.getBool("muteDark", false);
              pref.end();
              break;
            }
            else if (count == 1)
            {
              pref.putBool("muteDark", false);
              muteDark = pref.getBool("muteDark", false);
              pref.end();
              break;
            }
          }
        }
        count = 4;
      }
      else if (count == 3)
      {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_logisoso20_tr);
        u8g2.setCursor(5, 43);
        u8g2.print("VOLUME");
        u8g2.sendBuffer();
        delay(500);
        count = 0;
        while (true)
        {
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_t0_11_mf);
          u8g2.setCursor(5, 10);
          u8g2.print("ALARMS: VOLUME");
          u8g2.drawLine(0, 11, 127, 11);
          u8g2.setCursor(5, 32);
          u8g2.print("-");
          if (buzzVol < 100)
            u8g2.setCursor(20, 32);
          else if (buzzVol >= 100)
            u8g2.setCursor(17, 32);
          u8g2.print(buzzVol);
          u8g2.setCursor(40, 32);
          u8g2.print("+");
          u8g2.setCursor(5, 62);
          u8g2.print("EXIT");

          u8g2.setFont(u8g2_font_waffle_t_all);
          if (count == 0)
            u8g2.drawUTF8(3, 38, "\ue15a");
          else if (count == 1)
            u8g2.drawUTF8(38, 38, "\ue15a");
          else if (count == 2)
            u8g2.drawUTF8(32, 62, "\ue14e");

          if (analogRead(NEXT_BUTTON) > 1000)
          {
            delay(100);
            count++;
            if (count > 2)
              count = 0;
          }

          u8g2.sendBuffer();
          if (analogRead(SELECT_BUTTON) > 1000)
          {
            delay(50);

            if (count == 0)
            {
              buzzer(50, 1);
              buzzVol -= 5;
              if (buzzVol < 5)
                buzzVol = 255;
            }
            if (count == 1)
            {
              buzzer(50, 1);
              buzzVol += 5;
              if (buzzVol > 255)
                buzzVol = 5;
            }

            if (count == 2)
            {
              pref.putInt("buzzVol", buzzVol); // SAVE THE VALUE
              buzzVol = pref.getInt("buzzVol", 255);
              pref.end();
              count = 2;
              break;
            }
          }
        }
        count = 4;
      }
      else if (count == 4)
      {
        delay(100);
        pref.end();
        return;
      }
    }
  }
}

/**
 * @brief Configure WiFi settings
 */
void wifiConfig()
{
  pref.begin("database", false);
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(0, 43);
  u8g2.print("WIFI");
  u8g2.sendBuffer();
  delay(500);
  byte count;
  if (useWifi == true)
    count = 0;
  else
    count = 1;
  while (true)
  {
    delay(100);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("SETTINGS: WIFI");
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("ON");
    u8g2.setCursor(5, 32);
    u8g2.print("OFF");

    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(24, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(24, 32, "\ue14e");

    u8g2.sendBuffer();
    if (analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      count++;
      if (count > 1)
        count = 0;
    }

    if (analogRead(SELECT_BUTTON) > 1000)
    {
      delay(100);
      if (count == 0)
      {
        pref.putBool("useWifi", true);
        useWifi = pref.getBool("useWifi", false);
        pref.end();
        break;
      }
      else if (count == 1)
      {
        pref.putBool("useWifi", false);
        useWifi = pref.getBool("useWifi", false);
        pref.end();
        break;
      }
    }
  }
  byte i = 5;
  while (i > 0)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(5, 21);
    u8g2.print("DEVICE WILL RESTART");
    u8g2.setCursor(60, 51);
    u8g2.print(i);
    u8g2.sendBuffer();
    delay(1000);
    i--;
  }
  ESP.restart();
}

/**
 * @brief Display GPS status and data
 */
void aboutGPS()
{
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(0, 43);
  u8g2.print("GPS");
  u8g2.sendBuffer();
  delay(500);
  while (true)
  {
    gpsInfo("GPS DATA");
    if (analogRead(SELECT_BUTTON) > 1000 || analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      break;
    }
  }
  return;
}

/**
 * @brief Display device information and credits
 */
void displayInfo()
{
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(0, 43);
  u8g2.print("INFO");
  u8g2.sendBuffer();
  delay(500);
  while (true)
  {
    delay(100);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_tiny_simon_tr);
    u8g2.setCursor(1, 5);
    u8g2.print("Software Ver. ");
    u8g2.print(SWVersion);
    u8g2.setCursor(1, 11);
    u8g2.print("Dev.: ");
    u8g2.print("A. Patra");
    u8g2.setCursor(1, 19);
    u8g2.print("This is a special version of");
    u8g2.setCursor(1, 25);
    u8g2.print("my existing code ESP32-GPS-Clock");
    u8g2.setCursor(1, 31);
    u8g2.print("which was for my sister Nini.");
    u8g2.setCursor(1, 37);
    u8g2.print("This is for my student Mini on");
    u8g2.setCursor(1, 43);
    u8g2.print("her birthday 14th November 2024");
    u8g2.setCursor(1, 49);
    u8g2.print("I have kept it OPEN SOURCE at");
    u8g2.setCursor(1, 55);
    u8g2.print("github.com/KAMADOTANJIRO-BEEP");
    u8g2.setCursor(1, 61);
    u8g2.print("named ESP32-GPS-Clock-V2");

    u8g2.sendBuffer();

    if (analogRead(SELECT_BUTTON) > 1000 || analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      break;
    }
  }
}

/**
 * @brief Reset all settings to default values
 */
void resetAll()
{
  pref.begin("database", false);
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(0, 43);
  u8g2.print("RESET");
  u8g2.sendBuffer();
  delay(500);
  bool temp;
  byte count = 1;
  while (true)
  {
    delay(100);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("SETTINGS: RESET ALL");
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("YES");
    u8g2.setCursor(5, 32);
    u8g2.print("NO");

    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(24, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(24, 32, "\ue14e");

    u8g2.sendBuffer();
    if (analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      count++;
      if (count > 1)
        count = 0;
    }

    if (analogRead(SELECT_BUTTON) > 1000)
    {
      delay(100);
      if (count == 0)
        temp = true;
      else if (count == 1)
        temp = false;
      break;
    }
  }
  if (temp)
  {
    pref.putInt("lcd_bright", 250);
    pref.putBool("autoBright", false);
    pref.putBool("hourlyAlarm", false);
    pref.putBool("halfHourlyAlarm", false);
    pref.putBool("useWifi", false);
    pref.putBool("muteDark", false);
    pref.putString("ssid", "");
    pref.putString("password", "");
    pref.putInt("buzzVol", 50);
    pref.end();

    byte i = 5;
    while (i > 0)
    {
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_luRS08_tr);
      u8g2.setCursor(5, 21);
      u8g2.print("DEVICE WILL RESTART");
      u8g2.setCursor(60, 51);
      u8g2.print(i);
      u8g2.sendBuffer();
      delay(1000);
      i--;
    }
    ESP.restart();
  }
}

/**
 * @brief Custom delay function that ensures GPS data is processed
 * @param ms Time to delay in milliseconds
 */
static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (Serial1.available())
      gps.encode(Serial1.read());
  } while (millis() - start < ms);
}

/**
 * @brief Display GPS information on the LCD screen
 * @param msg Title message to display
 */
void gpsInfo(String msg)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_luRS08_tr);
  u8g2.setCursor(1, 9);
  u8g2.print(msg);
  u8g2.setFont(u8g2_font_5x7_mr);
  u8g2.setCursor(1, 24);
  u8g2.print("Satellites");
  u8g2.setCursor(25, 36);
  u8g2.print(gps.satellites.value());

  u8g2.setCursor(58, 24);
  u8g2.print("HDOP");
  u8g2.setCursor(58, 36);
  u8g2.print(gps.hdop.hdop());

  u8g2.setCursor(92, 24);
  u8g2.print("Speed");
  u8g2.setCursor(86, 36);
  u8g2.print(int(gps.speed.kmph()));
  u8g2.setFont(u8g2_font_micro_tr);
  u8g2.print("kmph");
  u8g2.setFont(u8g2_font_5x7_mr);

  u8g2.setCursor(1, 51);
  u8g2.print("Fix Age");
  u8g2.setCursor(6, 63);
  u8g2.print(gps.time.age());
  u8g2.print("ms"); //

  u8g2.setCursor(42, 51);
  u8g2.print("Altitude");
  u8g2.setCursor(42, 63);
  u8g2.print(gps.altitude.meters());
  u8g2.print("m");

  u8g2.setCursor(88, 51);
  u8g2.print("Lat & Lng");
  u8g2.setCursor(88, 57);
  u8g2.setFont(u8g2_font_4x6_tn);
  u8g2.print(gps.location.lat(), 7);
  u8g2.setCursor(88, 64);
  u8g2.print(gps.location.lng(), 7);

  u8g2.sendBuffer();
  smartDelay(900);
}

/**
 * @brief Generate buzzer sounds with specified parameters
 * @param Delay Duration of each beep in milliseconds
 * @param count Number of beeps to generate
 */
void buzzer(int Delay, byte count)
{
  for (int i = 0; i < count; i++)
  {
    analogWrite(BUZZER_PIN, buzzVol);
    delay(Delay);
    analogWrite(BUZZER_PIN, 0);
    delay(Delay);
  }
}

/**
 * @brief Display WiFi manager configuration instructions
 */
void wifiManagerInfoPrint()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_luRS08_tr);
  u8g2.setCursor(1, 10);
  u8g2.print("Turn ON WiFi");
  u8g2.setCursor(1, 22);
  u8g2.print("on your phone/laptop.");
  u8g2.setCursor(1, 34);
  u8g2.print("Connect to->");
  u8g2.setCursor(1, 46);
  u8g2.print("SSID: WIFI_MANAGER");
  u8g2.setCursor(1, 58);
  u8g2.print("Password: WIFImanager");
  u8g2.sendBuffer();
}

/**
 * @brief Handle WiFi events and display appropriate messages
 * @param event WiFiEvent type indicating the event that occurred
 */
void WiFiEvent(WiFiEvent_t event)
{
  if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(1, 10);
    u8g2.print("On browser, go to");
    u8g2.setCursor(1, 22);
    u8g2.print("192.168.4.1/wifi");
    u8g2.setCursor(1, 34);
    u8g2.print("Enter the your Wifi");
    u8g2.setCursor(1, 46);
    u8g2.print("credentials of 2.4Ghz");
    u8g2.setCursor(1, 58);
    u8g2.print("network. Then Submit. ");
    u8g2.sendBuffer();
  }
}

/**
 * @brief Display error messages with countdown
 * @param device Name of the device causing error
 * @param msg Error message to display
 */
void errorMsgPrint(String device, String msg)
{
  byte i = 5;
  while (i > 0)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("ERROR: " + device);
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print(msg);
    u8g2.setFont(u8g2_font_luRS08_tr);
    u8g2.setCursor(60, 51);
    u8g2.print(i);
    u8g2.sendBuffer();
    delay(1000);
    i--;
  }
}
