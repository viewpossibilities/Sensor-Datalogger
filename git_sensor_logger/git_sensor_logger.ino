#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <virtuabotixRTC.h> // DS1302 Library
#include <SD.h>
#include "DHT.h"

// DHT sensor
#define DHTPIN 14
#define DHTTYPE DHT11   // DHT 11
DHT dht(DHTPIN, DHTTYPE);

// GUVA-S12SD Sensor
const int uvSensorPin = A0;  // Analog pin connected to UV sensor output
float voltage;
float uvIndex;

// DS18B20 Temperature Sensors
const int numTemperatureSensors = 10;
const int temperatureSensorPins[] = {2, 3, 4, 5, 9, 10, 11, 12, 13, 23};

// Soil Moisture Sensor
const int numMoistureSensors = 10;
const int moistureVccPins[] = {22, 24, 26, 28, 30, 32, 34, 36, 38, 40};
const int moistureAnalogPins[] = {A1, A2, A3, A4, A5, A6, A7, A8, A9, A10};

// BMP280 Sensor
Adafruit_BME280 lowerBme;  // Change "groundBme" to "lowerBme" for consistency

// DS1302 Real-Time Clock
virtuabotixRTC myRTC(6, 7, 8); // CLK -> 6, DAT -> 7, RST -> 8

// SD Card
const int chipSelectPin = 53;
File temperatureLogFile;
File moistureLogFile;
File environmentLogFile;

const String _soilTempFileName = "soil_Tmp.csv";
const String _soilMoistFileName = "moistur.csv";
const String _envFileName = "env.csv";

void setup() {
  Serial.begin(9600);
  dht.begin();
  
  // Set the current date and time for DS1302 RTC if used
  // myRTC.setDS1302Time(00, 31, 16, 06, 26, 05, 2023);

  // Initialize temperature sensors
  for (int i = 0; i < numTemperatureSensors; i++) {
    pinMode(temperatureSensorPins[i], INPUT);
  }
  
  // Initialize soil moisture sensors
  for (int i = 0; i < numMoistureSensors; i++) {
    pinMode(moistureVccPins[i], OUTPUT);
    digitalWrite(moistureVccPins[i], LOW);
  }
  
  // Initialize environmental sensors
  Wire.begin();
  /*if (!groundBme.begin(0x76)) {
    Serial.println("Could not find a valid lower BME280 sensor. Please check the wiring!");
    while (1);
  }*/

  if (!lowerBme.begin(0x76)) {
    Serial.println("Could not find a valid UPPer BME280 sensor. Please check the wiring!");
    while (1);
  }
  
  // Initialize SD card
  if (!SD.begin(chipSelectPin)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  
  // Create and open temperature log file
  temperatureLogFile = SD.open(_soilTempFileName, FILE_WRITE);
  if (!temperatureLogFile) {
    Serial.println("Error opening temperature log file!");
    return;
  }
  temperatureLogFile.println("DATE,TIME,POT1 (*C), POT2 (*C),POT3 (*C), POT4 (*C), POT5 (*C), POT6 (*C), POT7 (*C), POT8 (*C), POT9 (*C), POT10 (*C)");
  
  // Create and open moisture log file
  moistureLogFile = SD.open(_soilMoistFileName, FILE_WRITE);
  if (!moistureLogFile) {
    Serial.println("Error opening moisture log file!");
    return;
  }
  moistureLogFile.println("DATE,TIME,POT1 (%),POT2 (%),POT3 (%),POT4 (%),POT5 (%), POT6 (%), POT7 (%), POT8 (%), POT9 (%), POT10 (%)");
  
  // Create and open environment log file
  environmentLogFile = SD.open(_envFileName, FILE_WRITE);
  if (!environmentLogFile) {
    Serial.println("Error opening environment log file!");
    return;
  }
  environmentLogFile.println("DATE,TIME,GROUND Humidity (%),UPPER Humidity (%),Pressure (hPa),GROUND Temperature (*C),UPPER Temperature (*C), UV Index");
}

void loop() {
  myRTC.updateTime();

  int currentHour = myRTC.hours;
  int currentMinutes = myRTC.minutes;
  int currentSeconds = myRTC.seconds;

  // Call logDataToSD() when the time is exactly 12:30 or every 3 hours
  if (currentHour == 0 && currentMinutes == 0 && currentSeconds == 0) {
    sensorValues();
  } 
  else if (currentHour % 3 == 0 && currentMinutes == 0 && currentSeconds == 0) {
    sensorValues();
  }

  delay(1000);  // Delay for 1 second
}

void sensorValues(){
  logTemperatureData();
  logMoistureData();
  logEnvironmentData();
}

void logTemperatureData() {
  String temperatureData = getCurrentDateTime();
  
  for (int i = 0; i < numTemperatureSensors; i++) {
    float temperature = getTemperature(temperatureSensorPins[i]);
    temperatureData += "," + String(temperature) + "*C";
  }
  
  Serial.println(temperatureData);
  temperatureLogFile.println(temperatureData);
  temperatureLogFile.flush();
}

float getTemperature(int sensorPin) {
  OneWire oneWire(sensorPin);
  DallasTemperature sensors(&oneWire);
  sensors.begin();
  sensors.requestTemperatures();
  return sensors.getTempCByIndex(0);
}

void logMoistureData() {
  String moistureData = getCurrentDateTime();
  
  for (int i = 0; i < numMoistureSensors; i++) {
    int moisture = readMoisture(moistureVccPins[i], moistureAnalogPins[i]);
    moistureData += "," + String(moisture) + "%";
  }
  Serial.println( moistureData );
  moistureLogFile.println(moistureData);
  moistureLogFile.flush();
}

int readMoisture(int vccPin, int analogPin) {
  digitalWrite(vccPin, HIGH);
  delay(10);
  int val = analogRead(analogPin);
  digitalWrite(vccPin, LOW);
  int moisturePercentage = map(val, 280, 8, 0, 100);
  if (moisturePercentage < 0){
    moisturePercentage = 0;
  }
  return moisturePercentage;
}

void logEnvironmentData() {
  String environmentData = getCurrentDateTime();
  
  float top_humidity = dht.readHumidity();
  float top_temperature = dht.readTemperature();
  if (isnan(top_humidity) || isnan(top_temperature)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  float uvIndex = getUVIndex();
  float gnd_humidity = lowerBme.readHumidity();
  float gnd_pressure = lowerBme.readPressure() / 100.0F;
  float gnd_temperature = lowerBme.readTemperature();
  
  environmentData += "," + String(gnd_humidity) + "%" + "," + String(top_humidity) + "%" +  "," + String(gnd_pressure)+ "hPa" +"," + String(gnd_temperature) + "*C" + "," + String(top_temperature)+ "*C" +"," + String(uvIndex);
  Serial.println(environmentData);
  environmentLogFile.println(environmentData);
  environmentLogFile.flush();
}

float getUVIndex() {
  int sensorValue = analogRead(uvSensorPin);
  float voltage = sensorValue * (5.0 / 1023.0);
  float uvIndex = voltage / 0.1;
  return uvIndex;
}

String getCurrentDateTime() {
  // Replace this with your method of getting the current date and time
  myRTC.updateTime();

  String dayOfMonth = String(myRTC.dayofmonth);
  String month = String(myRTC.month);
  String year = String(myRTC.year);

  String hour = String(myRTC.hours);
  String minutes = String(myRTC.minutes);
  String seconds = String(myRTC.seconds);

  String date = dayOfMonth + "/" + month + "/" + year;
  String time = hour + ":" + minutes + ":" + seconds;
  return date + "," + time;
}

