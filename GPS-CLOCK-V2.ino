/*
  GPS-CLOCK-V2.ino

  This is an enhanced version of https://github.com/desiFish/GPS-CLOCK-V2
  with added buttons and menu system for improved user interaction and control.

  Copyright (C) 2024-2025 desiFish

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
#define SWVersion "2.1.0"

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
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

// GPS and Timing
#include <TinyGPSPlus.h>
#include <time.h>
// Data Storage
#include <Preferences.h>
// JSON handling
#include <ArduinoJson.h>

// --- Filesystem for web UI ---
#include <LittleFS.h>

// --- Async API for WiFi credentials ---
// GET: /api/wifi/creds  -> { "ssid": "...", "password": "..." }
// POST: /api/wifi/creds { "ssid": "...", "password": "..." }

//========== Hardware Configuration ==========//
/**
 * Display Configuration
 * Using ST7920 128x64 LCD with Software SPI
 * clock=18: Clock signal pin
 * data=23: Data signal pin
 * No CS or Reset pins used (U8X8_PIN_NONE)
 */
U8G2_ST7920_128X64_F_SW_SPI u8g2(U8G2_R0, /* clock=*/18, /* data=*/23, /* cs=*/U8X8_PIN_NONE, /* reset=*/U8X8_PIN_NONE);
// BME280 Object
Adafruit_BME280 bme;
// Preference library object or instance
Preferences pref;
// Light Sensor Object
BH1750 lightMeter;

// Time related
time_t currentEpoch;
struct tm timeinfo;

//========== Pin Definitions ==========//
/**
 * GPS Module Pins
 * RXPin = 16: Connect to GPS TX
 * TXPin = 17: Connect to GPS RX
 */
static const int RXPin = 16, TXPin = 17;

// HW Control Pins

#define LCD_LIGHT 4      // LCD backlight control (PWM)
#define BUZZER_PIN 33    // Active buzzer control
#define NEXT_BUTTON 36   // Menu navigation (ADC input)
#define SELECT_BUTTON 39 // Menu selection (ADC input)

// GPS instance
TinyGPSPlus gps;

//========== Global Variables ==========//
/**
 * Time/Date Arrays
 * week[]: Day names for display
 * monthChar[]: Month names for display
 */
String week[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
String monthChar[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// variables for holding time data globally
byte days = 0, months = 0, hours = 0, minutes = 0, seconds = 0;
int years = 0;

// LUX (BH1750) update frequency
unsigned long lastLightRead = 0;
unsigned long lastBrightnessUpdate = 0;

const unsigned long lightInterval = 4000;    // lux sampling
const unsigned long brightnessInterval = 20; // animation

// BME280 update frequency
unsigned long lastTime2 = 0;
const long timerDelay2 = 12000; // temperature sensor update delay

bool isDark, menuOpen = false; // Tracks ambient light state
float temperature = 0.0;
int humidity = 0, pressure = 0;

// features config, saved in preference library
int LCD_BRIGHTNESS, buzzVol;
bool autoBright, hourlyAlarm, halfHourlyAlarm, useWifi, muteDark, offInDark, hour12Mode;

// your wifi name and password (saved in preference library)
String ssid;
String password;

const int pwmChannel = 0;    // PWM channel 0–15
const int pwmFreq = 5000;    // PWM frequency in Hz
const int pwmResolution = 8; // 8-bit resolution (0–255)

const int BUZZER_CHANNEL = 1;
const int PWM_FREQ = 2700; // default tone frequency
const int PWM_RES = 8;     // 8-bit resolution

// for various server related stuff
AsyncWebServer server(80);

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
  updateInProgress = false;
  delay(1000);
}

time_t prevDisplay = 0; // when the digital clock was displayed
// for creating task attached to CORE 0 of CPU
TaskHandle_t loop1Task;
byte currentBrightness = 250; // Track current brightness level
float lux = 0;
byte targetBrightness = 0;

/**
 * @brief Generic menu for editing boolean settings
 * @param displayText The text to display in the menu
 * @param prefKey The Preferences key to store the value
 * @param var Reference to the variable to update
 */
void editBoolSetting(const char *displayText, const char *prefKey, bool &var)
{
  pref.begin("database", false);
  byte count = !var;
  while (true)
  {
    delay(100);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print(displayText);
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("ON");
    u8g2.setCursor(5, 32);
    u8g2.print("OFF");
    u8g2.setCursor(5, 62);
    u8g2.print("EXIT");

    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(24, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(24, 32, "\ue14e");
    else if (count == 2)
      u8g2.drawUTF8(32, 62, "\ue14e");

    u8g2.sendBuffer();
    if (analogRead(NEXT_BUTTON) > 1000)
    {
      count++;
      delay(100);
      if (count > 2)
        count = 0;
    }

    if (analogRead(SELECT_BUTTON) > 1000)
    {
      if (count == 2)
      {
        pref.end();
        return;
      }
      delay(100);
      pref.putBool(prefKey, count == 0 ? true : false);
      var = pref.getBool(prefKey, true);
      pref.end();
      break;
    }

    if (updateInProgress)
      return;
  }
}

// For confirmation dialogs: no reference, no preference
byte editBoolSetting(const char *displayText)
{
  byte count = 0;
  while (true)
  {
    delay(100);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print(displayText);
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("YES");
    u8g2.setCursor(5, 32);
    u8g2.print("NO");
    u8g2.setCursor(5, 62);
    u8g2.print("EXIT");

    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(24, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(24, 32, "\ue14e");
    else if (count == 2)
      u8g2.drawUTF8(32, 62, "\ue14e");

    u8g2.sendBuffer();
    if (analogRead(NEXT_BUTTON) > 1000)
    {
      count++;
      delay(100);
      if (count > 2)
        count = 0;
    }

    if (analogRead(SELECT_BUTTON) > 1000)
    {
      if (count == 2)
        return 5; // random number returned to indicate exit
      delay(100);
      break;
    }
    if (updateInProgress)
      return 5; // random number returned to indicate exit
  }
  return count;
}

// --- Serve static files from LittleFS (SPIFFS) and provide async API for WiFi credentials ---
void setupWebServer()
{
  pref.begin("database", false);
  // Serve static files (index.html, etc.)
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Add a handler for root path
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    if (LittleFS.exists("/index.html")) {
      request->send(LittleFS, "/index.html", "text/html");
    } else {
      request->send(404, "text/plain", "File not found! Please check if files are uploaded to LittleFS");
    } });

  // API: Get WiFi credentials
  server.on("/api/wifi/creds", HTTP_GET, [](AsyncWebServerRequest *request)
            {
    DynamicJsonDocument doc(128);
    doc["ssid"] = ssid;
    doc["password"] = password;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out); });

  // API: Set WiFi credentials (async JSON POST)
  server.on("/api/wifi/creds", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t, size_t)
            {
      DynamicJsonDocument doc(256);
      if (len > 0 && deserializeJson(doc, (const char*)data, len) == DeserializationError::Ok) {
        if (doc.containsKey("ssid") && doc.containsKey("password")) {
          ssid = String(doc["ssid"].as<const char*>());
          password = String(doc["password"].as<const char*>());
          ssid.trim();
          password.trim();
          pref.putString("ssid", ssid);
          pref.putString("password", password);
          pref.end();
          DynamicJsonDocument resp(64);
          resp["ok"] = true;
          String out;
          serializeJson(resp, out);
          
          // Send response with longer delay for browser
          request->send(200, "application/json", out);
          
          // Schedule restart using a task
          static TaskHandle_t restartTask = NULL;
          xTaskCreate([](void* parameter) {
            delay(2000);  // Wait for response to be sent and processed
            ESP.restart();
            vTaskDelete(NULL);
          }, "restart", 4096, NULL, 1, &restartTask);
          
          return;
        }
      }
      request->send(400, "application/json", "{\"error\":\"Invalid data\"}"); });
}

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
  Serial.begin(115200);
  Wire.begin();
  Serial1.begin(9600, SERIAL_8N1, RXPin, TXPin); // for GPS running on Hardware Serial

  pinMode(LCD_LIGHT, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Attach buzzer to PWM channel
  ledcAttachChannel(BUZZER_PIN, PWM_FREQ, PWM_RES, BUZZER_CHANNEL);
  // Setup ESP32 PWM for backlight
  ledcAttachChannel(LCD_LIGHT, pwmFreq, pwmResolution, pwmChannel);
  ledcWrite(LCD_LIGHT, 150);

  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.drawLine(0, 17, 127, 17);
  u8g2.setFont(u8g2_font_7x14B_mr);
  u8g2.setCursor(12, 30);
  u8g2.print("GPS Clock V2");
  u8g2.drawLine(0, 31, 127, 31);
  u8g2.sendBuffer();
  delay(1000);

  if (!pref.begin("database", false)) // open database
    errorMsgPrint("DATABASE", "ERROR INITIALIZE");

  if (!pref.isKey("lcd_bright"))
  {
    pref.putInt("lcd_bright", 250);
  }
  LCD_BRIGHTNESS = pref.getInt("lcd_bright", 250);

  if (!pref.isKey("autoBright"))
  {
    pref.putBool("autoBright", true);
  }
  autoBright = pref.getBool("autoBright", true);

  if (!pref.isKey("hourlyAlarm"))
  {
    pref.putBool("hourlyAlarm", true);
  }
  hourlyAlarm = pref.getBool("hourlyAlarm", true);

  if (!pref.isKey("halfHourlyAlarm"))
  {
    pref.putBool("halfHourlyAlarm", true);
  }
  halfHourlyAlarm = pref.getBool("halfHourlyAlarm", true);

  if (!pref.isKey("useWifi"))
  {
    pref.putBool("useWifi", false);
  }
  useWifi = pref.getBool("useWifi", false);

  if (!pref.isKey("muteDark"))
  {
    pref.putBool("muteDark", true);
  }
  muteDark = pref.getBool("muteDark", true);

  if (!pref.isKey("buzzVol"))
  {
    pref.putInt("buzzVol", 255);
  }
  buzzVol = pref.getInt("buzzVol", 255);

  simpleBeep(1, 100, 255);

  if (!pref.isKey("offInDark"))
  {
    pref.putBool("offInDark", true);
  }
  offInDark = pref.getBool("offInDark", true);

  if (!pref.isKey("hour12Mode"))
  {
    pref.putBool("hour12Mode", true);
  }
  hour12Mode = pref.getBool("hour12Mode", true);

  if (!lightMeter.begin(BH1750::ONE_TIME_HIGH_RES_MODE))
    errorMsgPrint("BH1750", "CANNOT FIND");

  if (!bme.begin(BME280_ADDRESS_ALTERNATE))
  {
    errorMsgPrint("BME280", "CANNOT FIND");
  }
  // Set up oversampling and filter initialization
  bme.setSampling(Adafruit_BME280::MODE_FORCED,      // use forced mode for power saving
                  Adafruit_BME280::SAMPLING_X16,     // temperature
                  Adafruit_BME280::SAMPLING_X16,     // pressure
                  Adafruit_BME280::SAMPLING_X16,     // humidity
                  Adafruit_BME280::FILTER_X16,       // filter
                  Adafruit_BME280::STANDBY_MS_1000); // set delay between measurements
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
    { // --- LittleFS Init ---
      if (!LittleFS.begin())
      {
        Serial.println("[ERROR] LittleFS Mount Failed");
        Serial.println("Formatting LittleFS...");
        if (LittleFS.format())
        {
          Serial.println("LittleFS formatted successfully");
          if (!LittleFS.begin())
          {
            Serial.println("[ERROR] LittleFS Mount Failed even after formatting");
          }
        }
        else
        {
          Serial.println("[ERROR] LittleFS Format Failed");
        }
      }
      else
      {
        Serial.println("LittleFS mounted successfully!");
      }
      delay(100);
      Serial.println("No values saved for ssid or password");
      Serial.println("Setting AP (Access Point)");
      WiFi.mode(WIFI_AP);
      WiFi.setHostname("MiniGPSClock");
      WiFi.softAP("WIFI_MANAGER", "WIFImanager");
      IPAddress IP = WiFi.softAPIP();
      Serial.print("AP IP address: ");
      Serial.println(IP);
      wifiManagerInfoPrint();
      setupWebServer();
      server.begin();
      Serial.println("HTTP server started");
      WiFi.onEvent(WiFiEvent);
      while (true)
      {
        delay(50);
        if (analogRead(SELECT_BUTTON) > 1000)
        {
          errorMsgPrint("WIFI", "CANCELLED");
          delay(100);
          server.end();
          WiFi.softAPdisconnect(true);
          break; // exit loop to continue normal flow
        }
      }
    }
    else
    {
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
      {
        Serial.println(ssid);
        Serial.println(WiFi.localIP());
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_luRS08_tr);
        u8g2.setCursor(1, 20);
        u8g2.print("WIFI CONNECTED");
        u8g2.setCursor(1, 42);
        u8g2.print(WiFi.localIP());
        u8g2.sendBuffer();
        ElegantOTA.begin(&server); // Start ElegantOTA
        // ElegantOTA callbacks
        ElegantOTA.onStart(onOTAStart);
        ElegantOTA.onProgress(onOTAProgress);
        ElegantOTA.onEnd(onOTAEnd);
        // Start server last after all routes are set
        server.begin();
        delay(4000);
      }
    }
  }

  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_mr);
  u8g2.setCursor(44, 30);
  u8g2.print("HELLO!");
  u8g2.setCursor(36, 52);
  u8g2.print("MINI");
  u8g2.setFont(u8g2_font_streamline_food_drink_t);
  u8g2.drawUTF8(80, 54, "U+4"); // birthday cake icon
  u8g2.sendBuffer();
  delay(1500);
  pref.end();
  if (!autoBright) // set initial brightness, in manual mode
    ledcWrite(LCD_LIGHT, LCD_BRIGHTNESS);
  xTaskCreatePinnedToCore(
      loop1,       /* Task function. */
      "loop1Task", /* name of task. */
      10000,       /* Stack size of task */
      NULL,        /* parameter of the task */
      1,           /* priority of the task */
      &loop1Task,  /* Task handle to keep track of created task */
      0);          /* pin task to core 0 */
}

/**
 * @brief Secondary loop running on Core 0
 *
 * Responsibilities:
 * - Periodically read the light sensor (BH1750) and update ambient light state.
 * - Adjust LCD brightness automatically and smoothly if enabled.
 * - Read temperature and humidity from the AHT sensor at intervals.
 * - Trigger alarms (hourly/half-hourly) and buzzer as needed.
 * - Perform power-saving operations based on ambient light and settings.
 *
 * @param pvParameters Required by FreeRTOS
 */
void loop1(void *pvParameters)
{
  Serial.println("[DEBUG] loop1: Started on Core 0");
  for (;;)
  {
    if (millis() - lastLightRead > lightInterval)
    {
      lastLightRead = millis();

      lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);

      // Block until measurement ready (with timeout for safety)
      unsigned long start = millis();
      while (!lightMeter.measurementReady(true))
      {
        if (millis() - start > 3000)
        { // 3s timeout
          Serial.println("[ERROR] Light sensor timeout!");
          break;
        }
        yield();
      }

      // Read lux (may be stale if timeout triggered)
      lux = lightMeter.readLightLevel();

      // Map lux to target brightness
      byte val1 = constrain(lux, 1, 120);
      targetBrightness = map(val1, 1, 120, 10, 255);

      Serial.print("[DEBUG] Lux = ");
      Serial.print(lux);
      Serial.print(", targetBrightness = ");
      Serial.print(targetBrightness);
      Serial.print(", isDark = ");
      Serial.println(isDark);
    }

    // --- Brightness animation every 200ms ---
    if (millis() - lastBrightnessUpdate > brightnessInterval)
    {
      lastBrightnessUpdate = millis();

      if (autoBright)
      {
        byte previousBrightness = currentBrightness; // store old value
        // Update darkness flag
        isDark = lux <= 1;

        if (isDark && offInDark)
        {
          if (!menuOpen)
            currentBrightness = 0; // force off
        }
        else
        {
          // exponential moving average (EMA)
          float alpha = 0.7; // smoother 0.7-0.9
          float tempBrightness = currentBrightness;
          tempBrightness = tempBrightness * alpha + targetBrightness * (1.0 - alpha);
          currentBrightness = (byte)tempBrightness;
        }

        // Only write PWM if it changed
        if (currentBrightness != previousBrightness)
        {
          ledcWrite(LCD_LIGHT, currentBrightness);
          Serial.print("[DEBUG] Brightness = ");
          Serial.println(currentBrightness);
        }
      }
    }

    if ((millis() - lastTime2) > timerDelay2)
    {
      if (!(isDark && offInDark)) // Only read sensor if not in dark mode with offInDark enabled
      {
        Serial.println("[DEBUG] loop1: Reading temperature/humidity sensor...");
        bme.takeForcedMeasurement();
        temperature = bme.readTemperature();
        humidity = bme.readHumidity();
        pressure = bme.readPressure() / 100.0F;
        Serial.print("[DEBUG] loop1: Temp=");
        Serial.print(temperature);
        Serial.print(", Hum=");
        Serial.print(humidity);
        Serial.print(", Pressure=");
        Serial.println(pressure);
      }
      lastTime2 = millis();
    }

    static bool alarmTriggered = false; // persists across loop iterations

    if (!(isDark && muteDark) && years > 2024)
    {
      int beeps = 0;

      if (minutes == 0 && hourlyAlarm)
        beeps = 1;
      else if (minutes == 30 && halfHourlyAlarm)
        beeps = 2;

      if (beeps > 0)
      {
        if (!alarmTriggered) // only trigger once per minute
        {
          Serial.print("[DEBUG] Alarm triggered at ");
          Serial.print(hours);
          Serial.print(":");
          Serial.println(minutes);

          simpleBeep(beeps, 1000, buzzVol);
          alarmTriggered = true;
        }
      }
      else
      {
        // reset flag when not matching alarm minute
        alarmTriggered = false;
      }
    }

    if (menuOpen)
      simpleBeep(1, 100, 20);

    delay(50);
  }
}

/**
 * @brief Main program loop running on Core 1
 *
 * Responsibilities:
 * - Handles GPS data processing and time synchronization
 * - Adjusts global time variables to IST immediately after GPS update
 * - Updates the display with current time, date, and sensor data
 * - Manages the menu system and user input
 * - Handles OTA updates and WiFi events if enabled
 * - Manages power-saving (dark mode) and display state
 */
void loop(void)
{
  // used for blinking ":" in time (for display)
  static bool pulse = true;
  static bool wasInDarkMode = false;

  if (WiFi.status() == WL_CONNECTED)
    ElegantOTA.loop();

  if (offInDark && isDark)
  {
    if (!wasInDarkMode)
    {
      Serial.println("[DEBUG] loop: Entering dark mode (display off, CPU slow)");
      wasInDarkMode = true;
      setCpuFrequencyMhz(10); // Lower CPU frequency
      u8g2.clearBuffer();     // Clear display
      u8g2.setPowerSave(1);   // Turn off display
      Serial1.flush();        // Clear any pending GPS data
    }
    delay(1000);
    return;
  }
  else if (wasInDarkMode)
  {
    Serial.println("[DEBUG] loop: Exiting dark mode (display on, CPU normal)");
    wasInDarkMode = false;
    setCpuFrequencyMhz(240); // Restore CPU frequency
    u8g2.setPowerSave(0);    // Turn on display
  }

  while (gps.hdop.hdop() > 100 && gps.satellites.value() < 2)
  {
    Serial.println("[DEBUG] loop: Waiting for GPS fix...");
    gpsInfo("Waiting for GPS...");
  }
  const char *ampmStr;
  while (Serial1.available())
  {
    if (gps.encode(Serial1.read()))
    { // process gps messages
      // when TinyGPSPlus reports new data...
      unsigned long age = gps.time.age();
      if (age < 500)
      {
        // Build tm struct with GPS UTC
        struct tm tmUTC = {};
        int gpsYear = gps.date.year();
        tmUTC.tm_year = gpsYear - 1900;      // struct tm wants "years since 1900"
        tmUTC.tm_mon = gps.date.month() - 1; // months 0-11
        tmUTC.tm_mday = gps.date.day();
        tmUTC.tm_hour = gps.time.hour();
        tmUTC.tm_min = gps.time.minute();
        tmUTC.tm_sec = gps.time.second();

        // Convert to epoch (UTC)
        time_t t = mktime(&tmUTC);

        // Apply IST offset (+5:30 → 19800 seconds)
        t += 19800;

        // Save epoch and also convert to broken-down local time
        currentEpoch = t;
        localtime_r(&t, &timeinfo);

        // Update globals for your display
        days = timeinfo.tm_mday;
        months = timeinfo.tm_mon + 1;
        years = timeinfo.tm_year + 1900;
        minutes = timeinfo.tm_min;
        seconds = timeinfo.tm_sec;

        // Convert to 12-hour format
        byte hour12 = timeinfo.tm_hour % 12;
        if (hour12 == 0)
          hour12 = 12; // midnight or noon → 12
        const char *ampm = (timeinfo.tm_hour < 12) ? "AM" : "PM";

        // If you want globals for 12h format:
        hours = hour12Mode ? hour12 : timeinfo.tm_hour;
        ampmStr = hour12Mode ? ampm : "";
      }
    }
  }

  if (!updateInProgress)
  {
    static time_t prevEpoch = 0;
    if (currentEpoch != prevEpoch)
    {
      prevEpoch = currentEpoch;
      Serial.println("[DEBUG] loop: Updating display");
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_t0_11_tf);
      u8g2.setCursor(2, 13);
      u8g2.print(String(temperature, 1)); // Show temperature with 1 decimal place
      u8g2.setFont(u8g2_font_threepix_tr);
      u8g2.setCursor(28, 8);
      u8g2.print("o");
      u8g2.setFont(u8g2_font_t0_11_tf);
      u8g2.setCursor(32, 13);
      u8g2.print("C ");
      String var = String(int(pressure)) + "hPa";        // Ensure font is set
      int stringWidth = u8g2.getStrWidth(var.c_str());   // Get exact pixel width
      u8g2.setCursor(((128 - stringWidth) / 2) + 3, 13); // Center using screen width
      u8g2.print(var);
      if (humidity > 99)
        u8g2.setCursor(90, 13);
      else
        u8g2.setCursor(96, 13);
      u8g2.print(humidity);
      u8g2.print("%rH");

      u8g2.drawLine(0, 17, 127, 17);
      u8g2.setFont(u8g2_font_samim_12_t_all); // u8g2_font_t0_11_mf  u8g2_font_t0_16_mr
      u8g2.setCursor(4, 29);
      if (days < 10)
        u8g2.print("0");
      u8g2.print(days);
      byte x = days % 10;
      u8g2.setFont(u8g2_font_tiny_simon_tr); //  u8g2_font_profont10_mr
      u8g2.setCursor(19, 25);
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
      u8g2.setCursor(29, 29);
      u8g2.print(monthChar[months - 1]);
      u8g2.setCursor(52, 29);
      u8g2.print(years);
      u8g2.setCursor(100, 29);
      u8g2.print(week[timeinfo.tm_wday]);

      u8g2.drawLine(0, 31, 127, 31);

      if (days == 14 && months == 11) // set these to ZERO to disable birthday message
      {                               // special message on birthday
        u8g2.setFont(u8g2_font_6x13_tr);
        u8g2.setCursor(5, 43);
        u8g2.print("HAPPY BIRTHDAY MINI!");

        u8g2.setFont(u8g2_font_logisoso16_tr);
        u8g2.setCursor(25, 63);
        if (hours < 10)
          u8g2.print("0");
        u8g2.print(hours);

        u8g2.print(pulse ? ":" : "");

        u8g2.setCursor(51, 63);
        if (minutes < 10)
          u8g2.print("0");
        u8g2.print(minutes);

        u8g2.print(pulse ? ":" : "");

        u8g2.setCursor(77, 63);
        if (seconds < 10)
          u8g2.print("0");
        u8g2.print(seconds);

        if (hour12Mode)
        {
          u8g2.setCursor(105, 63);
          u8g2.print(ampmStr);

          u8g2.setFont(u8g2_font_waffle_t_all);
          if ((hourlyAlarm || halfHourlyAlarm) && !(muteDark && isDark)) // only show if not muted in dark
            u8g2.drawUTF8(5, 54, "\ue271");                              // symbol for hourly/half-hourly alarm

          if (WiFi.status() == WL_CONNECTED)
            u8g2.drawUTF8(5, 64, "\ue2b5"); // wifi-active symbol
        }
        else
        {
          u8g2.setFont(u8g2_font_waffle_t_all);
          if ((hourlyAlarm || halfHourlyAlarm) && !(muteDark && isDark))
            u8g2.drawUTF8(105, 54, "\ue271");

          if (WiFi.status() == WL_CONNECTED)
            u8g2.drawUTF8(105, 64, "\ue2b5");
        }
      }
      else
      {
        u8g2.setFont(u8g2_font_logisoso30_tn);
        u8g2.setCursor(15, 63);
        if (hours < 10)
          u8g2.print("0");
        u8g2.print(hours);
        u8g2.print(pulse ? ":" : "");

        u8g2.setCursor(63, 63);
        if (minutes < 10)
          u8g2.print("0");
        u8g2.print(minutes);

        u8g2.setFont(u8g2_font_tenthinnerguys_tu);
        u8g2.setCursor(105, 42);
        if (seconds < 10)
          u8g2.print("0");
        u8g2.print(seconds);
        if (hour12Mode)
        {
          u8g2.setCursor(105, 63);
          u8g2.print(ampmStr);

          u8g2.setFont(u8g2_font_waffle_t_all);
          if ((hourlyAlarm || halfHourlyAlarm) && !(muteDark && isDark)) // only show if not muted in dark
            u8g2.drawUTF8(103, 52, "\ue271");                            // symbol for hourly/half-hourly alarm

          if (WiFi.status() == WL_CONNECTED)
            u8g2.drawUTF8(112, 52, "\ue2b5"); // wifi-active symbol
        }
        else
        {
          u8g2.setFont(u8g2_font_waffle_t_all);
          if ((hourlyAlarm || halfHourlyAlarm) && !(muteDark && isDark))
            u8g2.drawUTF8(103, 63, "\ue271");

          if (WiFi.status() == WL_CONNECTED)
            u8g2.drawUTF8(112, 63, "\ue2b5");
        }
      }
      u8g2.sendBuffer();
      pulse = !pulse;
    }

    // check if the touchValue is below the threshold
    // if it is, set ledPin to HIGH
    if (analogRead(NEXT_BUTTON) > 1000)
    {
      menuOpen = true;
      Serial.println("[DEBUG] loop: NEXT_BUTTON pressed, entering menu");
      menu();
      menuOpen = false;
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
  delay(300);
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
    u8g2.print("MODE");
    u8g2.setCursor(80, 22);
    u8g2.print("ABOUT");
    u8g2.setCursor(80, 32);
    u8g2.print("RESET");
    u8g2.setCursor(80, 42);
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
      u8g2.drawUTF8(30, 62, "\ue14e");
    else if (count == 5)
      u8g2.drawUTF8(111, 22, "\ue14e");
    else if (count == 6)
      u8g2.drawUTF8(111, 32, "\ue14e");
    else if (count == 7)
      u8g2.drawUTF8(104, 42, "\ue14e");

    if (analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      count++;
      if (count > 7)
        count = 0;
    }

    u8g2.sendBuffer();
    if (analogRead(SELECT_BUTTON) > 1000)
    {
      delay(100);
      if (count == 0)
      {
        adjustBrightness();
        count = 7;
      }
      else if (count == 1)
      {
        editAlarms();
        count = 7;
      }
      else if (count == 2)
      {
        wifiConfig();
        count = 7;
      }
      else if (count == 3)
      {
        aboutGPS();
        count = 7;
      }
      else if (count == 4)
      {
        changeMode();
        count = 7;
      }
      else if (count == 5)
      {
        displayInfo();
        count = 7;
      }
      else if (count == 6)
      {
        resetAll();
        count = 7;
      }
      else if (count == 7)
        return;
    }
    if (updateInProgress)
      return;
  }
}

/**
 * @brief Configure display brightness settings
 */
void adjustBrightness()
{
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(5, 43);
  u8g2.print("DISPLAY");
  u8g2.sendBuffer();
  delay(300);
  byte count = 0;
  while (true)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("SETTINGS: DISPLAY");
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("MANUAL CONTROL");
    u8g2.setCursor(5, 32);
    u8g2.print("AUTO BRIGHTNESS");
    u8g2.setCursor(5, 42);
    u8g2.print("DISPLAY OFF IN DARK");
    u8g2.setCursor(5, 52);
    u8g2.print("EXIT");

    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(90, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(96, 32, "\ue14e");
    else if (count == 2)
      u8g2.drawUTF8(119, 42, "\ue14e");
    else if (count == 3)
      u8g2.drawUTF8(30, 52, "\ue14e");

    if (analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      count++;
      if (count > 3)
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
          delay(300);
          while (true)
          {
            pref.begin("database", false);
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
            ledcWrite(LCD_LIGHT, LCD_BRIGHTNESS);
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
                count = 3;
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
        u8g2.print("AUTOBRIGHT");
        u8g2.sendBuffer();
        delay(300);
        editBoolSetting("BRIGHTNESS: AUTO", "autoBright", autoBright);
        count = 3;
      }
      else if (count == 2)
      {
        if (autoBright == false)
        {
          byte i = 3;
          while (i > 0)
          {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_t0_11_mf);
            u8g2.setCursor(5, 22);
            u8g2.print("PLEASE ENABLE");
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
          delay(100);
          u8g2.clearBuffer();
          u8g2.setFont(u8g2_font_logisoso20_tr);
          u8g2.setCursor(0, 43);
          u8g2.print("DARK-OFF");
          u8g2.sendBuffer();
          delay(300);
          editBoolSetting("BRIGHTNESS: DARK OFF", "offInDark", offInDark);
          count = 3;
        }
      }
      else if (count == 3)
      {
        delay(100);
        pref.end();
        return;
      }
    }
    if (updateInProgress)
      return;
  }
}

/**
 * @brief Configure alarm settings
 */
void editAlarms()
{
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(0, 43);
  u8g2.print("ALARMS");
  u8g2.sendBuffer();
  delay(300);
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
        delay(300);
        if (hourlyAlarm == true)
          count = 0;
        else
          count = 1;

        editBoolSetting("ALARMS: HOURLY", "hourlyAlarm", hourlyAlarm);
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
        delay(300);
        if (halfHourlyAlarm == true)
          count = 0;
        else
          count = 1;

        editBoolSetting("ALARMS: HALF-HOURLY", "halfHourlyAlarm", halfHourlyAlarm);
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
        delay(300);
        if (muteDark == true)
          count = 0;
        else
          count = 1;

        editBoolSetting("ALARMS: MUTE IN DARK", "muteDark", muteDark);
        count = 4;
      }
      else if (count == 3)
      {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_logisoso20_tr);
        u8g2.setCursor(5, 43);
        u8g2.print("VOLUME");
        u8g2.sendBuffer();
        delay(300);
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
              simpleBeep(1, 100, buzzVol);
              buzzVol -= 5;
              if (buzzVol < 5)
                buzzVol = 255;
            }
            if (count == 1)
            {
              simpleBeep(1, 100, buzzVol);
              buzzVol += 5;
              if (buzzVol > 255)
                buzzVol = 5;
            }

            if (count == 2)
            {
              pref.begin("database", false);
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
    if (updateInProgress)
      return;
  }
}

/**
 * @brief Change display mode settings
 */
void changeMode()
{
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(5, 43);
  u8g2.print("MODE");
  u8g2.sendBuffer();
  delay(300);
  byte count = 0;
  while (true)
  {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("SETTINGS: MODE");
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("12-HOUR");
    u8g2.setCursor(5, 32);
    u8g2.print("EXIT");
    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(52, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(30, 32, "\ue14e");
    if (analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      count++;
      if (count > 1)
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
        u8g2.print("12-HOUR");
        u8g2.sendBuffer();
        delay(300);
        if (hour12Mode == true)
          count = 0;
        else
          count = 1;

        editBoolSetting("SETTINGS: 12-HOUR", "hour12Mode", hour12Mode);
        count = 1;
      }
      else if (count == 1)
      {
        delay(100);
        return;
      }
    }
  }
  if (updateInProgress)
    return;
}
/**
 * @brief Configure WiFi settings
 */
void wifiConfig()
{
  delay(100);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_logisoso20_tr);
  u8g2.setCursor(0, 43);
  u8g2.print("WIFI");
  u8g2.sendBuffer();
  delay(300);
  byte count;
  if (useWifi == true)
    count = 0;
  else
    count = 1;

  editBoolSetting("SETTINGS: WIFI", "useWifi", useWifi);
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
  delay(300);
  while (true)
  {
    gpsInfo("GPS DATA");
    if (analogRead(SELECT_BUTTON) > 1000 || analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      break;
    }
    if (updateInProgress)
      return;
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
  delay(300);
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
    u8g2.print("desiFish");
    u8g2.setCursor(1, 19);
    u8g2.print("This is a special version of");
    u8g2.setCursor(1, 25);
    u8g2.print("my existing code GPS-Clock-V1");
    u8g2.setCursor(1, 31);
    u8g2.print("which was for my sister Nini.");
    u8g2.setCursor(1, 37);
    u8g2.print("This is for my student Mini on");
    u8g2.setCursor(1, 43);
    u8g2.print("her birthday 14th November 2024");
    u8g2.setCursor(1, 49);
    u8g2.print("I have kept it OPEN SOURCE at");
    u8g2.setCursor(1, 55);
    u8g2.print("github.com/desiFish");
    u8g2.setCursor(1, 61);
    u8g2.print("named ESP32-GPS-Clock-V2");

    u8g2.sendBuffer();

    if (analogRead(SELECT_BUTTON) > 1000 || analogRead(NEXT_BUTTON) > 1000)
    {
      delay(100);
      break;
    }
    if (updateInProgress)
      return;
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
  delay(300);
  byte count = 0;
  while (true)
  {
    delay(100);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_t0_11_mf);
    u8g2.setCursor(5, 10);
    u8g2.print("SETTINGS: RESET");
    u8g2.drawLine(0, 11, 127, 11);
    u8g2.setCursor(5, 22);
    u8g2.print("RESET ALL");
    u8g2.setCursor(5, 32);
    u8g2.print("RESET WIFI");
    u8g2.setCursor(5, 42);
    u8g2.print("EXIT");

    u8g2.setFont(u8g2_font_waffle_t_all);
    if (count == 0)
      u8g2.drawUTF8(60, 22, "\ue14e");
    else if (count == 1)
      u8g2.drawUTF8(68, 32, "\ue14e");
    else if (count == 2)
      u8g2.drawUTF8(30, 42, "\ue14e");

    u8g2.sendBuffer();
    if (analogRead(NEXT_BUTTON) > 1000)
    {
      count++;
      delay(100);
      if (count > 2)
        count = 0;
    }

    if (analogRead(SELECT_BUTTON) > 1000)
    {
      delay(100);
      break;
    }
    if (updateInProgress)
      return;
  }
  if (count == 0)
  {
    count = editBoolSetting("RESET: ALL");
    if (!count)
    {
      pref.putInt("lcd_bright", 250);
      pref.putBool("autoBright", true);
      pref.putBool("hourlyAlarm", true);
      pref.putBool("halfHourlyAlarm", true);
      pref.putBool("useWifi", false);
      pref.putBool("muteDark", true);
      pref.putBool("offInDark", true);
      pref.putString("ssid", "");
      pref.putString("password", "");
      pref.putInt("buzzVol", 250);
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
  else if (count == 1)
  {
    count = editBoolSetting("RESET: WIFI");
    if (!count)
    {
      pref.putString("ssid", "");
      pref.putString("password", "");
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
  else if (count == 2)
  {
    pref.end();
    return;
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
 * @brief Generate simple beeps using the buzzer
 * @param numBeeps Number of beeps to generate
 * @param duration_ms Duration of each beep in milliseconds
 * @param maxVolume Maximum volume level (0-255)
 */
void simpleBeep(int numBeeps, int duration_ms, byte maxVolume)
{
  int pauseTime = 150; // pause between double beeps

  if (maxVolume > 255)
    maxVolume = 255;

  for (int b = 0; b < numBeeps; b++)
  {
    // Turn on buzzer at maxVolume
    ledcWriteTone(BUZZER_PIN, PWM_FREQ);
    ledcWrite(BUZZER_PIN, maxVolume);

    // Hold for the specified duration
    delay(duration_ms);

    // Turn off buzzer
    ledcWriteTone(BUZZER_PIN, 0);
    ledcWrite(BUZZER_PIN, 0);

    // Pause between beeps
    if (b < numBeeps - 1)
      delay(pauseTime);
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
  u8g2.print("Connect to:");
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
    u8g2.print("http://192.168.4.1");
    u8g2.setCursor(1, 34);
    u8g2.print("Enter your Wifi");
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
