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

// Test payloads pour LDA2 selon la documentation
// Uplink: 4, 8, 12, 16, 20, 24 bytes
// Pour ACK et Downlink REQUEST, on utilise les mêmes tailles que l'uplink

// Toutes les payloads (pour uplink, ack request, downlink request)
const char* ALL_PAYLOADS[] = {
  "A1B2C3D4",                          // 4 bytes
  "A1B2C3D4A1B2C3D4",                  // 8 bytes
  "A1B2C3D4A1B2C3D4A1B2C3D4",          // 12 bytes
  "A1B2C3D4A1B2C3D4A1B2C3D4A1B2C3D4",  // 16 bytes
  "A1B2C3D4A1B2C3D4A1B2C3D4A1B2C3D4A1B2C3D4", // 20 bytes
  "A1B2C3D4A1B2C3D4A1B2C3D4A1B2C3D4A1B2C3D4A1B2C3D4" // 24 bytes
};

const int NUM_TESTS = 6;

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
  Serial.println("5 = Test toutes les tailles LDA2");
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

  Serial.println("\nPret. Tapez 1 / 2 / 3 / 4 / 5");
}

void loop() {
  // Lecture clavier PC
  if (Serial.available()) {
    char c = Serial.read();

    if (c == '1') testUART();
    if (c == '2') testUplink();
    if (c == '3') testAck();
    if (c == '4') testDownlink();
    if (c == '5') testAllSizesLDA2();
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

void testAllSizesLDA2() {
  Serial.println("\n[TEST 5] TOUTES LES TAILLES LDA2");
  Serial.println("=================================");
  
  // Test Uplink (0x00)
  Serial.println("\n--- UPLINK (0x00) ---");
  for (int i = 0; i < NUM_TESTS; i++) {
    Serial.print("\nTest Uplink ");
    Serial.print((i+1)*4); // 4, 8, 12, 16, 20, 24
    Serial.println(" bytes");
    
    // Envoi de la commande
    Serial.print("[AT>>] AT+TX=");
    Serial.print(ALL_PAYLOADS[i]);
    Serial.println(",00");
    
    Serial1.print("AT+TX=");
    Serial1.print(ALL_PAYLOADS[i]);
    Serial1.println(",00");
    
    // Attente et lecture des réponses
    Serial.println("[INFO] Attente cycle radio...");
    waitForResponse(TX_DELAY);
  }
  
  // Test Ack (0x04) - MÊMES TAILLES QUE UPLINK
  Serial.println("\n--- ACK REQUEST (0x04) ---");
  for (int i = 0; i < NUM_TESTS; i++) {
    Serial.print("\nTest Ack Request ");
    Serial.print((i+1)*4); // 4, 8, 12, 16, 20, 24
    Serial.println(" bytes");
    
    // Envoi de la commande
    Serial.print("[AT>>] AT+TX=");
    Serial.print(ALL_PAYLOADS[i]);
    Serial.println(",04");
    
    Serial1.print("AT+TX=");
    Serial1.print(ALL_PAYLOADS[i]);
    Serial1.println(",04");
    
    // Attente et lecture des réponses
    Serial.println("[INFO] Attente cycle radio...");
    waitForResponse(TX_DELAY);
  }
  
  // Test Downlink request (0x01) - MÊMES TAILLES QUE UPLINK
  Serial.println("\n--- DOWNLINK REQUEST (0x01) ---");
  for (int i = 0; i < NUM_TESTS; i++) {
    Serial.print("\nTest Downlink Request ");
    Serial.print((i+1)*4); // 4, 8, 12, 16, 20, 24
    Serial.println(" bytes");
    
    // Envoi de la commande
    Serial.print("[AT>>] AT+TX=");
    Serial.print(ALL_PAYLOADS[i]);
    Serial.println(",01");
    
    Serial1.print("AT+TX=");
    Serial1.print(ALL_PAYLOADS[i]);
    Serial1.println(",01");
    
    // Attente et lecture des réponses
    Serial.println("[INFO] Attente cycle radio...");
    waitForResponse(TX_DELAY);
  }
  
  // Tests supplémentaires : Vérification des tailles invalides
  Serial.println("\n--- TESTS TAILLES INVALIDES (pour vérification) ---");
  
  // Test avec taille 2 bytes (devrait échouer pour uplink)
  Serial.println("\nTest Uplink 2 bytes (invalide pour LDA2 uplink)");
  sendTX("A1B2", "00");
  waitForResponse(TX_DELAY);
  
  // Test avec taille 22 bytes (devrait échouer pour uplink)
  Serial.println("\nTest Uplink 22 bytes (invalide pour LDA2 uplink)");
  sendTX("A1B2C3D4A1B2C3D4A1B2C3D4A1B2C3D4A1B2C3D4A1B2", "00");
  waitForResponse(TX_DELAY);
  
  Serial.println("\n[INFO] Tous les tests de tailles LDA2 sont terminés!");
  Serial.println("[INFO] Résumé :");
  Serial.println("[INFO] - Uplink (0x00) : 4, 8, 12, 16, 20, 24 bytes");
  Serial.println("[INFO] - Ack Request (0x04) : mêmes tailles que uplink");
  Serial.println("[INFO] - Downlink Request (0x01) : mêmes tailles que uplink");
  Serial.println("[INFO] - ACK reçu / Downlink reçu : 2, 6, 10, 14, 18, 22 bytes");
}

// ==================================================
// COMMUNICATION
// ==================================================

void sendCmd(const char* cmd) {
  Serial.print("[AT>>] ");
  Serial.println(cmd);
  Serial1.println(cmd);
  waitForResponse(1200);
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
  waitForResponse(TX_DELAY);
}

// Nouvelle fonction pour attendre et lire les réponses
void waitForResponse(unsigned long waitTime) {
  unsigned long startTime = millis();
  
  while (millis() - startTime < waitTime) {
    readKIM2();
    delay(10); // Petite pause pour éviter de bloquer
  }
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
    // Affiche le code d'erreur si disponible
    int errorCode = msg.substring(7).toInt();
    Serial.print("Code erreur: ");
    Serial.println(errorCode);
    
    // Interprétation des codes d'erreur courants
    switch(errorCode) {
      case 5:
        Serial.println("Interpretation: Parametre invalide (longueur de donnees incorrecte)");
        break;
      case 4:
        Serial.println("Interpretation: Radio occupee");
        break;
      case 3:
        Serial.println("Interpretation: Mode radio invalide");
        break;
      case 2:
        Serial.println("Interpretation: Commande inconnue");
        break;
      case 1:
        Serial.println("Interpretation: Mauvaise syntaxe");
        break;
      default:
        Serial.println("Interpretation: Erreur inconnue");
    }
  }
}