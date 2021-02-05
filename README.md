# esp32_clock_ssd1306

![ESP32 Clock](/images/clock.jpg)

An ESP32 lolin32-clone with a small OLED SSD1306 display serves as a small clock with some additional features:

* clock is syncronized with NTP over the network
* automatic DST (daylight saving time) adjustment
* a subscription to an MQTT broker is used to show temperature from my living room temperature sensor (ESPEasy) (top right)
* two separate configs for fetching json data and extract values by using a json path - I use it to display measurements from my particle sensor from [luftdaten.info](https://luftdaten.info/) (top left/top center)
* [IotWebConf](https://github.com/prampec/IotWebConf) is used to configure wifi connection, mqtt server and topics, json urls and paths.
For ESP32 I needed to patch the IotWebConf library - see https://github.com/prampec/IotWebConf/issues/124 for details.
* automatic brightness adjustment via analog light sensor (only extra hardware used besides ESP32) (debug value is shown bottom left)
* OTA (Over The Air) update is supported

Project is under development, so display is quite crowded to see what is possible and if everything works :-)

This is a platform.io project.

This project was inspired from [danielkucera/esp32-oled-clock](https://github.com/danielkucera/esp32-oled-clock).
