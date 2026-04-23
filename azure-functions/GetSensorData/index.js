const { TableClient } = require("@azure/data-tables");

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
        const tableClient = TableClient.fromConnectionString(connectionString, "sensordata");

        // Parametr hours — domyślnie zwraca wszystkie dane
        // Przykład użycia: /api/GetSensorData?hours=24
        const hours = parseInt(req.query.hours) || 99999;
        const cutoff = new Date();
        cutoff.setHours(cutoff.getHours() - hours);

        const records = [];

        const entities = tableClient.listEntities();
        for await (const entity of entities) {
            const ts = new Date(entity.timestamp);
            if (ts >= cutoff) {
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
        }

        // Sortuj po czasie rosnąco
        records.sort((a, b) => new Date(a.timestamp) - new Date(b.timestamp));

        context.res.status = 200;
        context.res.body   = JSON.stringify(records);

    } catch (err) {
        context.res.status = 500;
        context.res.body   = JSON.stringify({ error: err.message });
    }
};
