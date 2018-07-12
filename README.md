# esp32_clock_ssd1306

An ESP32 lolin32-clone with a small OLED SSD1306 display serves as a small clock with some additional features:

* clock is syncronized with NTP over the network
* internal temperatur sensor value is shown (needs some manual correction to match the real temperature)
* a subscription to an MQTT broker is used to show temperature from my outside temperature sensor (ESPEasy).
* OTA (Over The Air) update is supported

This is a platform.io project.

This project was inspired from [danielkucera/esp32-oled-clock](https://github.com/danielkucera/esp32-oled-clock).
