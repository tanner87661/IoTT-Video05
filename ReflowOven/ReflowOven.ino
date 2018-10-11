
/*  ESP8266-12E / NodeMCU LocoNet Gateway

 *  Source code can be found here: https://github.com/tanner87661/LocoNet-MQTT-Gateway
 *  Copyright 2018  Hans Tanner IoTT
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 *  in compliance with the License. You may obtain a copy of the License at:
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  Unless required by applicable law or agreed to in writing, software distributed under the License is distributed
 *  on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License
 *  for the specific language governing permissions and limitations under the License.
 */

#define sendLogMsg

#define useIOTAppStory   //uncomment this to use OTA mechanism via IOTAppStory.com. If commented out, WifiManager will be used instead

//#define debugMode

#define useSonoff


#ifdef debugMode
  #define DEBUG_BUFFER_SIZE 1024
  bool doDebug = true;
#else
  #define DEBUG_BUFFER_SIZE 0
  bool doDebug = false;
#endif

#ifdef sendLogMsg
  #if defined(ESP8266)
    #include "EspSaveCrash.h" //standard library, install using library manager
    //https://github.com/espressif/arduino-esp32/issues/449
  #else
    #include <rom/rtc.h>
  #endif
#endif

#if defined(ESP8266)
  #ifdef useSonoff
    #define espButton 0 //onboard Button on Sonoff Basic
    #define thermoDO 3 //Input SO -- RxD Pin on Sonoff Basic
    #define thermoCLK 1  //Output SCK -- TxD Pin on Sonoff Basic
    #define espRelay 12 //onboard relay on Sonoff Basic
    #define esp12LED 13 //onboard LED on ESP32 Chip Board
    #define thermoCS 14  //Output CS -- pin 5 on Sonoff Basic
  #else
    #define espButton 0 //onboard Button on Sonoff Basic
    #define thermoDO 12 //SO -- RxD Pin on Sonoff Basic
    #define thermoCLK 14  //CS -- TxD Pin on Sonoff Basic
    #define espRelay 2 //onboard relay on Sonoff Basic
    #define esp12LED 13 //onboard LED on ESP32 Chip Board
    #define thermoCS 15  //SCK -- pin 5 on Sonoff Basic
  #endif
#else
  error
#endif

//#ifdef useIOTAppStory
  #define APPNAME "ReflowOven"
  #define VERSION "1.0.0"
  #define COMPDATE __DATE__ __TIME__
  #define MODEBUTTON espButton
//#endif

#include <PubSubClient.h> //standard library, install using library manager
#include <AutoPID.h>  //standard library, install using library manager

#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>

char mqtt_server[100] = "192.168.87.52"; // = Mosquitto Server IP "192.168.xx.xx" as loaded from mqtt.cfg
uint16_t mqtt_port = 1883; // = Mosquitto port number, standard is 1883, 8883 for SSL connection;
char mqtt_user[100] = "";
char mqtt_password[100] = "";

char* mqtt_server_field = "https://broker.hivemq.com";
char* mqtt_port_field = "1883";
char* mqtt_user_field = "User Name";
char* mqtt_password_field = "User Password";

//Outgoing Topics
char rflPingTopic[] = "pingReflow";  //ping topic, do not change. This is helpful to find Gateway IP Address if not known. 
char rflDataTopic[] = "dataReflow";  //data topic, do not change. This is helpful to find Gateway IP Address if not known. 

//Incoming Topics
char rflCommand[] = "rflCmd";
char rflCurve[] = "rflCurve";
char rflSetup[] = "rflSetup";

#include <max6675.h>
MAX6675 thermocouple(thermoCLK, thermoCS, thermoDO);

#include <ArduinoJson.h> //standard JSON library, can be installed in the Arduino IDE

#include <FS.h> //standard File System library, can be installed in the Arduino IDE
#if defined(ESP32)
  #include "SPIFFS.h"
  #define FORMAT_SPIFFS_IF_FAILED true
#endif

#include <TimeLib.h> //standard library, can be installed in the Arduino IDE
#include <Ticker.h>

#include <NTPtimeESP.h> //NTP time library from Andreas Spiess, download from https://github.com/SensorsIot/NTPtimeESP
#include <EEPROM.h> //standard library, can be installed in the Arduino IDE

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncWebServer.h>
#else
  #include <WiFi.h>
  #include <ESPAsyncWebServer.h>
  #include <WiFiClientSecure.h>
#endif
#include <DNSServer.h>

#if defined(ESP8266)
  #define ESP_getChipId()   (ESP.getChipId())
#else
  #include <esp_wifi.h>
  #define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())
#endif

//#include <RollAvgSmall.h> //used to calculate LocoNet network load

//#ifdef useIOTAppStory //OTA library, can be installed using library manager
  #include <IOTAppStory.h> //standard library, can be installed in the Arduino IDE. See https://github.com/iotappstory for more information
  IOTAppStory IAS(COMPDATE, MODEBUTTON);    // Initialize IOTAppStory
//#endif

// SET YOUR NETWORK MODE TO USE WIFI
const char* ssid = ""; //add your ssid and password. If left blank, ESP8266 will enter AP mode and let you enter this information from a browser
const char* password = "";
String NetBIOSName = "ReflowOven";

#ifdef sendLogMsg
char lnLogMsg[] = "lnLog";
#endif

char mqttMsg[800]; //buffer used to publish messages via mqtt

String ntpServer = "us.pool.ntp.org"; //default server for US. Change this to the best time server for your region
NTPtime NTPch(ntpServer); 
int ntpTimeout = 5000; //ms timeout for NTP update request

WiFiClientSecure wifiClientSec;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);


//intervall NTP server is contacted to update time/date
const uint32_t ntpIntervallDefault = 1800000; //half hour in milliseconds
const uint32_t ntpIntervallShort = 10000; //10 Seconds
const uint32_t lnLoadIntervall = 1000; //1 Seconds to measure network load
const uint32_t lnPingIntervall = 60000; //30 Seconds to send ping string to MQTT server on Topic lnPing
const uint32_t lnDataIntervall = 5000; //3 Seconds to send ping string to MQTT server on Topic lnPing
const uint32_t lnThermoIntervall = 1000; //1 Seconds to read thermocouple

#ifdef useIOTAppStory
//intervall IOTAppStory is contacted to check for new firmware version
  const uint32_t callHomeIntervall = 86400000; //change to 86400000 before deployument, check every 24 hours
#endif

strDateTime dateTime;
int timeZone = -5;

//Ajax Command sends a JSON with information about internal status
char ajaxCmdStr[] = "ajax_input";
char ajaxPingStr[] = "/ajax_ping";
char ajaxDataStr[] = "/ajax_data";
char ajaxCurveStr[] = "/ajax_curves";
//const char* PARAM_MESSAGE = "message";

//constants
const uint32_t updateEEIntervall = 60000; //wait 1 Minutes before updating EEPROM data in case more changes are coming

// OTHER VARIALBES
uint32_t ntpTimer = millis();
uint32_t lnLoadTimer = millis();
uint32_t lnPingTimer = millis();
uint32_t lnDataTimer = millis();
uint32_t lnThermoTimer = millis();
uint32_t pwmHeaterStart = millis();

int millisRollOver = 0; //used to keep track of system uptime after millis rollover
unsigned long lastMillis = 0;

//bool saveEEData = false;
//const int   memBase = 1024; //EEPROM location where data can be stored. Addresses below are reserved for OTA settings

File uploadFile;

#if defined(ESP8266)
  AsyncWebServer server(80);
#else
  AsyncWebServer server(80);
#endif

bool    ntpOK = false;
bool    useNTP = true;

double currTemp, targetTemp, targetPower;
int heatPower; //percentage 0-100%
int    workMode = 0; //0: OFF; 1: PWM (use heatPower); 2: PID (use targetTemp, set targetPower); 3: RFCurve (follow profile)
const int TempThreshold = 10; //temperature band in which we advance to next phase in RF Curve Mode

typedef struct {
  float targetTemp; //degrees celsius
  long  targetTime; //milliseconds from start of profile phase
  long  targetHold; //milliseconds from start of profile phase
} curvePoint;

typedef struct {
  String profileName;
  int numPoints;
  curvePoint reflowCurve[10];
} reflowProfile;

typedef struct {
  int numProfiles =0;   // number of valid profiles in array
  int activeProfile = -1; //index of active profile
  int curveStatus = -1;   //-1: invalid; 0: ready, not started; 1: awaiting start condition; 2: processing current step; 3: execution completed
  uint32_t startTimeProfile = 0;
  uint32_t startTimePhase = 0;
  int currentPhase = 0;
  int intraPhase = 0; //for heated phases; 0: ready; 1: heating/cooling; 2: holding;
  int sendVoiceMsg = 0;
  
  reflowProfile targetProfile[5];
  
  void loadProfile(int curveNr)
  {
    if ((curveNr >= 0) && (curveNr < numProfiles))
    {
      activeProfile = curveNr;
      curveStatus = 0; //ready to go
    }
    else
    {
      activeProfile = -1; //none selected
      curveStatus = -1; //invalid
    }
  }
  
  void startExecution()
  {
    if ((activeProfile >= 0) && (activeProfile < numProfiles) && (curveStatus == 0)) //check for valid starting conditions
    {
      curveStatus = 1; //awaiting start conditions (temp < 50C)
    }
  }
  
  void stopExecution()
  {
    activeProfile = -1; //none selected
    curveStatus = -1; //invalid
  }
  
  void executeProfile()
  {
    switch (curveStatus)
    {
      case 0 : break; //loaded but not yet started, waiting call for startExecution()
      case 1 : {
                 if (currTemp <= 50)
                 {
                   startTimeProfile = millis(); //start measuring the time
                   currentPhase = 0;
                   intraPhase = 0;
                   curveStatus = 2; //start execution of the curve because Temperature is below Starting Temp
                 }
                 break;
               }
      case 2 : {
                 switch (intraPhase)
                 {
                    case 0: { //initialize phase
                              startTimePhase = millis();
                              int oldTemp = targetTemp;
                              if (oldTemp > targetTemp)
                                sendVoiceMsg = 1;
                              targetTemp = targetProfile[activeProfile].reflowCurve[currentPhase].targetTemp;
                              intraPhase = 1;
                              break;
                            }
                    case 1: { //wait to reach target temperature
                              if ((currTemp > (targetTemp - TempThreshold)) && (currTemp < (targetTemp + TempThreshold)) && (millis() > (startTimePhase + (targetProfile[activeProfile].reflowCurve[currentPhase].targetTime * 1000))))
                              {
                                intraPhase = 2;
                              }
                              break;
                            }
                    case 2: { //wait for additional holding time
                              if ((millis() > (startTimePhase + ((targetProfile[activeProfile].reflowCurve[currentPhase].targetTime + targetProfile[activeProfile].reflowCurve[currentPhase].targetHold) * 1000))))
                              {
                                intraPhase = 0; //next step is initializtion of next phase
                                currentPhase++; // go to next phase
                                if (currentPhase >= targetProfile[activeProfile].numPoints) //completed
                                {
                                  curveStatus = 3;
                                  targetTemp = 0;
                                }
                                
                              }
                              break;
                            }
                 }   
                 break;
               }
      case 3:  {
                 sendVoiceMsg = 2;
                 workMode = 0; 
               }
      default: break; //no curve selected or invalid curve Status
      }
    }
  
} reflowProfiles;

reflowProfiles listProfiles;


//#define TEMP_READ_DELAY 800 //can only read digital temp sensor every ~750ms
#define PWM_TIME_WINDOW 5000 //PWM cycle for the relay
#define PID_TIME_WINDOW 3000 //PID cycle for calculation

//pid settings and gains
float OUTPUT_MIN = 0;
float OUTPUT_MAX = 100;
float KP = 7;
float KI = 0;
float KD = 250000000;

AutoPID myPID(&currTemp, &targetTemp, &targetPower, OUTPUT_MIN, OUTPUT_MAX, KP, KI, KD);

unsigned long lastTempUpdate; //tracks clock time of last temp update


void setup()
{
  #ifdef debugMode
    Serial.begin(115200);
  #endif
  pinMode(thermoCLK, FUNCTION_3); 
  pinMode(thermoDO, FUNCTION_3);   

  pinMode(thermoCLK, OUTPUT);
  pinMode(thermoCS, OUTPUT);
  pinMode(thermoDO, INPUT);
  
#if defined(ESP8266)
  WiFi.hostname(NetBIOSName);
#else
  WiFi.softAP(NetBIOSName.c_str());
#endif

  IAS.preSetDeviceName(String(ESP_getChipId()));        // preset deviceName this is also your MDNS responder: http://deviceName.local
  IAS.preSetAppName(F(APPNAME));     // preset appName
  IAS.preSetAppVersion(VERSION);       // preset appVersion
  IAS.preSetAutoUpdate(true);            // automaticUpdate (true, false)

//  IAS.addField(mqtt_server_field, "Textarea", 80, 'T');            // reference to org variable | field label value | max char return | Optional "special field" char
//  IAS.addField(mqtt_port_field, "Number", 8, 'I');                     // Find out more about the optional "special fields" at https://iotappstory.com/wiki
//  IAS.addField(mqtt_user_field, "textLine", 20);                        // These fields are added to the "App Settings" page in config mode and saved to eeprom. Updated values are returned to the original variable.
//  IAS.addField(mqtt_user_field, "textLine", 20);                        // These fields are added to the "App Settings" page in config mode and saved to eeprom. Updated values are returned to the original variable.
  
#ifdef useIOTAppStory
  IAS.begin('P');                                     // Optional parameter: What to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase(default) | 'L' Leave intact
  IAS.setCallHome(true);                              // Set to true to enable calling home frequently (disabled by default)
  IAS.setCallHomeInterval(300);                        // Call home interval in seconds, use 60s only for development. Please change it to at least 2 hours in production
#endif
  #ifdef debugMode
    Serial.println("Init SPIFFS");
  #endif
  SPIFFS.begin(); //File System. Size is set to 1 MB during compile time

  readNodeConfig(); //Reading configuration files from File System
  readMQTTConfig();
  readProfileConfig();
//  myPID.setGains(KP, KI, KD);
  
#ifdef sendLogMsg
  File dataFile = SPIFFS.open("/crash.txt", "a");
  if (dataFile)
  {
    #if defined(ESP8266)   
      SaveCrash.print(dataFile);
      SaveCrash.clear();
    #else
      //add code for ESP32
    #endif
      
    dataFile.close();
    #ifdef debugMode
      Serial.println("Writing Crash Dump File complete");
    #endif
  }
#endif

  pinMode(espRelay, OUTPUT);
  pinMode(esp12LED, OUTPUT);
  digitalWrite(espRelay, false);
  digitalWrite(esp12LED, true);

  //if temperature is more than 4 degrees below or above setpoint, OUTPUT will be set to min or max respectively
  targetTemp = 0;
  currTemp = thermocouple.readCelsius();
//  myPID.setBangBang(15,5);
  //set PID update interval to PWM_TIME_WINDOW
  myPID.setTimeStep(PID_TIME_WINDOW);

  
//start Watchdog Timer

#if defined(ESP8266)
#ifdef debugMode
  Serial.println("Init WDT");
#endif
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
  ESP.wdtFeed();
#else
  //add E
#endif

  startWebServer();
}

void startWebServer()
{
    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    server.on(ajaxDataStr, HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(handleJSON_Data()));
    });

    server.on(ajaxPingStr, HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(handleJSON_Ping()));
    });

    server.on(ajaxCurveStr, HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(handleJSON_Curve()));
    });


    // Send a GET request to <IP>/get?message=<message>
    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String message;
        if (request->hasParam(ajaxCmdStr)) {
            message = request->getParam(ajaxCmdStr)->value();
            #ifdef debugMode
              Serial.println(message);
            #endif
            byte newCmd[message.length()+1];
            message.getBytes(newCmd, message.length()+1);     //add terminating 0 to make it work
            if (processRflCommand(newCmd))          
              request->send(200, "text/plain", String(handleJSON_Data()));
            else
              request->send(400, "text/plain", "Invalid Command");
        } else {
            message = "No message sent";
            request->send(200, "text/plain", String(handleJSON_Ping()));
        }
    });

    server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
      String hlpStr = "/" + filename;
      if(!index)
      {
        uploadFile = SPIFFS.open(hlpStr.c_str(), "w");
        Serial.printf("UploadStart: %s\n", filename.c_str());
      }
      int byteOK = uploadFile.write(data, len);
      Serial.printf("writing %i, %i bytes to: %s\n", len, byteOK, hlpStr.c_str());
      if(final)
      {
        uploadFile.close();
        Serial.printf("Upload Complete: %s\n", hlpStr.c_str());
      }
    });



    
    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      if(!index)
        Serial.printf("BodyStart: %u\n", total);
      Serial.printf("%s", (const char*)data);
      if(index + len == total)
        Serial.printf("BodyEnd: %u\n", total);
    });
    // Send a POST request to <IP>/post with a form field message set to <message>
/*    server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request){
        String message;
        if (request->hasParam("/delete", true)) {
            message = request->getParam("/delete", true)->value();
            request->send(200, "text/plain", "Hello, POST: " + message);
        } 
        else if (request->hasParam("/upload", true)) {
            message = request->getParam("/upload", true)->value();
            request->send(200, "text/plain", "Hello, POST: " + message);
        } 
        else 
          request->send(200, "text/plain", "No Params ");
    });
*/
    server.onNotFound([](AsyncWebServerRequest *request)
    {
        Serial.printf("NOT_FOUND: ");
      if(request->method() == HTTP_GET)
        Serial.printf("GET");
      else if(request->method() == HTTP_POST)
        Serial.printf("POST");
      else if(request->method() == HTTP_DELETE)
        Serial.printf("DELETE");
      else if(request->method() == HTTP_PUT)
        Serial.printf("PUT");
      else if(request->method() == HTTP_PATCH)
        Serial.printf("PATCH");
      else if(request->method() == HTTP_HEAD)
        Serial.printf("HEAD");
      else if(request->method() == HTTP_OPTIONS)
        Serial.printf("OPTIONS");
      else
        Serial.printf("UNKNOWN");
      Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

      if(request->contentLength()){
        Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
        Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
      }
    });
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");
    server.begin();
}

void getInternetTime()
{
  int thisIntervall = ntpIntervallDefault;
  if (!ntpOK)
    thisIntervall = ntpIntervallShort;
  if (millis() > (ntpTimer + thisIntervall))
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      #ifdef debugMode
        Serial.println("getInternetTime");
      #endif
      uint32_t NTPDelay = millis();
      dateTime = NTPch.getNTPtime(timeZone, 2);
      ntpTimer = millis();
      while (!dateTime.valid)
      {
        delay(100);
//        Serial.println("waiting for Internet Time");
        dateTime = NTPch.getNTPtime(timeZone, 2);
        if (millis() > NTPDelay + ntpTimeout)
        {
          ntpOK = false;
//          Serial.println("Getting NTP Time failed");
          return;
        }
      }
      NTPDelay = millis() - NTPDelay;
      setTime(dateTime.hour, dateTime.minute, dateTime.second, dateTime.day, dateTime.month, dateTime.year);
      ntpOK = true;
      NTPch.printDateTime(dateTime);

      String NTPResult = "NTP Response Time [ms]: ";
      NTPResult += NTPDelay;
//      Serial.println(NTPResult);
    }
    else
    {
#ifdef sendLogMsg
//      Serial.println("No Internet when calling getInternetTime()");
#endif
      
    }
  }
}

String handleJSON_Data()
{
  String response;
  float float1;
  long curTime = now();
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["CurrMode"] = workMode;
  root["HeatLevel"] = heatPower;
  root["SetTempC"] = targetTemp;
  root["CurveNr"] = listProfiles.activeProfile;
  root["CurrTempC"] = currTemp;
  root["PIDOutput"] = targetPower;
  root["HeaterStatus"] = digitalRead(espRelay);
  if (listProfiles.sendVoiceMsg > 0)
  {
    root["VoiceMsg"] = listProfiles.sendVoiceMsg;
    listProfiles.sendVoiceMsg = 0;
  }
  if (listProfiles.curveStatus == 3)
  {
    listProfiles.curveStatus = 0;
    workMode = 0;
  }
  if (workMode == 3)
  {
    root["Main Status"] = listProfiles.curveStatus;
    root["Phase"] = listProfiles.currentPhase;
    root["Intra"] = listProfiles.intraPhase;
    root["ProfileTime"] = listProfiles.startTimeProfile;
    root["PhaseTime"] = listProfiles.startTimePhase;
  }
  root.printTo(response);
//  Serial.println(response);
  return response;
}

void handlePingMessage()
{
  String hlpStr = handleJSON_Ping();
  hlpStr.toCharArray(mqttMsg, hlpStr.length()+1);
  if (!mqttClient.connected())
    MQTT_connect();
  if (mqttClient.connected())
  {
    if (!mqttClient.publish(rflPingTopic, mqttMsg)) 
    {
    } else {
    }
  }
}

void handleDataMessage()
{
  String hlpStr = handleJSON_Data();
  hlpStr.toCharArray(mqttMsg, hlpStr.length()+1);
  if (!mqttClient.connected())
    MQTT_connect();
  if (mqttClient.connected())
  {
    if (!mqttClient.publish(rflDataTopic, mqttMsg)) 
    {
    } else {
    }
  }
}

void handleHeaterPWM()
{
  if (millis() > pwmHeaterStart + PWM_TIME_WINDOW)
    pwmHeaterStart += PWM_TIME_WINDOW;
  if (((100*(millis() - pwmHeaterStart) / PWM_TIME_WINDOW) <= heatPower) && (heatPower > 0))
  {
    if (digitalRead(espRelay) == false)
    {
      digitalWrite(espRelay, true);
      handleDataMessage();
    }
  }
  else
  {
    if (digitalRead(espRelay) == true)
    {
      digitalWrite(espRelay, false);
      handleDataMessage();
    }
  }
//  Serial.print(digitalRead(espRelay));
//  Serial.print(" ");
//  Serial.print(heatPower);
// Serial.print(" ");
//  Serial.println((100*(millis() - pwmHeaterStart) / PWM_TIME_WINDOW));
  digitalWrite(esp12LED, digitalRead(espRelay));
}

void handleReflowCurve()
{
  listProfiles.executeProfile();
  myPID.run();
  heatPower = round(targetPower);
  handleHeaterPWM();        
}

void loop()
{
#if defined(ESP8266)
  ESP.wdtFeed();
#else
  //add ESP32 code
#endif
  if (millis() < lastMillis)
  {
    millisRollOver++;
  //in case of Rollover, update all other affected timers
    ntpTimer = millis();
    lnLoadTimer = millis();
    lnPingTimer = millis();
    lnDataTimer = millis();
  }
  else
    lastMillis = millis(); 
     
#ifdef useIOTAppStory
  IAS.loop();                                                 // this routine handles the reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration
#endif
  //-------- Your Sketch starts from here ---------------

  if (useNTP)
    getInternetTime();

  if (mqttClient.connected())
    mqttClient.loop();
  else
    MQTT_connect();

  if (millis() > lnPingTimer + lnPingIntervall)                           // only for development. Please change it to longer interval in production
  {
    handlePingMessage();
    lnPingTimer = millis();
  }
  if (millis() > lnDataTimer + lnDataIntervall)                           // only for development. Please change it to longer interval in production
  {
    handleDataMessage();
    lnDataTimer = millis();
  }
#ifdef useSonoff  
  if (millis() > lnThermoTimer + lnThermoIntervall)                          
  {
    currTemp = thermocouple.readCelsius();
    lnThermoTimer = millis();
  }
#endif

  switch (workMode)
  {
    case 0: //OFF
      {
        heatPower = 0;
        targetTemp = 0; 
        digitalWrite(espRelay, false); 
        break;
      }
    case 1: //PWM
      {
        handleHeaterPWM();
        targetTemp = 0;
        break;
      }
    case 2: //PID
      {
        myPID.run(); //call every loop, updates automatically at certain time interval
        heatPower = round(targetPower);
        handleHeaterPWM();        
        break;
      }
    case 3: //run Reflow Curve
      {
        handleReflowCurve();
        break;
      }
  }

} //loop

// function to check existence of nested key see https://github.com/bblanchon/ArduinoJson/issues/322
bool containsNestedKey(const JsonObject& obj, const char* key) {
    for (const JsonPair& pair : obj) {
        if (!strcmp(pair.key, key))
            return true;

        if (containsNestedKey(pair.value.as<JsonObject>(), key)) 
            return true;
    }

    return false;
}

//read the MQTT config file with server address etc.
int readMQTTConfig()
{
  StaticJsonBuffer<800> jsonBuffer;
  if (SPIFFS.exists("/mqtt.cfg"))
  {
    File dataFile = SPIFFS.open("/mqtt.cfg", "r");
    if (dataFile)
    {
//      Serial.print("Reading MQTT Config File ");
//      Serial.println(dataFile.size());
      String jsonData;
      while (dataFile.position()<dataFile.size())
      {
        jsonData = dataFile.readStringUntil('\n');
        jsonData.trim();
//        Serial.println(jsonData);
      } 
      dataFile.close();
      
//      JsonObject& root = jsonBuffer.parseObject(jsonData);
//      if (root.success())
//      {
//        if (root.containsKey("modeNetwork"))
//        {
//            useNetworkMode = bool(root["modeNetwork"]);       
//        }
//        if (root.containsKey("useTimeStamp"))
//          useTimeStamp = bool(root["useTimeStamp"]);
//      }
//      else
//        Serial.println("Error Parsing JSON");
    }
  }
}

int writeMQTTConfig()
{
  StaticJsonBuffer<800> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  String newMsg = "";
  root.printTo(newMsg);
//  Serial.println(newMsg);
//  Serial.println("Writing MQTT Config File");
  File dataFile = SPIFFS.open("/mqtt.cfg", "w");
  if (dataFile)
  {
    dataFile.println(newMsg);
    dataFile.close();
//    Serial.println("Writing Config File complete");
  }
}


//read node config file with variable settings
int readNodeConfig()
{
  StaticJsonBuffer<500> jsonBuffer;
  if (SPIFFS.exists("/node.cfg"))
  {
    File dataFile = SPIFFS.open("/node.cfg", "r");
    if (dataFile)
    {
//      Serial.print("Reading Node Config File ");
//      Serial.println(dataFile.size());
      String jsonData;
      while (dataFile.position()<dataFile.size())
      {
        jsonData = dataFile.readStringUntil('\n');
        jsonData.trim();
//        Serial.println(jsonData);
      } 
      dataFile.close();
      
      JsonObject& root = jsonBuffer.parseObject(jsonData);
      if (root.success())
      {
        if (root.containsKey("NetBIOSName"))
        {
          String hlpStr = root["NetBIOSName"];
          NetBIOSName = hlpStr;
        }
        if (root.containsKey("useNTP"))
          useNTP = bool(root["useNTP"]);
        if (root.containsKey("NTPServer"))
        {
          String hlpStr = root["NTPServer"];
          ntpServer = hlpStr;
          NTPch.setNTPServer(ntpServer);
        }
        if (root.containsKey("ntpTimeZone"))
          timeZone = int(root["ntpTimeZone"]);
        if (root.containsKey("PIDParams"))
        {
          JsonObject& PIDParams = root["PIDParams"];
          if (PIDParams.containsKey("KP"))
            KP = PIDParams["KP"]; // 0.5
          if (PIDParams.containsKey("KI"))
            KI = PIDParams["KI"]; // 0.5
          if (PIDParams.containsKey("KD"))
            KD = PIDParams["KD"]; // 0.5
        }
      }
    }
  } 
}

int writeNodeConfig()
{
  StaticJsonBuffer<800> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["NetBIOSName"] = NetBIOSName;
  root["useNTP"] = int(useNTP);
  root["NTPServer"] = ntpServer;
  root["ntpTimeZone"] = timeZone;
  //add code to save Kx values
  String newMsg = "";
  root.printTo(newMsg);
//  Serial.println(newMsg);
//  Serial.println("Writing Node Config File");
  File dataFile = SPIFFS.open("/node.cfg", "w");
  if (dataFile)
  {
    dataFile.println(newMsg);
    dataFile.close();
//    Serial.println("Writing Config File complete");
  }
}

//read node config file with variable settings
int readProfileConfig()
{
  StaticJsonBuffer<800> jsonBuffer;
  if (SPIFFS.exists("/curves.cfg"))
  {
    File dataFile = SPIFFS.open("/curves.cfg", "r");
    if (dataFile)
    {
      String jsonData;
      while (dataFile.position()<dataFile.size())
      {
        jsonData += dataFile.readStringUntil('\n');
      } 
      jsonData.trim();
      dataFile.close();
      
      JsonObject& root = jsonBuffer.parseObject(jsonData);
      if (root.success())
      {
        int numCurves = int(root["NumCurves"]);
        if (numCurves > 0)
        {
          listProfiles.numProfiles = numCurves;
          #ifdef debugMode
            Serial.println(numCurves);
          #endif

          for (int i=0; i < numCurves; i++)
          {
            String myStr = root["Curves"][i]["Name"];
            listProfiles.targetProfile[i].profileName = myStr;

            int numPoints = 4;
            listProfiles.targetProfile[i].numPoints = numPoints;
            for (int j=0; j < numPoints; j++)
            {
              listProfiles.targetProfile[i].reflowCurve[j].targetTemp = float(root["Curves"][i]["Points"][j][0]);
              listProfiles.targetProfile[i].reflowCurve[j].targetTime = long(root["Curves"][i]["Points"][j][1]);
              listProfiles.targetProfile[i].reflowCurve[j].targetHold = long(root["Curves"][i]["Points"][j][2]);
            }
            #ifdef debugMode
              Serial.println(listProfiles.targetProfile[i].profileName);
            #endif
          }
        }
      }
    }
    #ifdef debugMode
      else
        Serial.println("not opened");
    #endif
    
  } 
}

//==============================================================Web Server=================================================

String extractValue(String keyWord, String request)
{
  int startPos = request.indexOf(keyWord);
  int endPos = -1;
  if (startPos >= 0)
  {
    startPos = request.indexOf("=", startPos) + 1;
//    Serial.println(startPos);
    endPos = request.indexOf("&", startPos);
//    Serial.println(endPos);
    if (endPos < 0)
      endPos = request.length();
//    Serial.println(request.substring(startPos, endPos));   
    return request.substring(startPos, endPos);  
  }
  else
    return("");
}


String handleJSON_Ping()
{
  String response;
  float float1;
  long curTime = now();
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["IP"] = WiFi.localIP().toString();
  if (WiFi.status() == WL_CONNECTED)
  {
    long rssi = WiFi.RSSI();
    root["SigStrength"] = rssi;
  }
  root["SWVersion"] = VERSION;
  root["NetBIOSName"] = NetBIOSName;
  root["useNTP"] = int(useNTP);
  root["NTPServer"] = ntpServer;  
  root["ntpTimeZone"] = timeZone;
  root["mem"] = ESP.getFreeHeap();
  float1 = (millisRollOver * 4294967.296) + millis()/1000;
  root["uptime"] = round(float1);
  if (ntpOK && useNTP)
  {
    if (NTPch.daylightSavingTime(curTime))
      curTime -= (3600 * (timeZone+1));
    else
      curTime -= (3600 * timeZone);
    root["currenttime"] = curTime;  //seconds since 1/1/1970
  }
  JsonObject& PIDParams = root.createNestedObject("PIDParams");
  PIDParams["KP"] = KP;
  PIDParams["KI"] = KI;
  PIDParams["KD"] = KD;
    
  root.printTo(response);
//  Serial.println(response);
  return response;
}

String handleJSON_Curve()
{
  //              listProfiles.targetProfile[i].reflowCurve[j].targetTemp = float(root["Curves"][i]["Points"][j][0]);
  String response;
  StaticJsonBuffer<800> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["NumCurves"] = listProfiles.numProfiles;
  JsonArray& curves = root.createNestedArray("Curves");

  for (int i = 0; i < listProfiles.numProfiles; i++)
  {
    JsonObject& thisCurve = curves.createNestedObject();
    thisCurve["Name"] = listProfiles.targetProfile[i].profileName;
    JsonArray& points = thisCurve.createNestedArray("Points");
    for (int j = 0; j < listProfiles.targetProfile[i].numPoints; j++)
    {
      JsonArray& thisPoint = points.createNestedArray();
      thisPoint.add(listProfiles.targetProfile[i].reflowCurve[j].targetTemp);
      thisPoint.add(listProfiles.targetProfile[i].reflowCurve[j].targetTime);
      thisPoint.add(listProfiles.targetProfile[i].reflowCurve[j].targetHold);
    }
  }

  root.printTo(response);
  return response;

}

void MQTT_connect() {
  // Loop until we're reconnected sdk_common.h: No such file or directoryntp--  no, not anymore, see below
  while (!mqttClient.connected()) {
    #ifdef debugMode
      Serial.println("Attempting MQTT connection...");
      Serial.print(mqtt_server);
      Serial.println(mqtt_port);
    #endif
    // Create a random client ID
    String clientId = "LNGW";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password)) 
    {
      #ifdef debugMode
        Serial.println("connected");
      #endif
      // ... and resubscribe
        mqttClient.subscribe(rflCommand);
    } else {
//      Serial.print("failed, rc=");
//      Serial.print(mqttClient.state());
//      Serial.println(" try again in 5 seconds");
      return; //break the loop to make sure web server can be accessed to enter a valid MQTT server address
    }
  }
#ifdef sendLogMsg
//  sendLogMessage("MQTT Connected");
#endif
#ifdef debugMode
  Serial.println("MQTT Connected!");
#endif
}

bool processRflCurve(byte* newCmd)
{
   StaticJsonBuffer<800> jsonBuffer;
   JsonObject& root = jsonBuffer.parseObject(newCmd);
   if (!root.success())
   {
     #ifdef debugMode
       Serial.println("invalid payload");
     #endif
     return false;
   }
   if (root.containsKey("ReportCurve"))
   {
   }
   if (root.containsKey("AddCurve"))
   {
   }
   if (root.containsKey("DeleteCurve"))
   {
   }
   if (root.containsKey("SelectProfile"))
   {
      int curveNr = int(root["SelectProfile"]);
      listProfiles.loadProfile(curveNr);
   }
   if (root.containsKey("RunProfile"))
   {
     int newStatus = int(root["RunProfile"]); 
     if (newStatus == 0)
       listProfiles.stopExecution();
     else                 //start execution
       listProfiles.startExecution();
   }
   return true;
}

bool processRflSetup(byte* newCmd)
{
   StaticJsonBuffer<800> jsonBuffer;
   JsonObject& root = jsonBuffer.parseObject(newCmd);
   if (!root.success())
   {
     #ifdef debugMode
       Serial.println("invalid payload");
     #endif
     return false;
   }
   if (root.containsKey("ReportCurve"))
   {
   }
}

bool processRflCommand(byte* newCmd)
{
    StaticJsonBuffer<800> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(newCmd);
    //double currTemp, targetTemp, heatPower;
    //int    workMode = 0; //0: OFF; 1: PWM (use heatPower); 2: PID (use targetTemp); 3: RFCurve (follow profile)
    
    if (!root.success())
    {
      #ifdef debugMode
        Serial.println("invalid payload");
      #endif
      return false;
    }
    if (root.containsKey("WorkMode")) //Must have a Valid Work Mode: OFF, RFCurve, PID, PWM
    {
      int newWorkMode = root["WorkMode"];
      
      if (newWorkMode == 0)
      {
        workMode = newWorkMode;  
        heatPower = 0;
        targetTemp = 0;      
        return true;
      } else

      if (newWorkMode == 1) //PWM Mode, set Power Output, but not temperature
      {
        workMode = newWorkMode;        
        if (root.containsKey("HeatLevel")) //now also check heat power
        {
          #ifdef debugMode
            Serial.println("decoding HeatLevel");
          #endif
          int newStatus = int(root["HeatLevel"]); 
          heatPower = newStatus;
        }
        else
          heatPower = 0;
        targetTemp = 0;
        return true;
      } else

      if (newWorkMode == 2) //Regulated output, set temperature, but not power level
      {
        workMode = newWorkMode;        
        if (root.containsKey("TargetTemp"))
        {
          double newStatus = double(root["TargetTemp"]); 
          targetTemp = newStatus;
        }
        else
          targetTemp = 0;
        heatPower = 0;
        return true;
      } else
      if (newWorkMode == 3)
      {
        workMode = newWorkMode;    
        if (root.containsKey("SelectProfile"))
        {
          int curveNr = int(root["SelectProfile"]);
          listProfiles.loadProfile(curveNr);
        }
        if (root.containsKey("RunProfile"))
        {
          int newStatus = int(root["RunProfile"]); 
          if (newStatus == 0)
            listProfiles.stopExecution();
          else                 //start execution
            listProfiles.startExecution();
        }
      }
    }
    if (root.containsKey("PIDParams"))  //{"PIDParams":[{"KP":0.5,"KI": 0.5,"KD": 0.5}]}
    {
      JsonObject& PIDParams = root["PIDParams"];
      if (PIDParams.containsKey("KP"))  
        KP = float(PIDParams["KP"]); // 0.5
      if (PIDParams.containsKey("KI"))  
        KI = PIDParams["KI"]; // 0.5
      if (PIDParams.containsKey("KD"))  
        KD = PIDParams["KD"]; // 0.5
      myPID.setGains(KP, KI, KD);
      return true;
    }
    return false;
}

//called when mqtt message with subscribed topic is received
void mqttCallback(char* topic, byte* payload, unsigned int length) 
{
  if (String(topic) == rflCommand)
  {
    processRflCommand(payload);
  }
  if (String(topic) == rflCurve)
  {
    processRflCurve(payload);
  }
  if (String(topic) == rflSetup)
  {
    processRflSetup(payload);
  }
}

// Standard Web Server Code starts here

void returnOK() {
//  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
//  server.send(500, "text/plain", msg + "\r\n");
}

//loading of web pages available in SPIFFS. By default, this is upload.htm and delete.htm. You can add other pages as needed
//note that ESP8266 has some difficulties in timing with Chrom browser when delivering larger pages. Normally no problem from Smart Phone browser
bool loadFromSdCard(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";

//  Serial.print("Load from SPIFFS - Path: ");
//  Serial.println(path);

  
  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SPIFFS.open(path.c_str(), "r");

  if (!dataFile)
  {
//    Serial.println("File not found");
    return false;
  }

//  if (server.hasArg("download")) dataType = "application/octet-stream";
//  Serial.println(dataFile.size());
  int siz = dataFile.size();

//  int i = server.streamFile(dataFile, dataType);
//  if (i != dataFile.size()) 
  {
//    Serial.println(i);
//    Serial.println("Sent less data than expected!");
  }
//    Serial.println("all sent");
  dataFile.close();
  return true;
}

void handleFileUpload()
{
//  Serial.println("Handle Upload");
//  Serial.println(server.uri());
//  if(server.uri() != "/edit") 
//    return;
//  HTTPUpload& upload = server.upload();
//  if(upload.status == UPLOAD_FILE_START)
//  {
//    if(SPIFFS.exists((char *)upload.filename.c_str())) 
//    {
//      Serial.print("Upload selected file "); Serial.println(upload.filename);
//    }
//    String hlpStr = "/" + upload.filename;
//    uploadFile = SPIFFS.open(hlpStr, "w");
//    if (!uploadFile)
//      Serial.print("Upload of file failed");
//    else
//      Serial.print("Upload: START, filename: "); Serial.println(upload.filename);
//  } 
//  else 
//    if(upload.status == UPLOAD_FILE_WRITE)
//    {
//      if (uploadFile) 
//      {
//        uploadFile.write(upload.buf, upload.currentSize);
//        Serial.print("Upload: WRITE, Bytes: "); Serial.println(upload.currentSize);
//      }
//      else
//        Serial.println("Write operation failed");
//    } 
//    else 
//      if(upload.status == UPLOAD_FILE_END)
//      {
//        if (uploadFile)
//        { 
//          uploadFile.close();
//          Serial.print("Upload: END, Size: "); Serial.println(upload.totalSize);
//        }
//        else
//          Serial.println("Closing failed");
//      }
//      else
//      {
//        Serial.print("Unknown File Status "); Serial.println(upload.status);
//      }
}

//recuersive deletion not implemented
/*

void deleteRecursive(String path){
  File file = SPIFFS.open((char *)path.c_str(), "r");
  if(!file.isDirectory()){
    file.close();
    SPIFFS.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while(true) {
    File entry = file.openNextFile();
    if (!entry) break;
    String entryPath = path + "/" +entry.name();
    if(entry.isDirectory()){
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SPIFFS.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
  
}
*/

void deleteFile(fs::FS &fs, const char * path){
//    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
//        Serial.println("- file deleted");
    } else {
//        Serial.println("- delete failed");
    }
}

void handleDelete()
{
//  Serial.println("Handle Delete");
//  Serial.println(server.uri());
//  if(server.uri() != "/delete") 
//    return;
//  String path = server.arg(0);
//  Serial.print("Trying to delete ");
//  Serial.println((char *)path.c_str());
//  if(server.args() == 0) return returnFail("BAD ARGS");
//  if(path == "/" || !SPIFFS.exists((char *)path.c_str())) {
//    returnFail("BAD PATH");
//    return;
//  }
//  deleteFile(SPIFFS, (char *)path.c_str());
//  deleteRecursive(path);
  returnOK();
}

//file creation not implemented
void handleCreate(){
/*
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if(path.indexOf('.') > 0){
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file){
      file.write((const char *)0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  */
  returnOK();
}

//print directory not implemented
void printDirectory() {
/*
  if(!server.hasArg("dir")) return returnFail("BAD ARGS /list");
  String path = server.arg("dir");
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry)
    break;

    String output;
    if (cnt > 0)
      output = ',';

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }
 server.sendContent("]");
 dir.close();
 */
}

//all calls for web pages start here
void handleNotFound(){
//this is the hook to handle async requests
//  Serial.println(server.uri());
//  if ((server.uri().indexOf(ajaxCmdStr) != -1) && handleAjaxCommand(server.uri())) {return; }
//this is the default file handler
//  if(loadFromSdCard(server.uri())) {return;}
  String message = "SPIFFS Not available or File not Found\n\n";
//  message += "URI: ";
//  message += server.uri();
//  message += "\nMethod: ";
//  message += (server.method() == HTTP_GET)?"GET":"POST";
//  message += "\nArguments: ";
//  message += server.args();
  message += "\n";
//  for (uint8_t i=0; i<server.args(); i++){
//    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
//  }
//  server.send(404, "text/plain", message);
}


char* dbgprint ( const char* format, ... )
{
  static char sbuf[DEBUG_BUFFER_SIZE] ;                // For debug lines
  va_list varArgs ;                                    // For variable number of params

  va_start ( varArgs, format ) ;                       // Prepare parameters
  vsnprintf ( sbuf, sizeof(sbuf), format, varArgs ) ;  // Format the message
  va_end ( varArgs ) ;                                 // End of using parameters
  if ( doDebug )                                         // DEBUG on?
  {
    Serial.print ( "D: " ) ;                           // Yes, print prefix
    Serial.println ( sbuf ) ;                          // and the info
  }
  return sbuf ;                                        // Return stored string
}


