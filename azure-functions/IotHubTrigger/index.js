const { TableClient } = require("@azure/data-tables");

module.exports = async function(context, IoTHubMessages) {
    context.log("Odebrano wiadomosci z IoT Hub:", IoTHubMessages.length);

    const connectionString = process.env["AzureWebJobsStorage"];
    const tableName = "sensordata";

    const tableClient = TableClient.fromConnectionString(connectionString, tableName);

    // Utwórz tabelę jeśli nie istnieje
    try {
        await tableClient.createTable();
    } catch (e) {
        // Tabela już istnieje — ignoruj błąd
    }

    // Zapisz każdą wiadomość do tabeli
    for (const message of IoTHubMessages) {
        try {
            const data = typeof message === "string" ? JSON.parse(message) : message;

            const timestamp = new Date().toISOString();
            const rowKey    = Date.now().toString();

            const entity = {
                partitionKey: data.deviceId || "esp32-sensor-01",
                rowKey:       rowKey,
                timestamp:    timestamp,
                // Indoor — AHT10
                indoor_temperature: data.indoor?.temperature_c ?? null,
                indoor_humidity:    data.indoor?.humidity_pct  ?? null,
                // Outdoor — AHT20 + BMP280
                outdoor_temperature: data.outdoor?.temperature_c ?? null,
                outdoor_humidity:    data.outdoor?.humidity_pct  ?? null,
                outdoor_pressure:    data.outdoor?.pressure_hpa  ?? null,
                outdoor_altitude:    data.outdoor?.altitude_m    ?? null,
                // Delta
                delta_temperature: data.delta?.temperature_c ?? null,
                delta_humidity:    data.delta?.humidity_pct  ?? null,
            };

            await tableClient.createEntity(entity);
            context.log("Zapisano rekord:", rowKey);

        } catch (err) {
            context.log.error("Blad zapisu:", err.message);
        }
    }
};
