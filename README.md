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

## API-proxy API 代理

``` bash
cd apiporxy
pip install -r requirements.txt
flask run -h 0.0.0.0 # for development
```

## NodeMCU

为 NodeMCU 通电，首次烧入固件后由于配置文件不存在会自动进入 AP 模式，查看显示屏上 AP 信息以及 ip 地址，使用浏览器打开 web/index.html 输入相关信息即可开始配置，默认密码为 123456

Power on the NodeMCU. After the firmware is burned for the first time, it will automatically enter the AP mode because the configuration file does not exist. Check the AP information and ip address on the display. Use a browser to open web/index.html and enter the relevant information to start the configuration. The default password is 123456
