# GRA-AFCH-IN-12-Arduino-Nixie-clock-kit-with-NTP
  While the build in GPS works great, it can be an issue with well insulated home to receive GPS/ DCF signals from the outside. Hence the desire to add the needed hardware and code to get a working NTP sync that has fail safes incorporated.

Used Hardware:
* GRA-AFCH; IN-12 Arduino Shield+Adapter+Raspberry Pi shield Nixie Tubes Clock https://gra-afch.com/catalog/nixie-tubes-clock-without-cases/raspberry-pi-shield-nixie-tubes-clock-ncs312-for-in-12-nixie-tubes-options-tubes-gps-remote-arduino-columns/
* Arduino Mega2560 + Wifi R3 (ESP8266) https://www.aliexpress.com/item/32950536539.html?spm=a2g0s.9042311.0.0.29054c4d1L9QCd

First things first:
* Flash the ESP8266 to the latest firmware (V3.0.4 in my case), I've used option 2 from page 5 (post by Attila_r): https://forum.arduino.cc/index.php?topic=495840.60
* Added the ESP8266 library in Arduino

The code itself tries to fetch the time through NTP from my router (local IP) and IOT wireless network (a VLAN that only can do certain things).
When it fails after 3 times it tries the GPS for a time sync.

Then there is also the inclusion of DST! My time will change/ update itself when DST is enabled and the month falls within either summer (+2) or wintertime (+1).
Since NTP works with UTC time only.

I've tried to keep the code as close as can be to the original from GRA-AFCH.
Perhaps my code can be optimized, still new to Arduino (C++) coding, if so feel free to let me know.
