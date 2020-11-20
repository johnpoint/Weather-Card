# Weather-panel

# Import library 引入库

| name | url |
| ---- | --- |
|TFT_eSPI|https://github.com/Bodmer/TFT_eSPI|
|SPI|https://www.arduino.cc/en/Reference/SPI|
|ESP8266WiFi||
|ArduinoOTA||
|Arduino_JSON|https://github.com/arduino-libraries/Arduino_JSON|
|ESP8266WiFiMulti||
|ESP8266HTTPClient||
|WiFiUdp||
|NTP|https://github.com/sstaub/NTP|

# Configuration 配置

## Firmware 固件

edit `config.example.h`

```C++
#define WIFINAME "" // WIFI SSID
#define WIFIPW ""   // WIFI PASSWORD
#define NTPADDRESS "pool.ntp.org"

const String APIKEY = ""; // https://dev.qweather.com/docs/start/get-api-key
const String LOCATION = ""; // https://dev.qweather.com/docs/api/geo/
const String PROXYAPI = ""; // See README
``` 

```
mv config.example.h config.h
```

## API-proxy API 代理

```bash
cd apiporxy
pip install -r requirements.txt
flask run -h 0.0.0.0 # for development
```