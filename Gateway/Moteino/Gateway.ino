#include <RFM69.h>       
#include <RFM69_ATC.h>   
#include <SPIFlash.h>    
#include <SPI.h>   
#include <ArduinoJson.h>

#define SERIAL_BAUD   115200

//#define SERIAL_EN      //uncomment this line to enable serial IO debug messages, leave out if you want low power
#ifdef SERIAL_EN
  #define DEBUG(input)   Serial.print(input);
  #define DEBUGln(input) Serial.println(input); 
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif


#define NODEID        1    
#define NETWORKID     100  
#define FREQUENCY     RF69_433MHZ

#define ENCRYPTKEY    "sampleEncryptKey" 
#define IS_RFM69HW    

#define ENABLE_ATC    
#define SERIAL_BAUD   115200

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif

RFM69_ATC radio;

SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)
bool promiscuousMode = false; //set to 'true' to sniff all packets on the same network

byte ackCount=0;
uint32_t packetCount = 0;
typedef struct {
  int nodeId; 
  double temperature; 
  double pressure; 
  double humidity;   
  unsigned int battery;
} Payload;

Payload payload;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(10);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);

  radio.setHighPower(); //only for RFM69HW!

  radio.encrypt(ENCRYPTKEY);
  radio.promiscuous(promiscuousMode);

  char buff[50];
  sprintf(buff, "\nListening at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  DEBUGln(buff);
  if (flash.initialize())
  {
    DEBUG("SPI Flash Init OK. Unique MAC = [");
  }
  else
    DEBUGln("SPI Flash MEM not found (is chip soldered?)...");
    
#ifdef ENABLE_ATC
  DEBUGln("RFM69_ATC Enabled (Auto Transmission Control)");
#endif
}

void loop() {
  //process any serial input
  if (Serial.available() > 0)
  {
    char input = Serial.read();
    if (input == 'r') //d=dump all register values
      radio.readAllRegs();
    if (input == 'E') //E=enable encryption
      radio.encrypt(ENCRYPTKEY);
    if (input == 'e') //e=disable encryption
      radio.encrypt(null);
    if (input == 'p')
    {
      promiscuousMode = !promiscuousMode;
      radio.promiscuous(promiscuousMode);
      DEBUG("Promiscuous mode ");DEBUGln(promiscuousMode ? "on" : "off");
    }
    
    if (input == 'd') //d=dump flash area
    {
      DEBUGln("Flash content:");
      int counter = 0;

      while(counter<=256){
        DEBUG(flash.readByte(counter++));
        DEBUG('.');
      }
      while(flash.busy());
      DEBUGln();
    }
    if (input == 'D')
    {
      DEBUG("Deleting Flash chip ... ");
      flash.chipErase();
      while(flash.busy());
      DEBUGln("DONE");
    }
    if (input == 'i')
    {
      DEBUG("DeviceID: ");
      word jedecid = flash.readDeviceId();
      DEBUGln(jedecid);
    }
    if (input == 't')
    {
      byte temperature =  radio.readTemperature(-1); // -1 = user cal factor, adjust for correct ambient
      byte fTemp = 1.8 * temperature + 32; // 9/5=1.8
      DEBUG( "Radio Temp is ");
      DEBUG(temperature);
      DEBUG("C, ");
      DEBUG(fTemp); //converting to F loses some resolution, obvious when C is on edge between 2 values (ie 26C=78F, 27C=80F)
      DEBUGln('F');
    }
  }

  if (radio.receiveDone())
  {
    DEBUG("#[");
    DEBUG(++packetCount);
    DEBUG(']');
    DEBUG('[');DEBUG(radio.SENDERID);DEBUG("] ");
    if (promiscuousMode)
    {
      DEBUG("to [");DEBUG(radio.TARGETID);DEBUG("] ");
    }
 
    if (radio.DATALEN != sizeof(Payload))
      {DEBUG("Invalid payload received, not matching Payload struct!");}
    else
    {
      StaticJsonBuffer<200> jsonBuffer;
      JsonObject& jsonObject = jsonBuffer.createObject();

      payload = *(Payload*)radio.DATA; //assume radio.DATA actually contains our struct and not something else
      DEBUG("NI=");
      DEBUG(payload.nodeId);
      jsonObject["NI"] = payload.nodeId;

      DEBUG(",TE=");
      DEBUG(payload.temperature);
      jsonObject["TE"] = payload.temperature;

      DEBUG(",HU=");
      DEBUG(payload.humidity);
      jsonObject["HU"] = payload.humidity;
      
      DEBUG(",PR=");
      DEBUG(payload.pressure);
      jsonObject["PR"] = payload.pressure;

      DEBUG(",BAT=");
      DEBUG(payload.battery);
      jsonObject["BAT"] = payload.battery;      

      DEBUG(",RSSI=");
      DEBUG(radio.readRSSI());
      jsonObject["RSSI"] = radio.readRSSI();

      jsonObject.printTo(Serial);
      Serial.println();
    }
       
    if (radio.ACKRequested())
    {
      byte theNodeID = radio.SENDERID;
      radio.sendACK();
      DEBUG(" - ACK sent.");

      // When a node requests an ACK, respond to the ACK
      // and also send a packet requesting an ACK (every 3rd one only)
      // This way both TX/RX NODE functions are tested on 1 end at the GATEWAY
      if (ackCount++%3==0)
      {
        DEBUG(" Pinging node ");
        DEBUG(theNodeID);
        DEBUG(" - ACK...");
        delay(3); //need this when sending right after reception .. ?
        if (radio.sendWithRetry(theNodeID, "ACK TEST", 8, 0))  // 0 = only 1 attempt, no retries
          {DEBUG("ok!");}
        else {DEBUG("nothing");}
      }
    }
    DEBUGln();
    Blink(LED,3);
  }
}

void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}
