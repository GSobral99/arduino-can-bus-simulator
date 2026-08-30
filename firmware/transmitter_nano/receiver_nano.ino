/*
 * CAN Bus Simulator - Receiver Node (Arduino Nano) - Stage 2
 * ----------------------------------------------------
 * Decodes:
 *   - ID 0x010: brake/emergency alert (event-driven, highest priority)
 *   - ID 0x050: engine temperature (from LDR)
 *   - ID 0x100: engine RPM (from potentiometer)
 *
 * Hardware: Arduino Nano + MCP2515 CAN controller (SPI), 500 kbps bus.
 */

#include <SPI.h>
#include <mcp_can.h>

const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);

void setup() {
  Serial.begin(115200);

  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    Serial.println("CAN init failed, retrying...");
    delay(500);
  }
  CAN.setMode(MCP_NORMAL);
  Serial.println("CAN Receiver (Stage 2 - real sensors) started");
}

void loop() {
  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    long unsigned int rxId;
    byte len = 0;
    byte buf[8];

    CAN.readMsgBuf(&rxId, &len, buf);

    if (rxId == 0x010) {
      Serial.println("!!! BRAKE ALERT [0x010] !!!");
    }
    else if (rxId == 0x050) {
      Serial.print("MEDIUM PRIORITY [0x050] | Temp: ");
      Serial.print(buf[0]);
      Serial.println(" C");
    }
    else if (rxId == 0x100) {
      unsigned int receivedRpm = (buf[0] << 8) | buf[1];
      Serial.print("LOW PRIORITY [0x100] | RPM: ");
      Serial.println(receivedRpm);
    }
  }
}