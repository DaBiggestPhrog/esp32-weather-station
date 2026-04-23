const { TableClient } = require("@azure/data-tables");

// Endpoint zwracający wyłącznie ostatni rekord.
// Używany przez Grafana Alerting — unika przekroczenia limitu 1000 rekordów
// narzuconego przez Grafana na zapytania alertów.

module.exports = async function(context, req) {
    context.res = {
        headers: {
            "Content-Type": "application/json",
            "Access-Control-Allow-Origin": "*",
            "Access-Control-Allow-Methods": "GET",
            "Access-Control-Allow-Headers": "Content-Type"
        }
    };

    try {
        const connectionString = process.env["AzureWebJobsStorage"];
        const tableClient = TableClient.fromConnectionString(
            connectionString, "sensordata"
        );

        const records = [];
        const entities = tableClient.listEntities({
            queryOptions: {
                filter: `PartitionKey eq 'esp32-sensor-01'`
            }
        });

        for await (const entity of entities) {
            records.push({
                timestamp:           entity.timestamp,
                indoor_temperature:  parseFloat(entity.indoor_temperature)  || null,
                indoor_humidity:     parseFloat(entity.indoor_humidity)     || null,
                outdoor_temperature: parseFloat(entity.outdoor_temperature) || null,
                outdoor_humidity:    parseFloat(entity.outdoor_humidity)    || null,
                outdoor_pressure:    parseFloat(entity.outdoor_pressure)    || null,
                outdoor_altitude:    parseFloat(entity.outdoor_altitude)    || null,
                delta_temperature:   parseFloat(entity.delta_temperature)   || null,
                delta_humidity:      parseFloat(entity.delta_humidity)      || null,
            });
        }

        records.sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp));
        const latest = records[0] || null;

        context.res.status = 200;
        context.res.body   = JSON.stringify(latest ? [latest] : []);

    } catch (err) {
        context.res.status = 500;
        context.res.body   = JSON.stringify({ error: err.message });
    }
};
