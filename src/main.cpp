#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "main.h"
#include "index.h"
#include "credentials.h"
// #include "ioadafruit.h"
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <TimeLib.h>
#include <Timezone.h>


//#include "WiFi.h"
#include <WiFiClientSecure.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_ADXL345_U.h> //Accelerometer
#include <Adafruit_MQTT.h>
#include <Adafruit_MQTT_Client.h>
#if WPS_ON
  #include "esp_wps.h"
#endif
#include <ESPmDNS.h>
#include "ESPAsyncWebServer.h"
#include <Button.h>
#include <TFT_eSPI.h> // Graphics and font library for ST7735 driver chip
#include <RH_CC110.h> // Radio part
#include <SPI.h> // Not actually used but needed to compile


// --------- WPS Settings -----------------
// press configPin during the reset or power on to reset old passwords and wait for WPS button
// once receive new passwordds reset system

#if WPS_ON
  #define ESP_WPS_MODE      WPS_TYPE_PBC
  #define ESP_MANUFACTURER  "ESPRESSIF"
  #define ESP_MODEL_NUMBER  "ESP32"
  #define ESP_MODEL_NAME    "ESPRESSIF IOT"
  #define ESP_DEVICE_NAME   "ESP STATION"
  static esp_wps_config_t config;
#endif
boolean wpsNeeded=true;    // global flag for WPS main (empty) loop mode
// -----------------------------------------

// ---------- TFT Display ------------------
TFT_eSPI tft = TFT_eSPI();  // Invoke library, pins defined in platformio.ini or User_Setup_Select.h

#define backlight 32
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;


uint8_t currentState;
#define numberOfStates 4
States tableStates[] = {_Time, _Sensor1, _Sensor2, _Sensor3};

// -----------------------------------------

// ---------- Mutex for data buffer --------
SemaphoreHandle_t bufMutex; 
// -----------------------------------------



// ---------- TFT Display ------------------
Button button = Button(configPin, BUTTON_PULLUP_INTERNAL, true, 50);
// -----------------------------------------

void updateJson();
void blink();
void printData(Tdata&);
void printDateTime(Timezone, time_t, const char *);
void wpsInitConfig();
String wpspin2string(uint8_t []);
void WiFiEvent(WiFiEvent_t, system_event_info_t);
void tftUpdate(States, Timezone);
void taskMQTT( void * parameter );
boolean MQTT_connect();


// -------------- Start Time & NTP Definitions ------------------------------------------------
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.google.com", 0);     /// get UTC
// Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     // Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       // Central European Standard Time
Timezone CE(CEST, CET);
TimeChangeRule *tcr; 

time_t syncNTP();
getExternalTime NtpTime = &syncNTP;
time_t last_utc=0;

AsyncWebServer server(80);
AsyncEventSource events("/events");
// ---------------- End Time & NTP Definitions ------------------------------------------------

// --------- Define  MQTT -----------------------------------------------------------------------
WiFiClientSecure client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, IO_USERNAME, IO_KEY);
Adafruit_MQTT_Publish *MQTTTable[sensorNumber][feedNumber];

// Bug workaround for Arduino 1.6.6, it seems to need a function declaration
// for some reason (only affects ESP8266, likely an arduino-builder bug).
boolean MQTT_connect();
// -------------------------------------------------------------------------------------------

// -------- Define local sensor ---------------------------------------------------------------
Adafruit_BME280 bme;
// -------------------------------------------------------------------------------------------

// --------- global sensors and its Json represenation ----------------------------------------
Tdata sensorsData[sensorNumber];
char jsonData[370];
// --------- end sensors and its Json represenation -------------------------------------------


// --------- Radio (Head) -------
RH_CC110 cc110(5 /*slaveSelectPin SS,*/ , 4 /* interruptPin*/ , true /* bool is27MHz */);
// ------------------------------

void setup()
{
  #if  DEBUG_ON
    Serial.begin(115200);
  #endif
  
  int i;
  for (i=0; i<sensorNumber; i++) {
    sensorsData[i].timeStamp = 0;
    sensorsData[i].temperature = 0.0;
    sensorsData[i].humidity = 0.0;
    sensorsData[i].pressure = 0.0;
    sensorsData[i].battery = 0.0;
}

  // bme.setSampling(Adafruit_BME280::MODE_FORCED,
  //               Adafruit_BME280::SAMPLING_X1, // temperature
  //               Adafruit_BME280::SAMPLING_X1, // pressure
  //               Adafruit_BME280::SAMPLING_X1, // humidity
  //               Adafruit_BME280::FILTER_OFF);
  // bme.begin();
 
  pinMode(LED_BUILTIN, OUTPUT);     // setup buildin LED for future error and message info
  pinMode(configPin, INPUT_PULLUP); // setup wps and menu button as imput always high

  tft.init();
  //* Set PWM for backlight */
  ledcSetup(ledChannel, freq, resolution);
  ledcAttachPin(backlight, ledChannel);
  ledcWrite(ledChannel, 255);

  tftUpdate(_None, CE);

  //wait for WPS request via configPin button press
  i=0;
  wpsNeeded = false;
  tftUpdate(_WPS, CE);
  while (i<1000 && !wpsNeeded) {
    //wpsNeeded=!digitalRead(configPin); // GND->0->wpsNotNeeded=false  
    if (button.uniquePress()) wpsNeeded = true;
    delay(1);
    i++;
  }
  
  #if WPS_ON
  if (wpsNeeded) {
        PRINTS("Starting WPS\n");
        PRINTS("Reseting WiFi Settings\n");
        WiFi.mode(WIFI_STA);
        WiFi.disconnect(true,true);
        WiFi.onEvent(WiFiEvent);
        WiFi.mode(WIFI_MODE_STA);
        wpsInitConfig();
        esp_wifi_wps_enable(&config);
        esp_wifi_wps_start(0);
  }
  #endif

  if (!wpsNeeded) {
    tftUpdate(_WIFI, CE);
    WiFi.begin();
    i = 1;
    blink();
    while ((WiFi.status() != WL_CONNECTED) && i!=0) {
      if (button.uniquePress()) i = 0; // skip wifi run withou time current stamp
      PRINTS(".");
      blink();
    }

    // client.setCACert(root_ca);

    timeClient.begin();

    if (i!=0) {
      tftUpdate(_WIFIConnected, CE);

      if (!timeClient.update()) {
        tftUpdate(_NTPFailed, CE);
      };

      if (!MDNS.begin(localDomain)) {
          PRINTS("Error setting up MDNS responder!\n");
          while(1) {
              blink();
          }
      }
      PRINTS("mDNS responder started\n");
    }

    // Create semaphore for data buffer
    bufMutex = xSemaphoreCreateMutex(); 
    if (bufMutex == NULL) { 
        PRINTS("Mutex can not be created\n"); 
        while(1) blink(); /* no mutex => no further program exec. */
    } 

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send(200, "text/html", index_html);
    });

    server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(404);
      });
    
    events.onConnect([](AsyncEventSourceClient *client){
      //setTime(syncNTP());
      //setSyncProvider(NtpTime);
      setSyncInterval(0); // force imediate resync via syncNTP by next call of now()
      last_utc = now();   // resync and update last_utc for refreshing 
      setSyncInterval(_setSyncInterval); // restore default sync delay
      // printDateTime(CE, now(),(timeStatus()==timeSet)? " OK":(timeStatus()==timeNeedsSync)? " Need Sync":" Not Set");
      xSemaphoreTake(bufMutex, portMAX_DELAY);
        updateJson();
      xSemaphoreGive(bufMutex);
      client->send(jsonData,"data",millis());
      PRINT("\nonConnect on core: ", xPortGetCoreID());PRINTLN;
    });
    
    server.addHandler(&events);
    server.begin();
    MDNS.addService("http", "tcp", 80);

    if (!cc110.init()) {
      PRINTS("Radio init failed\n");
        while(1) blink(); /* no radio => no further program exec. */
      }
    else { // Setup radio
      cc110.setFrequency(433.0);
      cc110.setTxPower(RH_CC110::TransmitPower10dBm);  /* max transmit power not needed in receiver, test and set to 0?*/
      cc110.setModemConfig(RH_CC110::GFSK_Rb1_2Fd5_2); /* Modulation: GFSK_Rb1_2Fd5_2 (GFSK, Data Rate: 1.2kBaud, Dev: 5.2kHz, RX BW 58kHz, optimised for sensitivity) */
    }
  
    setSyncInterval(_setSyncInterval);
    setSyncProvider(NtpTime);
    last_utc = now();

    currentState = 0;  // initial display status

    for (i = 0; i < sensorNumber; i++) {
      for (int j = 0; j < feedNumber; j++) {
        // Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
        if (feedsTable[i][j] !=fD) {
          MQTTTable[i][j] = new Adafruit_MQTT_Publish(&mqtt, feedsTable[i][j].c_str());
      //   PRINT("Feed : ",i);
      //   PRINT(" , ",j);
      //   PRINT(" => ", feedsTable[i][j]); // MQTTTable[i][j]->topic);
      //   PRINTLN;
        }
      }
    }

// while(1) blink();
    // xTaskCreate(
    //   taskMQTT,   /* start regular  MQTT update task*/
    // "taskMQTT",  /* name of task. */
    //   8000,       /* Stack size of task */
    //   NULL,       /* parameter of the task */
    //   1,          /* priority of the task */
    //   NULL);      /* Task handle to keep track of created task */


    xTaskCreatePinnedToCore(
      taskMQTT,   /* start regular  MQTT update task*/
    "taskMQTT",   /* name of task. */
      8000,       /* Stack size of task */
      NULL,       /* parameter of the task */
      0,          /* priority of the task */
      NULL,       /* Task handle to keep track of created task */
      0);         /* pin to core 0, as loop is on core 1 */


  } // end WPS not needed
}

void loop()
{
   uint8_t buf[RH_CC110_MAX_MESSAGE_LEN];
   uint8_t buflen = sizeof(buf);
   uint8_t sensorID;
   uint16_t static volatile wpsSeconds = 0;

  if (wpsNeeded) {
    PRINT("WPS ", wpsSeconds); PRINTLN;
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Wlacz WPS:",0,40,4);
    tft.fillRect(0,80,160,48,TFT_BLACK);
    tft.drawNumber(wpsSeconds++, 0,80,4);
    blink();
 } else {
    if (cc110.available()) // data available => process
    if (cc110.recv(buf, &buflen)) { // Message with a good checksum received, dump it.
      #if  DEBUG_ON
        //driver.printBuffer("Got:", buf, buflen);
        PRINT("Buf Len:", buflen); PRINTLN;
        PRINT("Size of TData:", sizeof(Tdata)); PRINTLN;
      #endif
      if (buflen==dataSize) {
        //data = *(Tdata *) buf;
        sensorID=(*reinterpret_cast<Tdata *> (buf)).sensorID;
        
        xSemaphoreTake(bufMutex, portMAX_DELAY);
        memcpy(&sensorsData[sensorID],&buf, sizeof(Tdata));
        sensorsData[sensorID].timeStamp = now();
        xSemaphoreGive(bufMutex);        
        PRINT("Sensor ID: ", sensorID); PRINTLN;

        updateJson();
        events.send(jsonData,"data",millis());
        printData(sensorsData[sensorID]);
      }
    }
    if (now()-last_utc >= 1) {  // every second
      last_utc = now();
      updateJson();
      events.send(jsonData,"data",millis());
      tftUpdate(tableStates[currentState], CE);
      //printDateTime(CE, last_utc,(timeStatus()==timeSet)? " OK":(timeStatus()==timeNeedsSync)? " Need Sync":" Not Set");
    }
    if (button.uniquePress()) {
      currentState = (currentState + 1) % numberOfStates;
      tftUpdate(tableStates[currentState], CE);
      PRINT("CurrentState :", currentState);PRINTLN;

      PRINT("\nRun on core: ",xPortGetCoreID());PRINTLN;

    }
 }
}

void updateJson() {
  time_t  t = CE.toLocal(now(), &tcr);
  time_t t0 = CE.toLocal(sensorsData[0].timeStamp, &tcr);
  time_t t1 = CE.toLocal(sensorsData[1].timeStamp, &tcr);
  time_t t2 = CE.toLocal(sensorsData[2].timeStamp, &tcr);
  sprintf(jsonData,jsonStruc, 
      (timeStatus()==timeSet)? "white":(timeStatus()==timeNeedsSync)? "red":"yellow",
      hour(t), minute(t), second(t), day(t), month(t), year(t),
      hour(t0), minute(t0), second(t0), day(t0), month(t0), year(t0)%100U, 
      sensorsData[0].temperature,
      sensorsData[0].humidity,
      sensorsData[0].pressure,
      sensorsData[0].battery,
      hour(t1), minute(t1), second(t1), day(t1), month(t1), year(t1)%100U,  
      sensorsData[1].temperature,
      sensorsData[1].humidity,
      sensorsData[1].pressure,
      sensorsData[1].battery,
      hour(t2), minute(t2), second(t2), day(t2), month(t2), year(t2)%100U,  
      sensorsData[2].temperature,
      sensorsData[2].humidity,
      (float)sensorsData[0].counter);
}

time_t syncNTP() {
  bool NTPSyncOK=true;
  PRINTS("\nNTP Syncing ... ");
  // WiFi.printDiag(Serial);  
  if (WiFi.isConnected()) {
    NTPSyncOK = timeClient.forceUpdate();
  } else {
      PRINTS("\nRecovering WiFi connection\n");
      //WiFi.mode(WIFI_STA);
      WiFi.begin();
      if (WiFi.isConnected()) {
        NTPSyncOK = timeClient.forceUpdate();
        //WiFi.mode(WIFI_OFF);
      } else {
        PRINTS("\nWIFI failed ...\n");
        NTPSyncOK = false;
      }
  }
  if (NTPSyncOK) {
    PRINTS("NTP Sync OK\n");
    return timeClient.getEpochTime();
  }
  else { 
    PRINTS("NTP Sync Failed\n");
    return 0;
  }
}

void blink() {
  digitalWrite(LED_BUILTIN, HIGH);
  tft.fillCircle(LEDX, LEDY, LEDR, TFT_RED);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  tft.fillCircle(LEDX, LEDY, LEDR, TFT_BLACK);
  delay(500);
}


// ------------- Start WPS Functions --------------------------------------
void wpsInitConfig() {
 
#if WPS_ON 
  config.crypto_funcs = &g_wifi_default_wps_crypto_funcs;
  config.wps_type = ESP_WPS_MODE;
  strcpy(config.factory_info.manufacturer, ESP_MANUFACTURER);
  strcpy(config.factory_info.model_number, ESP_MODEL_NUMBER);
  strcpy(config.factory_info.model_name, ESP_MODEL_NAME);
  strcpy(config.factory_info.device_name, ESP_DEVICE_NAME);
#endif  
}

String wpspin2string(uint8_t a[]){
#if WPS_ON 
  char wps_pin[9];
  for(int i=0;i<8;i++){
    wps_pin[i] = a[i];
  }
  wps_pin[8] = '\0';
  return (String)wps_pin;
#endif  
}

void WiFiEvent(WiFiEvent_t event, system_event_info_t info){
#if WPS_ON
  switch(event){
    case SYSTEM_EVENT_STA_START:
      PRINTS("Station Mode Started\n");
      break;
    case SYSTEM_EVENT_STA_GOT_IP:
      PRINT("Connected to :", String(WiFi.SSID()));PRINTLN;
      PRINT("Got IP: ", WiFi.localIP());PRINTLN;
      PRINTS("Restarting ... in 10 seconds.");
      delay(10000);
      ESP.restart();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      PRINTS("Disconnected from station, attempting reconnection\n");
      WiFi.reconnect();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
      PRINT("WPS Successfull, stopping WPS and connecting to: ", String(WiFi.SSID()));PRINTLN;
      esp_wifi_wps_disable();
      delay(10);
      WiFi.begin();
      break;
    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
      PRINTS("WPS Failed, retrying\n");
      esp_wifi_wps_disable();
      esp_wifi_wps_enable(&config);
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
      PRINTS("WPS Timedout, retrying\n");
      esp_wifi_wps_disable();
      esp_wifi_wps_enable(&config);
      esp_wifi_wps_start(0);
      break;
    case SYSTEM_EVENT_STA_WPS_ER_PIN:
      PRINT("WPS_PIN = ", wpspin2string(info.sta_er_pin.pin_code));PRINTLN;
      break;
    default:
      break;
  }
  #endif
}
// -------------- End WPS Functions --------------------------------------

// -------------- Start debug and other print functions ------------------
void printData(Tdata& data)
{ 
    PRINT ("Sensor      = ", data.sensorID);    PRINTLN;
    PRINT ("Temperature = ", data.temperature); PRINTS(" *C\n");
    PRINT ("Pressure    = ", data.pressure);    PRINTS(" hPa\n");
    PRINT ("Humidity    = ", data.humidity);    PRINTS(" %\n");
    PRINT ("Battery     = ", data.battery);     PRINTS(" V\n");
    PRINTS("Time        = "); printDateTime(CE, data.timeStamp,(timeStatus()==timeSet)? " OK":(timeStatus()==timeNeedsSync)? " Need Sync":" Not Set");
    PRINT ("Counter     = ", data.counter);     PRINTLN;
}

void printDateTime(Timezone tz, time_t utc, const char *descr)
{
 #if DEBUG_ON
    char buf[40];
    char m[4];    // temporary storage for month string (DateStrings.cpp uses shared buffer)
    TimeChangeRule *tcr;        // pointer to the time change rule, use to get the TZ abbrev

    time_t t = tz.toLocal(utc, &tcr);
    strcpy(m, monthShortStr(month(t)));
    sprintf(buf, "%.2d:%.2d:%.2d %s %.2d %s %d %s",
        hour(t), minute(t), second(t), dayShortStr(weekday(t)), day(t), m, year(t), tcr -> abbrev);
    PRINTS(buf); PRINTS(descr); PRINTLN;
#endif
}

// ---------------- End debug and other print functions ------------------

void tftUpdate(States displayState, Timezone tz) {
  TimeChangeRule *tcr;
  
  static volatile int lastMinute = 99;
  static volatile int lastDay    = 0;
  static volatile States lastState  = _None;

  uint8_t posX, posY, colonX;
  uint8_t sensor;
  uint16_t color;
  String place;

  time_t t = tz.toLocal(now(), &tcr);

  char buf[20];

  tft.setRotation(1);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  
  if (lastState != displayState) {
    tft.fillScreen(TFT_BLACK);
  }

  if (displayState == _Init ) {
  ;
  }
  else if (displayState == _NTP_Sync) {
    ;
  }
  else if (displayState == _Time) {
    posX  = TIMEX;
    posY  = TIMEY;
    colonX = TIMEX + 64;
    if (lastMinute != minute(t) || lastState != displayState) {
      lastMinute = minute(t);
      tft.setTextColor(TFT_myGray, TFT_BLACK); // ghost 88:88 image
      tft.drawString("88:88",posX,posY,7); // Overwrite the text to clear it
      if (timeStatus()==timeSet) tft.setTextColor(TFT_WHITE);
      else                       tft.setTextColor(TFT_YELLOW);

      sprintf(buf, "%.2d:%.2d", hour(t), minute(t));
      tft.drawString(buf, posX, posY,7); 
    }
    if (second(t)%2) { // Flash the colon 0x39C4
      tft.setTextColor(TFT_myGray, TFT_BLACK);
      posX+= tft.drawChar(':', colonX, posY, 7);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    }
    else {
      tft.drawChar(':', colonX, posY, 7);
    }
    if (lastDay != day(t) || lastState != displayState) {
      lastDay = day(t);
      sprintf(buf, "%.2d.%.2d.%.2d", day(t), month(t), year(t)%100U);
      tft.fillRect (0, 85, 160, 26, TFT_BLACK);
      tft.drawCentreString(buf, 80, 85, 4); 
    }
  }
  else if (displayState == _Sensor1 || displayState == _Sensor2 || displayState == _Sensor3) {
     switch (displayState) {
      case _Sensor1:
        sensor = 0;
        color = TFT_GREEN;
        place = "Dom ";
        tft.fillTriangle(160,30,130,30,145,0, (second(t)%2)? color:TFT_BLACK);
        break;
      case _Sensor2:
        sensor = 1;
        color = TFT_BLUE;
        place = "Ogrod ";
        tft.fillCircle(145,15,15,(second(t)%2)? color:TFT_BLACK);
        break;
      case _Sensor3:
        sensor = 2;
        color = TFT_ORANGE;
        place = "Piwnica ";
        tft.fillRect(135,0,25,25,(second(t)%2)? color:TFT_BLACK);
     }
    t = tz.toLocal(sensorsData[sensor].timeStamp, &tcr);
    sprintf(buf, "%.2d:%.2d:%.2d", hour(t), minute(t), second(t));
    place += String(buf);
    tft.setTextColor(color, TFT_BLACK);
    posX = 0; posY =  0;
    tft.drawString(place, posX, posY, 2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    sprintf(buf, "T: %.1f*C", sensorsData[sensor].temperature);
    posY += 20;
    tft.drawString(buf, posX, posY, 4);
    sprintf(buf, "W: %.0f%%", sensorsData[sensor].humidity);
    posY += FONTSIZE;
    tft.drawString(buf,  posX, posY, 4);
    sprintf(buf, "C: %.0fHPa", sensorsData[sensor].pressure);
    posY += FONTSIZE;
    tft.drawString(buf, posX, posY, 4);
    sprintf(buf, "B: %.2fV", sensorsData[sensor].battery);
    posY += FONTSIZE;
    tft.drawString(buf,  posX, posY, 4);           
  }
  else if (displayState == _WIFI) {
    PRINTS("Connecting WIFI\n");
    tft.drawCentreString("WiFi ...?", 80, 85, 4);
  }
  else if (displayState == _WIFIConnected) {
    PRINTS(" connected\n");
    PRINTS(WiFi.localIP());PRINTLN;
    tft.drawCentreString("Adres IP:", 80, 10, 4);
    tft.drawCentreString(WiFi.localIP().toString(), 80, 85, 2);
    delay(5000);
  }
  else if (displayState == _NTPFailed) {
    PRINTS("1st NTP Failed\n");
    tft.drawCentreString("Blad NTP", 80, 85, 4);
    delay(5000);
  }
  else if (displayState == _MDSFailed) {
    PRINTS("MDP Failed\n");
    tft.drawCentreString("Blad MDP", 80, 85, 4);
    delay(5000);
  }
  else if (displayState == _WPS) {
    PRINTS("Connecting WPS\n");
    tft.drawCentreString("WPS ...?", 80, 85, 4);      
  }
  else {
    tft.drawCentreString("00:00", 80, 20, 7);
    tft.drawCentreString("Pogoda", 80, 85, 4);  
  }
  lastState = displayState;
}

void taskMQTT( void * parameter ) {

  char buf[40];
  TimeChangeRule *tcr;

  const TickType_t xTicksToWait = pdMS_TO_TICKS(Time2UpdateMQTT);
  // const TickType_t xTicksToWait = Time2UpdateMQTT / portTICK_PERIOD_MS;
  UBaseType_t uxHighWaterMark;

  while (true) {
    uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    // Serial.print("\nStack IN:"); Serial.println(uxHighWaterMark);
    vTaskDelay( xTicksToWait );
    if ( MQTT_connect() ) {
      xSemaphoreTake(bufMutex, portMAX_DELAY); 
      PRINTS("MQTT publishing\n");
      PRINT("\nMQTT on core: ",xPortGetCoreID());PRINTLN;
      for (int i = 0; i < sensorNumber; i++) {
        if (feedsTable[i][0] !=fD) MQTTTable[i][0] -> publish( sensorsData[i].temperature );
        if (feedsTable[i][1] !=fD) MQTTTable[i][1] -> publish( sensorsData[i].humidity );
        if (feedsTable[i][2] !=fD) MQTTTable[i][2] -> publish( sensorsData[i].pressure );
        if (feedsTable[i][3] !=fD) MQTTTable[i][3] -> publish( sensorsData[i].battery );
        if (feedsTable[i][4] !=fD) {
          time_t t = CE.toLocal(sensorsData[i].timeStamp, &tcr);
          sprintf(buf, "%.2d:%.2d:%.2d  %.2d.%.2d.%.4d", hour(t), minute(t), second(t), day(t), month(t), year(t));
          MQTTTable[i][4] -> publish( buf );
        }
      }
      xSemaphoreGive(bufMutex);        
    }
    uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
    // Serial.print("\nStack OUT:"); Serial.println(uxHighWaterMark);
  }
}


// Function to connect and reconnect as necessary to the MQTT server.
// Should be called in the loop function and it will take care if connecting.
boolean MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return true;
  }

  if (!WiFi.isConnected()) {
    PRINTS("MQTT Connecting to WIFI... ");
    WiFi.begin();
    delay(1000);
  }
  
  PRINTS("MQTT Connecting to MQTT... ");
  uint8_t retries = 3;
  while ( (ret = mqtt.connect()) != 0 && (retries > 0) ) { // connect will return 0 for connected
       PRINTS(mqtt.connectErrorString(ret));PRINTLN;
       PRINTS("Retrying MQTT connection in 5 seconds...\n");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
  }
  if (ret == 0) {
    PRINTS("MQTT Connected!\n");
    return true;
  }
  else return false;
}