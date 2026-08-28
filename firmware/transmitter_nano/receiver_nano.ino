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
 * FAULT RECOVERY (Bus-Off / corrupted-signal watchdog):
 * ----------------------------------------------------
 * Removing the 120-ohm termination resistor while the bus was running
 * was tested and produced two different failure modes, depending on
 * the exact fault:
 *
 *   1) Silence: the MCP2515 stops delivering any frames at all (a
 *      classic Bus-Off state per ISO 11898). Restoring the physical
 *      fault alone does not resume communication - the controller's
 *      error counters need to be reset via CAN.begin().
 *
 *   2) Noise flood: the degraded differential signal is misread as a
 *      storm of malformed/duplicate frames (e.g. implausible RPM
 *      values, or the same value repeated many times within a few
 *      milliseconds). In this case the controller is technically still
 *      "receiving", so a pure silence-timeout watchdog never triggers.
 *
 * To catch both cases, this firmware combines two checks:
 *   - a timeout-based watchdog (case 1), and
 *   - a plausibility check on decoded values plus a "too many bad
 *     frames in a row" counter (case 2).
 * Either one reinitializes the MCP2515 automatically, the same way a
 * production ECU would recover on its own without a technician
 * plugging in a laptop.
 *
 * Hardware: Arduino Nano + MCP2515 CAN controller (SPI), 500 kbps bus.
 */

#include <SPI.h>
#include <mcp_can.h>

// Chip Select pin used to talk to the MCP2515 over SPI
const int SPI_CS_PIN = 10;
MCP_CAN CAN(SPI_CS_PIN);

// --- Watchdog #1: silence timeout ---
// If no valid frame arrives within this window, assume the controller
// is stuck (e.g. Bus-Off) and force a reinitialization. The transmitter
// sends roughly every 500ms in normal operation, so 3000ms gives
// plenty of margin before treating it as a fault.
const unsigned long FRAME_TIMEOUT_MS = 3000;
unsigned long lastValidFrameAt = 0;

// --- Watchdog #2: implausible-value flood ---
// Valid ranges the transmitter can actually produce (see transmitter_uno.ino).
// Anything outside these ranges must be noise, not a real signal.
const unsigned int RPM_MIN = 800;
const unsigned int RPM_MAX = 6000;
const byte TEMP_MIN = 85;
const byte TEMP_MAX = 110;

// If this many corrupted/out-of-range frames arrive in a row, treat it
// as a fault and reinitialize immediately (don't wait for the silence
// timeout, since the bus is clearly not silent in this failure mode).
const byte MAX_CONSECUTIVE_BAD_FRAMES = 5;
byte consecutiveBadFrames = 0;

void initCan() {
  // Initialize the MCP2515:
  //  - MCP_ANY: accept any operating mode during init
  //  - CAN_500KBPS: bus speed, must match the transmitter node
  //  - MCP_8MHZ: crystal frequency on the module (check yours - some use 16MHz)
  while (CAN_OK != CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)) {
    Serial.println("CAN init failed, retrying...");
    delay(500);
  }
  CAN.setMode(MCP_NORMAL);
}

void reinitializeAfterFault(const char* reason) {
  Serial.print("WATCHDOG: fault detected (");
  Serial.print(reason);
  Serial.println("), reinitializing MCP2515...");

  initCan();

  lastValidFrameAt = millis();
  consecutiveBadFrames = 0;

  Serial.println("WATCHDOG: MCP2515 reinitialized, resuming normal operation.");
}

void setup() {
  Serial.begin(115200);

  initCan();
  Serial.println("CAN Receiver (Multi-ID) started");

  // Start the watchdog clock now, so it doesn't immediately trigger
  // before the first frame has had a chance to arrive.
  lastValidFrameAt = millis();
}

void loop() {
  // Poll the MCP2515 to check whether a new frame has arrived.
  // (No interrupt pin is used here to keep the wiring minimal -
  // this is fine for a low-traffic bus like this simulation.)
  if (CAN_MSGAVAIL == CAN.checkReceive()) {
    long unsigned int rxId;   // identifier of the received frame
    byte len = 0;             // number of data bytes in the frame
    byte buf[8];               // frame payload (CAN frames carry up to 8 bytes)

    CAN.readMsgBuf(&rxId, &len, buf);

    bool frameIsValid = false;

    if (rxId == 0x050 && len >= 1) {
      byte temperature = buf[0];
      if (temperature >= TEMP_MIN && temperature <= TEMP_MAX) {
        frameIsValid = true;
        Serial.print("HIGH PRIORITY [0x050] | Temp: ");
        Serial.print(temperature);
        Serial.println(" C");
      }
    }
    else if (rxId == 0x100 && len >= 2) {
      unsigned int receivedRpm = (buf[0] << 8) | buf[1];
      if (receivedRpm >= RPM_MIN && receivedRpm <= RPM_MAX) {
        frameIsValid = true;
        Serial.print("LOW PRIORITY [0x100] | RPM: ");
        Serial.println(receivedRpm);
      }
    }
    // Frames with any other ID are silently ignored (not counted as bad)

    if (frameIsValid) {
      lastValidFrameAt = millis();
      consecutiveBadFrames = 0;
    } else if (rxId == 0x050 || rxId == 0x100) {
      // A frame with a known ID but an implausible/corrupted payload -
      // this is the "noise flood" failure mode.
      consecutiveBadFrames++;
      Serial.println("WATCHDOG: discarded out-of-range/corrupted frame.");

      if (consecutiveBadFrames >= MAX_CONSECUTIVE_BAD_FRAMES) {
        reinitializeAfterFault("too many corrupted frames in a row");
      }
    }
  }

  // --- Watchdog #1: detect a stuck/silent (Bus-Off) controller ---
  if (millis() - lastValidFrameAt > FRAME_TIMEOUT_MS) {
    reinitializeAfterFault("no valid frames received recently");
  }
}
