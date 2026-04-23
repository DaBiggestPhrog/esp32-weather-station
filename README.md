# ESP32 Weather Station — Azure IoT

System IoT do monitorowania warunków środowiskowych w dwóch lokalizacjach jednocześnie (wewnątrz i na zewnątrz pomieszczenia). Dane zbierane są przez mikrokontroler ESP32, przesyłane do chmury Azure przez MQTT i wizualizowane w czasie rzeczywistym z systemem automatycznego alertowania.

---

## Architektura systemu

```
ESP32 + Czujniki
    │
    │ MQTT/TLS (port 8883)
    ▼
Azure IoT Hub
    │
    │ Event Trigger
    ▼
Azure Functions ──────────────────── REST API
    │                                    │
    │ Zapis                              │ GET /api/GetSensorData
    ▼                                    ▼
Azure Table Storage              Grafana Cloud
                                     │
                                     ├── Dashboard (wykresy)
                                     └── Alerty (email)
```

---

## Komponenty sprzętowe

| Komponent | Opis | Połączenie |
|---|---|---|
| ESP32 DEVKITV1 | Mikrokontroler z WiFi | — |
| AHT10 | Temperatura i wilgotność (wewnątrz) | I2C #0: SDA=GPIO21, SCL=GPIO22 |
| AHT20 + BMP280 | Temperatura, wilgotność, ciśnienie (zewnątrz) | I2C #1: SDA=GPIO16, SCL=GPIO17 |

### Schemat podłączenia

**AHT10 — wewnątrz (I2C #0):**
```
AHT10   →   ESP32
VCC     →   3.3V
GND     →   GND
SDA     →   GPIO21
SCL     →   GPIO22
```

**AHT20 + BMP280 — zewnątrz (I2C #1):**
```
AHT20+BMP280  →   ESP32
VCC           →   3.3V
GND           →   GND
SDA           →   GPIO16
SCL           →   GPIO17
```

---

## Technologie

**Warstwa sprzętowa:**
- ESP32 DEVKITV1
- Czujnik AHT10 (temperatura, wilgotność)
- Czujnik AHT20 + BMP280 (temperatura, wilgotność, ciśnienie)
- Arduino IDE + biblioteki: Adafruit AHTX0, Adafruit BMP280

**Warstwa komunikacji:**
- Protokół MQTT z szyfrowaniem TLS (port 8883)
- Format danych JSON z timestampem NTP
- Uwierzytelnianie przez tokeny SAS generowane na ESP32

**Warstwa chmurowa (Azure):**
- Azure IoT Hub B1 — brama IoT
- Azure Functions (Consumption) — przetwarzanie danych i REST API
- Azure Table Storage — przechowywanie danych szeregów czasowych

**Warstwa wizualizacji i alertowania:**
- Grafana Cloud (Free tier) — dashboard
- Grafana Alerting — powiadomienia email

---

## Format danych JSON

Każdy pomiar wysyłany przez ESP32 do Azure IoT Hub:

```json
{
  "deviceId": "esp32-sensor-01",
  "timestamp": 1711234567,
  "indoor": {
    "sensor": "AHT10",
    "temperature_c": 22.30,
    "humidity_pct": 48.10
  },
  "outdoor": {
    "sensor": "AHT20+BMP280",
    "temperature_c": 8.45,
    "humidity_pct": 79.30,
    "pressure_hpa": 1007.82,
    "altitude_m": 43.20
  },
  "delta": {
    "temperature_c": -13.85,
    "humidity_pct": 31.20
  }
}
```

---

## Struktura repozytorium

```
esp32-weather-station/
│
├── README.md
│
├── esp32/
│   └── main.ino                 ← kod mikrokontrolera
│
├── azure-functions/
│   ├── IotHubTrigger/
│   │   └── index.js             ← zapis danych z IoT Hub do Table Storage
│   ├── GetSensorData/
│   │   └── index.js             ← REST API — dane historyczne
│   └── GetLatestSensorData/
│       └── index.js             ← REST API — ostatni rekord (dla alertów)
│
└── docs/
    └── opis-projektu.md
```

---

## Instrukcja uruchomienia

### 1. Wymagania

- Arduino IDE z pakietem ESP32
- Konto Azure (Azure for Students lub inne)
- Konto Grafana Cloud (darmowe)
- Biblioteki Arduino:
  - `Adafruit AHTX0`
  - `Adafruit BMP280`
  - `Adafruit Unified Sensor`
  - `PubSubClient`

### 2. Konfiguracja Azure

1. Utwórz grupę zasobów `esp32-weather-rg`
2. Utwórz Azure IoT Hub (warstwa B1)
3. Zarejestruj urządzenie `esp32-sensor-01` w IoT Hub
4. Utwórz Azure Storage Account
5. Utwórz Azure Function App (Node.js 20 LTS, Consumption)
6. Wdróż funkcje z folderu `azure-functions/`
7. Dodaj zmienną środowiskową `AzureWebJobsStorage` z connection stringiem do Storage

### 3. Konfiguracja ESP32

Otwórz `esp32/main.ino` i uzupełnij dane konfiguracyjne:

```cpp
// WiFi
const char* WIFI_SSID     = "TWOJA_NAZWA_SIECI";
const char* WIFI_PASSWORD = "TWOJE_HASLO";

// Azure IoT Hub
const char* IOT_HUB_HOST  = "TWOJ_HUB.azure-devices.net";
const char* IOT_DEVICE_ID = "TWOJE_DEVICE_ID";
const char* IOT_SAS_KEY   = "TWOJ_KLUCZ_SAS";
```

Następnie:
1. Podłącz czujniki zgodnie ze schematem
2. Zmień Partition Scheme na `Huge APP (3MB No OTA)` w Arduino IDE
3. Wgraj kod na ESP32
4. Otwórz Serial Monitor (115200 baud) i sprawdź czy dane są wysyłane

### 4. Konfiguracja Grafana Cloud

1. Załóż konto na grafana.com
2. Zainstaluj plugin Infinity
3. Dodaj źródło danych Infinity
4. Utwórz dashboard z wykresami używając endpointu:
```
https://TWOJA_FUNKCJA.azurewebsites.net/api/GetSensorData?code=TWOJ_KLUCZ
```
5. Skonfiguruj alerty email w Grafana Alerting — użyj endpointu GetLatestSensorData

---

## Reguły alertów

| Alert | Warunek | Próg |
|---|---|---|
| Wysoka temperatura zewnętrzna | `outdoor_temperature` > | 35°C |
| Temperatura poniżej zera | `outdoor_temperature` < | 0°C |
| Niska temperatura wewnętrzna | `indoor_temperature` < | 10°C |
| Wysoka wilgotność zewnętrzna | `outdoor_humidity` > | 85% |
| Duża różnica temperatur | `delta_temperature` > | 15°C |

---

## Wymagane biblioteki Arduino

Zainstaluj przez `Sketch → Include Library → Manage Libraries`:

| Biblioteka |
|---|
| Adafruit AHTX0 |
| Adafruit BMP280 |
| Adafruit Unified Sensor |
| PubSubClient |

---

## Autor
Bielec M. | Fiszbach W. | Chmielewski G.

Projekt zrealizowany w ramach przedmiotu **Technologie i oprogramowanie chmurowe**  
Politechnika Rzeszowska — 2025/2026
