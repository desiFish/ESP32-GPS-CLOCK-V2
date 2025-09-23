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
#define SWVersion "2.0.0"

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

// Preference library object or instance
Preferences pref;
// Light Sensor Object
BH1750 lightMeter;

float ahtValue; // to store Temp result

AHTxx aht20(AHTXX_ADDRESS_X38, AHT2x_SENSOR); // sensor address, sensor type

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
String week[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
String monthChar[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// variables for holding time data globally
byte days = 0, months = 0, hours = 0, minutes = 0, seconds = 0;
int years = 0;

// LUX (BH1750) update frequency
unsigned long lastTime1 = 0;
int timerDelay1 = 2000; // Light Sensor update delay

// AHT25 update frequency
unsigned long lastTime2 = 0;
const long timerDelay2 = 12000; // temperature sensor update delay

bool isDark; // Tracks ambient light state
float ahtTemp = 0.0;
int ahtHum = 0;

// features config, saved in preference library
int LCD_BRIGHTNESS, buzzVol;
bool autoBright, hourlyAlarm, halfHourlyAlarm, useWifi, muteDark, offInDark, hour12Mode;

// your wifi name and password (saved in preference library)
String ssid;
String password;

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
  // <Add your own code here>
  updateInProgress = false;
  delay(1000);
}

time_t prevDisplay = 0; // when the digital clock was displayed

// for creating task attached to CORE 0 of CPU
TaskHandle_t loop1Task;
byte currentBrightness = 250; // Track current brightness level

/**
 * @brief Generic menu for editing boolean settings
 * @param displayText The text to display in the menu
 * @param prefKey The Preferences key to store the value
 * @param var Reference to the variable to update
 */
void editBoolSetting(const char *displayText, const char *prefKey, bool &var)
{
  pref.begin("database", false);
  int count = var ? 0 : 1;
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
      pref.putBool(prefKey, count == 0);
      var = pref.getBool(prefKey, false);
      pref.end();
      break;
    }
  }
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

  // --- LittleFS Init ---
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

  pinMode(LCD_LIGHT, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  analogWrite(LCD_LIGHT, 150);

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
  analogWrite(BUZZER_PIN, buzzVol);
  delay(20);
  analogWrite(BUZZER_PIN, 0);

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

  if (aht20.begin() != true)
  {
    errorMsgPrint("AHT25", "CANNOT FIND");
  }
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
  ahtTemp = aht20.readTemperature(); // NEED TO CHANGE THE SENSOR, it shows +3 degrees extra
  ahtHum = aht20.readHumidity();
  xTaskCreatePinnedToCore(
      loop1,                              /* Task function. */
      "loop1Task",                        /* name of task. */
      10000,                              /* Stack size of task */
      NULL,                               /* parameter of the task */
      1,                                  /* priority of the task */
      &loop1Task,                         /* Task handle to keep track of created task */
      0);                                 /* pin task to core 0 */
  analogWrite(LCD_LIGHT, LCD_BRIGHTNESS); // set brightness
  delay(100);
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
    if ((millis() - lastTime1) > timerDelay1)
    {
      timerDelay1 = 4000;
      Serial.println("[DEBUG] loop1: Reading light sensor...");
      lightMeter.configure(BH1750::ONE_TIME_HIGH_RES_MODE);
      float lux;
      while (!lightMeter.measurementReady(true))
      {
        yield();
      }
      lux = lightMeter.readLightLevel();

      Serial.print("[DEBUG] loop1: LUXRaw = ");
      Serial.println(lux);

      isDark = lux <= 1; // Consider it dark if LUX is 1 or below

      if (autoBright)
      {
        Serial.print("[DEBUG] loop1: autoBright enabled, isDark=");
        Serial.print(isDark);
        Serial.print(", offInDark=");
        Serial.println(offInDark);
        if (isDark && offInDark)
        {
          currentBrightness = 0; // Set to minimum brightness in dark if offInDark is enabled
        }
        else
        {
          byte targetBrightness;
          byte val1 = constrain(lux, 1, 120);
          targetBrightness = map(val1, 1, 120, 10, 255);
          if (currentBrightness != targetBrightness)
          {
            timerDelay1 = 200; // Faster updates during transition
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
            Serial.print("[DEBUG] loop1: Adjusting brightness, current=");
            Serial.print(currentBrightness);
            Serial.print(", target=");
            Serial.println(targetBrightness);
          }
        }
        analogWrite(LCD_LIGHT, currentBrightness);
      }

      lastTime1 = millis();
    }

    if ((millis() - lastTime2) > timerDelay2)
    {
      if (!(isDark && offInDark)) // Only read sensor if not in dark mode with offInDark enabled
      {
        Serial.println("[DEBUG] loop1: Reading temperature/humidity sensor...");
        ahtTemp = aht20.readTemperature();
        ahtHum = aht20.readHumidity();
        Serial.print("[DEBUG] loop1: Temp=");
        Serial.print(ahtTemp);
        Serial.print(", Hum=");
        Serial.println(ahtHum);
      }
      lastTime2 = millis();
    }

    if (!(isDark && muteDark) && years > 2024 && seconds == 0 && (hourlyAlarm || halfHourlyAlarm))
    {
      if (minutes == 0 && hourlyAlarm)
      {
        Serial.print("[DEBUG] loop1: Hourly alarm triggered at ");
        Serial.print(hours);
        Serial.print(":");
        Serial.println(minutes);
        buzzer(600, 1);
      }
      else if (minutes == 30 && halfHourlyAlarm)
      {
        Serial.print("[DEBUG] loop1: Half-hourly alarm triggered at ");
        Serial.print(hours);
        Serial.print(":");
        Serial.println(minutes);
        buzzer(400, 2);
      }
    }
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
      u8g2.print(String(ahtTemp, 1)); // Show temperature with 1 decimal place
      u8g2.setFont(u8g2_font_threepix_tr);
      u8g2.setCursor(28, 8);
      u8g2.print("o");
      u8g2.setFont(u8g2_font_t0_11_tf);
      u8g2.setCursor(32, 13);
      u8g2.print("C ");
      String temp = String(1008) + "hPa";               // Ensure font is set
      int stringWidth = u8g2.getStrWidth(temp.c_str()); // Get exact pixel width
      u8g2.setCursor((128 - stringWidth) / 2, 13);      // Center using screen width
      u8g2.print(temp);
      if (ahtHum > 99)
        u8g2.setCursor(90, 13);
      else
        u8g2.setCursor(96, 13);
      u8g2.print(ahtHum);
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
      Serial.println("[DEBUG] loop: NEXT_BUTTON pressed, entering menu");
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
    u8g2.print("DISPLAY: BRIGHTNESS");
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
        if (autoBright == true)
          count = 0;
        else
          count = 1;

        editBoolSetting("AUTO BRIGHTNESS", "autoBright", autoBright);
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
          if (offInDark == true)
            count = 0;
          else
            count = 1;
          editBoolSetting("OFF IN DARK", "offInDark", offInDark);
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
  Serial.println("[DEBUG] buzzer: Beeping " + String(count) + " times with volume " + String(buzzVol));
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
    u8g2.print("192.168.4.1");
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
