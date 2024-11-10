  #include <Wire.h>
  #include <Adafruit_MPU6050.h>
  #include <MAX30105.h>
  #include "heartRate.h"
  #include "spo2_algorithm.h"
  #include <WebSocketsClient.h>
  #include <WiFi.h>

  // Red settings
  // const char* ssid = "6A-BIS-ISW";          
  // const char* password = "6AB1s1SW%#";

  const char* ssid = "FRIX";          
  const char* password = "amolapaloma7";

  // Variables WebSockets
  WebSocketsClient webSocket;
  const char* serverAddress = "192.168.0.115"; // IP de la Raspberry Pi
  // const char* serverAddress = "192.168.0.115"; // IP de la Raspberry Pi
  const uint16_t webSocketPort = 8081;

  // Custom I2C instances
  TwoWire I2C_MPU6050 = TwoWire(0);
  TwoWire I2C_MAX30102 = TwoWire(1);

  // Sensor instances
  Adafruit_MPU6050 mpu;
  MAX30105 particleSensor;

  // Constantes para medir frecuencia cardíaca y SpO2
  const byte RATE_SIZE = 4; 
  byte rates[RATE_SIZE]; 

  bool readingsStarted = false;
  unsigned long startTime = 0;

  // Variables para el cálculo de SpO2
  #define BUFFER_SIZE 100
  uint32_t irBuffer[BUFFER_SIZE]; 
  uint32_t redBuffer[BUFFER_SIZE];
  int32_t bufferLength = 100;
  int32_t spo2;
  int8_t validSPO2;
  int32_t heartRate;
  int8_t validHeartRate;
  #define MAX_VALID_SPO2_READINGS 100
  int32_t validSpo2Readings[MAX_VALID_SPO2_READINGS];
  int validReadingCount = 0;
  int32_t averageSpo2; 

  long lastBeat = 0;
  float beatsPerMinute = 0;
  byte rateSpot = 0;
  int beatAvg = 0;

  // Variables para detección de caídas
  long tiempo_prev;
  float dt;
  float ang_x, ang_y;
  float ang_x_prev = 0, ang_y_prev = 0;
  bool fallDetected = false;
  unsigned long startTimeFall = 0;

  // Función para reiniciar las lecturas de SpO2 y pulso
  void resetReadings() {
      startTime = 0;
      startTimeFall = 0;
      beatsPerMinute = 0;
      beatAvg = 0;
      rateSpot = 0;
      validReadingCount = 0;

      for (int i = 0; i < MAX_VALID_SPO2_READINGS; i++) {
          validSpo2Readings[i] = 0;
      }
      for (byte i = 0; i < RATE_SIZE; i++) {
          rates[i] = 0;
      }
  }

  void setup() {
      Serial.begin(115200);
      
      // Conexión WiFi
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
          delay(1000);
          Serial.println("Conectando a WiFi...");
      }
      Serial.println("Conectado a WiFi");

      // Conectar WebSocket
      webSocket.begin(serverAddress, webSocketPort, "/");
      webSocket.onEvent(webSocketEvent);

      // Inicializar MPU6050
      I2C_MPU6050.begin(19, 18, 400000);
      if (!mpu.begin(0x68, &I2C_MPU6050)) {
          Serial.println("Error iniciando MPU6050.");
          while (1);
      }
      Serial.println("MPU6050 iniciado.");

      // Inicializar MAX30102
      I2C_MAX30102.begin(22, 21, 400000);
      if (!particleSensor.begin(I2C_MAX30102, 400000)) {
          Serial.println("Error iniciando MAX30102.");
          while (1);
      }
      Serial.println("MAX30102 iniciado.");

      // Configurar MAX30102
      byte ledBrightness = 200;
      byte sampleAverage = 2;
      byte ledMode = 2;
      int sampleRate = 400;
      int pulseWidth = 411;
      int adcRange = 16384;
      particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

      Serial.println("Ingrese '1' para iniciar las lecturas.");
  }

  void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
    String message;  // Declare message outside of the switch

    switch(type) {
        case WStype_DISCONNECTED:
            Serial.println("WebSocket Desconectado");
            break;
        case WStype_CONNECTED:
            Serial.println("WebSocket Conectado");
            break;
        case WStype_TEXT:
            message = String((char*)payload);  // Assign the payload to the message inside the case
            Serial.printf("Mensaje WebSocket: %s\n", payload);

            // Check if the message contains "start_readings"
            if (message.indexOf("start_readings") != -1) {
                readingsStarted = true;
                resetReadings();
                Serial.println("Lecturas iniciadas por WebSocket...");
                startTime = millis();  // Start the timer for readings
            }
            break;
        case WStype_BIN:
            Serial.printf("Mensaje binario WebSocket: %d bytes\n", length);
            break;
    }
  }



  void loop() {
      webSocket.loop(); // Maintain WebSocket connection

      // MPU6050 - Continuously check for falls
      sensors_event_t a, g, temp;
      mpu.getEvent(&a, &g, &temp);

      float ax_m_s2 = a.acceleration.x;
      float ay_m_s2 = a.acceleration.y;
      float az_m_s2 = a.acceleration.z;
      float aTotal = sqrt(pow(ax_m_s2, 2) + pow(ay_m_s2, 2) + pow(az_m_s2, 2));

      // Check for fall detection
      if (aTotal <= 1.5 && millis() - startTimeFall >= 1000) {
          if (!fallDetected) { // Only send message if a fall is detected for the first time
              Serial.println("Caída detectada");
              fallDetected = true;
              sendFallMessage();
          }
      } else {
          // Reset fall detection if no fall is detected
          fallDetected = false;
      }

      // Process heartbeat and SpO2 readings if initiated
      if (readingsStarted) {
          bufferLength = 100; // Size of the buffer

          for (byte i = 0; i < bufferLength; i++) {
              while (!particleSensor.available()) {
                  particleSensor.check();
              }
              redBuffer[i] = particleSensor.getRed();
              irBuffer[i] = particleSensor.getIR();
              particleSensor.nextSample();

              int beatDetected = checkForBeat(irBuffer[i]) ? 1 : 0;

              if (beatDetected) {
                  sendBeatMessage(1);
                  long delta = millis() - lastBeat;
                  lastBeat = millis();
                  beatsPerMinute = 60 / (delta / 1000.0);
                  if (beatsPerMinute < 255 && beatsPerMinute > 20) {
                      rates[rateSpot++] = (byte)beatsPerMinute;
                      rateSpot %= RATE_SIZE;
                      beatAvg = 0;
                      for (byte x = 0; x < RATE_SIZE; x++) {
                          beatAvg += rates[x];
                      }
                      beatAvg /= RATE_SIZE;
                  }
              }

              if (i == bufferLength - 1) {
                  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);
                  if (validSPO2 == 1) {
                      validSpo2Readings[validReadingCount++] = spo2;
                      sendSpO2Message(spo2);
                  }
              }
          }

          if (millis() - startTime >= 60000) {  // 60 seconds elapsed
              readingsStarted = false;

              averageSpo2 = 0;
              if (validReadingCount > 0) {
                  for (int i = 0; i < validReadingCount; i++) {
                      averageSpo2 += validSpo2Readings[i];
                  }
                  averageSpo2 /= validReadingCount;
              } else {
                  averageSpo2 = -1;
              }

              Serial.print("Promedio final BPM: ");
              Serial.println(beatAvg);
              Serial.print("Promedio final SpO2: ");
              Serial.println(averageSpo2);

              sendFinalResults(beatAvg, averageSpo2);
              resetReadings();
          }
      }

      delay(10); // Pause for stability
  }


  void sendBeatMessage(int beatDetected) {
      // Construct the JSON message for the beat detection
      String message = String("{\"beat\": ") + String(beatDetected) + "}";
      webSocket.sendTXT(message);
  }

  void sendSpO2Message(int spO2Value) {
      // Construct the JSON message for the SpO2 value
      String message = String("{\"spo2\": ") + String(spO2Value) + "}";
      webSocket.sendTXT(message);
  }

  void sendFallMessage() {
      String message = String("{\"fall\": 1}");
      webSocket.sendTXT(message);
  }

  void sendFinalResults(int finalBPM, int finalSpO2) {
      // Construct the JSON message for final results with double quotes
      String resultMessage = String("{\"final_bpm\": ") + String(finalBPM) + ", \"final_spo2\": " + String(finalSpO2) + "}";
      webSocket.sendTXT(resultMessage);
  }

