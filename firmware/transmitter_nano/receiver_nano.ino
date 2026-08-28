/*
 * CAN Bus Simulator - Receiver Node (Arduino Nano)
 * ----------------------------------------------------
 * Listens to the CAN bus and decodes incoming frames based on their ID:
 *   - ID 0x050: engine temperature (1 byte payload)
 *   - ID 0x100: engine RPM (2 byte payload, big-endian)
 *
 * Any other ID received is currently ignored - in a real ECU this is
 * where you would add a hardware or software filter/mask so the
 * controller only wakes up for IDs it actually cares about.
 *
 * Hardware: Arduino Nano + MCP2515 CAN controller (SPI), 500 kbps bus.
 */

#include <SPI.h>
#include <mcp_can.h>

// Chip Select pin used to talk to the MCP2515 over SPI
const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);

void setup() {
  Serial.begin(115200);

  // Initialize the MCP2515:
  //  - MCP_ANY: accept any operating mode during init
  //  - CAN_500KBPS: bus speed, must match the transmitter node
  //  - MCP_8MHZ: crystal frequency on the module (some use 16MHz)
  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    Serial.println("CAN init failed, retrying...");
    delay(500);
  }

  // Switch from configuration mode to normal operation
  CAN.setMode(MCP_NORMAL);
  Serial.println("CAN Receiver (Multi-ID) started");
}

void loop() {
  // Poll the MCP2515 to check whether a new frame has arrived.
  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    long unsigned int rxId;   // identifier of the received frame
    byte len = 0;             // number of data bytes in the frame
    byte buf[8];               // frame payload (CAN frames carry up to 8 bytes)

    CAN.readMsgBuf(&rxId, &len, buf);

    if (rxId == 0x050) {
      // Temperature: single byte
      Serial.print("HIGH PRIORITY [0x050] | Temp: ");
      Serial.print(buf[0]);
      Serial.println(" C");
    }
    else if (rxId == 0x100) {
      // RPM: two bytes, reassemble as big-endian (high byte first)
      unsigned int receivedRpm = (buf[0] << 8) | buf[1];
      Serial.print("LOW PRIORITY [0x100] | RPM: ");
      Serial.println(receivedRpm);
    }
    // Frames with any other ID are silently ignored
  }
}
