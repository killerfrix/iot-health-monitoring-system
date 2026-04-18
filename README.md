# IoT Health Monitoring System

An end-to-end IoT system for real-time health monitoring using ESP32, WebSockets, and PostgreSQL.

## 🚀 Features

- Real-time heart rate (BPM) monitoring
- SpO2 (oxygen saturation) measurement
- Fall detection using MPU6050
- Live data streaming via WebSockets
- Backend data processing and storage
- Integration with a Kotlin mobile client

## 🏗️ Architecture

ESP32 (Sensors)
↓
WebSocket
↓
Node.js Server (Express + WS)
↓
PostgreSQL Database
↓
Client Apps (Kotlin / Web)

## 🧠 Technologies

- ESP32 (C++)
- Node.js (Express + WebSocket)
- PostgreSQL
- Kotlin (client visualization)
- Sensors:
  - MAX30102 (Heart rate & SpO2)
  - MPU6050 (Fall detection)

## 📡 Data Flow

- ESP32 sends:
  - Beat detection (1)
  - SpO2 values
  - Fall events
  - Final averaged results
- Server:
  - Broadcasts real-time data
  - Stores final results in PostgreSQL
  - Stores fall events

## ⚙️ Setup

### Backend

```bash
npm install
node server.js
```

Database

```bash
Create PostgreSQL database:

CREATE DATABASE sensor_data;
```

Tables:

```bash
CREATE TABLE measurements (
  id SERIAL PRIMARY KEY,
  pulse INT,
  spo2 INT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE fall_records (
  id SERIAL PRIMARY KEY,
  fall INT,
  detected_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

📊 Example Messages
{ "beat": 1 }
{ "spo2": 97 }
{ "fall": 1 }
{ "final_bpm": 72, "final_spo2": 98 }


👨‍💻 Author

Jose Torres
