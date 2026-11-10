/**
 * @file esp32-digital-clock.ino
 * @brief ESP32 Smart Clock with Dual-WiFi Multi support, mDNS (.local domain),
 *        TM1637 Display, DS3231 RTC, NTP Sync, Fixed IST Timezone, OTA Updates, and Credits.
 * @author Baljit Panda
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiMulti.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <TM1637Display.h>
#include <RTClib.h>

// --- PIN DEFINITIONS ---
#define CLK_PIN 18
#define DIO_PIN 19
#define STATUS_LED 2 // Onboard blue LED at GPIO 2

// --- WIFI MULTI SETTINGS ---
WiFiMulti wifiMulti;

// Locked Timezone Settings (IST / UTC +5:30)
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;
const String currentTzName = "IST (UTC +5:30)";

// --- OBJECT INITIALIZATION ---
TM1637Display display(CLK_PIN, DIO_PIN);
RTC_DS3231 rtc;
WebServer server(80);

// --- GLOBAL VARIABLES ---
bool rtcAvailable = false;
unsigned long lastDisplayUpdate = 0;
unsigned long lastBlinkToggle = 0;
bool colonState = true;
bool statusLedState = false;

// Display Preferences
bool is24HourFormat = true;
bool autoBrightness = true;
uint8_t manualBrightness = 7;

/**
 * @brief Updates the TM1637 4-digit display using NTP or RTC time source.
 */
void updateDisplayTime()
{
  int hours = 0;
  int minutes = 0;
  bool timeValid = false;

  struct tm timeinfo;
  if (WiFi.status() == WL_CONNECTED && getLocalTime(&timeinfo, 10))
  {
    if (timeinfo.tm_year > 70)
    {
      hours = timeinfo.tm_hour;
      minutes = timeinfo.tm_min;
      timeValid = true;
    }
  }

  if (!timeValid && rtcAvailable)
  {
    DateTime now = rtc.now();
    hours = now.hour();
    minutes = now.minute();
    timeValid = true;
  }

  if (timeValid)
  {
    int currentMinutes = (hours * 60) + minutes;

    // Auto-dimming schedule (Night mode between 9:30 PM and 6:30 AM)
    if (autoBrightness)
    {
      if (currentMinutes >= 1290 || currentMinutes < 390)
      {
        display.setBrightness(0x01, true);
      }
      else
      {
        display.setBrightness(0x0f, true);
      }
    }
    else
    {
      display.setBrightness(manualBrightness, true);
    }

    int displayHours = hours;
    if (!is24HourFormat)
    {
      displayHours = hours % 12;
      if (displayHours == 0)
        displayHours = 12;
    }

    int displayValue = (displayHours * 100) + minutes;
    uint8_t dots = colonState ? 0b11000000 : 0b00000000;
    display.showNumberDecEx(displayValue, dots, true);
  }
  else
  {
    display.clear();
  }
}

/**
 * @brief Reads internal ESP32 chip temperature.
 */
float getChipTemperature()
{
  return temperatureRead();
}

/**
 * @brief API Endpoint to provide real-time telemetry JSON to the web frontend.
 */
void handleApiData()
{
  float chipTemp = getChipTemperature();
  uint32_t freeRam = ESP.getFreeHeap() / 1024;

  String connectedSSID = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "Disconnected";
  String gatewayIP = (WiFi.status() == WL_CONNECTED) ? WiFi.gatewayIP().toString() : "0.0.0.0";
  String networkInfo = connectedSSID + " (" + gatewayIP + ")";

  uint32_t totalSecs = millis() / 1000;
  uint32_t upHours = totalSecs / 3600;
  uint32_t upMins = (totalSecs % 3600) / 60;
  uint32_t upSecs = totalSecs % 60;
  String uptimeFormatted = String(upHours) + "h " + String(upMins) + "m " + String(upSecs) + "s";

  int curHour = 0;
  int curMin = 0;
  int curSec = 0;
  if (rtcAvailable)
  {
    DateTime now = rtc.now();
    curHour = now.hour();
    curMin = now.minute();
    curSec = now.second();
  }

  String json = "{";
  json += "\"temp\":\"" + String(chipTemp, 1) + "\",";
  json += "\"ram\":\"" + String(freeRam) + "\",";
  json += "\"network\":\"" + networkInfo + "\",";
  json += "\"uptime\":\"" + uptimeFormatted + "\",";
  json += "\"tz\":\"" + currentTzName + "\",";
  json += "\"hour\":" + String(curHour) + ",";
  json += "\"minute\":" + String(curMin) + ",";
  json += "\"second\":" + String(curSec);
  json += "}";

  server.send(200, "application/json", json);
}

/**
 * @brief Serves the web dashboard.
 */
void handleRoot()
{
  DateTime now;
  if (rtcAvailable)
  {
    now = rtc.now();
  }

  float chipTemp = getChipTemperature();
  uint32_t freeRam = ESP.getFreeHeap() / 1024;

  String connectedSSID = (WiFi.status() == WL_CONNECTED) ? WiFi.SSID() : "Disconnected";
  String gatewayIP = (WiFi.status() == WL_CONNECTED) ? WiFi.gatewayIP().toString() : "0.0.0.0";
  String networkInfo = connectedSSID + " (" + gatewayIP + ")";

  uint32_t totalSecs = millis() / 1000;
  uint32_t upHours = totalSecs / 3600;
  uint32_t upMins = (totalSecs % 3600) / 60;
  uint32_t upSecs = totalSecs % 60;
  String uptimeFormatted = String(upHours) + "h " + String(upMins) + "m " + String(upSecs) + "s";

  String html = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>ESP32 Smart Clock Dashboard</title><style>";
  html += ":root { --bg-color: #0b0f19; --text-color: #f8fafc; --card-bg: #131b2e; --card-border: #1e293b; --title-color: #38bdf8; --accent-color: #6366f1; --accent-hover: #4f46e5; --sub-color: #94a3b8; --stat-bg: #080c14; --input-bg: #0b0f19; --success: #10b981; }";
  html += "body.light-mode { --bg-color: #f1f5f9; --text-color: #0f172a; --card-bg: #ffffff; --card-border: #cbd5e1; --title-color: #0284c7; --accent-color: #4f46e5; --accent-hover: #4338ca; --sub-color: #64748b; --stat-bg: #f8fafc; --input-bg: #ffffff; --success: #059669; }";
  html += "body { font-family: system-ui, -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background-color: var(--bg-color); color: var(--text-color); margin: 0; padding: 20px; display: flex; justify-content: center; line-height: 1.5; }";
  html += ".container { width: 100%; max-width: 600px; box-sizing: border-box; }";
  html += ".header-flex { display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px; }";
  html += "h1 { color: var(--title-color); margin: 0; font-size: 22px; display: flex; align-items: center; gap: 8px; }";
  html += ".subtitle { color: var(--sub-color); margin-bottom: 24px; font-size: 13px; display: flex; align-items: center; justify-content: space-between; }";
  html += ".badge { background: rgba(16, 185, 129, 0.15); color: var(--success); padding: 4px 10px; border-radius: 12px; font-size: 11px; font-weight: 600; border: 1px solid var(--success); }";
  html += ".theme-toggle { background: var(--card-border); color: var(--text-color); border: 1px solid var(--accent-color); padding: 6px 12px; border-radius: 16px; cursor: pointer; font-size: 12px; font-weight: 600; transition: all 0.2s; }";
  html += ".card { background: var(--card-bg); border: 1px solid var(--card-border); border-radius: 16px; padding: 20px; margin-bottom: 18px; box-shadow: 0 4px 6px -1px rgba(0, 0, 0, 0.1); }";
  html += ".card h3 { margin-top: 0; color: var(--text-color); border-bottom: 1px solid var(--card-border); padding-bottom: 10px; font-size: 15px; display: flex; align-items: center; gap: 8px; }";
  html += ".grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px, 1fr)); gap: 12px; }";
  html += ".stat-item { background: var(--stat-bg); padding: 12px 14px; border-radius: 12px; border: 1px solid var(--card-border); }";
  html += ".stat-label { font-size: 11px; color: var(--sub-color); text-transform: uppercase; font-weight: 600; letter-spacing: 0.5px; }";
  html += ".stat-value { font-size: 14px; font-weight: 700; color: var(--title-color); margin-top: 6px; word-break: break-all; }";
  html += ".btn { background: var(--accent-color); color: #fff; border: none; padding: 11px 16px; border-radius: 10px; cursor: pointer; width: 100%; font-weight: 600; margin-top: 12px; font-size: 13px; transition: background 0.2s; }";
  html += ".btn:hover { background: var(--accent-hover); }";
  html += ".btn-alt { background: var(--card-border); color: var(--text-color); border: 1px solid var(--card-border); }";
  html += ".btn-alt:hover { background: var(--accent-color); color: #fff; }";
  html += ".btn-danger { background: #ef4444; }";
  html += ".btn-danger:hover { background: #dc2626; }";
  html += ".btn-ota { background: #8b5cf6; margin-top: 12px; display: block; text-align: center; text-decoration: none; box-sizing: border-box; color: #fff; padding: 11px 16px; border-radius: 10px; font-weight: 600; font-size: 13px; }";
  html += ".btn-ota:hover { background: #7c3aed; }";
  html += "label { font-size: 12px; color: var(--text-color); font-weight: 600; display: block; margin-top: 10px; }";
  html += "input, select { background: var(--input-bg); border: 1px solid var(--card-border); color: var(--text-color); padding: 10px 14px; border-radius: 10px; width: 100%; box-sizing: border-box; margin-top: 6px; margin-bottom: 10px; font-size: 13px; outline: none; }";
  html += "input:focus, select:focus { border-color: var(--accent-color); }";
  html += ".divider { border: 0; height: 1px; background: var(--card-border); margin: 30px 0; }";
  html += ".doc-section { background: var(--card-bg); border: 1px solid var(--card-border); border-radius: 16px; padding: 22px; margin-bottom: 18px; }";
  html += ".doc-section h2 { font-size: 16px; color: var(--title-color); margin-top: 0; display: flex; align-items: center; gap: 8px; border-bottom: 1px solid var(--card-border); padding-bottom: 10px; }";
  html += ".doc-section p, .doc-section li { font-size: 13px; color: var(--sub-color); margin-bottom: 10px; }";
  html += ".doc-section ul { padding-left: 20px; margin: 8px 0; }";
  html += ".footer-credit { text-align: center; color: var(--sub-color); font-size: 12px; margin-top: 25px; margin-bottom: 10px; }";
  html += "</style><script>";
  html += "let serverHour = " + String(now.hour()) + ";";
  html += "let serverMinute = " + String(now.minute()) + ";";
  html += "let serverSecond = " + String(now.second()) + ";";
  html += "let currentTz = '" + currentTzName + "';";
  html += "let localMillisOffset = Date.now();";

  html += "function toggleTheme() { document.body.classList.toggle('light-mode'); const isLight = document.body.classList.contains('light-mode'); localStorage.setItem('theme', isLight ? 'light' : 'dark'); document.getElementById('theme-btn').textContent = isLight ? '🌙 Dark Mode' : '☀️ Light Mode'; }";
  html += "function loadTheme() { if (localStorage.getItem('theme') === 'light') { document.body.classList.add('light-mode'); document.getElementById('theme-btn').textContent = '🌙 Dark Mode'; } }";

  html += "function updateStats() {";
  html += "  fetch('/api/data').then(response => response.json()).then(data => {";
  html += "    document.getElementById('val-temp').innerHTML = data.temp + '&deg;C';";
  html += "    document.getElementById('val-ram').innerHTML = data.ram + ' KB';";
  html += "    document.getElementById('val-network').innerText = data.network;";
  html += "    document.getElementById('val-uptime').innerText = data.uptime;";
  html += "    serverHour = data.hour;";
  html += "    serverMinute = data.minute;";
  html += "    serverSecond = data.second;";
  html += "    currentTz = data.tz;";
  html += "    localMillisOffset = Date.now();";
  html += "  }).catch(err => console.log('Telemetry error', err));";
  html += "}";

  html += "function tickClock() {";
  html += "  let elapsedSecs = Math.floor((Date.now() - localMillisOffset) / 1000);";
  html += "  let totalSecs = serverHour * 3600 + serverMinute * 60 + serverSecond + elapsedSecs;";
  html += "  let curH = Math.floor(totalSecs / 3600) % 24;";
  html += "  let curM = Math.floor((totalSecs % 3600) / 60);";
  html += "  let curS = totalSecs % 60;";
  html += "  let timeStr = String(curH).padStart(2,'0') + ':' + String(curM).padStart(2,'0') + ':' + String(curS).padStart(2,'0') + ' | ' + currentTz;";
  html += "  document.getElementById('val-livetime').innerText = timeStr;";

  html += "  let hInput = document.getElementById('hourInput');";
  html += "  let mInput = document.getElementById('minInput');";
  html += "  if (document.activeElement !== hInput) hInput.value = curH;";
  html += "  if (document.activeElement !== mInput) mInput.value = curM;";
  html += "}";

  html += "function submitManualTime(event) {";
  html += "  event.preventDefault();";
  html += "  let elapsedSecs = Math.floor((Date.now() - localMillisOffset) / 1000);";
  html += "  let totalSecs = serverHour * 3600 + serverMinute * 60 + serverSecond + elapsedSecs;";
  html += "  let currentSecsBackground = totalSecs % 60;";
  html += "  let h = document.getElementById('hourInput').value;";
  html += "  let m = document.getElementById('minInput').value;";
  html += "  window.location.href = '/set-time?hour=' + h + '&minute=' + m + '&second=' + currentSecsBackground;";
  html += "}";

  html += "function rebootDevice() {";
  html += "  if(confirm('Are you sure you want to restart the ESP32 clock?')) {";
  html += "    fetch('/restart').then(() => { alert('Device is restarting...'); setTimeout(() => location.reload(), 4000); });";
  html += "  }";
  html += "}";

  html += "setInterval(updateStats, 5000);";
  html += "setInterval(tickClock, 1000);";
  html += "</script></head><body onload=\"loadTheme()\"><div class=\"container\">";

  html += "<div class=\"header-flex\"><h1>🕒 ESP32 Smart Clock</h1><button id=\"theme-btn\" class=\"theme-toggle\" onclick=\"toggleTheme()\">☀️ Light Mode</button></div>";
  html += "<div class=\"subtitle\"><span>System Control Panel (<a href=\"http://esp32-clock.local\" style=\"color:var(--title-color); text-decoration:none;\">esp32-clock.local</a>)</span><span class=\"badge\">● Online</span></div>";

  html += "<div class=\"card\"><h3>📊 Real-Time Telemetry & Status</h3><div class=\"grid\">";
  html += "<div class=\"stat-item\" style=\"grid-column: 1 / -1;\"><div class=\"stat-label\">Current Time</div><div id=\"val-livetime\" class=\"stat-value\">--:--:-- | " + currentTzName + "</div></div>";
  html += "<div class=\"stat-item\"><div class=\"stat-label\">🌡️ Chip Temp</div><div id=\"val-temp\" class=\"stat-value\">" + String(chipTemp, 1) + "&deg;C</div></div>";
  html += "<div class=\"stat-item\"><div class=\"stat-label\">💾 Free RAM</div><div id=\"val-ram\" class=\"stat-value\">" + String(freeRam) + " KB</div></div>";
  html += "<div class=\"stat-item\" style=\"grid-column: 1 / -1;\"><div class=\"stat-label\">🛜 Network Connection</div><div id=\"val-network\" class=\"stat-value\">" + networkInfo + "</div></div>";
  html += "<div class=\"stat-item\" style=\"grid-column: 1 / -1;\"><div class=\"stat-label\">⏱️ System Uptime</div><div id=\"val-uptime\" class=\"stat-value\">" + uptimeFormatted + "</div></div>";
  html += "</div></div>";

  html += "<div class=\"card\"><h3>🖥️ Display Preferences</h3><form action=\"/set-display\" method=\"GET\">";
  html += "<label>Time Format Style:</label><select name=\"format\">";
  html += "<option value=\"24\" " + String(is24HourFormat ? "selected" : "") + ">24-Hour Format (21:30)</option>";
  html += "<option value=\"12\" " + String(!is24HourFormat ? "selected" : "") + ">12-Hour Format (09:30)</option>";
  html += "</select><label>Brightness Control Mode:</label>";
  html += "<select name=\"brightMode\" onchange=\"document.getElementById('manualSlider').style.display = (this.value=='manual')?'block':'none';\">";
  html += "<option value=\"auto\" " + String(autoBrightness ? "selected" : "") + ">Auto Schedule (Night Dimming)</option>";
  html += "<option value=\"manual\" " + String(!autoBrightness ? "selected" : "") + ">Manual Fixed Level</option>";
  html += "</select>";
  html += "<div id=\"manualSlider\" style=\"display:" + String(!autoBrightness ? "block" : "none") + ";\">";
  html += "<label>Brightness Level (0 to 7):</label><input type=\"number\" name=\"brightLevel\" min=\"0\" max=\"7\" value=\"" + String(manualBrightness) + "\"></div>";
  html += "<button type=\"submit\" class=\"btn\">💾 Save Display Settings</button></form></div>";

  html += "<div class=\"card\"><h3>⚙️ System & Time Management</h3><form onsubmit=\"submitManualTime(event)\"><div class=\"grid\">";
  html += "<div><label>Hour (0-23):</label><input type=\"number\" id=\"hourInput\" min=\"0\" max=\"23\" value=\"" + String(now.hour()) + "\"></div>";
  html += "<div><label>Minute (0-59):</label><input type=\"number\" id=\"minInput\" min=\"0\" max=\"59\" value=\"" + String(now.minute()) + "\"></div>";
  html += "</div><button type=\"submit\" class=\"btn btn-alt\">⏱️ Set RTC Time Manually</button></form>";
  html += "<form action=\"/sync-ntp\" method=\"GET\"><button type=\"submit\" class=\"btn\" style=\"background:#0284c7; margin-top: 10px;\">🛜 Force NTP Auto-Sync</button></form>";
  html += "<button type=\"button\" class=\"btn btn-danger\" onclick=\"rebootDevice()\" style=\"margin-top: 10px;\">🔄 Restart ESP32 Hardware</button>";
  html += "<a href=\"/update\" class=\"btn-ota\">🚀 Firmware Over-The-Air (OTA) Update</a></div>";

  html += "<hr class=\"divider\">";

  // --- DOCUMENTATION SECTIONS ---
  html += "<div class=\"doc-section\"><h2>💡 How the Code Works</h2>";
  html += "<p>This firmware runs an embedded Web Server on your ESP32 while simultaneously maintaining time precision across multiple layers:</p>";
  html += "<ul>";
  html += "<li><strong>WiFiMulti & mDNS:</strong> Connects automatically to your pre-configured Wi-Fi networks and broadcasts an mDNS responder so you can visit <code>http://esp32-clock.local</code> directly.</li>";
  html += "<li><strong>DS3231 RTC & NTP Sync:</strong> Pulls high-accuracy atomic time via NTP servers upon connection and backs it up locally using a hardware Real-Time Clock module over I2C.</li>";
  html += "<li><strong>Background Ticker Script:</strong> The web dashboard features a client-side JavaScript ticker that calculates second offsets locally so time updates instantly without lagging or page refreshing.</li>";
  html += "</ul></div>";

  html += "<div class=\"doc-section\"><h2>📱 How to Use the Web Page</h2>";
  html += "<p>Once connected, you can access this control panel from any phone or computer on the same Wi-Fi network:</p>";
  html += "<ul>";
  html += "<li><strong>Telemetry & Status:</strong> Monitors live hardware temperature, memory allocation, network signal, and device uptime in real-time.</li>";
  html += "<li><strong>Display Preferences:</strong> Switch between 12/24 hour styles or toggle automatic night dimming.</li>";
  html += "<li><strong>Manual Time Entry:</strong> Input fields auto-update every minute, and clicking save automatically preserves your background second offsets to prevent drift.</li>";
  html += "</ul></div>";

  html += "<div class=\"doc-section\"><h2>🚀 How to Do OTA Updates (.ino to .bin)</h2>";
  html += "<p>Because microcontrollers cannot compile source code directly, follow these steps to update firmware wirelessly:</p>";
  html += "<ul>";
  html += "<li><strong>Step 1:</strong> Open your sketch code inside the Arduino IDE on your computer.</li>";
  html += "<li><strong>Step 2:</strong> Go to top menu bar and select <strong>Sketch > Export Compiled Binary</strong>. This compiles your <code>.ino</code> file into a machine-readable <code>.bin</code> file inside your project folder.</li>";
  html += "<li><strong>Step 3:</strong> Click the <strong>Firmware Over-The-Air (OTA) Update</strong> button on this dashboard, upload the generated <code>.bin</code> file, and click Flash!</li>";
  html += "</ul></div>";

  html += "<div class=\"footer-credit\">Designed & Built by <strong>Baljit Panda</strong></div>";
  html += "</div></body></html>";

  server.send(200, "text/html; charset=UTF-8", htmlContent);
}

/**
 * @brief Sets up HTTP server routes and endpoints.
 */
void setupWebServer()
{
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/data", HTTP_GET, handleApiData);

  server.on("/restart", HTTP_GET, []()
            {
    server.send(200, "text/plain", "Rebooting...");
    delay(500);
    ESP.restart(); });

  server.on("/set-display", HTTP_GET, []()
            {
    if (server.hasArg("format")) is24HourFormat = (server.arg("format") == "24");
    if (server.hasArg("brightMode")) autoBrightness = (server.arg("brightMode") == "auto");
    if (server.hasArg("brightLevel")) manualBrightness = server.arg("brightLevel").toInt();
    updateDisplayTime();
    server.sendHeader("Location", "/");
    server.send(303); });

  server.on("/set-time", HTTP_GET, []()
            {
    if (server.hasArg("hour") && server.hasArg("minute")) {
      int h = server.arg("hour").toInt();
      int m = server.arg("minute").toInt();
      int s = server.hasArg("second") ? server.arg("second").toInt() : 0;
      if (rtcAvailable) {
        DateTime now = rtc.now();
        rtc.adjust(DateTime(now.year(), now.month(), now.day(), h, m, s));
        updateDisplayTime();
      }
    }
    server.sendHeader("Location", "/");
    server.send(303); });

  server.on("/sync-ntp", HTTP_GET, []()
            {
    if (WiFi.status() == WL_CONNECTED) {
      configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");
      struct tm timeinfo;
      if (getLocalTime(&timeinfo, 3000) && rtcAvailable) {
        rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, 
                            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      }
      updateDisplayTime();
    }
    server.sendHeader("Location", "/");
    server.send(303); });

  // OTA Firmware Update interface page
  server.on("/update", HTTP_GET, []()
            {
    String html = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    html += "<title>OTA Update</title><style>";
    html += "body{font-family:system-ui,sans-serif;background:#0b0f19;color:#f8fafc;padding:30px;text-align:center;}";
    html += ".card{background:#131b2e;border:1px solid #1e293b;border-radius:16px;padding:30px;max-width:400px;margin:40px auto;}";
    html += "input[type=file]{margin:20px 0;color:#f8fafc;}";
    html += ".btn{background:#8b5cf6;color:#fff;border:none;padding:11px 16px;border-radius:10px;cursor:pointer;width:100%;font-weight:600;}";
    html += "</style></head><body><div class=\"card\"><h2>🚀 Wireless Firmware Update</h2>";
    html += "<form method=\"POST\" action=\"/update\" enctype=\"multipart/form-data\">";
    html += "<input type=\"file\" name=\"update\" accept=\".bin\"><br>";
    html += "<input type=\"submit\" value=\"Upload & Flash Firmware\" class=\"btn\"></form></div></body></html>";
    server.send(200, "text/html", html); });

  // OTA POST execution handler
  server.on("/update", HTTP_POST, []()
            {
    String responsePage = "<!DOCTYPE html><html><head><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><style>";
    responsePage += "body{font-family:system-ui,sans-serif;background:#0b0f19;color:#f8fafc;padding:40px;text-align:center;}";
    responsePage += ".card{background:#131b2e;border:1px solid #1e293b;border-radius:16px;padding:30px;max-width:400px;margin:40px auto;}";
    responsePage += "h2{color:#10b981;}</style></head><body><div class=\"card\"><h2>Success!</h2>";
    responsePage += "<p>Firmware updated cleanly. Restarting device...</p></div></body></html>";
    
    server.send(200, "text/html", Update.hasError() ? "Update Failed!" : responsePage);
    delay(1000);
    ESP.restart(); }, []()
            {
    HTTPUpload& upload = server.upload();
    
    if (upload.status == UPLOAD_FILE_START) {
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { /* Error handling */ }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      Update.write(upload.buf, upload.currentSize);
    } else if (upload.status == UPLOAD_FILE_END) {
      Update.end(true);
    } });

  server.begin();
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  // Initialize hardware pins
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  display.setBrightness(0x0f, false);
  display.clear();

  // Initialize I2C for RTC
  Wire.begin(21, 22);
  Wire.setTimeOut(50);

  if (rtc.begin())
  {
    rtcAvailable = true;
    updateDisplayTime();
  }

  // --- ADD THE WIFI NETWORKS ---
  wifiMulti.addAP("WIFI_NAME_1", "WIFI_PASSWORD_1"); // Change the text in the brackets, and add, in the same format a new line to add another Wi-Fi network for as many as needed
  wifiMulti.addAP("WIFI_NAME_2", "WIFI_PASSWORD_2");
  WiFi.mode(WIFI_STA);

  // Try connecting to whichever AP is in range
  int retries = 0;
  while (wifiMulti.run() != WL_CONNECTED && retries < 20)
  {
    delay(500);
    retries++;
  }

  setupWebServer();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi Connected Successfully!");
    Serial.print("IP Address: http://");
    Serial.println(WiFi.localIP());

    // --- START mDNS RESPONDER ---
    if (!MDNS.begin("esp32-clock"))
    {
      Serial.println("Error setting up MDNS responder!");
    }
    else
    {
      Serial.println("mDNS responder started: http://esp32-clock.local");
    }

    configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org", "time.nist.gov");

    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 3000) && rtcAvailable)
    {
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                          timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
    }
  }
  else
  {
    Serial.println("");
    Serial.println("Failed to connect to Wi-Fi. Operating offline via RTC.");
  }
}

void loop()
{
  server.handleClient();

  // --- STATUS LED BLINK (GPIO 2) - Alternates every 1 second ---
  unsigned long currentMillis = millis();
  if (currentMillis - lastBlinkToggle >= 1000)
  {
    lastBlinkToggle = currentMillis;
    statusLedState = !statusLedState;
    digitalWrite(STATUS_LED, statusLedState ? HIGH : LOW);
  }

  // --- WIFI MULTI RECONNECTION MANAGEMENT ---
  if (WiFi.status() != WL_CONNECTED)
  {
    wifiMulti.run();
  }

  // --- CLOCK DISPLAY REFRESH ---
  if (millis() - lastDisplayUpdate >= 1000)
  {
    lastDisplayUpdate = millis();
    colonState = !colonState;
    updateDisplayTime();
  }

  delay(10);
}
