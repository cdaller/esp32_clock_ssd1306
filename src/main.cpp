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
#include "config.h"

#define DISPLAY_I2C 0x3c
#define DISPLAY_SDA 5
#define DISPLAY_SCL 4
SSD1306Wire  display(DISPLAY_I2C, DISPLAY_SDA, DISPLAY_SCL);
WiFiUDP ntpUDP;

//#define TOUCH_PIN T1 //connected to 0
#define TOUCH_PIN T3 //connected to 15
int touch_value = 100; // default value

char* mqttSubscribeValue = NULL;

// allow to overwrite the configuration from external file:

WiFiClient espClient;
PubSubClient mqttClient(espClient);

Preferences preferences;

int offset = 2;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", offset * 3600, 60* 60 * 1000);

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
  sprintf(tempString, "%2d°C", temperatureInC);
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
    sprintf(tempString, "%s°C", mqttSubscribeValue);
  } else {
    sprintf(tempString, "%s°C", "??");
  }
  display.drawString(display.width(), 4, tempString);
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

void setup() {
  Serial.begin(115200);

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

  WiFi.begin(ssid, password);

  while ( WiFi.status() != WL_CONNECTED ) {
    displayText("CONN");
    delay ( 500 );
  }

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
  
  // NTP
  timeClient.begin();

  while (!timeClient.forceUpdate()){
    delay(10);
  }

  // MQTT Server
  display.clear();
  displayText("MQTT");
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(mqttCallback);

  mqttConnect();
  Serial.println("setup end");
}



char buffer[5];

void loop() {
  //Serial.println("loop start");

  ArduinoOTA.handle();
  //Serial.println("After OTA handle");
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

  //Serial.println("display start");
  display.clear();

  //display second bar
  display.fillRect(1, 0, display.width() * timeClient.getSeconds() / 59, 2);

  displayTemperature();
  displayMqttValue();

  //display time
  sprintf(buffer, "%2d:%02d", timeClient.getHours(), timeClient.getMinutes());
  displayText(buffer);

  // display.setTextAlignment(TEXT_ALIGN_RIGHT);
  // display.setFont(ArialMT_Plain_10);
  // display.drawString(display.width(), 4, "12345678890");

  display.display();

  delay(300);
}
