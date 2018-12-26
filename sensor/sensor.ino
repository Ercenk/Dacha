#include "settings.h"

#include <RFM69.h> 
#include <RFM69_ATC.h>
#include <RFM69_OTA.h> 
#include <LowPower.h> 
#include <SparkFunBME280.h>
#include <SPIFlash.h>    
#include <SPI.h>

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

BME280 sensor;
SPIFlash flash(FLASH_SS, 0xEF30);

typedef struct {
  int nodeId; 
  double temperature; 
  double pressure; 
  double humidity;   
  unsigned int battery;
} Payload;

Payload payload;

void setup()
{
  setupBase();
  setupBME();
  setupFlash();
  setupRadio();
}

void loop()
{
  sendData();

  for (byte i = 0; i < SLEEP_COUNT; i++) {
    LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF);
  }
}

void setupBase()
{
  #ifdef SERIAL_EN
    Serial.begin(SERIAL_BAUD);
  #endif  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BATTERY_PIN, INPUT);
  delay(1);
}

void setupBME()
{
  sensor.beginI2C();

  //Start by sleeping
  sensor.setMode(MODE_SLEEP); 
}

void setupFlash()
{
  if (flash.initialize()) {
    flash.sleep();
  }
}

void setupRadio()
{
  radio.initialize(FREQUENCY,NODEID,NETWORKID);

  radio.setHighPower(); //must include this only for RFM69HW/HCW!

  radio.encrypt(ENCRYPTKEY);

  radio.enableAutoPower(ATC_RSSI);

  radio.sendWithRetry(GATEWAYID, "START", 6);
}

void sendData()
{
  payload.nodeId = NODEID;
  payload.battery = BATTERY_RATIO * analogRead(BATTERY_PIN);

  // BME temp, etc...
  sensor.setMode(MODE_FORCED); 
  
  while(sensor.isMeasuring() == false) ; 
  while(sensor.isMeasuring() == true) ; 
 
  payload.temperature = sensor.readTempF();
  payload.humidity = sensor.readFloatHumidity();
  payload.pressure = sensor.readFloatPressure() * 0.0002953; //read Pa and convert to inHg

  if (radio.sendWithRetry(GATEWAYID, (const void*)(&payload), sizeof(payload)))
  {
    DEBUG( payload.temperature ); DEBUG(", ");
    DEBUGln( NODEID );
    blink(LED_PIN,300);
    if (radio.receiveDone())
    {
      DEBUG("RSSI: "); DEBUGln(radio.readRSSI());
      
      if (radio.ACKRequested())
      {
        radio.sendACK();
        DEBUGln("-> ACK sent");
      }
    }
  }
  else
  {
    DEBUGln("not OK");
  }

  sensor.setMode(MODE_SLEEP);
  delay(10);
}

void blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
