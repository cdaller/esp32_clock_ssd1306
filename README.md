# esp32_clock_ssd1306

![ESP32 Clock](/images/clock.jpg)

An ESP32 lolin32-clone with a small OLED SSD1306 display serves as a small clock with some additional features:

* clock is syncronized with NTP over the network
* automatic DST (daylight saving time) adjustment
* internal temperatur sensor value is shown (needs some manual correction to match the real temperature) (top left)
* a subscription to an MQTT broker is used to show temperature from my outside temperature sensor (ESPEasy) (top right)
* a json file can be fetched via http and a jsonpath is used to extract/display a value from it - I use it to display measurements from my particle sensor from [luftdaten.info](https://luftdaten.info/) (top center)
* WifiManager is used to configure wifi connection, mqtt server and topics, json url and json path.
* automatic brightness adjustment via analog light sensor (only extra hardware used besides ESP32) (debug value is shown bottom left)
* OTA (Over The Air) update is supported

Project is under development, so display is quite crowded to see what is possible and if everything works :-)

This is a platform.io project.

This project was inspired from [danielkucera/esp32-oled-clock](https://github.com/danielkucera/esp32-oled-clock).
