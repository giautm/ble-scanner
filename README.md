# BLE-Scanner

ESP32-based Bluetooth Low Energy (BLE) scanner to report presence of bluetooth devices into an MQTT service.

## Challenge

Nowadays more and more bluetooth devices are around for personal use, like smart phones or smart watches. Why not use these devices to check presence of tenants at home?

There are [already some solutions around](https://github.com/search?q=ble+scan+esp32&type=Repositories). So why add another one?

I was quite astonished how many devices are detected if you scan the air for bluetooth. Only a few of these report their name, so its hard to distinguish these. So I decided to use [macaddress.io](https://macaddres.io) to lookup the vendor by the devices MAC address. I had to realize that only very few vendors can be looked up this way (can't tell so far, if this MAC address register ist the official one).

## Solution

I used an [Espressif ESP32](https://www.espressif.com/en/products/socs/esp32) device which has WiFi and Bluetooth on-board. The bluetooth scan results are published to an MQTT server via WiFi.

The MAC adresses of the scanned devices are looked up via [macaddress.io](https://macaddress.io).

The BLE scanning device is configured via a Web Frontend which is inspired by [Tasmota](https://github.com/arendst/Tasmota).

The BLE-Scanner doesn't need any external circuit -- just flash the software on it and configure the device.

## Prepare the build environment

To intall the ESP32 device support in the Arduino IDE, do as follows:

* Open the preferences in the Arduino IDE and add the following URLs to the _Additional Boards Manager URLs_ 
  * [https://dl.espressif.com/dl/package_esp32_index.json](https://dl.espressif.com/dl/package_esp32_index.json)
  * [http://arduino.esp8266.com/stable/package_esp8266com_index.json](http://arduino.esp8266.com/stable/package_esp8266com_index.json)
* Open the _Boards Manager_ and search for `esp32`. Install the found library.  
* Under `Tools`
  * select the Board `ESP32 Arduino` and your matching variant which was `WEMOS D1 MINI ESP32` in my case. This depends on the board you use.
  * select the hightest `Upload Speed`
  * select the right `CPU Frequency` for your board
  * select the `Flash Frequency`of `80MHz`
  * select the `Partition Scheme` of `No OTA (Large App)`

## Initialization Procedure

Whenever the BLE-Scanner starts and is not able to connect to your WiFi (eg. because of a missing configuration due to a fresh installation), it enters the configuration mode.
In configuration mode the BLE-Scanner opens an WiFi Access Point with the SSID `BLE-Scanner-AP-XX:XX:XX`. Connect to it with your smartphone or notebook and configure at lease the WiFi settings. That restart the BLE-Scanner.

To flash, open the sketch, build and upload to a connected ESP32. Then follow the **Initialization Procedure** above.


## What is in this respository?

### [BLE-Scanner Sketch](BLE-Scanner/)

This is the sketch for the ESP32 micro controller. Use the [ArduinoIDE](https://www.arduino.cc/en/main/software) to compile and upload into the ESP32 micro controller.

Follow the section **Prepare the build environtment** above, then open the sketch in the Arduino IDE to build and upload to a connected ESP32.
Then follow the **Initialization Procedure** above.


### [Helper Script](scripts/)

Thie directory holds some helper script.
