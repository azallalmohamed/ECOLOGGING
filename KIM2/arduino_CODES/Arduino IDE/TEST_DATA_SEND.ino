// ==================================================
// KIM2 – TEST FINAL COMMUNICATION SATELLITE
// Arduino MEGA / LDA2 / Kinéis
// ==================================================

#define KIM2_POWER_PIN 4
#define STARTUP_DELAY 8000
#define TX_DELAY      7000   // délai cycle radio 

// Radio configuration Kinéis (LDA2)
const char* RADIO_CONFIG =
  "AT+RCONF=3d678af16b5a572078f3dbc95a1104e7";

// Payload test (4 bytes)
const char* DATA_TEST = "A1B2C3D4";

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);

  pinMode(KIM2_POWER_PIN, OUTPUT);

  Serial.println("\n==============================");
  Serial.println(" KIM2 – TEST SATELLITE ");
  Serial.println("==============================");
  Serial.println("1 = Test UART (PING)");
  Serial.println("2 = TX Uplink simple");
  Serial.println("3 = TX + ACK satellite");
  Serial.println("4 = TX + Downlink request");
  Serial.println();

  // -----------------------------
  // Power ON
  // -----------------------------
  Serial.println("[POWER] ON KIM2...");
  digitalWrite(KIM2_POWER_PIN, HIGH);
  delay(STARTUP_DELAY);
  Serial.println("[POWER] KIM2 READY");

  // -----------------------------
  // Initialisation
  // -----------------------------
  Serial.println("\n[INIT] Initialisation...");
  sendCmd("AT+PING=?");
  sendCmd(RADIO_CONFIG);
  sendCmd("AT+RCONF=?");
  sendCmd("AT+KMAC=1");
  Serial.println("[INIT] OK");

  Serial.println("\nPret. Tapez 1 / 2 / 3 / 4");
}

void loop() {
  // Lecture clavier PC
  if (Serial.available()) {
    char c = Serial.read();

    if (c == '1') testUART();
    if (c == '2') testUplink();
    if (c == '3') testAck();
    if (c == '4') testDownlink();
  }

  readKIM2();
}

// ==================================================
// TESTS
// ==================================================

void testUART() {
  Serial.println("\n[TEST 1] UART / PING");
  sendCmd("AT+PING=?");
}

void testUplink() {
  Serial.println("\n[TEST 2] UPLINK SIMPLE");
  sendTX(DATA_TEST, "00");
}

void testAck() {
  Serial.println("\n[TEST 3] UPLINK + ACK SATELLITE");
  sendTX(DATA_TEST, "04");
}

void testDownlink() {
  Serial.println("\n[TEST 4] UPLINK + DOWNLINK");
  sendTX(DATA_TEST, "01");
}

// ==================================================
// COMMUNICATION
// ==================================================

void sendCmd(const char* cmd) {
  Serial.print("[AT>>] ");
  Serial.println(cmd);
  Serial1.println(cmd);
  delay(1200);
}

void sendTX(const char* data, const char* attr) {
  Serial.print("[AT>>] AT+TX=");
  Serial.print(data);
  Serial.print(",");
  Serial.println(attr);

  Serial1.print("AT+TX=");
  Serial1.print(data);
  Serial1.print(",");
  Serial1.println(attr);

  Serial.println("[INFO] Attente cycle radio...");
  delay(TX_DELAY);
}

void readKIM2() {
  static String line = "";

  while (Serial1.available()) {
    char c = Serial1.read();

    // filtrage caractères non ASCII
    if (c < 0x20 && c != '\n' && c != '\r') continue;

    if (c == '\n') {
      line.trim();
      if (line.length() > 0) parseKIM2(line);
      line = "";
    } else {
      line += c;
    }
  }
}

void parseKIM2(String msg) {
  Serial.print("[KIM2] ");
  Serial.println(msg);

  // Résultat transmission
  if (msg.startsWith("+TX=")) {
    Serial.println(">>> TX EFFECTUE <<<");
  }

  // Downlink
  if (msg.startsWith("+DL=") || msg.startsWith("+RX=")) {
    Serial.println(">>> DOWNLINK RECU <<<");
    Serial.print("Payload = ");
    Serial.println(msg.substring(msg.indexOf('=') + 1));
  }

  // Erreurs
  if (msg.startsWith("+ERROR")) {
    Serial.println(">>> ERREUR MODULE <<<");
  }
}
