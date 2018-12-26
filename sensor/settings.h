#define NODEID            40
#define NETWORKID         100  
#define GATEWAYID         1

#define FREQUENCY         RF69_433MHZ
#define ENCRYPTKEY        "sampleEncryptKey" 

#define IS_RFM69HW 
#define SENDEVERYXLOOPS   8 

#define ENABLE_ATC    
#define ATC_RSSI -80

#define LED_PIN             9
#define BATTERY_PIN         A7
#define USE_MOSFET          0
#define BATTERY_ENABLE_PIN  A3

#define FLASH_SS            8

#define SERIAL_EN      //uncomment this line to enable serial IO debug messages, leave out if you want low power
#ifdef SERIAL_EN
  #define SERIAL_BAUD   115200
  #define DEBUG(input)   {Serial.print(input); delay(1);}
  #define DEBUGln(input) {Serial.println(input); delay(1);}
#else
  #define DEBUG(input);
  #define DEBUGln(input);
#endif

#define SLEEP_COUNT   1     // 32

#define BATTERY_RATIO       4.73
