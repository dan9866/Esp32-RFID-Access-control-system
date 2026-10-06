#include <SPI.h>
#include <MFRC522.h>
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>
#include <ESP32Servo.h>

#define RST_PIN 22
#define SS_PIN 21
#define SERVO_PIN 4
#define BATTERY_ADC_PIN 34
#define LOW_BATTERY_LED 26
#define LOW_BATTERY_THRESHOLD 3.3
#define MOTION_SENSOR_IN 14
#define EEPROM_SIZE 1024
#define UID_LENGTH 4
#define NAME_LENGTH 16
#define ID_LENGTH 8
#define MAX_TAGS 15
#define RECORD_SIZE (UID_LENGTH + NAME_LENGTH + ID_LENGTH)

const char* ssid = "ACCESS CONTROL 10";
const char* password = "controversy";

MFRC522 rfid(SS_PIN, RST_PIN);
WebServer server(80);
Servo doorServo;

byte masterCard[UID_LENGTH] = {0xF3, 0x01, 0x03, 0x29};
bool wifiActive = false;
String pendingUID = "";
int tagCount = 0;

unsigned long lastMotionTime = 0;
const unsigned long motionCooldown = 5000;
const float referenceVoltage = 4.2;
const int adcResolution = 4095;
int servoOpenAngle = 90;
int servoCloseAngle = 0;
String accessLogs = "";

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  EEPROM.begin(EEPROM_SIZE);

  pinMode(LOW_BATTERY_LED, OUTPUT);
  pinMode(MOTION_SENSOR_IN, INPUT);
  digitalWrite(LOW_BATTERY_LED, HIGH);

  doorServo.attach(SERVO_PIN);
  doorServo.write(servoCloseAngle);

  tagCount = EEPROM.read(0);
  if (tagCount > MAX_TAGS) tagCount = 0;
  Serial.println("System Ready. Scan RFID card...");
}

void loop() {
  monitorBattery();
  handleMotionSensor();

  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input == "clear") clearEEPROM();
  }

  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    String cardUID = getCardUID();
    int index = findUIDIndex(cardUID);

    if (!wifiActive && isMasterCard()) {
      startWiFiAP();
      wifiActive = true;
    } else if (wifiActive && isMasterCard()) {
      stopWiFiAP();
      wifiActive = false;
    } else if (wifiActive) {
      if (index != -1) {
        unregisterTag(index);
        Serial.println("Card unregistered: " + cardUID);
      } else {
        pendingUID = cardUID;
        Serial.println("New card detected. Waiting for name and ID from web interface...");
      }
    } else {
      if (index != -1) {
        unlockDoor();
        String logEntry = "Access granted: " + getName(index) + " (UID: " + cardUID + ") at " + getTimeStamp() + "<br>";
        accessLogs += logEntry;
        Serial.println(logEntry);
      }
    }

    rfid.PICC_HaltA();
    delay(500);
  }

  if (wifiActive) server.handleClient();
}

String getCardUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

bool isMasterCard() {
  for (byte i = 0; i < UID_LENGTH; i++) {
    if (rfid.uid.uidByte[i] != masterCard[i]) return false;
  }
  return true;
}

void unlockDoor() {
  doorServo.write(servoOpenAngle);
  delay(3000);
  doorServo.write(servoCloseAngle);
}

void monitorBattery() {
  int sensorValue = analogRead(BATTERY_ADC_PIN);
  float voltage = sensorValue * (referenceVoltage / adcResolution);
  digitalWrite(LOW_BATTERY_LED, voltage < LOW_BATTERY_THRESHOLD);
}

void handleMotionSensor() {
  if (digitalRead(MOTION_SENSOR_IN) == HIGH && millis() - lastMotionTime > motionCooldown) {
    unlockDoor();
    lastMotionTime = millis();
  }
}

void startWiFiAP() {
  WiFi.softAP(ssid, password);
  server.on("/", handleRoot);
  server.on("/register", handleRegister);
  server.on("/logs", handleLogs);
  server.on("/authorized", handleAuthorized);
  server.begin();
  Serial.println("Access Point started. IP: " + WiFi.softAPIP().toString());
}

void stopWiFiAP() {
  server.stop();
  WiFi.softAPdisconnect(true);
  pendingUID = "";
  Serial.println("WiFi Access Point stopped.");
}

void handleRoot() {
  String html = "<html><body><h1>RFID Registration</h1>";
  html += "<form action='/register' method='POST'>";
  html += "Name: <input type='text' name='name'><br>";
  html += "ID No: <input type='text' name='id'><br>";
  html += "<input type='submit' value='Register'></form>";
  html += "<br><a href='/logs'>View Access Logs</a>";
  html += "<br><a href='/authorized'>View Authorized List</a></body></html>";
  server.send(200, "text/html", html);
}

void handleRegister() {
  if (server.hasArg("name") && server.hasArg("id") && pendingUID != "") {
    String name = server.arg("name");
    String idno = server.arg("id");
    if (findUIDIndex(pendingUID) == -1 && tagCount < MAX_TAGS) {
      storeTagWithName(tagCount, pendingUID, name, idno);
      tagCount++;
      EEPROM.write(0, tagCount);
      EEPROM.commit();
      server.send(200, "text/html", "<h1>Registration successful!</h1><a href='/'>Back</a>");
    } else {
      server.send(200, "text/html", "<h1>Error: Already registered or full!</h1><a href='/'>Back</a>");
    }
    pendingUID = "";
  } else {
    server.send(200, "text/html", "<h1>Error: Missing data or no new card!</h1><a href='/'>Back</a>");
  }
}

void handleLogs() {
  String html = "<html><body><h1>Access Logs</h1>";
  html += accessLogs;
  html += "<br><a href='/'>Back</a></body></html>";
  server.send(200, "text/html", html);
}

void handleAuthorized() {
  String html = "<html><body><h1>Authorized Users</h1><ul>";
  for (int i = 0; i < tagCount; i++) {
    html += "<li>" + getName(i) + "</li>";
  }
  html += "</ul><br><a href='/'>Back</a></body></html>";
  server.send(200, "text/html", html);
}

int findUIDIndex(String uidStr) {
  for (int i = 0; i < tagCount; i++) {
    int addr = 1 + (i * RECORD_SIZE);
    String storedUID = "";
    for (int j = 0; j < UID_LENGTH; j++) {
      byte val = EEPROM.read(addr++);
      if (val < 0x10) storedUID += "0";
      storedUID += String(val, HEX);
      if (j < UID_LENGTH - 1) storedUID += " ";
    }
    storedUID.toUpperCase();
    if (storedUID == uidStr) return i;
  }
  return -1;
}

String getName(int index) {
  int addr = 1 + index * RECORD_SIZE + UID_LENGTH;
  char name[NAME_LENGTH + 1];
  for (int i = 0; i < NAME_LENGTH; i++) name[i] = EEPROM.read(addr + i);
  name[NAME_LENGTH] = '\0';
  return String(name);
}

void storeTagWithName(int index, String uidStr, String name, String idno) {
  int addr = 1 + (index * RECORD_SIZE);
  for (int i = 0; i < UID_LENGTH; i++) {
    int byteIndex = i * 3;
    byte b = strtol(uidStr.substring(byteIndex, byteIndex + 2).c_str(), NULL, 16);
    EEPROM.write(addr++, b);
  }
  for (int i = 0; i < NAME_LENGTH; i++) EEPROM.write(addr++, (i < name.length()) ? name[i] : 0);
  for (int i = 0; i < ID_LENGTH; i++) EEPROM.write(addr++, (i < idno.length()) ? idno[i] : 0);
  EEPROM.commit();
}

void unregisterTag(int index) {
  int startAddr = 1 + index * RECORD_SIZE;
  for (int i = startAddr; i < startAddr + RECORD_SIZE; i++) EEPROM.write(i, 0);
  for (int i = index; i < tagCount - 1; i++) {
    for (int j = 0; j < RECORD_SIZE; j++) {
      byte nextByte = EEPROM.read(1 + ((i + 1) * RECORD_SIZE) + j);
      EEPROM.write(1 + (i * RECORD_SIZE) + j, nextByte);
    }
  }
  tagCount--;
  EEPROM.write(0, tagCount);
  EEPROM.commit();
}

void clearEEPROM() {
  for (int i = 0; i < EEPROM_SIZE; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  tagCount = 0;
  Serial.println("EEPROM cleared.");
}

String getTimeStamp() {
  char timeStr[20];
  unsigned long seconds = millis() / 1000;
  sprintf(timeStr, "%02d:%02d:%02d", (seconds / 3600) % 24, (seconds / 60) % 60, seconds % 60);
  return String(timeStr);
}
