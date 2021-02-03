# Weather-panel

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

## 连线

| 部件    | 针脚 | 部件   | 针脚      |
|---------|------|--------|-----------|
| ESP8266 | D0   | TTP223 | OUT       |
| ESP8266 | D1   | SGP30  | SCL       |
| ESP8266 | D2   | SGP30  | SDA       |
| ESP8266 | D3   | ST7796 | DC        |
| ESP8266 | D4   | ST7796 | CS        |
| ESP8266 | D5   | ST7796 | SCK       |
| ESP8266 | D7   | ST7796 | SDI(MOSI) |
| ESP8266 | RST  | ST7796 | RST       |

# 成品

## 本体

![](https://cdn.lvcshu.info/img/20201202002.jpg)
![](https://cdn.lvcshu.info/img/20201202003.jpg)

## WEB 管理界面

![](https://cdn.lvcshu.info/img/20201202005.jpg)
![](https://cdn.lvcshu.info/img/20201202004.jpg)
