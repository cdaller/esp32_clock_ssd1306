 #include "SSD1306Wire.h" // legacy include: `#include "SSD1306.h"`

#include <NTPClient.h>
// change next line to use with another board/shield
//#include <ESP8266WiFi.h>
#include <WiFi.h> // for WiFi shield
//#include <WiFi101.h> // for WiFi 101 shield or MKR1000
#include <WiFiUdp.h>

#include <ESPmDNS.h>
#include <ArduinoOTA.h>

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
int touch_value = 100;


Preferences preferences;

int offset = 2;

// You can specify the time server pool and the offset (in seconds, can be
// changed later with setTimeOffset() ). Additionaly you can specify the
// update interval (in milliseconds, can be changed using setUpdateInterval() ).
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", offset * 3600, 60* 60 * 1000);

void display_text(String text){
  display.setColor(WHITE);
  display.setFont(Dialog_plain_40);
  // works: display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, text);
}

float readInternalTemperature() {
  uint8_t temperatureInF = temprature_sens_read();
  uint8_t temperatureInC = (temperatureInF - 32) / 1.8;
  //temperatureInC += 17; // correction to real temperature
  // Serial.print(temperatureInF);
  // Serial.println("°F");
  // Serial.print(temperatureInC);
  // Serial.println("°C");
  return temperatureInC;
}

void display_temperature() {
  uint8_t temperatureInC = readInternalTemperature();

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  char tempString[8];
  sprintf(tempString, "%2d°C", temperatureInC);
  display.drawString(0, 4, tempString);
}

void setup(){
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
    display_text("CONN");
    delay ( 500 );
  }

  char ssid[15];
  uint64_t chipid=ESP.getEfuseMac();//The chip ID is essentially its MAC address(length: 6 bytes).
  uint16_t chip = (uint16_t)(chipid>>32);
  snprintf(ssid,15,"CDEsp32-%04X",chip);
  
  Serial.print("ESP client ID: ");
  Serial.println(ssid);


  // OTA setup:
  /* create a connection at port 3232 */
  ArduinoOTA.setPort(3232);
  /* we use mDNS instead of IP of ESP32 directly */
  ArduinoOTA.setHostname(ssid);

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
  display_text("NTP");

  timeClient.begin();

  while (!timeClient.forceUpdate()){
    delay(10);
  }
}

char buffer[5];

void loop() {
  ArduinoOTA.handle();

  timeClient.update();

  // change time zone (offset):
  // touch_value = touchRead(TOUCH_PIN);
  // Serial.print("Touch Value: ");
  // Serial.println(touch_value);

  if (touch_value < 20 ){
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

  display.clear();

  //display second bar
  display.fillRect(1, 0, display.width() * timeClient.getSeconds() / 59, 2);

  display_temperature();
  //display time
  sprintf(buffer, "%2d:%02d", timeClient.getHours(), timeClient.getMinutes());
  display_text(buffer);
  display.display();

  delay(300);
}
