#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_VEML7700.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

// ================= KIM2 =================
#define KIM2_POWER_PIN 4
#define STARTUP_DELAY 8000
const char* RADIO_CONFIG = "AT+RCONF=3d678af16b5a572078f3dbc95a1104e7";

// ================= SD =================
#define SD_CS 53
File dataFile;

// ================= Capteurs =================
Adafruit_BME280 bme;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
RTC_DS3231 rtc;

// ================= TIMERS =================
unsigned long lastMeasure = 0;
const unsigned long measureInterval = 30000; // 30 sec

unsigned long lastSend = 0;
const unsigned long sendInterval = 600000; // 10 min

// ================= MOYENNE =================
float sumTemp=0, sumHum=0, sumPres=0, sumLux=0;
int count=0;

// ================= DECLARATIONS =================
void sendCmd(const char* cmd);
void sendData(const char* data);

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  Serial1.begin(9600);
  Wire.begin();

  pinMode(KIM2_POWER_PIN, OUTPUT);

  Serial.println("\n===== STATION METEO SATELLITE ECOLOGGING =====");

  // ================= SD =================
  Serial.print("SD...");
  if (!SD.begin(SD_CS)) {
    Serial.println("ECHEC");
    while(1);
  }
  Serial.println("OK");

  if (!SD.exists("data.csv")) {
    dataFile = SD.open("data.csv", FILE_WRITE);
    dataFile.println("Date,Heure,Temp,Hum,Pres,Lux,Type");
    dataFile.close();
  }

  // ================= Capteurs =================
  if (!bme.begin(0x77)) { Serial.println("BME280 ERREUR"); while(1); }
  if (!veml.begin())    { Serial.println("VEML7700 ERREUR"); while(1); }
  if (!rtc.begin())     { Serial.println("RTC ERREUR"); while(1); }

  if (rtc.lostPower())
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  Serial.println("Capteurs OK");

  // ================= KIM2 =================
  Serial.println("Power KIM2...");
  digitalWrite(KIM2_POWER_PIN, HIGH);
  delay(STARTUP_DELAY);

  sendCmd("AT+PING=?");
  sendCmd(RADIO_CONFIG);
  sendCmd("AT+RCONF=?");
  sendCmd("AT+KMAC=1");

  Serial.println("SYSTEME PRET");
}

// ================= LOOP =================
void loop() {

  // ================= MESURE 30s =================
  if (millis() - lastMeasure >= measureInterval) {
    lastMeasure = millis();

    DateTime now = rtc.now();
    float temp = bme.readTemperature();
    float hum  = bme.readHumidity();
    float pres = bme.readPressure() / 100.0;
    float lux  = veml.readLux();

    Serial.println("\n--- MESURE 30s ---");
    Serial.print("Temp: "); Serial.println(temp);
    Serial.print("Hum: ");  Serial.println(hum);
    Serial.print("Pres: "); Serial.println(pres);
    Serial.print("Lux: ");  Serial.println(lux);

    // ===== cumul pour moyenne =====
    sumTemp += temp;
    sumHum  += hum;
    sumPres += pres;
    sumLux  += lux;
    count++;

    // ===== stockage brut SD =====
    dataFile = SD.open("data.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.print(now.year());   dataFile.print("/");
      dataFile.print(now.month());  dataFile.print("/");
      dataFile.print(now.day());    dataFile.print(",");

      dataFile.print(now.hour());   dataFile.print(":");
      dataFile.print(now.minute()); dataFile.print(":");
      dataFile.print(now.second()); dataFile.print(",");

      dataFile.print(temp); dataFile.print(",");
      dataFile.print(hum);  dataFile.print(",");
      dataFile.print(pres); dataFile.print(",");
      dataFile.print(lux);  dataFile.print(",");
      dataFile.println("RAW");
      dataFile.close();
      Serial.println("SD RAW OK");
    }
  }

  // ================= ENVOI MOYENNE 10 min =================
  if (millis() - lastSend >= sendInterval && count > 0) {
    lastSend = millis();

    float avgTemp = sumTemp / count;
    float avgHum  = sumHum  / count;
    float avgPres = sumPres / count;
    float avgLux  = sumLux  / count;

    DateTime now = rtc.now();

    Serial.println("\n===== MOYENNE 10 MIN =====");
    Serial.print("Temp: "); Serial.println(avgTemp);
    Serial.print("Hum: ");  Serial.println(avgHum);
    Serial.print("Pres: "); Serial.println(avgPres);
    Serial.print("Lux: ");  Serial.println(avgLux);

    // ===== sauvegarde moyenne SD =====
    dataFile = SD.open("data.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.print(now.year());   dataFile.print("/");
      dataFile.print(now.month());  dataFile.print("/");
      dataFile.print(now.day());    dataFile.print(",");

      dataFile.print(now.hour());   dataFile.print(":");
      dataFile.print(now.minute()); dataFile.print(":");
      dataFile.print(now.second()); dataFile.print(",");

      dataFile.print(avgTemp); dataFile.print(",");
      dataFile.print(avgHum);  dataFile.print(",");
      dataFile.print(avgPres); dataFile.print(",");
      dataFile.print(avgLux);  dataFile.print(",");
      dataFile.println("AVG");
      dataFile.close();
      Serial.println("SD AVG OK");
    }

    // ===== encodage payload =====
    uint16_t t = (uint16_t)((int16_t)(avgTemp * 100));
    uint16_t h = (uint16_t)(avgHum  * 100);
    uint16_t p = (uint16_t)(avgPres * 10);
    uint32_t l = (uint32_t)(avgLux);

    char payload[33];
    sprintf(payload, "%04X%04X%04X%08lX000000000000", t, h, p, l);

    Serial.print("Payload HEX = ");
    Serial.println(payload);

    sendData(payload);

    // reset moyenne
    sumTemp = 0; sumHum = 0; sumPres = 0; sumLux = 0;
    count = 0;
  }

  // lecture retour KIM2
  if (Serial1.available())
    Serial.write(Serial1.read());
}

// ================= FONCTIONS =================

void sendCmd(const char* cmd) {
  Serial.print("[AT>>] ");
  Serial.println(cmd);
  Serial1.println(cmd);
  delay(1200);
}

void sendData(const char* data) {
  Serial.println("---- ENVOI SAT ----");

  Serial.print("AT+TX=");
  Serial.print(data);
  Serial.println(",00");

  Serial1.print("AT+TX=");
  Serial1.print(data);
  Serial1.println(",00");
}