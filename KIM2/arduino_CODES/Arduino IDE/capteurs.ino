#include <Wire.h>
#include <Adafruit_BME280.h>
#include <Adafruit_VEML7700.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

// ---- SD ----
#define SD_CS 53   // Pin CS sur Arduino Mega
File dataFile;

// ---- Capteurs ----
Adafruit_BME280 bme;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
RTC_DS3231 rtc;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  Serial.println("Initialisation du systeme...");

  // ---- I2C ----
  Wire.begin();

  // ---- SD ----
  Serial.print("Initialisation carte SD...");
  if (!SD.begin(SD_CS)) {
    Serial.println(" ECHEC !");
    while (1);
  }
  Serial.println(" OK");

  // Créer fichier si pas existant
  if (!SD.exists("data.csv")) {
    dataFile = SD.open("data.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Date,Heure,Temperature,Humidite,Pression,Lux");
      dataFile.close();
    }
  }

  // ---- BME280 ----
  if (!bme.begin(0x77)) {
    Serial.println("ERREUR: BME280 non detecte !");
    while (1);
  }
  Serial.println("BME280 OK");

  // ---- VEML7700 ----
  if (!veml.begin()) {
    Serial.println("ERREUR: VEML7700 non detecte !");
    while (1);
  }
  Serial.println("VEML7700 OK");

  veml.setGain(VEML7700_GAIN_1);
  veml.setIntegrationTime(VEML7700_IT_100MS);

  // ---- RTC ----
  if (!rtc.begin()) {
    Serial.println("ERREUR: DS3231 non detecte !");
    while (1);
  }
  Serial.println("DS3231 OK");

  if (rtc.lostPower()) {
    Serial.println("RTC a perdu l'heure, mise a l'heure !");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  Serial.println("Systeme pret !");
}

void loop() {
  DateTime now = rtc.now();

  float temp = bme.readTemperature();
  float hum = bme.readHumidity();
  float pres = bme.readPressure() / 100.0F;
  float lux = veml.readLux();

  // ---- Affichage Serial ----
  Serial.print("Date/Heure: ");
  Serial.print(now.year()); Serial.print("/");
  Serial.print(now.month()); Serial.print("/");
  Serial.print(now.day()); Serial.print(" ");
  Serial.print(now.hour()); Serial.print(":");
  Serial.print(now.minute()); Serial.print(":");
  Serial.println(now.second());

  Serial.print("Temp: "); Serial.print(temp); Serial.println(" C");
  Serial.print("Hum: "); Serial.print(hum); Serial.println(" %");
  Serial.print("Pres: "); Serial.print(pres); Serial.println(" hPa");
  Serial.print("Lux: "); Serial.print(lux); Serial.println(" lux");

  // ---- Ecriture SD ----
  dataFile = SD.open("data.csv", FILE_WRITE);
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
    Serial.println("Donnees enregistrees sur SD");
  } else {
    Serial.println("ERREUR ouverture fichier SD");
  }

  Serial.println("----------------------------");

  delay(2000);
}
