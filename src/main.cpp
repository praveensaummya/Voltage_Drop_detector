#define BLYNK_TEMPLATE_ID "TMPL6A8TFGztT"
#define BLYNK_TEMPLATE_NAME "Voltage and Temp Monitor"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <BlynkSimpleEsp32.h>

// WiFi credentials (for local web server AP & Blynk connection)
char auth[] = "w5eXpkVI9oSJwTyx_Q8tJ5fcW_XtkGgb";
char ssid[] = "psa";
char pass[] = "12345678";

const int zmptPin = 34; // ZMPT101B OUT
WebServer server(80);

// --- DHT11 Sensor Configuration ---
#define DHTPIN 23     
#define DHTTYPE DHT11   
DHT dht(DHTPIN, DHTTYPE);

// --- Relay Configuration ---
const int relay1Pin = 18; // Relay 1 (Voltage Protection)
const int relay2Pin = 19; // Relay 2 (Fan / Temperature Control)

bool relay1State = false;
bool relay2State = false;

// Mode selections: 0 = Auto, 1 = Manual
int relay1Mode = 0; 
int relay2Mode = 0; 

// Manual override states from Blynk
bool manualR1 = false;
bool manualR2 = false;

// Temperature threshold for Relay 2 (Fan control)
const float TEMP_THRESHOLD = 30.0; // Turns ON if temp > 30°C

// --- OLED Display Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// 7 Direct GPIO pins for LEDs P7 down to P1
const int ledPins[7] = {32, 33, 25, 26, 27, 14, 12}; 

// Global variables
float rmsVoltage = 0.0;
float temperature = 0.0;
float humidity = 0.0;
int rawPeakToPeak = 0;

BlynkTimer timer;

// Self-contained offline HTML page
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: Arial, sans-serif; text-align: center; background: #f4f4f9; margin: 0; padding: 20px; }
    h2 { color: #333; }
    .card { background: white; max-width: 600px; margin: auto; padding: 20px; border-radius: 8px; box-shadow: 0px 4px 10px rgba(0,0,0,0.1); margin-bottom: 15px; }
    .value-box { font-size: 24px; color: #007bff; margin: 15px 0; font-weight: bold; }
  </style>
</head>
<body>
  <div class="card">
    <h2>ZMPT101B AC Voltage & Environment</h2>
    <div class="value-box">RMS Voltage: <span id="voltVal">0.0</span> V</div>
    <div style="font-size: 18px; color: #333; margin: 10px 0;">Temperature: <span id="tempVal">0.0</span> °C</div>
    <div style="font-size: 16px; color: #555;">Humidity: <span id="humVal">0.0</span> %</div>
  </div>
  <script>
    async function fetchData() {
      try {
        const response = await fetch('/readings');
        const data = await response.json();
        document.getElementById('voltVal').innerText = data.voltage.toFixed(1) + " V";
        document.getElementById('tempVal').innerText = data.temperature.toFixed(1);
        document.getElementById('humVal').innerText = data.humidity.toFixed(1);
      } catch (e) {
        console.error("Fetch error:", e);
      }
    }
    setInterval(fetchData, 500);
  </script>
</body>
</html>
)rawliteral";

// --- Blynk Virtual Pin Handlers ---
BLYNK_WRITE(V1) { // Relay 1 Manual Switch
  manualR1 = param.asInt();
}

BLYNK_WRITE(V2) { // Relay 2 Manual Switch
  manualR2 = param.asInt();
}

BLYNK_WRITE(V3) { // Relay 1 Mode (0 = Auto, 1 = Manual)
  relay1Mode = param.asInt();
}

BLYNK_WRITE(V4) { // Relay 2 Mode (0 = Auto, 1 = Manual)
  relay2Mode = param.asInt();
}

// Function to send data to Blynk every 5 seconds
void sendBlynkData() {
  Blynk.virtualWrite(V5, rmsVoltage);
  Blynk.virtualWrite(V6, temperature);
  Blynk.virtualWrite(V7, humidity);
  Blynk.virtualWrite(V8, relay1State ? "ON (Auto)" : "OFF (Auto)");
  if(relay1Mode == 1) Blynk.virtualWrite(V8, relay1State ? "ON (Manual)" : "OFF (Manual)");
  
  Blynk.virtualWrite(V9, relay2State ? "ON (Auto)" : "OFF (Auto)");
  if(relay2Mode == 1) Blynk.virtualWrite(V9, relay2State ? "ON (Manual)" : "OFF (Manual)");
}

void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleData() {
  String json = "{\"voltage\":" + String(rmsVoltage, 1) + 
                ",\"temperature\":" + String(temperature, 1) + 
                ",\"humidity\":" + String(humidity, 1) + 
                ",\"relay1\":" + String(relay1State ? "true" : "false") + 
                ",\"relay2\":" + String(relay2State ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

// Update LED Bar Graph (Option A)
void updateLEDs(float voltage) {
  for (int i = 0; i < 7; i++) {
    digitalWrite(ledPins[i], LOW);
  }

  if (voltage < 180.0)      digitalWrite(ledPins[0], HIGH); // P7: Under 180V
  else if (voltage < 200.0) digitalWrite(ledPins[1], HIGH); // P6
  else if (voltage < 215.0) digitalWrite(ledPins[2], HIGH); // P5
  else if (voltage < 240.0) digitalWrite(ledPins[3], HIGH); // P4: Nominal
  else if (voltage < 250.0) digitalWrite(ledPins[4], HIGH); // P3
  else if (voltage <= 250.1) digitalWrite(ledPins[5], HIGH); // P2
  else                       digitalWrite(ledPins[6], HIGH); // P1: Over 250V
}

// Independent Relay Control Logic (Auto vs Manual)
void processRelays(float voltage, float temp) {
  // --- Relay 1 Logic (Voltage Protection) ---
  if (relay1Mode == 1) {
    // Manual Mode
    relay1State = manualR1;
  } else {
    // Auto Mode: Trips OFF if voltage is unsafe
    if (voltage >= 180.0 && voltage <= 250.0) {
      relay1State = true; 
    } else {
      relay1State = false; 
    }
  }

  // --- Relay 2 Logic (Temperature / Fan Control) ---
  if (relay2Mode == 1) {
    // Manual Mode
    relay2State = manualR2;
  } else {
    // Auto Mode: Turns on if temp exceeds threshold, turns off if cool
    if (temp >= TEMP_THRESHOLD) {
      relay2State = true;  // Fan ON
    } else {
      relay2State = false; // Fan OFF
    }
  }

  // Apply to physical pins (Active-LOW modules)
  digitalWrite(relay1Pin, relay1State ? LOW : HIGH);
  digitalWrite(relay2Pin, relay2State ? LOW : HIGH);
}

void updateOLED(float voltage, float temp, float hum) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("AC VOLT & TEMP MONITOR"));
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);

  display.setTextSize(2);
  display.setCursor(0, 14);
  display.print(voltage, 1);
  display.setTextSize(1);
  display.setCursor(95, 20);
  display.print(F("V"));

  display.setCursor(0, 36);
  display.print(F("Temp: ")); display.print(temp, 1); display.print(F(" C"));

  display.setCursor(0, 46);
  display.print(F("Humidity: ")); display.print(hum, 1); display.print(F(" %"));

  display.setCursor(0, 56);
  if (voltage < 180.0) display.print(F("Status: LOW VOLT"));
  else if (voltage > 250.0) display.print(F("Status: OVERVOLT"));
  else display.print(F("Status: NORMAL"));

  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  analogSetAttenuation(ADC_11db);

  dht.begin();

  pinMode(relay1Pin, OUTPUT);
  pinMode(relay2Pin, OUTPUT);
  digitalWrite(relay1Pin, HIGH); 
  digitalWrite(relay2Pin, HIGH); 

  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("OLED failed"));
    for(;;);
  }
  display.clearDisplay();
  display.display();

  for (int i = 0; i < 7; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW); 
  }

  // --- Clean Wi-Fi Connection & Non-Blocking Blynk Init ---
  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, pass);
  
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 20000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi. Check hotspot credentials.");
  }

  Blynk.config(auth, "blynk.cloud", 80);
  Blynk.connect();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/readings", HTTP_GET, handleData);
  server.begin();

  // Setup a timer to send data to Blynk every 5 seconds (5000 milliseconds)
  timer.setInterval(5000L, sendBlynkData);
}

void loop() {
  Blynk.run();
  timer.run();
  server.handleClient(); 

  // Sample voltage over a 200ms window
  const unsigned long sampleWindowMs = 200; 
  unsigned long startMillis = millis();
  unsigned int signalMax = 0;
  unsigned int signalMin = 4095;

  while (millis() - startMillis < sampleWindowMs) {
    server.handleClient(); 
    int sample = analogRead(zmptPin);
    if (sample > signalMax) signalMax = sample;
    if (sample < signalMin) signalMin = sample;
  }

  rawPeakToPeak = signalMax - signalMin; 
  rmsVoltage = (float)rawPeakToPeak * 0.14; 

  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t)) temperature = t;
  if (!isnan(h)) humidity = h;

  updateLEDs(rmsVoltage);
  processRelays(rmsVoltage, temperature);
  updateOLED(rmsVoltage, temperature, humidity);
}
