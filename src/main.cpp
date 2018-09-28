#define DEBUG 1

// framework libraries:
#include <debug.hpp>
#include <IoTBase.hpp>

// application specific libraries
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`

#include <NTPClient.h>
// change next line to use with another board/shield
//#include <ESP8266WiFi.h>
#include <WiFi.h> // for WiFi shield
//#include <WiFi101.h> // for WiFi 101 shield or MKR1000
#include <WiFiUdp.h>

#include <ESPmDNS.h>
#include <ArduinoOTA.h>

#include <PubSubClient.h>

#include <Preferences.h>

#include <HTTPClient.h>

// reading internal temperatur sensor:
#ifdef __cplusplus
extern "C" {
#endif
uint8_t temprature_sens_read();
#ifdef __cplusplus
}
#endif
uint8_t temprature_sens_read();

#include "font.h"

#define DISPLAY_I2C 0x3c
#define DISPLAY_SDA 5
#define DISPLAY_SCL 4
SSD1306Wire  display(DISPLAY_I2C, DISPLAY_SDA, DISPLAY_SCL);
WiFiUDP ntpUDP;

// PIN to start configuration portal:
#define TRIGGER_PIN 0
//#define TOUCH_PIN T1 //connected to 0
#define TOUCH_PIN T3 //connected to 15
int touch_value = 100; // default value

char* mqttSubscribeValue = NULL;
float jsonValue = -99999;

// allow to overwrite the configuration from external file:

WiFiClient espClient;
PubSubClient mqttClient(espClient);

Preferences preferences;

// timezone offset
int offset = 2;

IoTBase iot;

// configuration default values:
char mqttServer[40] = "192.168.8.99"; // IP of MQTT Server - DNS might not work!
int mqttPort = 1883;
char mqttUser[40] = "";
char mqttPassword[40] = "";
char mqttSubscribeTopic[100] = "/home/ESP_Easy_Mobile/Temperature";
char mqttSubscribeTopicUnit[5] = "°C";

// directly load values from local sensor luftdaten.info:
// char jsonUrl[100] = "http://192.168.8.100/data.json";
// char jsonPath[100] = "$.sensordatavalues[0].value";

// my sensor at luftdaten.info
char jsonUrl[100] = "http://api.luftdaten.info/v1/sensor/12758/";
char jsonPath[100] = "$[1].sensordatavalues[0].value";

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", offset * 3600, 60* 60 * 1000);

HTTPClient http;
long httpRequestDelayMs = 120000;
long httpLastRequest = -httpRequestDelayMs - 100; // ensure that request is done at start of device

void displayText(String text){
  display.setColor(WHITE);
  display.setFont(Dialog_plain_40);
  // works: display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, text);
}

float readInternalTemperature() {
  uint8_t temperatureInF = temprature_sens_read();
  uint8_t temperatureInC = (temperatureInF - 32) / 1.8;
  // internal termperature sensor is off, so 'calibrate' it
  // not very precise, but better than nothing :-)
  // see https://community.blynk.cc/t/esp32-internal-sensors/23041/44
  // for different systems, this correction probably needs to be changed!
  temperatureInC -= 12; // correction to real temperature
  return temperatureInC;
}

void displayTemperature() {
  uint8_t temperatureInC = readInternalTemperature();

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  char tempString[8];
  sprintf(tempString, "%2d%s", temperatureInC, mqttSubscribeTopicUnit);
  display.drawString(0, 4, tempString);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
 
  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
 
  Serial.print("Message value: ");
  payload[length] = '\0';
  // String stringValue = String((char*)payload);
  // float floatValue = stringValue.toFloat();
  mqttSubscribeValue = (char*) payload;
 
  Serial.println(mqttSubscribeValue);
}

void displayMqttValue() {
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_10);
  char tempString[10];
  if (mqttSubscribeValue != NULL) {
    sprintf(tempString, "%s%s", mqttSubscribeValue, mqttSubscribeTopicUnit);
  } else {
    sprintf(tempString, "%s%s", "??", mqttSubscribeTopicUnit);
  }
  display.drawString(display.width(), 4, tempString);
}

void fetchJsonValue() {
  Serial.println("requesting data via http");
  // from: https://github.com/espressif/arduino-esp32/blob/master/libraries/HTTPClient/examples/ReuseConnection/ReuseConnection.ino
  http.begin(jsonUrl);

  int httpCode = http.GET();
  if(httpCode > 0) {
    Serial.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println(payload);    

      jsonValue = iot.parseJson(&payload[0], jsonPath);
    }
  } else {
    Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }
  http.end();
}

void displayJsonValue() {
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  char tempString[10];
  if (jsonValue != NO_NUMBER_F) {
    sprintf(tempString, "%.1f%s", jsonValue, "ug");
  } else {
    sprintf(tempString, "%s%s", "??", "ug");
  }
  display.drawString(display.width() / 2, 4, tempString);
}

void mqttConnect() {
  // Loop until we're reconnected
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect("ESP32Client", mqttUser, mqttPassword)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //mqttClient.publish("outTopic","hello world");
      // ... and resubscribe
      mqttClient.subscribe(mqttSubscribeTopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
    }
  }
}

int getJsonValueIfExists(JsonObject &json, const char *key, int defaultValue) {
  if (json[key] != NULL) {
    return json[key];
  }
  DEBUG_PRINTF("no configuration found for key %s\n", key);
  return defaultValue;
}

void setJsonValueIfExists(char *value, JsonObject& json, const char *key) {
  if (json[key] != NULL) {
    strcpy(value, json[key]);
  } else {
    DEBUG_PRINTF("no configuration found for key %s\n", key);
  }
}

// load configuration (file or GUI) into variables
void loadConfigCallback(JsonObject& json) {
    DEBUG_PRINTLN("loadConfigCallback called");
    setJsonValueIfExists(mqttServer, json, "mqtt_server");
    mqttPort = getJsonValueIfExists(json, "mqtt_port", mqttPort);
    setJsonValueIfExists(mqttUser, json, "mqtt_user");
    setJsonValueIfExists(mqttPassword, json, "mqtt_password");
    setJsonValueIfExists(mqttSubscribeTopic, json, "mqtt_topic");
    setJsonValueIfExists(mqttSubscribeTopicUnit, json, "mqtt_topic_unit");
    setJsonValueIfExists(jsonUrl, json, "json_url");
    setJsonValueIfExists(jsonPath, json, "json_path");
    Serial.printf("mqtt_server = %s\n", mqttServer);
    Serial.printf("mqtt_port = %i\n", mqttPort);
    Serial.printf("mqtt_topic_unit = %s\n", mqttSubscribeTopicUnit);
    Serial.printf("json_url = %s\n", jsonUrl);
    Serial.printf("json_path = %s\n", jsonPath);
}

// save variables into configuration
void saveConfigCallback(JsonObject& json) {
    DEBUG_PRINTLN("saveConfigCallback called");
    json["mqtt_server"] = mqttServer;
    json["mqtt_port"] = mqttPort;
    json["mqtt_user"] = mqttUser;
    json["mqtt_password"] = mqttPassword;
    json["mqtt_topic"] = mqttSubscribeTopic;
    json["mqtt_topic_unit"] = mqttSubscribeTopicUnit;
    json["json_url"] = jsonUrl;
    json["json_path"] = jsonPath;
}

void setBrightness(uint8_t brightness) {
  uint8_t contrast = brightness;
  if (brightness < 128) {
    // Magic values to get a smooth/ step-free transition
    contrast = brightness * 1.171;
  } else {
    contrast = brightness * 1.171 - 43;
  }

  uint8_t precharge = 241;
  if (brightness == 0) {
    precharge = 0;
  }
  uint8_t comdetect = brightness / 8;

  display.setContrast(contrast, precharge, comdetect);
}


void setup() {
  Serial.begin(115200);

  //clean FS, for testing
  //SPIFFS.format();

  // preferences.begin("NTP", true);
  // offset = preferences.getInt("offset", offset);
  // Serial.print("offset from preferences: ");
  // Serial.println(offset);
  // preferences.end();

  timeClient.setTimeOffset(offset*3600);

  // pinMode(0,INPUT);
  // digitalWrite(0,HIGH);

  display.init();
  //display.flipScreenVertically();

  iot.setLoadConfigCallback(loadConfigCallback);
  iot.setSaveConfigCallback(saveConfigCallback);

  // if there was a configuration saved in SPIFFS, load it and call callback 
  iot.readConfiguration();

  iot.addParameter("mqtt_server", "mqtt server", mqttServer, 40);
  iot.addParameter("mqtt_port", "mqtt port", String(mqttPort), 6);
  iot.addParameter("mqtt_user", "mqtt user", mqttUser, 40);
  iot.addParameter("mqtt_password", "mqtt password", mqttPassword, 40);
  iot.addParameter("mqtt_topic", "mqtt topic", mqttSubscribeTopic, 100);
  iot.addParameter("mqtt_topic_unit", "mqtt topic unit", mqttSubscribeTopicUnit, 5);
  iot.addParameter("json_url", "json_url", jsonUrl, 100);
  iot.addParameter("json_path", "json_path", jsonPath, 100);

  iot.begin();
  //if you get here you have connected to the WiFi

  // display.clear();
  // while ( WiFi.status() != WL_CONNECTED ) {
  //   DEBUG_PRINTF("wifi status %i\n", WiFi.status());
  //   displayText("WIFI?");
  //   display.display();
  //   delay ( 500 );
  // }

  char deviceName[15];
  uint64_t chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
  uint16_t chip = (uint16_t)(chipid>>32);
  snprintf(deviceName,15,"CDEsp32-%04X",chip);
  
  // print device name to be used for OTA (platform.ini):
  Serial.print("ESP device name: ");
  Serial.println(deviceName);


  // OTA setup:
  /* create a connection at port 3232 */
  ArduinoOTA.setPort(3232);
  /* we use mDNS instead of IP of ESP32 directly */
  ArduinoOTA.setHostname(deviceName);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");
 
  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  
  /* this callback function will be invoked when updating start */
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
 
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  /* this callback function will be invoked when updating end */
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd updating");
  });
  /* this callback function will be invoked when a number of chunks of software was flashed
  so we can use it to calculate the progress of flashing */
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  /* this callback function will be invoked when updating error */
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  /* start updating */
  ArduinoOTA.begin();
  Serial.print("ESP IP address: ");
  Serial.println(WiFi.localIP());  

  display.clear();
  displayText("NTP");
  display.display();

  // NTP
  timeClient.begin();

  while (!timeClient.forceUpdate()){
    delay(10);
  }

  // MQTT Server
  display.clear();
  displayText("MQTT");
  display.display();
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);

  mqttConnect();

  // allow reuse (if server supports it)
  http.setReuse(true);

  Serial.println("setup end");
}

char buffer[5];

void loop() {
  //Serial.println("loop start");

  ArduinoOTA.handle();
  //Serial.println("After OTA handle");

  // start configuration portal:
  if (digitalRead(TRIGGER_PIN) == LOW ) {
      Serial.println("Triggered pin low!");
      iot.restartWithConfigurationPortal();
  }


  if (!mqttClient.connected()) {
    Serial.println("Try to connect to mqtt broker");
    mqttConnect();
  }

  //Serial.println("MQTT loop");
  mqttClient.loop();
  //Serial.println("NTP update");
  timeClient.update();

  // change time zone (offset):
  // reading touch value disabled, does not work reliable on single wire
  // needs some real switch or bigger metal plate to push on
  // see https://nick.zoic.org/art/esp32-capacitive-sensors/
  // touch_value = touchRead(TOUCH_PIN);
  if (touch_value < 20 ){
    Serial.print("Touch Value: ");
    Serial.println(touch_value);

    offset += 1;
    if (offset > 14) {
      offset = -11;
    }
  
    Serial.print("Set time offset to: ");
    Serial.println(offset);
    timeClient.setTimeOffset(offset*3600);

    preferences.begin("NTP", false);
    preferences.putInt("offset", offset);
    preferences.end();
  }

  if (millis() - httpLastRequest > httpRequestDelayMs) {
    httpLastRequest = millis();
    fetchJsonValue();
  }

  //Serial.println("display start");
  display.clear();

  //display second bar
  display.fillRect(1, 0, display.width() * timeClient.getSeconds() / 59, 2);

  displayTemperature();
  displayMqttValue();
  displayJsonValue();

  //display time
  sprintf(buffer, "%2d:%02d", timeClient.getHours(), timeClient.getMinutes());
  displayText(buffer);

  // display.setTextAlignment(TEXT_ALIGN_RIGHT);
  // display.setFont(ArialMT_Plain_10);
  // display.drawString(display.width(), 4, "12345678890");

  setBrightness(128);

  display.display();

  delay(300);
}