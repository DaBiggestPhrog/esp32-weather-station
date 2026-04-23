#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <time.h>
#include <mbedtls/md.h>
#include <mbedtls/base64.h>

// ─── WiFi ─────────────────────────────────────────────────────────────────────
const char* WIFI_SSID     = "TWOJA_NAZWA_SIECI";
const char* WIFI_PASSWORD = "TWOJE_HASLO";

// ─── Azure IoT Hub ────────────────────────────────────────────────────────────
// Z Connection String: HostName=XXX;DeviceId=YYY;SharedAccessKey=ZZZ
// Wypełnij poniższe trzema osobnymi wartościami:
const char* IOT_HUB_HOST  = "TWOJ_HUB.azure-devices.net";
const char* IOT_DEVICE_ID = "TWOJE_DEVICE_ID";
const char* IOT_SAS_KEY   = "TWOJ_KLUCZ_SAS";

// ─── Konfiguracja ─────────────────────────────────────────────────────────────
const char*          DEVICE_ID   = "esp32-sensor-01";
const unsigned long  INTERVAL_MS = 15000; // odczyt co 15 sekund

// ─── I2C ──────────────────────────────────────────────────────────────────────
TwoWire I2C_indoor  = TwoWire(0); // AHT10        — SDA=21, SCL=22
TwoWire I2C_outdoor = TwoWire(1); // AHT20+BMP280 — SDA=16, SCL=17

// ─── Czujniki ─────────────────────────────────────────────────────────────────
Adafruit_AHTX0  aht10;
Adafruit_AHTX0  aht20;
Adafruit_BMP280 bmp(&I2C_outdoor);
bool aht10_ok = false, aht20_ok = false, bmp_ok = false;

// ─── Dane czujników ───────────────────────────────────────────────────────────
float aht10_temp = NAN, aht10_hum = NAN;
float aht20_temp = NAN, aht20_hum = NAN;
float bmp_pres   = NAN, bmp_alt   = NAN;
float delta_temp = NAN, delta_hum = NAN;
unsigned long lastReadTime = 0;

// ─── MQTT / TLS ───────────────────────────────────────────────────────────────
WiFiClientSecure tlsClient;
PubSubClient     mqttClient(tlsClient);
const String MQTT_TOPIC = "devices/" + String(IOT_DEVICE_ID) + "/messages/events/";

// ─────────────────────────────────────────────────────────────────────────────
// JSON
// ─────────────────────────────────────────────────────────────────────────────

String buildJson() {
  auto f = [](float v) -> String {
    return isnan(v) ? "null" : String(v, 2);
  };
  time_t now = time(nullptr);
  String j = "{\n";
  j += "  \"deviceId\": \""       + String(DEVICE_ID) + "\",\n";
  j += "  \"timestamp\": "        + String(now)        + ",\n";
  j += "  \"indoor\": {\n";
  j += "    \"sensor\": \"AHT10\",\n";
  j += "    \"temperature_c\": "  + f(aht10_temp)      + ",\n";
  j += "    \"humidity_pct\": "   + f(aht10_hum)       + "\n";
  j += "  },\n";
  j += "  \"outdoor\": {\n";
  j += "    \"sensor\": \"AHT20+BMP280\",\n";
  j += "    \"temperature_c\": "  + f(aht20_temp)      + ",\n";
  j += "    \"humidity_pct\": "   + f(aht20_hum)       + ",\n";
  j += "    \"pressure_hpa\": "   + f(bmp_pres)        + ",\n";
  j += "    \"altitude_m\": "     + f(bmp_alt)         + "\n";
  j += "  },\n";
  j += "  \"delta\": {\n";
  j += "    \"temperature_c\": "  + f(delta_temp)      + ",\n";
  j += "    \"humidity_pct\": "   + f(delta_hum)       + "\n";
  j += "  }\n";
  j += "}";
  return j;
}

// ─────────────────────────────────────────────────────────────────────────────
// SAS Token
// ─────────────────────────────────────────────────────────────────────────────

String generateSASToken() {
  String resourceUri = String(IOT_HUB_HOST) + "%2Fdevices%2F" + String(IOT_DEVICE_ID);
  long   expiry      = time(nullptr) + 86400; // ważny 24h
  String toSign      = resourceUri + "\n" + String(expiry);

  unsigned char decodedKey[64];
  size_t decodedLen = 0;
  mbedtls_base64_decode(decodedKey, sizeof(decodedKey), &decodedLen,
                        (const unsigned char*)IOT_SAS_KEY, strlen(IOT_SAS_KEY));

  unsigned char hmac[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  mbedtls_md_hmac_starts(&ctx, decodedKey, decodedLen);
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)toSign.c_str(), toSign.length());
  mbedtls_md_hmac_finish(&ctx, hmac);
  mbedtls_md_free(&ctx);

  unsigned char sig64[64];
  size_t sig64Len = 0;
  mbedtls_base64_encode(sig64, sizeof(sig64), &sig64Len, hmac, 32);

  String sig = String((char*)sig64).substring(0, sig64Len);
  sig.replace("+", "%2B");
  sig.replace("/", "%2F");
  sig.replace("=", "%3D");

  return "SharedAccessSignature sr=" + resourceUri +
         "&sig=" + sig + "&se=" + String(expiry);
}

// ─────────────────────────────────────────────────────────────────────────────
// WiFi
// ─────────────────────────────────────────────────────────────────────────────

void connectWiFi() {
  Serial.print("[WiFi] Laczenie z: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Polaczono! IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] Blad polaczenia!");
  }
}

// ─────────────────────────────────────────────────────────────────────────────
// NTP
// ─────────────────────────────────────────────────────────────────────────────

void syncNTP() {
  Serial.println("[NTP]  Synchronizacja czasu...");
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov"); // UTC+1 Polska
  time_t now = time(nullptr);
  int attempts = 0;
  while (now < 1000000000 && attempts < 20) {
    delay(500); Serial.print("."); now = time(nullptr); attempts++;
  }
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
  Serial.println(now > 1000000000
    ? "\n[NTP]  OK: " + String(buf)
    : "\n[NTP]  Blad synchronizacji!");
}

// ─────────────────────────────────────────────────────────────────────────────
// MQTT
// ─────────────────────────────────────────────────────────────────────────────

bool connectMQTT() {
  String sasToken = generateSASToken();
  String username = String(IOT_HUB_HOST) + "/" +
                    String(IOT_DEVICE_ID) + "/?api-version=2021-04-12";
  Serial.println("[MQTT] Laczenie z Azure IoT Hub...");
  if (mqttClient.connect(IOT_DEVICE_ID, username.c_str(), sasToken.c_str())) {
    Serial.println("[MQTT] Polaczono!");
    return true;
  }
  Serial.println("[MQTT] Blad! Kod: " + String(mqttClient.state()));
  return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== ESP32 Dual Sensor Node ===");
  Serial.println("Device ID: " + String(DEVICE_ID));
  Serial.println("==============================\n");

  I2C_indoor.begin(21, 22);
  I2C_outdoor.begin(16, 17);

  aht10_ok = aht10.begin(&I2C_indoor);
  Serial.println(aht10_ok ? "[OK]  AHT10  — wnetrze (SDA=21, SCL=22)"
                           : "[ERR] AHT10  — nie wykryto!");

  aht20_ok = aht20.begin(&I2C_outdoor);
  Serial.println(aht20_ok ? "[OK]  AHT20  — zewnetrze (SDA=16, SCL=17)"
                           : "[ERR] AHT20  — nie wykryto!");

  if      (bmp.begin(0x76)) { bmp_ok = true; Serial.println("[OK]  BMP280 — 0x76"); }
  else if (bmp.begin(0x77)) { bmp_ok = true; Serial.println("[OK]  BMP280 — 0x77"); }
  else                        Serial.println("[ERR] BMP280 — nie wykryto!");

  if (bmp_ok) {
    bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                    Adafruit_BMP280::SAMPLING_X2,
                    Adafruit_BMP280::SAMPLING_X16,
                    Adafruit_BMP280::FILTER_X16,
                    Adafruit_BMP280::STANDBY_MS_500);
  }

  connectWiFi();

  if (WiFi.status() == WL_CONNECTED) {
    syncNTP();
    tlsClient.setInsecure();
    mqttClient.setServer(IOT_HUB_HOST, 8883);
    mqttClient.setBufferSize(1024);
    connectMQTT();
  }

  Serial.println("\n[START] Pomiary co " + String(INTERVAL_MS / 1000) + "s\n");
}

// ─────────────────────────────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────────────────────────────

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    mqttClient.loop();
    static unsigned long lastReconnect = 0;
    if (!mqttClient.connected() && millis() - lastReconnect > 5000) {
      lastReconnect = millis();
      connectMQTT();
    }
  }

  unsigned long now = millis();
  if (now - lastReadTime < INTERVAL_MS) return;
  lastReadTime = now;

  if (aht10_ok) {
    sensors_event_t h, t;
    aht10.getEvent(&h, &t);
    aht10_temp = t.temperature;
    aht10_hum  = h.relative_humidity;
  }
  if (aht20_ok) {
    sensors_event_t h, t;
    aht20.getEvent(&h, &t);
    aht20_temp = t.temperature;
    aht20_hum  = h.relative_humidity;
  }
  if (bmp_ok) {
    bmp_pres = bmp.readPressure() / 100.0F;
    bmp_alt  = bmp.readAltitude(1013.25);
  }

  if (!isnan(aht10_temp) && !isnan(aht20_temp)) delta_temp = aht20_temp - aht10_temp;
  if (!isnan(aht10_hum)  && !isnan(aht20_hum))  delta_hum  = aht20_hum  - aht10_hum;

  String json = buildJson();
  Serial.println(json);

  if (WiFi.status() == WL_CONNECTED && mqttClient.connected()) {
    bool sent = mqttClient.publish(MQTT_TOPIC.c_str(), json.c_str());
    Serial.println(sent ? "[MQTT] Wyslano do Azure!" : "[MQTT] Blad wysylania!");
  }
}
