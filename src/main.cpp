//Define DEBUG to get the Output from DEBUG_PRINTLN
#define DEBUG 1

#include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`

#include <NTPClient.h>

#include <Basecamp.hpp>

// Create a new Basecamp instance called iot that will start the ap in secure mode and the webserver ui only in setup mode
// Basecamp iot{Basecamp::SetupModeWifiEncryption::secured, Basecamp::ConfigurationUI::accessPoint};
// Uncomment the following line and comment to one above to start the ESP with open wifi and always running config ui
Basecamp iot;

// change next line to use with another board/shield
//#include <ESP8266WiFi.h>
#include <WiFi.h> // for WiFi shield
//#include <WiFi101.h> // for WiFi 101 shield or MKR1000
#include <WiFiUdp.h>

#include <ESPmDNS.h>

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

//This topic is called if an MQTT message is received
void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  DEBUG_PRINTLN(__func__);

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);
  Serial.print("payload: ");
  Serial.println(payload);
  Serial.print("len: ");
  Serial.println(len);
 
  payload[len] = '\0';
  // String stringValue = String((char*)payload);
  // float floatValue = stringValue.toFloat();
  mqttSubscribeValue = (char*) payload;
 
  Serial.print("Message value: ");
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

//This function is called when the MQTT-Server is connected
void onMqttConnect(bool sessionPresent) {
  DEBUG_PRINTLN(__func__);

  if (sessionPresent) {
    iot.mqtt.subscribe(mqttSubscribeTopic, 0);
  } else {
    DEBUG_PRINTLN("no mqtt session yet...");
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


  // Initialize Basecamp
  iot.begin();
  // Alternate example: optional initialization with a fixed ap password for setup-mode:
  // iot.begin("yoursecurepassword");

  //Set up the Callbacks for the MQTT instance. Refer to the Async MQTT Client documentation
  // TODO: We should do this actually _before_ connecting the mqtt client...
  iot.mqtt.onConnect(onMqttConnect);
  //iot.mqttOnPublish(suspendESP);
  iot.mqtt.onMessage(onMqttMessage);

  DEBUG_PRINT("Basecamp hostname: ");
  DEBUG_PRINTLN(iot.hostname);

  display.clear();
  displayText("NTP");
  
  // NTP
  timeClient.begin();

  while (!timeClient.forceUpdate()){
    delay(10);
  }

  Serial.println("setup end");
}



char buffer[5];

void loop() {
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
