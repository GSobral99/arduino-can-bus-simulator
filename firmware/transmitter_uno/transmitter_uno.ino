/*
 * CAN Bus Simulator - Transmitter Node (Arduino Uno) - Stage 2
 * ----------------------------------------------------
 * Reads three real sensors and sends them onto the CAN bus as three
 * separate ECU-style signals:
 *
 *   ID 0x010 - Brake/emergency alert (HIGHEST priority, event-driven)
 *              Source: push button. Only sent while the button is held.
 *
 *   ID 0x050 - Engine temperature (medium priority, periodic)
 *              Source: LDR (light sensor) via a voltage divider.
 *
 *   ID 0x100 - Engine RPM (LOWEST priority, periodic)
 *              Source: potentiometer.
 *
 * Because 0x010 < 0x050 < 0x100 numerically, the brake alert always
 * wins bus arbitration over the periodic sensor readings.
 *
 * Hardware: Arduino Uno + MCP2515 CAN controller (SPI), 500 kbps bus.
 */

#include <SPI.h>
#include <mcp_can.h>

// --- CAN controller ---
const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);

// Sensor pins
const int POT_PIN = A0;     // potentiometer wiper -> simulates RPM
const int LDR_PIN = A1;     // LDR voltage divider  -> simulates temperature
const int BUTTON_PIN = 3;   // push button          -> simulates brake alert

// Output ranges
const unsigned int RPM_MIN = 800;
const unsigned int RPM_MAX = 6000;
const byte TEMP_MIN = 85;
const byte TEMP_MAX = 110;

// How often periodic sensor frames (temperature, RPM) are sent
const unsigned long SENSOR_SEND_INTERVAL_MS = 200;
unsigned long lastSensorSendAt = 0;

void setup() {
  Serial.begin(115200);

  pinMode(BUTTON_PIN, INPUT_PULLUP); // no external resistor needed

  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    Serial.println("CAN init failed, retrying...");
    delay(500);
  }
  CAN.setMode(MCP_NORMAL);
  Serial.println("CAN Transmitter (Stage 2 - real sensors) started");
}

void loop() {
  // Event-driven frame: brake alert
  bool brakePressed = (digitalRead(BUTTON_PIN) == LOW);

  if (brakePressed) {
    byte alertData[1] = { 0x01 };
    CAN.sendMsgBuf(0x010, 0, 1, alertData);
    Serial.println("Sent -> BRAKE ALERT [0x010]");
    delay(100); // avoid flooding the bus while held
  }

  //Periodic frames: temperature and RPM
  if (millis() - lastSensorSendAt >= SENSOR_SEND_INTERVAL_MS) {
    lastSensorSendAt = millis();

    int potRaw = analogRead(POT_PIN);
    unsigned int rpm = map(potRaw, 0, 1023, RPM_MIN, RPM_MAX);

    int ldrRaw = analogRead(LDR_PIN);
    byte temperature = map(ldrRaw, 0, 1023, TEMP_MIN, TEMP_MAX);

    byte dataRpm[2] = {
      (byte)((rpm >> 8) & 0xFF),
      (byte)(rpm & 0xFF)
    };
    CAN.sendMsgBuf(0x100, 0, 2, dataRpm);

    byte dataTemp[1] = { temperature };
    CAN.sendMsgBuf(0x050, 0, 1, dataTemp);

    Serial.print("Sent -> Temp: ");
    Serial.print(temperature);
    Serial.print(" C | RPM: ");
    Serial.println(rpm);
  }
}