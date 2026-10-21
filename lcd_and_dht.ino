#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <DHT.h>

// ========================================
// WIFI SETTINGS
// ========================================

const char* ssid = "home203";
const char* password = "ekku2201";

// ========================================
// DHT22 SETTINGS
// ========================================

#define DHTPIN 27
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

// ========================================
// LCD SETTINGS
// ========================================

#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 4

#define TFT_MOSI 23
#define TFT_SCLK 18

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);

// ========================================
// WEB SERVER
// ========================================

WebServer server(80);

// ========================================
// SENSOR VARIABLES
// ========================================

float temperatureC = NAN;
float humidity = NAN;

float minTemp = NAN;
float maxTemp = NAN;

float minHumidity = NAN;
float maxHumidity = NAN;

unsigned long lastSensorRead = 0;
unsigned long sensorErrors = 0;

bool sensorValid = false;


// ========================================
// PHASE 4: TEMPERATURE & HUMIDITY HISTORY
// ========================================

// Store 24 hours of readings (1 per minute)
const int HISTORY_SIZE = 1440;

// Arrays to store sensor readings
float tempHistory[HISTORY_SIZE];
float humidityHistory[HISTORY_SIZE];

// History tracking
int historyCount = 0;
int historyNext = 0;

// Record one reading every 60 seconds
unsigned long lastHistorySample = 0;
const unsigned long HISTORY_INTERVAL = 60000;

// Save temperature and humidity
void recordHistory() {

  if (!sensorValid) {
    return;
  }

  tempHistory[historyNext] = temperatureC;
  humidityHistory[historyNext] = humidity;

  historyNext = (historyNext + 1) % HISTORY_SIZE;

  if (historyCount < HISTORY_SIZE) {
    historyCount++;
  }
}


// ========================================
// READ SENSOR
// ========================================

void readSensor() {

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  if (isnan(t) || isnan(h)) {
    sensorErrors++;
    sensorValid = false;
    Serial.println("DHT22 Read Error");
    return;
  }

  temperatureC = t;
  humidity = h;
  sensorValid = true;

  if (isnan(minTemp) || t < minTemp) minTemp = t;
  if (isnan(maxTemp) || t > maxTemp) maxTemp = t;

  if (isnan(minHumidity) || h < minHumidity) {
    minHumidity = h;
  }

  if (isnan(maxHumidity) || h > maxHumidity) {
    maxHumidity = h;
  }

  Serial.printf(
    "Temperature: %.1f C | Humidity: %.1f%%\n",
    temperatureC,
    humidity);
}

// ========================================
// UPDATE LCD
// ========================================


void drawLCDLayout() {
  // Draw the static layout only once
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  // Header
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(35, 8);
  tft.println("SMART HOME");

  tft.drawFastHLine(10, 34, 220, ST77XX_WHITE);

  // Temperature label
  tft.setTextColor(ST77XX_YELLOW);
  tft.setTextSize(2);
  tft.setCursor(10, 43);
  tft.println("TEMPERATURE");

  // Humidity label
  tft.setTextColor(ST77XX_CYAN);
  tft.setTextSize(2);
  tft.setCursor(10, 126);
  tft.println("HUMIDITY");

  // Network section separator
  tft.drawFastHLine(10, 183, 220, ST77XX_WHITE);
}

void updateLCD() {
  static float lastTempC = NAN;
  static float lastHumidity = NAN;
  static bool lastSensorValid = false;
  static bool firstUpdate = true;

  static bool lastWiFiConnected = false;
  static String lastIP = "";
  static unsigned long lastUptimeMinute = ULONG_MAX;

  // SENSOR VALUES
  bool sensorChanged =
    firstUpdate ||
    sensorValid != lastSensorValid ||
    (sensorValid &&
      (!isfinite(lastTempC) ||
       !isfinite(lastHumidity) ||
       fabsf(temperatureC - lastTempC) >= 0.05f ||
       fabsf(humidity - lastHumidity) >= 0.05f));

  if (sensorChanged) {
    // Clear only the changing sensor values
    tft.fillRect(10, 67, 220, 52, ST77XX_BLACK);
    tft.fillRect(10, 148, 220, 32, ST77XX_BLACK);

    if (sensorValid) {
      float tempF = temperatureC * 1.8f + 32.0f;

      // Fahrenheit first
      tft.setTextColor(ST77XX_WHITE);
      tft.setTextSize(3);
      tft.setCursor(10, 68);
      tft.print(tempF, 1);
      tft.println(" F");

      // Celsius second
      tft.setTextSize(2);
      tft.setCursor(10, 99);
      tft.print(temperatureC, 1);
      tft.println(" C");

      // Humidity
      tft.setTextColor(ST77XX_GREEN);
      tft.setTextSize(3);
      tft.setCursor(10, 149);
      tft.print(humidity, 1);
      tft.println("%");

      lastTempC = temperatureC;
      lastHumidity = humidity;
    } else {
      tft.setTextColor(ST77XX_RED);
      tft.setTextSize(2);
      tft.setCursor(10, 75);
      tft.println("Sensor Error");
    }

    lastSensorValid = sensorValid;
  }

  // WI-FI STATUS
  bool connected = WiFi.status() == WL_CONNECTED;
  String currentIP = connected
    ? WiFi.localIP().toString()
    : "Not available";

  if (firstUpdate ||
      connected != lastWiFiConnected ||
      currentIP != lastIP) {

    tft.fillRect(10, 189, 220, 27, ST77XX_BLACK);

    tft.setTextSize(1);
    tft.setCursor(10, 190);

    if (connected) {
      tft.setTextColor(ST77XX_GREEN);
      tft.println("WiFi: CONNECTED");
    } else {
      tft.setTextColor(ST77XX_RED);
      tft.println("WiFi: DISCONNECTED");
    }

    tft.setTextColor(ST77XX_WHITE);
    tft.setCursor(10, 203);
    tft.print("IP: ");
    tft.println(currentIP);

    lastWiFiConnected = connected;
    lastIP = currentIP;
  }

  // UPTIME: update only when minute changes
  unsigned long totalMinutes = millis() / 60000UL;

  if (firstUpdate ||
      totalMinutes != lastUptimeMinute) {

    unsigned long days = totalMinutes / 1440UL;
    unsigned long hours = (totalMinutes % 1440UL) / 60UL;
    unsigned long minutes = totalMinutes % 60UL;

    tft.fillRect(10, 219, 220, 15, ST77XX_BLACK);

    tft.setTextColor(ST77XX_CYAN);
    tft.setTextSize(1);
    tft.setCursor(10, 220);

    tft.printf(
      "Uptime: %lud %02luh %02lum",
      days,
      hours,
      minutes
    );

    lastUptimeMinute = totalMinutes;
  }

  firstUpdate = false;
}


// ========================================
// WEB DASHBOARD
// ========================================

void handleRoot() {

  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">

<title>ESP32 Manish Smart Home</title>

<style>
* {
  box-sizing: border-box;
}

body {
  margin: 0;
  padding: 20px;
  background: #0f172a;
  color: #e2e8f0;
  font-family: Arial, sans-serif;
}

h1 {
  text-align: center;
  color: #22d3ee;
  font-size: clamp(22px, 4vw, 32px);
  margin-bottom: 24px;
}

.card {
  background: #1e293b;
  border-radius: 14px;
  padding: 20px;
  margin-bottom: 20px;
  overflow: hidden;
}

.card h2 {
  margin-top: 0;
  font-size: 20px;
}

.label {
  color: #94a3b8;
  font-size: 14px;
}

/* Responsive two-column dashboard */
.dashboard-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 20px;
  width: 100%;
}

.dashboard-column {
  min-width: 0;
}

.dashboard-column .card {
  min-width: 0;
  box-sizing: border-box;
}

/* Responsive charts */
canvas {
  display: block;
  width: 100%;
  max-width: 100%;
  height: 260px;
}

/* Mobile layout */
@media (max-width: 700px) {
  body {
    padding: 12px;
  }

  .dashboard-grid {
    grid-template-columns: 1fr;
    gap: 0;
  }

  .card {
    padding: 14px;
  }

  canvas {
    height: 200px;
  }
}

/* TEMPERATURE CARD */
.temperature-card {
  border-top: 4px solid #ff7043;
}

.temperature-card h2 {
  color: #ff7043;
}

/* Primary Fahrenheit temperature */
#tempF {
  color: #ff7043;
  font-size: 48px;
  font-weight: bold;
}

/* Secondary Celsius temperature */
#temp {
  color: #ffab91;
  font-size: 22px;
}

/* Temperature min/max */
#tempRange {
  color: #ffab91 !important;
}

/* Humidity reading */
#humidity {
  color: #00bcd4;
  font-size: 48px;
  font-weight: bold;
}

/* Card headings */
.temperature-card h2,
.temperature-history h2 {
  color: #ff7043;
}

.humidity-card {
  border-top: 4px solid #00bcd4;
}
.humidity-card h2,
.humidity-history h2 {
  color: #00bcd4;
}
</style>
</head>

<body>
<h1>ESP32 Manish SMART HOME</h1>

<div class="dashboard-grid">

  <!-- COLUMN 1: TEMPERATURE -->
  <div class="dashboard-column">

    <div class="card temperature-card">
      <h2>Temperature</h2>

      <!-- Large Fahrenheit reading -->
      <div class="value" id="tempF">--</div>

      <!-- Smaller Celsius reading -->
      <p id="temp">--</p>

      <p class="label" id="tempRange">
        Min: -- | Max: --
      </p>
    </div>

    <div class="card temperature-history">
      <h2>Temperature History</h2>

      <canvas id="temperatureChart"></canvas>
      <p class="label">
        Last 24 hours - Fahrenheit
      </p>
    </div>

  </div>

  <!-- COLUMN 2: HUMIDITY -->
  <div class="dashboard-column">

    <div class="card humidity-card">
      <h2>Humidity</h2>

      <div class="value" id="humidity">--</div>

      <p class="label" id="humidityRange">
        Min: -- | Max: --
      </p>
    </div>

    <div class="card humidity-history">
      <h2>Humidity History</h2>
      <canvas id="humidityChart"></canvas>
      <p class="label">
        Last 24 hours - Relative humidity
      </p>
    </div>

  </div>

</div>

<!-- DEVICE STATUS -->
<div class="card">
  <h2>Device Status</h2>
  <p id="status">Connecting...</p>
  <p id="uptime">Uptime: --</p>
  <p id="errors">Sensor Errors: --</p>
</div>

<script>

function formatValue(value, suffix) {
  return value === null ? "--" : value.toFixed(1) + suffix;
}

async function updateData() {
  try {
    const response = await fetch('/data');

    if (!response.ok) {
      throw new Error('Request failed');
    }

    const data = await response.json();
    
    const toF = c => c * 1.8 + 32;

    // Fahrenheit: large primary reading
    document.getElementById("tempF").innerText =
      data.temperature === null
        ? "--"
        : toF(data.temperature).toFixed(1) + " \u00B0F";

    // Celsius: smaller secondary reading
    document.getElementById("temp").innerText =
      formatValue(data.temperature, " \u00B0C");

    // Minimum and maximum in Fahrenheit
    document.getElementById("tempRange").innerText =
      "Min: " +
      (data.minTemp === null
        ? "--"
        : toF(data.minTemp).toFixed(1) + " \u00B0F") +
      " | Max: " +
      (data.maxTemp === null
        ? "--"
        : toF(data.maxTemp).toFixed(1) + " \u00B0F");

    document.getElementById("humidity").innerText =
      formatValue(data.humidity, " %");

    document.getElementById("humidityRange").innerText =
      "Min: " + formatValue(data.minHumidity, " %") +
      " | Max: " + formatValue(data.maxHumidity, " %");

    document.getElementById("status").innerText =
      data.sensorValid ? "Sensor Online" : "Sensor Error";

    document.getElementById("uptime").innerText =
      "Uptime: " + data.uptime + " seconds";

    document.getElementById("errors").innerText =
      "Sensor Errors: " + data.errors;

  } catch (error) {
    document.getElementById("status").innerText =
      "Connection Lost";
  }
}



function drawChart(canvasId, values, color, unit) {
  const canvas = document.getElementById(canvasId);
  if (!canvas) return;

  const ctx = canvas.getContext("2d");
  const rect = canvas.getBoundingClientRect();

  const w = rect.width;
  const h = rect.height;

  if (w <= 0 || h <= 0) return;

  const dpr = window.devicePixelRatio || 1;

  canvas.width = Math.round(w * dpr);
  canvas.height = Math.round(h * dpr);

  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);

  const left = 55;
  const right = 15;
  const top = 20;
  const bottom = 35;

  const plotW = w - left - right;
  const plotH = h - top - bottom;

  ctx.font = "14px Arial";

  if (values.length === 0) {
    ctx.fillStyle = "#94a3b8";
    ctx.fillText("Waiting for history data...", 70, 120);
    return;
  }

  let min = Math.min(...values);
  let max = Math.max(...values);

  if (max - min < 1) {
    min -= 0.5;
    max += 0.5;
  }

  const y = value =>
    top + (max - value) / (max - min) * plotH;

  const x = index =>
    left + index / Math.max(values.length - 1, 1) * plotW;

  // Grid and Y-axis labels
  ctx.strokeStyle = "#334155";
  ctx.fillStyle = "#94a3b8";
  ctx.lineWidth = 1;

  for (let i = 0; i <= 4; i++) {
    const py = top + i * plotH / 4;
    const label = max - i * (max - min) / 4;

    ctx.beginPath();
    ctx.moveTo(left, py);
    ctx.lineTo(w - right, py);
    ctx.stroke();

    ctx.fillText(label.toFixed(1), 5, py + 4);
  }

  // History line
  ctx.strokeStyle = color;
  ctx.lineWidth = 3;
  ctx.beginPath();

  values.forEach((value, i) => {
    if (i === 0) ctx.moveTo(x(i), y(value));
    else ctx.lineTo(x(i), y(value));
  });

  if (values.length === 1) {
    ctx.lineTo(x(0) + 1, y(values[0]));
  }

  ctx.stroke();

  // Current value
  const latest = values[values.length - 1];

  ctx.fillStyle = color;
  ctx.fillText(
    "Latest: " + latest.toFixed(1) + unit,
    left,
    h - 8
  );

  ctx.fillStyle = "#94a3b8";
  ctx.fillText(
    "History: " + values.length + " minutes",
    w - 180,
    h - 8
  );
}

let historyBusy = false;

async function updateHistory() {
  if (historyBusy) return;

  historyBusy = true;

  try {
    const response = await fetch("/history", {
      signal: AbortSignal.timeout(10000)
    });

    if (!response.ok) {
      throw new Error("History request failed");
    }

    const data = await response.json();
    const samples = data.samples || [];

    const temperatures = samples.map(s => s[0] * 1.8 + 32);
    const humidities = samples.map(s => s[1]);

    drawChart("temperatureChart", temperatures, "#ff7043", " °F");
    drawChart("humidityChart", humidities, "#00bcd4", " %");

  } catch (error) {
    console.error("History update failed:", error);
  } finally {
    historyBusy = false;
  }
}

let resizeTimer;

window.addEventListener("resize", () => {
  clearTimeout(resizeTimer);

  resizeTimer = setTimeout(() => {
    updateHistory();
  }, 200);
});

updateHistory();
setInterval(updateHistory, 120000);


updateData();
setInterval(updateData, 2000);

</script>

</body>
</html>
)rawliteral";

  server.send(200, "text/html; charset=utf-8", html);
}

// ========================================
// JSON SENSOR DATA
// ========================================

void handleData() {

  String json = "{";

  json += "\"temperature\":";
  json += sensorValid ? String(temperatureC, 1) : "null";

  json += ",\"humidity\":";
  json += sensorValid ? String(humidity, 1) : "null";

  json += ",\"minTemp\":";
  json += isnan(minTemp) ? "null" : String(minTemp, 1);

  json += ",\"maxTemp\":";
  json += isnan(maxTemp) ? "null" : String(maxTemp, 1);

  json += ",\"minHumidity\":";
  json += isnan(minHumidity) ? "null" : String(minHumidity, 1);

  json += ",\"maxHumidity\":";
  json += isnan(maxHumidity) ? "null" : String(maxHumidity, 1);

  json += ",\"sensorValid\":";
  json += sensorValid ? "true" : "false";

  json += ",\"uptime\":";
  json += String(millis() / 1000);

  json += ",\"errors\":";
  json += String(sensorErrors);

  json += "}";

  server.send(200, "application/json", json);
}

void handleHistory() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  server.sendContent(
    "{\"intervalSeconds\":60,\"samples\":["
  );

  char entry[64];
  String chunk;
  chunk.reserve(1024);

  int oldest =
    (historyNext - historyCount + HISTORY_SIZE)
    % HISTORY_SIZE;

  for (int i = 0; i < historyCount; i++) {
    int index = (oldest + i) % HISTORY_SIZE;

    snprintf(
      entry,
      sizeof(entry),
      "%s[%.1f,%.1f]",
      i == 0 ? "" : ",",
      tempHistory[index],
      humidityHistory[index]
    );

    if (chunk.length() + strlen(entry) > 1000) {
      server.sendContent(chunk);
      chunk = "";
      yield();
    }

    chunk += entry;
  }

  if (chunk.length() > 0) {
    server.sendContent(chunk);
  }

  server.sendContent("]}");
  server.sendContent("");
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.println("[WiFi] Connected to router");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.println("[WiFi] Got IP address");
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf(
        "[WiFi] Disconnected! Reason code: %u\n",
        (unsigned int)info.wifi_sta_disconnected.reason
      );
      break;

    default:
      break;
  }
}

void checkWiFi() {
  static unsigned long disconnectedSince = 0;
  static unsigned long lastAttempt = 0;
  static bool firstAttempt = true;

  const unsigned long INITIAL_WAIT = 30000;
  const unsigned long RETRY_INTERVAL = 60000;

  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED) {
    disconnectedSince = 0;
    lastAttempt = 0;
    firstAttempt = true;
    return;
  }

  if (disconnectedSince == 0) {
    disconnectedSince = now;
    Serial.println("[WiFi] Disconnected. Waiting...");
    return;
  }

  if (now - disconnectedSince < INITIAL_WAIT) {
    return;
  }

  if (!firstAttempt && now - lastAttempt < RETRY_INTERVAL) {
    return;
  }

  firstAttempt = false;
  lastAttempt = now;

  Serial.println("[WiFi] Starting recovery attempt...");

  // Reset the station's current connection attempt.
  WiFi.disconnect(false, false);
  delay(200);

  WiFi.begin(ssid, password);
}


void scanWiFiNetworks() {
  Serial.println("[WiFi] Scanning nearby networks...");

  int count = WiFi.scanNetworks();

  Serial.printf("[WiFi] Found %d networks\n", count);

  for (int i = 0; i < count; i++) {
    Serial.printf(
      "[WiFi] SSID: %s | RSSI: %d dBm | Channel: %d\n",
      WiFi.SSID(i).c_str(),
      WiFi.RSSI(i),
      WiFi.channel(i)
    );
  }

  WiFi.scanDelete();
}


// ========================================
// SETUP
// ========================================

void setup() {

  Serial.begin(115200);

  dht.begin();

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);

  tft.init(240, 240);
  tft.setRotation(0);

  // Draw static LCD labels once
  drawLCDLayout();

  Serial.println("Connecting to WiFi...");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(false);
  WiFi.onEvent(onWiFiEvent);
  WiFi.begin(ssid, password);

  unsigned long wifiStart = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {

    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {

    Serial.println();
    Serial.println("WiFi Connected!");

    Serial.print("ESP32 IP Address: ");
    Serial.println(WiFi.localIP());

  } else {
    Serial.println();
    Serial.println("WiFi connection failed.");
    Serial.println("LCD monitoring will continue.");
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/history", handleHistory);

  server.begin();

  Serial.println("Web Server Started");

  readSensor();
  recordHistory();
  lastHistorySample = millis();

  updateLCD();

  lastSensorRead = millis();
  scanWiFiNetworks();
}

// ========================================
// MAIN LOOP
// ========================================

void loop() {

  server.handleClient();

  if (millis() - lastSensorRead >= 2000) {

    lastSensorRead = millis();

    readSensor();
    updateLCD();
  }

  // history every 60 seconds
  if (millis() - lastHistorySample >= HISTORY_INTERVAL) {

    lastHistorySample = millis();

    recordHistory();

    Serial.print("History sample saved. Total: ");
    Serial.println(historyCount);
  }

  
  // Retry Wi-Fi periodically if disconnected
  // Wi-Fi connection monitoring
  static bool wasConnected = false;
  static unsigned long lastWiFiCheck = 0;

  if (millis() - lastWiFiCheck >= 10000) {
    lastWiFiCheck = millis();

    bool connected = WiFi.status() == WL_CONNECTED;

    if (connected && !wasConnected) {
      Serial.println("WiFi Connected!");
      Serial.print("IP Address: ");
      Serial.println(WiFi.localIP());
    }

    if (!connected && wasConnected) {
      Serial.println("WARNING: WiFi disconnected!");
    }

    wasConnected = connected;
  }

  // Diagnostic heartbeat every 60 seconds
  static unsigned long lastHeartbeat = 0;

  if (millis() - lastHeartbeat >= 60000) {
    lastHeartbeat = millis();

    Serial.printf(
      "[HEARTBEAT] Uptime=%lus, WiFi=%s, FreeHeap=%u bytes\n",
      millis() / 1000,
      WiFi.status() == WL_CONNECTED ? "CONNECTED" : "DISCONNECTED",
      (unsigned int)ESP.getFreeHeap());

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[HEARTBEAT] IP=");
      Serial.println(WiFi.localIP());
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[HEARTBEAT] RSSI=");
      Serial.print(WiFi.RSSI());
      Serial.println(" dBm");
    }
  
    Serial.print("[HEARTBEAT] WiFi status code=");
    Serial.println(WiFi.status());
  }

  checkWiFi();
}
