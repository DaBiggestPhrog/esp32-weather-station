# Opis projektu

## Temat
**Zbieranie danych z mikrokontrolera ESP32 i wizualizacja w chmurze Azure IoT Hub**

## Cel systemu
System IoT do monitorowania warunków środowiskowych (temperatura, wilgotność, ciśnienie atmosferyczne) w dwóch lokalizacjach jednocześnie — wewnątrz i na zewnątrz pomieszczenia. Dane zbierane są przez mikrokontroler ESP32 z dwóch zestawów czujników, przesyłane do chmury Azure przez protokół MQTT, przechowywane w bazie danych i wizualizowane w czasie rzeczywistym z systemem automatycznego alertowania.

## Główne funkcje
- Zbieranie danych z czujników AHT10 (wewnątrz) i AHT20+BMP280 (zewnątrz) przez dwie niezależne magistrale I2C
- Przesyłanie danych w formacie JSON do Azure IoT Hub przez protokół MQTT z szyfrowaniem TLS
- Automatyczny zapis każdego pomiaru do Azure Table Storage przez Azure Functions (trigger IoT Hub)
- REST API (Azure Functions HTTP trigger) udostępniające dane historyczne z filtrowaniem po czasie
- Dashboard wizualizacji w Grafana Cloud z wykresami temperatury, wilgotności i ciśnienia w czasie
- System alertowania email — automatyczne powiadomienia gdy wartości czujników przekraczają zdefiniowane progi

## Cele projektowe (priorytety)

1. **Architektura IoT end-to-end** — zaprojektowanie i wdrożenie kompletnego systemu łączącego warstwę sprzętową (ESP32, czujniki I2C) z chmurą Azure przez protokół MQTT, z synchronizacją czasu NTP i generowaniem tokenów SAS do uwierzytelniania

2. **Serverless w praktyce** — wykorzystanie Azure Functions w planie Consumption jako bezserwerowej warstwy przetwarzania — IoT Hub trigger do zapisu danych oraz HTTP trigger jako REST API dla warstwy wizualizacji

3. **Wizualizacja danych szeregów czasowych** — budowa dashboardu w Grafana Cloud z danymi pobieranymi z Azure Table Storage przez REST API (plugin Infinity), z wykresami porównawczymi dwóch środowisk w różnych przedziałach czasowych

4. **System alertowania oparty na progach** — automatyczne powiadomienia email gdy temperatura zewnętrzna przekracza 35°C lub spada poniżej 0°C, wilgotność zewnętrzna przekracza 85%, temperatura wewnętrzna spada poniżej 10°C lub różnica temperatur między środowiskami przekracza 15°C

## Technologie

**Warstwa sprzętowa:**
- ESP32 DEVKITV1 — mikrokontroler z WiFi
- Czujnik AHT10 — temperatura i wilgotność (wewnątrz)
- Czujnik AHT20 + BMP280 — temperatura, wilgotność i ciśnienie (zewnątrz)
- Dwie niezależne magistrale I2C (GPIO21/22 i GPIO16/17)

**Warstwa komunikacji:**
- Protokół MQTT z szyfrowaniem TLS (port 8883)
- Format danych JSON z timestampem NTP
- Uwierzytelnianie przez tokeny SAS generowane na ESP32

**Warstwa chmurowa (Azure):**
- Azure IoT Hub B1 — brama IoT dla urządzeń
- Azure Functions (Consumption/Serverless) — przetwarzanie i API
- Azure Table Storage — przechowywanie danych szeregów czasowych

**Warstwa wizualizacji i alertowania:**
- Grafana Cloud (Free tier) — dashboard z wykresami
- Grafana Alerting — system powiadomień email

## Architektura systemu

```
ESP32 + Czujniki
    │
    │ MQTT/TLS
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

## Reguły alertów

| Alert | Warunek | Próg |
|---|---|---|
| Wysoka temperatura zewnętrzna | outdoor_temperature > | 35°C |
| Temperatura poniżej zera | outdoor_temperature < | 0°C |
| Niska temperatura wewnętrzna | indoor_temperature < | 10°C |
| Wysoka wilgotność zewnętrzna | outdoor_humidity > | 85% |
| Duża różnica temperatur | delta_temperature > | 15°C |
