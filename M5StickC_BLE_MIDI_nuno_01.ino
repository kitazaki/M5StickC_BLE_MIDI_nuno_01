#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <M5StickC.h>
#include <Arduino.h>
#include <Wire.h>
#include "MTCH6102.h"

#define ADDR 0x25
MTCH6102 mtch = MTCH6102();
int len = 8;

///// Button
boolean buttonFlags[8] = {false, false, false, false, false, false, false, false};
uint8_t buttonNote[8] = {60, 59, 57, 55, 53, 52, 50, 48}; // C B A G F E D C

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;

#define MIDI_SERVICE_UUID        "03b80e5a-ede8-4b33-a751-6ce34ec4c700"
#define MIDI_CHARACTERISTIC_UUID "7772e5db-3868-4112-a1a9-f2669d106bf3"

void printConnectionStatus(const char* message){
  M5.Lcd.setTextSize(1); // Adafruit 8ピクセルASCIIフォント
  M5.Lcd.setCursor(4,8);
  M5.Lcd.printf(message);
  M5.Lcd.println();
}

BLEServer *pServer;  // ■
BLESecurity *pSecurity;  //■

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    deviceConnected = true;
    printConnectionStatus("connected");
    Serial.println("connected");
  }
  void onDisconnect(BLEServer* pServer) {
    deviceConnected = false;
    printConnectionStatus("disconnected");
    Serial.println("disconnected");
  }
};

void noteOn(uint8_t channel, uint8_t pitch, uint8_t velocity) {
  uint8_t midiPacket[] = {0x80, 0x80,  (uint8_t)(0x90 | channel), pitch, velocity };
  //Serial.println((uint8_t)(0x90 | channel));
  pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes
  pCharacteristic->notify();
}
void noteOff(uint8_t channel, uint8_t pitch, uint8_t velocity) {
  uint8_t midiPacket[] = {0x80, 0x80,  (uint8_t)(0x80 | channel), pitch, velocity };
  //Serial.println((uint8_t)(0x80 | channel));
  pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes)
  pCharacteristic->notify();
}
void controlChange(uint8_t  channel, uint8_t  control, uint8_t  value) {
  uint8_t midiPacket[] = {0x80, 0x80, (uint8_t)(0xB0 | channel), control, value };
  pCharacteristic->setValue(midiPacket, 5); // packet, length in bytes)
  pCharacteristic->notify();
}

void setup() {  
  byte data;
  Serial.begin(115200);

  BLEDevice::init("M5StickCnuno"); //Device Name ■

  // Create the BLE Server
  //BLEServer *pServer = BLEDevice::createServer(); // ■
  pServer = BLEDevice::createServer(); // ■
  pServer->setCallbacks(new MyServerCallbacks());
  BLEDevice::setEncryptionLevel((esp_ble_sec_act_t)ESP_LE_AUTH_REQ_SC_BOND);
    
  // Create the BLE Service
  BLEService *pService = pServer->createService(BLEUUID(MIDI_SERVICE_UUID));

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      BLEUUID(MIDI_CHARACTERISTIC_UUID),
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_WRITE_NR
                    );
  pCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED | ESP_GATT_PERM_WRITE_ENCRYPTED);

  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  //BLESecurity *pSecurity = new BLESecurity(); //■
  pSecurity = new BLESecurity();
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  pSecurity->setCapability(ESP_IO_CAP_NONE);
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

  pServer->getAdvertising()->addServiceUUID(MIDI_SERVICE_UUID);
  pServer->getAdvertising()->start();

  M5.begin();  //LCD
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setCursor(4,0);
  M5.Lcd.setTextSize(1); // Adafruit 8ピクセルASCIIフォント
  M5.Lcd.printf("BLE MIDI nuno");

  // Wire.begin();
  mtch.begin();
  mtch.writeRegister(MTCH6102_MODE, MTCH6102_MODE_STANDBY);
  mtch.writeRegister(MTCH6102_NUMBEROFXCHANNELS, 0x10 );
  mtch.writeRegister(MTCH6102_NUMBEROFYCHANNELS, 0x03); //最低3点必要なため
  mtch.writeRegister(MTCH6102_MODE, MTCH6102_MODE_FULL);
  
  mtch.writeRegister(MTCH6102_CMD, 0x20);
  delay(500);
  
  // the operating mode (MODE)
  data = mtch.readRegister(MTCH6102_MODE);
  Serial.print("MODE: ");
  Serial.println(data,BIN);
  data = mtch.readRegister(MTCH6102_NUMBEROFXCHANNELS);
  Serial.print("NUMBER_OF_X_CHANNELS: ");
  Serial.println(data);
  data = mtch.readRegister(MTCH6102_NUMBEROFYCHANNELS);
  Serial.print("NUMBER_OF_Y_CHANNELS: ");
  Serial.println(data);
}  //setup

int sensStats[10] = {0,0,0,0,0,0,0,0,0};
float sensVals[10] = {0,0,0,0,0,0,0,0,0,0};

void loop() { 
  byte data;  

  M5.Lcd.setCursor(4, 16); 
  M5.Lcd.print("                                                ");
  M5.Lcd.setCursor(4, 16);

  for (int i = 0; i < len; i++) {
    data = mtch.readRegister(MTCH6102_SENSORVALUE_RX0+i);
    sensVals[i] = data;

    // Serial.print(data);
    // Serial.print(", ");
    M5.Lcd.print(data);
    M5.Lcd.print(", ");

    // Low Water Mark
    if (sensVals[i] < 150) {
      if (sensStats[i] > 0) {
        if (deviceConnected) {
          // noteOff
          noteOff(0, buttonNote[i], 127);
          Serial.print(i);
          Serial.println(": off");
          sensStats[i] = 0;
        }
      }
    }

    // High Water Mark
    if (sensVals[i] > 200) {
      if (sensStats[i] < 1) {
        if (deviceConnected) {
          noteOn(0, buttonNote[i], 127);
          Serial.print(i);
          Serial.println(": on");
          sensStats[i] = 1;
        }
      }
    }
  }

  for (int i = 0; i<len; i++){
    float prev;
    if(i==0){
       prev = 0;
    }else{
       prev=sensVals[i-1];
    }
  //  M5.Lcd.drawLine(i*20, 80-(prev/255)*80, (i+1)*20, 80-(sensVals[i]/255)*80, TFT_WHITE);
  }

  delay(100);
  
  for (int i = 0; i<len; i++){
    float prev;
    if(i==0){
       prev = 0;
    }else{
       prev=sensVals[i-1];
    }
  //  M5.Lcd.drawLine(i*20, 80-(prev/255)*80, (i+1)*20, 80-(sensVals[i]/255)*80, TFT_BLACK);
  }

  M5.Lcd.println();

}
