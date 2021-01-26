#define DEBUG 1
#include <debug.hpp>

#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#define TIME_GMT_OFFSET_SECS 3600
#define TIME_DAYLIGHT_OFFSET_SEC 3600


#include <debug.hpp>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson


// application specific libraries
#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`

// change next line to use with another board/shield
//#include <ESP8266WiFi.h>
#include <WiFi.h> // for WiFi shield
//#include <WiFi101.h> // for WiFi 101 shield or MKR1000

#include <IotWebConf.h>

#include <Arduino.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>


#include <PubSubClient.h>

#include <HTTPClient.h>

// #include <Adafruit_Sensor.h>
// #include <DHT.h>
// #include <DHT_U.h>

// display
#include "font.h"

#define DISPLAY_I2C 0x3c
#define DISPLAY_SDA 5
#define DISPLAY_SCL 4
SSD1306Wire  display(DISPLAY_I2C, DISPLAY_SDA, DISPLAY_SCL);

// DHT22
// #define DHTPIN            2         // Pin which is connected to the DHT sensor.
// #define DHTTYPE           DHT22     // DHT 22 (AM2302)
// DHT_Unified dht(DHTPIN, DHTTYPE);
// uint32_t sensorDelayMs;
// long sensorLastRequest = 0;

// light sensor
#define LIGHT_SENSOR_PIN A0

#define NO_NUMBER_F -99999

boolean jsonValueReceived = false;
float jsonValue = NO_NUMBER_F;

bool wifiStarted = false;

char* mqttSubscribeValue = NULL;

// allow to overwrite the configuration from external file:

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// IotWebConf configuration:
// -------------------------
// -- Configuration specific key. The value should be modified if config structure was changed.
#define CONFIG_VERSION "clk1"
// -- Status indicator pin.
//      First it will light up (kept LOW), on Wifi connection it will blink,
//      when connected to the Wifi it will turn off (kept HIGH).
// not working on my esp32s :-(
#define STATUS_PIN LED_BUILTIN

#ifdef ESP8266
String ChipId = String(ESP.getChipId(), HEX);
#elif ESP32
String ChipId = String((uint32_t)ESP.getEfuseMac(), HEX);
#endif

String thingName = String("esp32clock-") + ChipId;
// Initial password to connect to the Thing, when it creates an own Access Point.
const char wifiInitialApPassword[] = "clck1234";

// -- Method declarations.
void handleRoot();
// -- Callback methods.
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);

DNSServer dnsServer;
WebServer server(80);

IotWebConf iotWebConf(thingName.c_str(), &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

#define STRING_LEN 128
#define NUMBER_LEN 32

#define DEFAULT_MQTT_SERVER "192.168.8.99" // IP of MQTT Server - DNS might not work!
#define DEFAULT_MQTT_PORT "1883"
#define DEFAULT_MQTT_USER ""
#define DEFAULT_MQTT_PASSWORD ""
#define DEFAULT_MQTT_SUBSCRIBE_TOPIC "/home/ESP_Easy_Mobile/Temperature"
#define DEFAULT_MQTT_SUBSCRIBE_TOPIC_UNIT "°C"
#define DEFAULT_JSON_URL "http://data.sensor.community/airrohr/v1/sensor/12758/"
#define DEFAULT_JSON_PATH "$[1].sensordatavalues[0].value"
// directly load values from local sensor luftdaten.info:
// "http://192.168.8.100/data.json" / "$.sensordatavalues[0].value";

// configuration default values:
char mqttServerParamValue[STRING_LEN]; 
char mqttPortParamValue[NUMBER_LEN];
char mqttUserParamValue[STRING_LEN];
char mqttPasswordParamValue[STRING_LEN];
char mqttSubscribeTopicParamValue[STRING_LEN];
char mqttSubscribeTopicUnitParamValue[STRING_LEN];

// my sensor at luftdaten.info
char jsonUrlParamValue[STRING_LEN];
char jsonPathParamValue[STRING_LEN];

iotwebconf::ParameterGroup paramGroup = iotwebconf::ParameterGroup("group1", "Configuration");
iotwebconf::TextParameter jsonUrlParam = iotwebconf::TextParameter("Json Url", "jsonUrlParam", jsonUrlParamValue, STRING_LEN, DEFAULT_JSON_URL);
iotwebconf::TextParameter jsonPathParam = iotwebconf::TextParameter("Json Path", "jsonPathParam", jsonPathParamValue, STRING_LEN, DEFAULT_JSON_PATH);
iotwebconf::TextParameter mqttServerParam = iotwebconf::TextParameter("MQTT Server", "mqttServerParam", mqttServerParamValue, STRING_LEN, DEFAULT_MQTT_SERVER);
iotwebconf::NumberParameter mqttPortParam = iotwebconf::NumberParameter("MQTT Port", "mqttPortParam", mqttPortParamValue, NUMBER_LEN, DEFAULT_MQTT_PORT, "e.g. 1883", "step='1'");
iotwebconf::TextParameter mqttUserParam = iotwebconf::TextParameter("MQTT User", "mqttServerParam", mqttUserParamValue, STRING_LEN, DEFAULT_MQTT_USER);
iotwebconf::PasswordParameter mqttPasswordParam = iotwebconf::PasswordParameter("MQTT Password", "mqttServerParam", mqttPasswordParamValue, STRING_LEN, DEFAULT_MQTT_PASSWORD);
iotwebconf::TextParameter mqttSubscribeTopicParam = iotwebconf::TextParameter("MQTT Topic", "mqttTopicParam", mqttSubscribeTopicParamValue, STRING_LEN, DEFAULT_MQTT_SUBSCRIBE_TOPIC);
iotwebconf::TextParameter mqttSubscribeTopicUnitParam = iotwebconf::TextParameter("MQTT Topic Unit", "mqttTopicUnitParam", mqttSubscribeTopicUnitParamValue, STRING_LEN, DEFAULT_MQTT_SUBSCRIBE_TOPIC_UNIT);

boolean needMqttConnect = false; 
bool needReset = false;


HTTPClient http;
long httpRequestDelayMs = 120000;
long httpLastRequest = -httpRequestDelayMs - 100; // ensure that request is done at start of device
long lastActionDelayMs = 300;
long lastAction = 0;

void handleRoot()
{
  // Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // Captive portal request were already served.
    return;
  }
  int64_t uptimeTotalSeconds = esp_timer_get_time() / 1000 / 1000;
  int seconds = (uptimeTotalSeconds % 60);
  int minutes = (uptimeTotalSeconds % 3600) / 60;
  int hours = (uptimeTotalSeconds % 86400) / 3600;
  int days = (uptimeTotalSeconds % (86400 * 30)) / 86400;

  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>Configuration</title></head><body><h1>Status page of ";
  s += iotWebConf.getThingName();
  s += "<h2>Current Values</h2>";
  s += "<ul>";
  s += "<li>Last value: ";
  s += jsonValue;
  s += "</il>";
  s += "<li>Value received: ";
  s += jsonValueReceived ? "yes" : "no";
  s += "</il>";
  s += "<li>Uptime: ";
  s += days;
  s += " days ";
  s += hours;
  s += " hours ";
  s += minutes;
  s += " minutes ";
  s += seconds;
  s += " seconds";
  s += "</il>";
  s += "</ul>";
  s += "</h1><h2>Configuration</h2>";
  s += "<ul>";
  s += "<li>Json Url: ";
  s += jsonUrlParamValue;
  s += "</il>";
  s += "<li>Json Path: ";
  s +=jsonPathParamValue;
  s += "</il>";
  s += "<li>MQTT Server: ";
  s += mqttServerParamValue;
  s += "</il>";
  s += "<li>MQTT Port: ";
  s += mqttPortParamValue;
  s += "</il>";
  s += "<li>MQTT User: ";
  s += mqttUserParamValue;
  s += "</il>";
  s += "<li>MQTT Topic: ";
  s += mqttSubscribeTopicParamValue;
  s += "</il>";
  s += "<li>MQTT Topic Unit: ";
  s += mqttSubscribeTopicUnitParamValue;
  s += "</il>";
  s += "</ul>";
  s += "<div>Go to <a href='config'>configure page</a> to change configuration.</div>";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void setupOTA() {
  // Port defaults to 3232
  // ArduinoOTA.setPort(3232);

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd updating");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });

  ArduinoOTA.begin();

  Serial.println("OTA Initialized");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

// parse jsonPaths like $.foo[1].bar.baz[2][3].value equals to foo[1].bar.baz[2][3].value
float parseJson(char* jsonString, char *jsonPath) {
    float value;
    DynamicJsonBuffer jsonBuffer;
    
    JsonVariant root = jsonBuffer.parse(jsonString);
    JsonVariant element = root;

    if (root.success()) {
        // parse jsonPath and navigate through json object:
        char pathElement[40];
        int pathIndex = 0;

        DEBUG_PRINTF("parsing '%s'\n", jsonPath);
        for (int i = 0; jsonPath[i] != '\0'; i++){
            if (jsonPath[i] == '$') {
                element = root;
            } else if (jsonPath[i] == '.') {
                if (pathIndex > 0) {
                    pathElement[pathIndex++] = '\0';
                    // printf("pathElement '%s'\n", pathElement);
                    pathIndex = 0;
                    element = element[pathElement];
                    if (!element.success()) {
                        DEBUG_PRINTF("failed to parse key %s\n", pathElement);
                    }
                }
            } else if ((jsonPath[i] >= 'a' && jsonPath[i] <= 'z') 
                    || (jsonPath[i] >= 'A' && jsonPath[i] <= 'Z') 
                    || (jsonPath[i] >= '0' && jsonPath[i] <= '9')
                    || jsonPath[i] == '-' || jsonPath[i] == '_'
                    ) {
                pathElement[pathIndex++] = jsonPath[i];
            } else if (jsonPath[i] == '[') {
                if (pathIndex > 0) {
                    pathElement[pathIndex++] = '\0';
                    // printf("pathElement '%s'\n", pathElement);
                    pathIndex = 0;
                    element = element[pathElement];
                    if (!element.success()) {
                        DEBUG_PRINTF("failed in parsing key %s\n", pathElement);
                    }
                }
            } else if (jsonPath[i] == ']') {
                pathElement[pathIndex++] = '\0';
                int arrayIndex = strtod(pathElement, NULL);
                // printf("index '%s' = %d\n", pathElement, arrayIndex);
                pathIndex = 0;
                element = element[arrayIndex];
                if (!element.success()) {
                    DEBUG_PRINTF("failed in parsing index %d\n", arrayIndex);
                }
            }
        }  
        // final token if any:
        if (pathIndex > 0) {
            pathElement[pathIndex++] = '\0';
            // printf("pathElement '%s'\n", pathElement);
            pathIndex = 0;
            element = element[pathElement];
            if (!element.success()) {
                DEBUG_PRINTF("failed in parsing key %s\n", pathElement);
            }
        }

        value = element.as<float>();

        //jsonValue = measurements[1]["sensordatavalues"][0]["value"];
        DEBUG_PRINTF("success reading value: %f\n", value);
    } else {
        value = NO_NUMBER_F;
        DEBUG_PRINTLN("could not parse json for value");
    }
    return value;
}


float fetchJsonValue() {
  float value = NO_NUMBER_F;
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("requesting data via http");
    // from: https://github.com/espressif/arduino-esp32/blob/master/libraries/HTTPClient/examples/ReuseConnection/ReuseConnection.ino
    // use WiFiClient / WiFiSecureClient https://github.com/espressif/arduino-esp32/issues/3347
    WiFiClient client;
    HTTPClient http;
    http.begin(client, jsonUrlParamValue);

    int httpCode = http.GET();
    if(httpCode > 0) {
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);

      // file found at server
      if(httpCode == HTTP_CODE_OK) {
        Serial.println("Trace before http.getString");    
        String payload = http.getString();
        Serial.println(payload);    

        Serial.println("Trace before parseJson");    
        value = parseJson(&payload[0], jsonPathParamValue);
        Serial.println("Trace after parseJson");    
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    Serial.println("Trace before http.end");    
    http.end();    
    Serial.println("Trace after http.end");    
  }
  return value;
}


uint8_t _wifiQualityMeasurements[10];
uint8_t _wifiQualityMeasurementsIndex = 0;

/** 
 * take the last 10 Wifi quality values to get a stable average
 */
void recordWifiQuality() {
    if (WiFi.status() == WL_CONNECTED) {
        long dBm = WiFi.RSSI(); // values between -50 (good) and -100 (bad)
        long quality = (uint8_t) 2 * (dBm + 100);
        //DEBUG_PRINTF2("Wifi rssi=%ld, quality=%ld\n", dBm, quality);

        _wifiQualityMeasurements[_wifiQualityMeasurementsIndex++] = quality;
        if (_wifiQualityMeasurementsIndex >= 10) {
            _wifiQualityMeasurementsIndex = 0;
        }
    }
}

uint8_t getWifiQuality() {
    // calculate average of last 10 measurements:
    uint16_t sum = 0;
    for (uint8_t index = 0; index < 10; index++) {
        sum += _wifiQualityMeasurements[index];
    }
    return (uint8_t) (sum / 10);
}


void displayWifiRSSI() {
  int maxHeight = 4;
  if (WiFi.status() == WL_CONNECTED) {
    uint8_t quality = getWifiQuality();
    ESP_LOGD(TAG, "Wifi Quality: %d", quality);
    uint8_t numberOfBarsToShow = (quality - 1) / 25 + 1;
    int x = display.getWidth() - 7; // 4 bars of 1px + 3x space between bars
    // print 4 bars for wifi quality:
    for (int barIndex = 0; barIndex < numberOfBarsToShow; barIndex++) {
        // display.fillRect(59 + (b*5),33 - (b*5),3,b*5,WHITE); 
        uint8_t barHeight = barIndex + 1; // FIXME: relate to maxHeigth!
        display.fillRect(x + 2 * barIndex, maxHeight - barHeight, 1, barHeight); 
    }
  } else {
    display.setColor(WHITE);
    int x = display.getWidth() - maxHeight;
    display.drawLine(x, 0, display.getWidth() - 1 , maxHeight);
    display.drawLine(x, maxHeight, display.getWidth() - 1 , 0);
  }
}

void displayText(String text){
  display.setColor(WHITE);
  display.setFont(Dialog_plain_40);
  // works: display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, text);
}

uint16_t getXForSecond(int second) {
  // leave 20 pixel on right edge for wifi strength) 
  return (display.width() - 20) * second / 59;
}

void displayTime() {
  char timeStr[6];
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  strftime(timeStr, 6, "%H:%M", &timeinfo);
  displayText(timeStr);

  //display second bar
  display.fillRect(1, 0, getXForSecond(timeinfo.tm_sec), 2);

  // show 15sec markers in seconds bar:
  display.setColor(INVERSE);
  for (int second = 0; second <= 60; second+=10) {
    display.setPixel(getXForSecond(second), 0);
  }
  display.setColor(WHITE);

  char dateStr[14];
  strftime(dateStr, 14, "%d.%m.%Y", &timeinfo);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(display.getWidth(), display.getHeight() - 10, dateStr);
}

// void readSensorTemperature() {
//   sensors_event_t event;  
//   dht.temperature().getEvent(&event);
//   // returns value or nan - check with isnam(value)
//   //return event.temperature;
//   if (isnan(event.temperature)) {
//     Serial.println("Error reading temperature!");
//   } else {
//     Serial.print("Temperature: ");
//     Serial.print(event.temperature);
//     Serial.println(" *C");
//   }
// }

// void readSensorHumidity() {
//   // Get humidity event and print its value.
//   sensors_event_t event;  
//   dht.humidity().getEvent(&event);
//   // returns value or nan - check with isnam(value)
//   //return event.relative_humidity;
//   if (isnan(event.relative_humidity)) {
//     Serial.println("Error reading humidity!");
//   } else {
//     Serial.print("Humidity: ");
//     Serial.print(event.relative_humidity);
//     Serial.println("%");
//   }
// }
  

void displayTemperature() {
  uint8_t temperatureInC = 0;

  // if (millis() - sensorLastRequest > sensorDelayMs) {
  //   sensorLastRequest = millis();
  //   readSensorTemperature();
  //   readSensorHumidity();
  // }

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  char tempString[8];
  sprintf(tempString, "%2d%s", temperatureInC, mqttSubscribeTopicUnitParamValue);
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
    sprintf(tempString, "%s%s", mqttSubscribeValue, mqttSubscribeTopicUnitParamValue);
  } else {
    sprintf(tempString, "%s%s", "??", mqttSubscribeTopicUnitParamValue);
  }
  display.drawString(display.width(), 4, tempString);
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

// read value from analog light sensor and set as brightness:
void autoBrightnessFromLightSensor() {
  int rawValue = analogRead(LIGHT_SENSOR_PIN); 
  //Serial.printf("light: %d\n", rawValue);
  // FIXME: see https://learn.sparkfun.com/tutorials/tsl2561-luminosity-sensor-hookup-guide/all
  int displayBrightness = rawValue / 16;

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  char tempString[8];
  sprintf(tempString, "%3d", displayBrightness);
  display.drawString(0, display.getHeight() - 10, tempString);

  setBrightness(displayBrightness);
}

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("wifi", ESP_LOG_WARN); 

  // configuration param setup:
  paramGroup.addItem(&jsonUrlParam);
  paramGroup.addItem(&jsonPathParam);
  paramGroup.addItem(&mqttServerParam);
  paramGroup.addItem(&mqttPortParam);
  paramGroup.addItem(&mqttUserParam);
  paramGroup.addItem(&mqttPasswordParam);
  paramGroup.addItem(&mqttSubscribeTopicParam);
  paramGroup.addItem(&mqttSubscribeTopicUnitParam);

  iotWebConf.addParameterGroup(&paramGroup);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);

  iotWebConf.setStatusPin(STATUS_PIN);

  iotWebConf.init();

  // Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", [] { iotWebConf.handleConfig(); });
  server.onNotFound([]() {
    iotWebConf.handleNotFound();
  });


  display.init();
  //display.flipScreenVertically();


  // display.clear();
  // while ( WiFi.status() != WL_CONNECTED ) {
  //   DEBUG_PRINTF("wifi status %i\n", WiFi.status());
  //   displayText("WIFI?");
  //   display.display();
  //   delay ( 500 );
  // }

  // DHT22
  // dht.begin();
  //   sensor_t sensor;
  // dht.temperature().getSensor(&sensor);
  // Serial.println("------------------------------------");
  // Serial.println("Temperature");
  // Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  // Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  // Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  // Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" *C");
  // Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" *C");
  // Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" *C");  
  // Serial.println("------------------------------------");
  // // Print humidity sensor details.
  // dht.humidity().getSensor(&sensor);
  // Serial.println("------------------------------------");
  // Serial.println("Humidity");
  // Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  // Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  // Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  // Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println("%");
  // Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println("%");
  // Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println("%");  
  // Serial.println("------------------------------------");
  // Set delay between sensor readings based on sensor details.
  // sensorDelayMs = sensor.min_delay / 1000;

  // light sensor
  pinMode(LIGHT_SENSOR_PIN, INPUT);


  // print device name to be used for OTA (platform.ini):
  Serial.print("ESP device name for OTA: ");
  Serial.println(thingName);

  Serial.println("setup end");
}

void mqttConnect() {
  // Loop until we're reconnected
  if (!mqttClient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttClient.connect(thingName.c_str(), mqttUserParamValue, mqttPasswordParamValue)) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      //mqttClient.publish("outTopic","hello world");
      // ... and resubscribe
      mqttClient.subscribe(mqttSubscribeTopicParamValue);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
    }
  }
}

void setupMqttServer() {
  // MQTT Server
  display.clear();
  displayText("MQTT");
  display.display();
  mqttClient.setServer(mqttServerParamValue, atoi(mqttPortParamValue));
  mqttClient.setCallback(mqttCallback);
  mqttConnect();
}

void loop() {
  //Serial.println("loop start");

  // Check if Wifi is connected
  if (WiFi.status() == WL_CONNECTED) {
    // On the first time Wifi is connected, setup OTA
    if (!wifiStarted) {
      Serial.println("Setup OTA now");
      ArduinoOTA.setHostname(thingName.c_str());
      Serial.print("IP address: ");
      Serial.println(WiFi.softAPIP());
      setupOTA();

      wifiStarted = true;
      httpLastRequest = -httpRequestDelayMs - 100; // ensure fetching data asap
    } else {
      ArduinoOTA.handle();
    }
  }
  iotWebConf.doLoop();

  if (needReset)
  {
    Serial.println("Rebooting after 1 second.");
    iotWebConf.delay(1000);
    ESP.restart();
  }

  //Serial.println("MQTT loop");
  mqttClient.loop();

  // fetch json value ever 2min:
  if (millis() - httpLastRequest > httpRequestDelayMs) {
    httpLastRequest = millis();
    float value = fetchJsonValue(); 
    jsonValueReceived = value != NO_NUMBER_F;
    if (jsonValueReceived) {
      jsonValue = value;
    }
  }

  if (wifiStarted && !mqttClient.connected()) {
    Serial.println("Try to connect to mqtt broker");
    mqttConnect();
  }

  if (millis() - lastAction > lastActionDelayMs) {
    recordWifiQuality();

    //Serial.println("display start");
    display.clear();

    displayTime();

    displayTemperature();
    displayMqttValue();
    displayJsonValue();
    displayWifiRSSI();

    // display.setTextAlignment(TEXT_ALIGN_RIGHT);
    // display.setFont(ArialMT_Plain_10);
    // display.drawString(display.width(), 4, "12345678890");

    autoBrightnessFromLightSensor();

    display.display();
  }
}

void wifiConnected() {
  Serial.println("WifiConnected Callback");
  setupMqttServer();
  needMqttConnect = true;

  // Init and get the time
  configTime(TIME_GMT_OFFSET_SECS, TIME_DAYLIGHT_OFFSET_SEC, "pool.ntp.org");

}

void configSaved()
{
  Serial.println("Configuration was updated.");
  Serial.print("Json Url: ");
  Serial.println(jsonUrlParamValue);
  Serial.print("Json Path: ");
  Serial.println(jsonPathParamValue);
  Serial.print("MQTT Server: ");
  Serial.println(mqttServerParamValue);
  Serial.print("MQTT Port: ");
  Serial.println(mqttPortParamValue);
  Serial.print("MQTT Topic: ");
  Serial.println(mqttSubscribeTopicParamValue);
  Serial.print("MQTT Topic Unit: ");
  Serial.println(mqttSubscribeTopicUnitParamValue);

  // changing the mode needs reset in some cases (fire2012)
  //needReset = true;  
}

bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  Serial.println("Validating form.");
  bool valid = true;

/*
  int l = webRequestWrapper->arg(stringParam.getId()).length();
  if (l < 3)
  {
    stringParam.errorMessage = "Please provide at least 3 characters for this test!";
    valid = false;
  }
*/
  return valid;
}
