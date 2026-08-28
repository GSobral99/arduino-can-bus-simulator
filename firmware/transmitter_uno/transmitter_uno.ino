/*
 * CAN Bus Simulator - Transmitter Node (Arduino Uno)
 * ----------------------------------------------------
 * Sends two simulated ECU signals onto the CAN bus:
 *   - Engine temperature (ID 0x050, high priority)
 *   - Engine RPM         (ID 0x100, low priority)
 *
 * Because 0x050 is numerically lower than 0x100, it wins bus
 * arbitration whenever both frames are ready to send at the same
 * time - this mirrors how a real vehicle prioritizes safety-critical
 * signals (e.g. temperature/braking) over less urgent ones (e.g. RPM).
 *
 * Hardware: Arduino Uno + MCP2515 CAN controller (SPI), 500 kbps bus.
 */

#include <SPI.h>
#include <mcp_can.h>

// Chip Select pin used to talk to the MCP2515 over SPI
const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);

// Simulated sensor values, updated every loop iteration
unsigned long rpm = 800;      // engine RPM, ranges roughly 800-6000
byte temperature = 85;        // engine temperature in Celsius, ranges 85-110

void setup() {
  Serial.begin(115200);

  // Initialize the MCP2515:
  //  - MCP_ANY: accept any operating mode during init
  //  - CAN_500KBPS: bus speed, must match the receiver node
  //  - MCP_8MHZ: crystal frequency on the module (some use 16MHz)
  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    Serial.println("CAN init failed, retrying...");
    delay(500);
  }

  // Switch from configuration mode to normal operation
  CAN.setMode(MCP_NORMAL);
  Serial.println("CAN Transmitter (Multi-ID) started");
}

void loop() {
  // Update simulated sensor readings
  rpm += 200;
  if (rpm > 6000) rpm = 800;   // wrap back to idle RPM

  temperature++;
  if (temperature > 110) temperature = 85;  // wrap back to a normal operating temperature

  // Frame 1: RPM (ID 0x100, low priority)
  // RPM needs 2 bytes, so split it into high byte and low byte
  byte dataRpm[2] = {
    (byte)((rpm >> 8) & 0xFF),  // high byte
    (byte)(rpm & 0xFF)          // low byte
  };
  CAN.sendMsgBuf(0x100, 0, 2, dataRpm);

  // Frame 2: Temperature (ID 0x050, high priority)
  // Temperature fits in a single byte (0-110 range used here)
  byte dataTemp[1] = { temperature };
  CAN.sendMsgBuf(0x050, 0, 1, dataTemp);

  Serial.print("Sent -> Temp: ");
  Serial.print(temperature);
  Serial.print(" C | RPM: ");
  Serial.println(rpm);

  delay(500); // delay so that the Serial Monitor output is readable
}
