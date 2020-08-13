//***************************************************************************************************
//  social-media-board:         Gets some social media key values and displays them on 
//                              some 7 segment displays.
//                              Can handle Instagram, Youtube and Udemy and publishs all values to 
//                              an MQTT server.
//                              Changes to one of the values are shown by a blinking display.
// 
//                              By Ingo Hoffmann
//***************************************************************************************************
//
//  Hardware components:
//  Board:                      LOLIN D32
//
//  Libraries used:
//    WiFi                      connect to WiFi
//    WiFiClientSecure          provide a secure client connection
//    PubSubClient              connect to MQTT
//    TM1637Display             https://github.com/avishorp/TM1637
//    ArduinoJson               https://github.com/bblanchon/ArduinoJson    
//    JsonStreamingParser       https://github.com/squix78/json-streaming-parser  
//    InstagramStats            https://github.com/witnessmenow/arduino-instagram-stats
//    YoutubeApi                https://github.com/witnessmenow/arduino-youtube-api
//    UdemyApi                  developed by Ingo Hoffmann
//
//  Dev history:
//    17.11.2019, IH            First set-up
//    11.01.2020, IH            Instagram followers
//    12.01.2020, IH            Youtube subscriber
//    11.03.2020, IH            blinking when value is new
//    12.03.2020, IH            Udemy students
//    13.03.2020, IH            refactoring, no more delay(), MQTT
//    12.08.2020, IH            changed wifi name and Insta account name
//
//***************************************************************************************************

#define VERSION                 "0.8"   // 30.03.20

// libraries
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <TM1637Display.h>
#include <ArduinoJson.h>                // needs old version 5 !!!!!!!!!!!!
#include <JsonStreamingParser.h>
#include <InstagramStats.h>
#include <YoutubeApi.h>
#include "UdemyApi.h"

// project files
#include "secrets.h"
#include "settings.h"

//***************************************************************************************************
//  Global consts
//***************************************************************************************************
const int CLK_SMB = 16;                 // common clock pin for all 7 segment displays
const int CHECK_MINUTES = 30;           // instagram api refuses results when requesting too often
const int BLINK_SECONDS = 5;            // period after which the new values fade down and up again
const int NEW_MINUTES = 360;            // a value change is regarded as new for this length of time
const int NUM_DATA_POINTS = 8;          // data points are mapped to displays
const int NUM_DISPLAYS = 6;             // here one display shows the sum of several data points

//***************************************************************************************************
//  Global enums and structs
//***************************************************************************************************
enum dataPointType{
  instagram,
  udemy,
  youtube
};

struct dataPointStruct{
  dataPointType dataType;
  const char* id;
  int lastValue;
  int currentValue;
  bool valueHasChanged;
  unsigned long timeValueHasChanged;
  const char* mqttTopic;
};

struct displayStruct {
  TM1637Display sevenSegmentDisplay;
  int value;
  bool valueHasChanged;
};

//***************************************************************************************************
//  Global data
//***************************************************************************************************
dataPointStruct dataPoints[NUM_DATA_POINTS] = {
  {instagram, "made_by_sylli", -1, 0, true, 0, "made-by-sylli"},
  {instagram, "auditnexttoangie", -1, 0, true, 0, "audit-next-to-angie"},
  {instagram, "all.me.made", -1, 0, true, 0, "all-me-made"},
  {instagram, "supereugen", -1, 0, true, 0, "supereugen"},
  {youtube, "UCffYZyo5Hc4bym0tDB1CZKg", -1, 0, true, 0, "ingotube"},
  {udemy, "1084954", -1, 0, true, 0, "explainer-videos"},
  {udemy, "1083714", -1, 0, true, 0, "erklaervideos"},
  {udemy, "892546", -1, 0, true, 0, "comic-figur"}
};

displayStruct displays[NUM_DISPLAYS] = {
  {{CLK_SMB, 18}, 0, true},
  {{CLK_SMB, 5}, 0, true},
  {{CLK_SMB, 17}, 0, true},
  {{CLK_SMB, 19}, 0, true},
  {{CLK_SMB, 23}, 0, true},
  {{CLK_SMB, 21}, 0, true}
};

WiFiClientSecure wifiClient;
InstagramStats statsInsta(wifiClient);
YoutubeApi statsYoutube(YOUTUBE_API_KEY, wifiClient);
UdemyApi statsUdemy(UDEMY_AUTHORIZATION, wifiClient);
PubSubClient mqttClient(wifiClient);

bool wifiConnected = false;
bool mqttConnected = false;
unsigned long timerStatsCheck = 0;
unsigned long timerBlink = 0;
bool fadeDown = true;                 // fading down or up
bool showDisplays = true;

//***************************************************************************************************
//  SETUP
//***************************************************************************************************
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.print("Starting social-media-dashboard by Ingo Hoffmann. Version: ");
  Serial.println(VERSION);

  for(int i = 0; i < NUM_DISPLAYS; i++) {
    displays[i].sevenSegmentDisplay.clear();
  }

  // setup wifi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifiPassword);

  // connect
  connectToWifi();
  if(!wifiConnected) {
    Serial.println("No WiFi! Restarting ...");
    delay(1000);
    ESP.restart();
  }

  Serial.print("IP address: ");
  IPAddress ip = WiFi.localIP();
  Serial.println(ip);

  // setup MQTT
  mqttClient.setServer(SECRET_MQTT_BROKER, SECRET_MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  showDisplays = true;
}

//***************************************************************************************************
//  LOOP
//***************************************************************************************************
void loop() {
  InstagramUserStats instaResponse;
  
  unsigned long timeNow = millis();
  int brightness;

  if(WiFi.status() != WL_CONNECTED) {
    connectToWifi();
  }

  if(!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();

  // time for next check?
  if(timeNow > timerStatsCheck)  {
    // get values
    Serial.println("get values");
    for(int i = 0; i < NUM_DATA_POINTS; i++) {
      Serial.print("data point ");
      Serial.println(i);
      switch(dataPoints[i].dataType) {
        case instagram:
          instaResponse = statsInsta.getUserStats(dataPoints[i].id);
          delay(1000);
          instaResponse = statsInsta.getUserStats(dataPoints[i].id);
          dataPoints[i].currentValue = instaResponse.followedByCount;
          break;
        case udemy:
          if(statsUdemy.getCourseStatistics(dataPoints[i].id)) {
            dataPoints[i].currentValue = statsUdemy.courseStats.numSubscribers;      
          } else {
            Serial.println("no udemy stats");
          }
          break;
        case youtube:
          if(statsYoutube.getChannelStatistics(dataPoints[i].id)) {
            dataPoints[i].currentValue = statsYoutube.channelStats.subscriberCount;
          } else {
            Serial.println("no youtube stats");
          }
          break;
      }
      if(dataPoints[i].currentValue != dataPoints[i].lastValue) {
        dataPoints[i].lastValue = dataPoints[i].currentValue;
        dataPoints[i].valueHasChanged = true;
        dataPoints[i].timeValueHasChanged = timeNow;
        mqttPublishValue(dataPoints[i].mqttTopic, dataPoints[i].currentValue);
        Serial.print(dataPoints[i].mqttTopic);
        Serial.print(": ");
        Serial.println(dataPoints[i].currentValue);
      } else {
        Serial.println("value is same as last query");
      }
      if(dataPoints[i].valueHasChanged && 
        (timeNow > (dataPoints[i].timeValueHasChanged + (NEW_MINUTES * 60000)))) {
        Serial.print(dataPoints[i].mqttTopic);
        Serial.println(" is not new anymore");
        dataPoints[i].valueHasChanged = false;
      }
    }
    timerStatsCheck = timeNow + (CHECK_MINUTES * 60000);
    Serial.print("waiting ");
    Serial.print(CHECK_MINUTES);
    Serial.println(" minutes");
  }

  // map data points to displays
  for(int i = 0; i < (NUM_DISPLAYS - 1); i++) {
    displays[i].value = dataPoints[i].currentValue;
    displays[i].valueHasChanged = dataPoints[i].valueHasChanged;
  }
  // add all udemy courses to give just one number
  displays[5].value = dataPoints[5].currentValue +
                      dataPoints[6].currentValue +
                      dataPoints[7].currentValue;
  displays[5].valueHasChanged = dataPoints[5].valueHasChanged ||
                                dataPoints[6].valueHasChanged ||
                                dataPoints[7].valueHasChanged;
  
  // display on?
  if(showDisplays) {
    if(timeNow > timerBlink) {
      for(int fadeDirection = 0; fadeDirection < 2; fadeDirection++) {
        for(int fadeStep = 0; fadeStep < 8; fadeStep++) {
          for(int disp = 0; disp < NUM_DISPLAYS; disp++) {
            if(displays[disp].valueHasChanged) {
              if(fadeDown) {
                brightness = 7 - fadeStep;
              } else {
                brightness = fadeStep;
              }
              displays[disp].sevenSegmentDisplay.setBrightness(brightness);
              displays[disp].sevenSegmentDisplay.showNumberDec(displays[disp].value, false);
            }
          }
        }
        fadeDown = !fadeDown;
      }
      timerBlink = timeNow + (BLINK_SECONDS * 1000);
    }
  } else {
    for(int i = 0; i < NUM_DISPLAYS; i++) {
      displays[i].sevenSegmentDisplay.clear();
    }
  }
}

//***************************************************************************************************
//  connectToWifi
//  
//***************************************************************************************************
void connectToWifi() {
  int wifiRetries = 0;

  while ((WiFi.status() != WL_CONNECTED) && wifiRetries < 8) {
    delay(800);
    wifiRetries++;
  }
  wifiConnected = (WiFi.status() == WL_CONNECTED);
  if(wifiConnected) {
    Serial.println("WiFi connected");
  } else {
    Serial.println("WiFi not connected");
  }
}

//***************************************************************************************************
//  connectToMQTT
//  
//***************************************************************************************************
void connectToMQTT() {
  int mqttRetries = 0;

  while(!mqttClient.connected() && mqttRetries < 5) {
    if(mqttClient.connect(mqttClientID, SECRET_MQTT_USER, SECRET_MQTT_PASSWORD)) {
      mqttSubscribeToTopics();
    } else {
      delay(1000);
      mqttRetries++;
    }
  }
  mqttConnected = mqttClient.connected();
  if(mqttConnected) {
    Serial.println("MQTT connected");
  } else {
    Serial.println("MQTT not connected");
  }
}

//***************************************************************************************************
//  mqttSubscribeToTopics
//  to avoid feedback loops we subscribe to command messages and send status messages
//***************************************************************************************************
void mqttSubscribeToTopics() {
  String fullTopic;

  fullTopic = mqttBase + "command/" + mqttCmdDisplay;
  mqttClient.subscribe(fullTopic.c_str());
}

//***************************************************************************************************
//  mqttCallback
//  
//***************************************************************************************************
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String fullTopic;

  fullTopic = mqttBase + "command/" + mqttCmdDisplay;

  payload[length] = '\0';
  String payloadStr = (char*) payload;

  Serial.print("MQTT message received: "); 
  Serial.print(topic); 
  Serial.print(", payload = ");
  Serial.println(payloadStr);
  
  if(strcmp(topic, fullTopic.c_str()) == 0) {
    showDisplays = ((payloadStr.toInt() == 0) ? true : false);
  } else {
    // unrecognized command
    Serial.println("unrecognized MQTT message"); 
  }
}

//***************************************************************************************************
//  mqttPublishValue
//  
//***************************************************************************************************
void mqttPublishValue(const char* topic, int value) {
  String fullTopic;

  fullTopic = mqttBase + "status/" + topic;
  mqttClient.publish(fullTopic.c_str(), String(value).c_str());
}
