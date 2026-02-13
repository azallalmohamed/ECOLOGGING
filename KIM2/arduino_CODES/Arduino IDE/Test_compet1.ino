// ==================================================
// STATION METEOROLOGIQUE AVEC TRANSMISSION SATELLITE
// Arduino Mega + Capteurs + SD + KIM2 (Kinéis)
// ==================================================

#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_VEML7700.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

// ---- Pins ----
#define SD_CS 53          // Pin CS carte SD
#define KIM2_POWER_PIN 4  // Pin power KIM2

// ---- Capteurs ----
Adafruit_BME280 bme;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
RTC_DS3231 rtc;

// ---- SD ----
File dataFile;

// ---- Timing ----
const unsigned long METEOROLOGIQUE_INTERVAL = 20000;      // 20 secondes
const unsigned long METEOROLOGIQUE_AVG_INTERVAL = 420000; // 7 minutes (420s)
const unsigned long SOL_INTERVAL = 60000;                 // 1 minute
const unsigned long SOL_AVG_INTERVAL = 600000;           // 10 minutes (600s)

unsigned long lastMeteorologiqueRead = 0;
unsigned long lastSolRead = 0;
unsigned long lastMeteorologiqueTx = 0;
unsigned long lastSolTx = 0;

// ---- Données pour moyennes ----
struct MeteorologiqueData {
  float tempSum = 0;
  float humSum = 0;
  float presSum = 0;
  float luxSum = 0;
  int count = 0;
} meteoData;

struct SolData {
  // À compléter quand vous aurez les capteurs sol
  // float waterSum = 0;
  // float tempSolSum = 0;
  // float humSolSum = 0;
  int count = 0;
} solData;

// ---- KIM2 Configuration ----
#define STARTUP_DELAY 8000
#define TX_DELAY 7000
const char* RADIO_CONFIG = "AT+RCONF=3d678af16b5a572078f3dbc95a1104e7";

// ---- Prototypes ----
void initSD();
void initSensors();
void initKIM2();
void readMeteorologique();
void readSol();
void calculateMeteorologiqueAvg();
void calculateSolAvg();
void sendDataToKIM2(const String& data, const char* attr = "00");
void sendCmd(const char* cmd);
void sendTX(const char* data, const char* attr);
void waitForResponse(unsigned long waitTime);
void readKIM2();
void parseKIM2(String msg);

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);  // Communication avec KIM2
  
  Wire.begin();
  
  Serial.println("\n==================================");
  Serial.println("  STATION METEOROLOGIQUE KINEIS  ");
  Serial.println("==================================");
  
  // Initialisation SD
  initSD();
  
  // Initialisation capteurs
  initSensors();
  
  // Initialisation KIM2
  initKIM2();
  
  Serial.println("\nSysteme pret!");
  Serial.println("Acquisition automatique active:");
  Serial.println("- Meteo: lecture 20s, moyenne/transmission 7min");
  Serial.println("- Sol: lecture 1min, moyenne/transmission 10min");
  Serial.println("==================================\n");
}

void loop() {
  unsigned long currentMillis = millis();
  
  // ---- Lecture données météorologiques (20s) ----
  if (currentMillis - lastMeteorologiqueRead >= METEOROLOGIQUE_INTERVAL) {
    readMeteorologique();
    lastMeteorologiqueRead = currentMillis;
  }
  
  // ---- Lecture données sol (1min) ----
  if (currentMillis - lastSolRead >= SOL_INTERVAL) {
    readSol();
    lastSolRead = currentMillis;
  }
  
  // ---- Calcul et transmission moyenne météo (7min) ----
  if (currentMillis - lastMeteorologiqueTx >= METEOROLOGIQUE_AVG_INTERVAL) {
    calculateMeteorologiqueAvg();
    lastMeteorologiqueTx = currentMillis;
  }
  
  // ---- Calcul et transmission moyenne sol (10min) ----
  if (currentMillis - lastSolTx >= SOL_AVG_INTERVAL) {
    calculateSolAvg();
    lastSolTx = currentMillis;
  }
  
  // ---- Lecture messages KIM2 ----
  readKIM2();
  
  // Petite pause pour éviter de saturer le CPU
  delay(10);
}

// ==================================================
// INITIALISATION
// ==================================================

void initSD() {
  Serial.print("Initialisation carte SD... ");
  if (!SD.begin(SD_CS)) {
    Serial.println("ECHEC !");
    while (1);
  }
  Serial.println("OK");
  
  // Créer fichier avec en-tête si nécessaire
  if (!SD.exists("meteo.csv")) {
    dataFile = SD.open("meteo.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Date,Heure,Temperature(C),Humidite(%),Pression(hPa),Lux");
      dataFile.close();
      Serial.println("Fichier meteo.csv cree");
    }
  }
  
  if (!SD.exists("sol.csv")) {
    dataFile = SD.open("sol.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Date,Heure,Water,TempSol,HumSol");
      dataFile.close();
      Serial.println("Fichier sol.csv cree");
    }
  }
}

void initSensors() {
  Serial.print("Initialisation BME280... ");
  if (!bme.begin(0x77)) {
    Serial.println("ECHEC !");
    while (1);
  }
  Serial.println("OK");
  
  Serial.print("Initialisation VEML7700... ");
  if (!veml.begin()) {
    Serial.println("ECHEC !");
    while (1);
  }
  Serial.println("OK");
  
  veml.setGain(VEML7700_GAIN_1);
  veml.setIntegrationTime(VEML7700_IT_100MS);
  
  Serial.print("Initialisation RTC DS3231... ");
  if (!rtc.begin()) {
    Serial.println("ECHEC !");
    while (1);
  }
  Serial.println("OK");
  
  if (rtc.lostPower()) {
    Serial.println("RTC a perdu l'heure, mise a jour...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void initKIM2() {
  Serial.print("Initialisation KIM2... ");
  
  pinMode(KIM2_POWER_PIN, OUTPUT);
  
  // Power ON KIM2
  digitalWrite(KIM2_POWER_PIN, HIGH);
  delay(STARTUP_DELAY);
  
  // Configuration radio LDA2
  sendCmd("AT+PING=?");
  sendCmd(RADIO_CONFIG);
  sendCmd("AT+RCONF=?");
  sendCmd("AT+KMAC=1");
  
  Serial.println("KIM2 PRET");
}

// ==================================================
// ACQUISITION DONNEES
// ==================================================

void readMeteorologique() {
  DateTime now = rtc.now();
  
  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;
  float lux = veml.readLux();
  
  // Ajout aux données pour moyenne
  meteoData.tempSum += temp;
  meteoData.humSum += hum;
  meteoData.presSum += pres;
  meteoData.luxSum += lux;
  meteoData.count++;
  
  // Affichage console
  Serial.print("[METE0 ");
  Serial.print(now.hour()); Serial.print(":");
  Serial.print(now.minute()); Serial.print(":");
  Serial.print(now.second());
  Serial.print("] T:");
  Serial.print(temp, 1);
  Serial.print("C H:");
  Serial.print(hum, 0);
  Serial.print("% P:");
  Serial.print(pres, 1);
  Serial.print("hPa L:");
  Serial.print(lux, 0);
  Serial.println("lux");
  
  // Sauvegarde SD
  dataFile = SD.open("meteo.csv", FILE_WRITE);
  if (dataFile) {
    dataFile.print(now.year()); dataFile.print("/");
    dataFile.print(now.month()); dataFile.print("/");
    dataFile.print(now.day()); dataFile.print(",");
    
    dataFile.print(now.hour()); dataFile.print(":");
    dataFile.print(now.minute()); dataFile.print(":");
    dataFile.print(now.second()); dataFile.print(",");
    
    dataFile.print(temp); dataFile.print(",");
    dataFile.print(hum); dataFile.print(",");
    dataFile.print(pres); dataFile.print(",");
    dataFile.println(lux);
    
    dataFile.close();
  }
}

void readSol() {
  DateTime now = rtc.now();
  
  // Ici vous ajouterez la lecture des capteurs sol quand vous les aurez
  // Pour l'instant, on simule des valeurs ou on laisse vide
  
  float water = 0.0;      // À remplacer avec capteur eau
  float tempSol = 0.0;    // À remplacer avec capteur température sol
  float humSol = 0.0;     // À remplacer avec capteur humidité sol
  
  // Ajout aux données pour moyenne
  // solData.waterSum += water;
  // solData.tempSolSum += tempSol;
  // solData.humSolSum += humSol;
  solData.count++;
  
  // Affichage console
  Serial.print("[SOL   ");
  Serial.print(now.hour()); Serial.print(":");
  Serial.print(now.minute()); Serial.print(":");
  Serial.print(now.second());
  Serial.print("] W:");
  Serial.print(water, 1);
  Serial.print(" TS:");
  Serial.print(tempSol, 1);
  Serial.print("C HS:");
  Serial.print(humSol, 0);
  Serial.println("%");
  
  // Sauvegarde SD
  dataFile = SD.open("sol.csv", FILE_WRITE);
  if (dataFile) {
    dataFile.print(now.year()); dataFile.print("/");
    dataFile.print(now.month()); dataFile.print("/");
    dataFile.print(now.day()); dataFile.print(",");
    
    dataFile.print(now.hour()); dataFile.print(":");
    dataFile.print(now.minute()); dataFile.print(":");
    dataFile.print(now.second()); dataFile.print(",");
    
    dataFile.print(water); dataFile.print(",");
    dataFile.print(tempSol); dataFile.print(",");
    dataFile.println(humSol);
    
    dataFile.close();
  }
}

// ==================================================
// CALCUL MOYENNES ET TRANSMISSION
// ==================================================

void calculateMeteorologiqueAvg() {
  if (meteoData.count == 0) return;
  
  DateTime now = rtc.now();
  
  // Calcul des moyennes
  float avgTemp = meteoData.tempSum / meteoData.count;
  float avgHum = meteoData.humSum / meteoData.count;
  float avgPres = meteoData.presSum / meteoData.count;
  float avgLux = meteoData.luxSum / meteoData.count;
  
  // Formatage des données pour transmission
  // Format: JJ/MM/AAAA/HH/MM/T/H/P/L
  String payload = "";
  payload += now.day(); payload += "/";
  payload += now.month(); payload += "/";
  payload += now.year(); payload += "/";
  payload += now.hour(); payload += "/";
  payload += now.minute(); payload += "/";
  payload += String(avgTemp, 1); payload += "/";
  payload += String(avgHum, 0); payload += "/";
  payload += String(avgPres, 1); payload += "/";
  payload += String(avgLux, 0);
  
  Serial.print("\n[METEO AVG 7min] ");
  Serial.println(payload);
  
  // Sauvegarde de la moyenne
  dataFile = SD.open("meteo_avg.csv", FILE_WRITE);
  if (dataFile) {
    if (dataFile.size() == 0) {
      dataFile.println("Date,Heure,AvgTemp,AvgHum,AvgPres,AvgLux");
    }
    
    dataFile.print(now.year()); dataFile.print("/");
    dataFile.print(now.month()); dataFile.print("/");
    dataFile.print(now.day()); dataFile.print(",");
    
    dataFile.print(now.hour()); dataFile.print(":");
    dataFile.print(now.minute()); dataFile.print(",");
    
    dataFile.print(avgTemp); dataFile.print(",");
    dataFile.print(avgHum); dataFile.print(",");
    dataFile.print(avgPres); dataFile.print(",");
    dataFile.println(avgLux);
    
    dataFile.close();
  }
  
  // Transmission via KIM2
  sendDataToKIM2(payload, "00");
  
  // Réinitialisation des données
  meteoData.tempSum = 0;
  meteoData.humSum = 0;
  meteoData.presSum = 0;
  meteoData.luxSum = 0;
  meteoData.count = 0;
}

void calculateSolAvg() {
  if (solData.count == 0) return;
  
  DateTime now = rtc.now();
  
  // Ici vous calculerez les moyennes des capteurs sol
  // float avgWater = solData.waterSum / solData.count;
  // float avgTempSol = solData.tempSolSum / solData.count;
  // float avgHumSol = solData.humSolSum / solData.count;
  
  float avgWater = 0.0;
  float avgTempSol = 0.0;
  float avgHumSol = 0.0;
  
  // Formatage des données pour transmission
  String payload = "";
  payload += now.day(); payload += "/";
  payload += now.month(); payload += "/";
  payload += now.year(); payload += "/";
  payload += now.hour(); payload += "/";
  payload += now.minute(); payload += "/";
  payload += String(avgWater, 1); payload += "/";
  payload += String(avgTempSol, 1); payload += "/";
  payload += String(avgHumSol, 0);
  
  Serial.print("\n[SOL AVG 10min] ");
  Serial.println(payload);
  
  // Sauvegarde de la moyenne
  dataFile = SD.open("sol_avg.csv", FILE_WRITE);
  if (dataFile) {
    if (dataFile.size() == 0) {
      dataFile.println("Date,Heure,AvgWater,AvgTempSol,AvgHumSol");
    }
    
    dataFile.print(now.year()); dataFile.print("/");
    dataFile.print(now.month()); dataFile.print("/");
    dataFile.print(now.day()); dataFile.print(",");
    
    dataFile.print(now.hour()); dataFile.print(":");
    dataFile.print(now.minute()); dataFile.print(",");
    
    dataFile.print(avgWater); dataFile.print(",");
    dataFile.print(avgTempSol); dataFile.print(",");
    dataFile.println(avgHumSol);
    
    dataFile.close();
  }
  
  // Transmission via KIM2
  // sendDataToKIM2(payload, "00");
  
  // Réinitialisation des données
  // solData.waterSum = 0;
  // solData.tempSolSum = 0;
  // solData.humSolSum = 0;
  solData.count = 0;
}

// ==================================================
// COMMUNICATION KIM2
// ==================================================

void sendDataToKIM2(const String& data, const char* attr) {
  Serial.print("[KIM2 TX] Envoi: ");
  Serial.println(data);
  
  // Conversion en hexadécimal (simplifiée)
  // En réalité, vous devriez encoder proprement vos données
  String hexData = "";
  for (unsigned int i = 0; i < data.length(); i++) {
    hexData += String(data[i], HEX);
  }
  
  // Tronquer/pad pour avoir une taille valide pour LDA2
  // Tailles valides: 4, 8, 12, 16, 20, 24 bytes
  int targetLength = 24; // On utilise 24 bytes (max)
  while (hexData.length() < targetLength * 2) { // *2 car hexa
    hexData += "0";
  }
  hexData = hexData.substring(0, targetLength * 2);
  
  sendTX(hexData.c_str(), attr);
}

void sendCmd(const char* cmd) {
  Serial.print("[KIM2 AT>>] ");
  Serial.println(cmd);
  Serial1.println(cmd);
  waitForResponse(1200);
}

void sendTX(const char* data, const char* attr) {
  Serial.print("[KIM2 AT>>] AT+TX=");
  Serial.print(data);
  Serial.print(",");
  Serial.println(attr);

  Serial1.print("AT+TX=");
  Serial1.print(data);
  Serial1.print(",");
  Serial1.println(attr);

  Serial.println("[KIM2] Attente cycle radio...");
  waitForResponse(TX_DELAY);
}

void waitForResponse(unsigned long waitTime) {
  unsigned long startTime = millis();
  
  while (millis() - startTime < waitTime) {
    readKIM2();
    delay(10);
  }
}

void readKIM2() {
  static String line = "";

  while (Serial1.available()) {
    char c = Serial1.read();

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

  if (msg.startsWith("+TX=")) {
    Serial.println(">>> TRANSMISSION EFFECTUEE <<<");
  }

  if (msg.startsWith("+DL=") || msg.startsWith("+RX=")) {
    Serial.println(">>> DOWNLINK RECU <<<");
    Serial.print("Payload = ");
    Serial.println(msg.substring(msg.indexOf('=') + 1));
  }

  if (msg.startsWith("+ERROR")) {
    Serial.println(">>> ERREUR MODULE <<<");
    int errorCode = msg.substring(7).toInt();
    Serial.print("Code erreur: ");
    Serial.println(errorCode);
    
    switch(errorCode) {
      case 5:
        Serial.println("Interpretation: Parametre invalide");
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