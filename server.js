const express = require('express');
const WebSocket = require('ws');
const { Pool } = require('pg');

const app = express();
const PORT = 8081; // Set your port here
const pool = new Pool({
    user: 'postgres',
    host: 'localhost',
    database: 'sensor_data',
    password: 'adminadmin',
    port: 5432,
});

app.get('/start-readings', (req, res) => {
    // This message will be sent to the ESP32 via WebSocket
    const message = JSON.stringify({ action: 'start_readings' });

    // Broadcast the start reading action to ESP32
    broadcast(message);

    console.log('Sent start_readings message to ESP32');
    res.json({ success: true, message: 'Started readings' });
});


// Serve the REST API to get the fall data
app.get('/get_falls', async (req, res) => {
    try {
        const result = await pool.query('SELECT * FROM fall_records ORDER BY detected_at DESC');
        res.json(result.rows); // Send the result rows as JSON
    } catch (error) {
        console.error('Error fetching fall data:', error);
        res.status(500).json({ error: 'Database error' });
    }
});

// Start the Express server
const server = app.listen(PORT, () => {
    console.log(`Express server listening on port ${PORT}`);
});

// Create WebSocket server using the same HTTP server
const wss = new WebSocket.Server({ server });

const broadcast = (data) => {
    wss.clients.forEach(client => {
        if (client.readyState === WebSocket.OPEN) {
            client.send(JSON.stringify(data));
        } else {
            console.warn('Attempted to send message to a disconnected client');
        }
    });
};

wss.on('connection', (ws) => {
    console.log('New client connected');

    ws.on('message', async (message) => {
        console.log(`Message received: ${message}`);
        try {
            const data = JSON.parse(message); // Parse the JSON string

            // Handle incoming data as in your original code
            if (data.final_bpm !== undefined && data.final_spo2 !== undefined) {
                const { final_bpm, final_spo2 } = data;

                const query = 'INSERT INTO measurements (pulse, spo2) VALUES ($1, $2) RETURNING *';
                const values = [final_bpm, final_spo2];
                const res = await pool.query(query, values);
                console.log('Final data saved to database:', res.rows[0]);

                broadcast({ final_bpm, final_spo2 });

            } else if (data.beat !== undefined) {
                console.log(`Beat message received: ${data.beat}`);
                broadcast({ beat: data.beat });

            } else if (data.spo2 !== undefined) {
                console.log(`SpO2 message received: ${data.spo2}`);
                broadcast({ spo2: data.spo2 });

            } else if (data.fall !== undefined) {
                console.log(`Fall detected: ${data.fall}`);
                const fallQuery = 'INSERT INTO fall_records (fall) VALUES ($1) RETURNING *';
                const fallValues = [data.fall];
                const fallRes = await pool.query(fallQuery, fallValues);
                console.log('Fall data saved to database:', fallRes.rows[0]);
                broadcast({ fall: true });
            } else {
                console.log(`Received data does not match any expected format: ${message}`);
            }
        } catch (err) {
            console.error('Error processing message:', err);
        }
    });

    ws.on('close', () => {
        console.log('Client disconnected');
    });
});
