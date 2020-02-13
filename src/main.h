#include <Arduino.h>
#include "credentials.h"
#define DEBUG_ON 1
//#undef DEBUG_ON
#define WPS_ON 1

#define localDomain "pogoda" /* local brooadcast domain name pogoda.local */
#define _setSyncInterval 3600 /* how offten shall the standard NTP sync be fired in seconds*/
#define configPin 0  /* press button */

#define sensorNumber 3
#define feedNumber   5
#define fD          "-"

const uint8_t dataSize = 21; // data frame size

#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  8883             // use 8883 for SSL -1883

#define FEEDS "/feeds/"
// io.Adafruit free accound restricted only to 10 feeds! fD disables a feed
const String feedsTable [sensorNumber][feedNumber] = {
  // Temperature, Humidity, Preasure, Battery, Date
  {IO_USERNAME FEEDS "t0", IO_USERNAME FEEDS "h0", IO_USERNAME FEEDS "p0", IO_USERNAME FEEDS "b0", IO_USERNAME FEEDS "d0"},
  {IO_USERNAME FEEDS "t1", IO_USERNAME FEEDS "h1",                    fD,                     fD,                     fD },
  {IO_USERNAME FEEDS "t2", IO_USERNAME FEEDS "h2",                    fD,                     fD,  IO_USERNAME FEEDS "d2"}
};

// define freq. to update MQTT/IOT page in mili seconds
#define Time2UpdateMQTT 120000L

typedef struct 
{
  float temperature;
  float humidity;
  float pressure;
  float battery;
  long  counter;
  uint8_t  sensorID; 
  long  dummy;
  time_t timeStamp;
} Tdata;

#define _noSesnor 255

enum States
{
  _None,
  _Init,
  _NTP_Sync,
  _Time,
  _Sensor1,
  _Sensor2,
  _Sensor3,
  _WIFI,
  _WIFIConnected,
  _NTPFailed,
  _MDSFailed,
  _WPS,
};

#define TIMEX 10
#define TIMEY 20
#define TFT_myGray 0x2945
#define LEDX 20
#define LEDY 20
#define LEDR 10
#define FONTSIZE 26



const char jsonStruc[] PROGMEM = R"rawliteral(
{"XC1":"%s",
"clock":"%02d:%02d:%02d &nbsp; %02d:%02d:%02d",
"time0":"%02d:%02d:%02d &nbsp; %02d:%02d:%02d",
"temp0":"%.2f",
"hum0" :"%.0f",
"pres0":"%.0f",
"bat0" :"%.2f",
"time1":"%02d:%02d:%02d &nbsp; %02d:%02d:%02d",
"temp1":"%.2f",
"hum1" :"%.0f",
"pres1":"%.0f",
"bat1" :"%.2f",
"time2":"%02d:%02d:%02d &nbsp; %02d:%02d:%02d",
"temp2":"%.2f",
"hum2" :"%.0f",
"pres2":"%.0f"}
)rawliteral";

#if  DEBUG_ON
#define PRINT(s, v) { Serial.print(s); Serial.print(v); }    
#define PRINTX(s, v) { Serial.print(s); Serial.print(v, HEX); }  
#define PRINTS(s) Serial.print(s)   
#define PRINTLN Serial.println()
#else
#define PRINT(s, v)   
#define PRINTX(s, v)  
#define PRINTS(s)     
#define PRINTLN
#endif