#include <TFT_eSPI.h>     // https://github.com/Bodmer/TFT_eSPI
#include <SPI.h>          // https://www.arduino.cc/en/Reference/SPI
#include <Arduino_JSON.h> // https://github.com/arduino-libraries/Arduino_JSON
// https://github.com/sstaub/NTP
#include <WiFiUdp.h>
#include <NTP.h>
// https://github.com/adafruit/Adafruit_SGP30
#include "Adafruit_SGP30.h"
#include <Wire.h>
#include <ArduinoOTA.h> // https://github.com/jandrassy/ArduinoOTA
// https://github.com/esp8266/Arduino
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>

#include "FS.h"
#include <LittleFS.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

#include "fonts.h"
#include "html.h"

#ifndef APSSID
#define APSSID "NodeMCU"
#define APPSK "12345678"
#endif

#define VERSION "7.5"

#define NTPADDRESS "ntp5.aliyun.com"
#define TIMEZONE 8

String APIKEY = "";                            // https://dev.qweather.com/docs/start/get-api-key
String LOCATION = "";                          // https://dev.qweather.com/docs/api/geo/
String PROXYAPI = "";                          // Address for API proxy
TFT_eSPI tft = TFT_eSPI();                     // TFT control
String ssid = APSSID;                          // Access Point SSID
String password = APPSK;                       // Access Point Password
String configPass = "123456";                  // Configure password
time_t nowTime;                                // Now timestamp
String mainw = "";                             // Real-time weather overview
String desc = "";                              // Text for Air quality
String times = "";                             // Time Text Such as "2021-01-01 00:00:00"
int timeZone = 8;                              // TimeZone
String temp = "";                              // Temperature
uint32_t BG = TFT_BLACK;                       // Backgroud Color
uint32_t TC = TFT_WHITE;                       // Text Color
WiFiUDP wifiUdp;                               // Used by ntp
NTP ntp(wifiUdp);                              // NTP client
int n = 0, page = 1, reload = 0, day = 0;      // day=0 night=1
uint16_t tvoc = 1, eco2 = 1;                   // Real-time air quality
int mode = 0, sgpmode = 0;                     // 0 offline 1 online
long unsigned int offlineClock_lastUpdate = 0; // Localtime
int touch = 0;

JSONVar hrStatus;
JSONVar dayStatus;
JSONVar config;
Adafruit_SGP30 sgp;
ESP8266WebServer server(80);

// 声明函数
bool loadconfig();
void conntionWIFI();
void handleRoot();
void handlewifi();
void wificonfig(bool pass);
void showInfo();
void updateTime();
void changeIcon(String newI);
void offline();
JSONVar httpCom(String host, String path, bool https);

// arduino 初始化函数
void setup(void)
{
    pinMode(PIN_D0, INPUT); // 设定 pin 模式
    Serial.begin(115200); // 设定串口频率
    tft.init(); // 初始化 TFT 屏幕

    // 绘制界面
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_YELLOW); 
    tft.setRotation(1);
    tft.fillScreen(BG);
    tft.setTextColor(TC);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setCursor(20, 20);
    tft.println("[Chip] ESP8266EX");
    tft.setCursor(20, tft.getCursorY());
    tft.println("[Screen] ST8896S");
    tft.setCursor(20, tft.getCursorY());
    tft.print("[Firmware] v");
    tft.println(VERSION);
    tft.setCursor(20, tft.getCursorY());
    tft.println("[LittleFS] Mounting FS...");
    tft.setCursor(20, tft.getCursorY());

    // 挂载文件系统
    if (!LittleFS.begin())
    {
        tft.println("[LittleFS] Failed to mount file system");
        tft.setCursor(20, tft.getCursorY());
        while (1)
        {
            delay(0);
        }
    }

    // 读取 SGP 芯片状态（空气质量检测）
    if (!sgp.begin())
    {
        tft.println("[Sensor] SGP30 not found :(");
        tft.setCursor(20, tft.getCursorY());
    }
    else
    {
        sgpmode = 1;
        tft.println("[Sensor] SGP30");
        tft.setCursor(20, tft.getCursorY());
        tft.print("Found SGP30 serial #");
        tft.print(sgp.serialnumber[0], HEX);
        tft.print(sgp.serialnumber[1], HEX);
        tft.println(sgp.serialnumber[2], HEX);
        tft.setCursor(20, tft.getCursorY());
    }

    wificonfig(false); // 配置 WIFI
    tft.setCursor(20, tft.getCursorY());
    tft.println("[Web Config] HTTP server started");

    // 初始化 OTA 监听
    ArduinoOTA.setHostname("ESP8266"); 
    ArduinoOTA.begin();

    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setCursor(20, tft.getCursorY());
    tft.println("[OTA] Start");
    tft.setCursor(20, tft.getCursorY());
    tft.print("[NTP] Begin -> ");
    tft.println(NTPADDRESS);

    // 设置网络时间同步
    ntp.ntpServer(NTPADDRESS);
    ntp.begin();
    ntp.timeZone(TIMEZONE);
    delay(1000);
    showInfo();

    // http 服务监听
    server.on("/", handleRoot);
    server.on("/config/" + configPass, handlewifi);
    server.begin();
    File flagFile = LittleFS.open("/flag", "r");
    if (!flagFile)
    {
        File flagFilew = LittleFS.open("/flag", "w");
        flagFilew.print("2\n");
        flagFilew.close();
    }
    else
    {
        if (flagFile.readStringUntil('\n') == "2")
        {
            File flagFilew = LittleFS.open("/flag", "w");
            flagFilew.print("-1\n");
            flagFilew.close();
        }
    }
    flagFile.close();
}

// 监听触摸时间的变量
int touchStart = 0;
int touchTime = 0;

// touchCommand 判断触摸命令
int touchCommand()
{
    if (digitalRead(PIN_D0) == 1 && touch == 0)
    {
        touchStart = millis();
        touch = 1; // Press down
    }
    if (digitalRead(PIN_D0) == 0 && touch == 1)
    {
        touchTime = millis() - touchStart;
        touch = 2; // Leave
    }
    if (digitalRead(PIN_D0) == 1 && touch == 1)
    {
        if (millis() - touchStart >= 1300 && millis() - touchStart < 2000)
        {
            tft.fillCircle(460, 20, 5, TFT_YELLOW);
        }
        else if (millis() - touchStart >= 2000)
        {
            tft.fillCircle(460, 20, 5, TFT_RED);
        }
        else
        {
            tft.fillCircle(460, 20, 5, TFT_GREEN);
        }
    }
    else
    {manually rebuild your IntelliSense confi
        tft.fillCircle(460, 20, 5, BG);
    }

    if (touch == 2)
    {
        Serial.println(touchTime);
        touch = 0;
        if (touchTime >= 2000)
        {
            return 2; // Looooooooooong
        }
        return 1; // Short
    }
    return 0; // No
}

// 菜单，未实现的功能
void menu()
{
    tft.setCursor(20, 20);
    tft.println("Welcome!");
    tft.setCursor(20, tft.getCursorY());
    tft.print("Welcome!");
}

// 模式一
void modeOne(int o)
{
    // 更新时间
    updateTime();
    if (reload == 2)
    {
        n = 0;
        day = 0;
        ntp.stop();
        showInfo();
        desc = "";
        mainw = "";
        times = "";
        temp = "";
    }

    // 更新天气
    if (n % 3000 == 0 || reload >= 1)
    {
        ntp.update();
        tft.fillRoundRect(300, 20, 160, 20, 0, BG);
        tft.setCursor(320, 20);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.println("[Update] Weather Info");
        String a, b, c, d;
        JSONVar nowStatus = httpCom(PROXYAPI, "/v7/weather/now/" + LOCATION + "/" + APIKEY + "/en", false);
        if (JSON.typeof(nowStatus) == "undefined")
        {
            tft.fillRoundRect(300, 20, 160, 20, 0, BG);
            tft.setCursor(320, 20);
            tft.setTextFont(2);
            tft.setTextSize(1);
            tft.println("[Error] Weather Info");
            tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            delay(1000);
        }
        else
        {
            const char *aa = nowStatus["code"];
            String a = aa;
            if (a == "200")
            {
                int f = 0;
                if (mainw != nowStatus["now"]["text"] || reload >= 1) // Handling day and night change icon overload
                {
                    tft.setFreeFont(FF36);
                    tft.fillRect(18, 83, 220, 60, BG);
                    mainw = nowStatus["now"]["text"];
                    tft.setCursor(20, 120);
                    tft.setTextColor(TC);
                    tft.print(mainw);
                    GFXfont list[] = {
                        *FF35,
                        *FF34,
                        *FF33};
                    int i = 0;
                    while (tft.getCursorX() >= 240 && i < 3)
                    {
                        f = 1;
                        tft.fillRect(18, 83, 220, 60, BG);
                        tft.setFreeFont(&list[i]);
                        tft.setCursor(20, 120);
                        tft.setTextColor(TC);
                        tft.print(mainw);
                        i++;
                    }
                    changeIcon(mainw);
                }
                a = nowStatus["now"]["temp"];
                b = nowStatus["now"]["humidity"];
                if (a + "°C/" + b + "%" != temp || f == 1)
                {
                    tft.fillRect(238, 90, 110, 50, BG);
                    tft.setFreeFont(FF6);
                    tft.setCursor(240, 120);
                    tft.setTextColor(BG);
                    tft.print(temp);
                    temp = a + "°C/" + b + "%";
                    tft.setCursor(240, 120);
                    tft.setTextColor(TC);
                    tft.print(temp);
                }
            }
        }
        tft.fillRoundRect(300, 20, 160, 20, 0, BG);
        tft.setCursor(320, 20);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.println("[Update] Air Info");
        JSONVar StatusNew;
        JSONVar airStatus = httpCom(PROXYAPI, "/v7/air/now/" + LOCATION + "/" + APIKEY + "/en", false);
        if (JSON.typeof(airStatus) == "undefined")
        {
            tft.fillRoundRect(300, 20, 160, 20, 0, BG);
            tft.setCursor(320, 20);
            tft.setTextFont(2);
            tft.setTextSize(1);
            tft.println("[Error] Air Info");
            tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            delay(1000);
        }
        else
        {
            const char *aa = airStatus["code"];
            String a = aa;
            if (a == "200")
            {
                a = airStatus["now"]["category"];
                b = airStatus["now"]["aqi"];
                c = airStatus["now"]["pm2p5"];
                d = airStatus["now"]["pm10"];
                if (desc != a + b + c + d)
                {
                    tft.fillRoundRect(16, 135, 450, 22, 5, BG);
                    tft.setFreeFont(FF17);
                    desc = a + b + c;
                    tft.setCursor(20, 150);
                    tft.setTextColor(TC);
                    tft.print(a);
                    tft.print("   ");
                    tft.setCursor(tft.getCursorX(), 140);
                    tft.setFreeFont(FF0);
                    tft.print("AQI");
                    tft.setCursor(tft.getCursorX(), 150);
                    tft.setFreeFont(FF17);
                    tft.print(b);
                    tft.print("   ");
                    tft.setCursor(tft.getCursorX(), 140);
                    tft.setFreeFont(FF0);
                    tft.print("PM2.5");
                    tft.setCursor(tft.getCursorX(), 150);
                    tft.setFreeFont(FF17);
                    tft.print(c);
                    tft.print("   ");
                    tft.setCursor(tft.getCursorX(), 140);
                    tft.setFreeFont(FF0);
                    tft.print("PM10");
                    tft.setCursor(tft.getCursorX(), 150);
                    tft.setFreeFont(FF17);
                    tft.print(d);
                    tft.print("   ");
                }
            }
            int wx = tft.getCursorX();
            int wy = tft.getCursorY();
            tft.fillRoundRect(300, 20, 160, 20, 0, BG);
            tft.setCursor(320, 20);
            tft.setTextFont(2);
            tft.setTextSize(1);
            tft.println("[Update] Warning Info");
            String warn = "";
            uint32_t WC = TFT_WHITE; // wearning color
            StatusNew = httpCom(PROXYAPI, "/v7/warning/now/" + LOCATION + "/" + APIKEY + "/en", false);
            if (JSON.typeof(StatusNew) == "undefined")
            {
                tft.fillRoundRect(300, 20, 160, 20, 0, BG);
                tft.setCursor(320, 20);
                tft.setTextFont(2);
                tft.setTextSize(1);
                tft.println("[Error] Warning Info");
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
                delay(1000);
            }
            else
            {
                const char *aa = StatusNew["code"];
                String a = aa;
                if (a == "200" && StatusNew["warning"]["typeName"] != null)
                {
                    const char *tn = StatusNew["warning"]["typeName"];
                    const char *level = StatusNew["warning"]["level"];
                    warn = tn;
                    String typeLevel = level;
                    if (typeLevel == "White")
                    {
                        WC = TFT_WHITE;
                    }
                    else if (typeLevel == "Yellow")
                    {
                        WC = TFT_YELLOW;
                    }
                    else if (typeLevel == "Blue")
                    {
                        WC = TFT_BLUE;
                    }
                    else if (typeLevel == "Red")
                    {
                        WC = TFT_RED;
                    }
                    else if (typeLevel == "Orange")
                    {
                        WC = TFT_ORANGE;
                    }
                }
            }
            tft.setCursor(wx, wy);
            tft.setTextColor(WC);
            tft.setFreeFont(FF21);
            tft.print(warn);
            tft.setTextColor(TC);
        }
        tft.fillRoundRect(300, 20, 160, 20, 0, BG);
        tft.setCursor(320, 20);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.println("[Update] Hours Info");
        StatusNew = httpCom(PROXYAPI, "/v7/weather/24h/" + LOCATION + "/" + APIKEY + "/en", false);
        if (JSON.typeof(StatusNew) == "undefined")
        {
            tft.fillRoundRect(300, 20, 160, 20, 0, BG);
            tft.setCursor(320, 20);
            tft.setTextFont(2);
            tft.setTextSize(1);
            tft.println("[Error] Hours Info");
            tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            delay(1000);
        }
        else
        {
            const char *aa = StatusNew["code"];
            String a = aa;
            if (a == "200")
            {
                hrStatus = StatusNew;
            }
        }
        ESP.wdtFeed();
        tft.fillRoundRect(300, 20, 160, 20, 0, BG);
        tft.setCursor(320, 20);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.println("[Update] Days Info");
        StatusNew = httpCom(PROXYAPI, "/v7/weather/7d/" + LOCATION + "/" + APIKEY + "/en", false);
        if (JSON.typeof(StatusNew) == "undefined")
        {
            tft.fillRoundRect(300, 20, 160, 20, 0, BG);
            tft.setCursor(320, 20);
            tft.setTextFont(2);
            tft.setTextSize(1);
            tft.println("[Error] Days Info");
            tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            delay(1000);
        }
        else
        {
            const char *aa = StatusNew["code"];
            String a = aa;
            if (a == "200")
            {
                dayStatus = StatusNew;
            }
        }
        tft.fillRoundRect(300, 20, 160, 20, 0, BG);
    }

    // 更新空气质量信息
    if (n % 30 == 0 || reload >= 1)
    {
        if (reload >= 1)
        {
            tvoc = -1;
            eco2 = -1;
        }
        if (!sgp.IAQmeasure())
        {
            tft.drawRoundRect(300, 167, 170, 140, 10, TFT_RED);
        }
        else
        {
            tft.drawRoundRect(300, 167, 170, 140, 10, BG);
            tft.drawFastVLine(300, 170, 135, TFT_WHITE);
            if (sgp.TVOC != tvoc || reload == 1)
            {
                tft.setCursor(320, 220);
                tft.setFreeFont(FF17);
                tft.println("TVOC");
                tft.setCursor(320, tft.getCursorY() + 10);
                tft.setFreeFont(FF6);
                tft.fillRect(315, 230, 75, 30, BG);
                if (sgp.TVOC > 40 && sgp.TVOC <= 80)
                {
                    tft.setTextColor(TFT_YELLOW);
                }
                else if (sgp.TVOC > 80)
                {
                    tft.setTextColor(TFT_RED);
                }
                else
                {
                    tft.setTextColor(TFT_GREEN);
                }
                tft.print(sgp.TVOC);
                tft.setTextColor(TC);
                tvoc = sgp.TVOC;
            }
            if (sgp.eCO2 != eco2 || reload == 1)
            {
                tft.setCursor(400, 220);
                tft.setFreeFont(FF17);
                tft.println("CO2");
                tft.setCursor(400, tft.getCursorY() + 10);
                tft.setFreeFont(FF6);
                tft.fillRect(395, 230, 75, 30, BG);
                if (sgp.eCO2 > 800 && sgp.eCO2 <= 1000)
                {
                    tft.setTextColor(TFT_YELLOW);
                }
                else if (sgp.eCO2 > 1000)
                {
                    tft.setTextColor(TFT_RED);
                }
                else
                {
                    tft.setTextColor(TFT_GREEN);
                }
                tft.print(sgp.eCO2);
                tft.setTextColor(TC);
                eco2 = sgp.eCO2;
            }
        }
    }

    if (n % 150 == 0 || reload == 1 || o > 0)
    {
        tft.setFreeFont(FF33);
        if (page == 0)
        {
            if (JSON.typeof(hrStatus) == "undefined")
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            }
            else
            {
                const char *aa = hrStatus["code"];
                String a = aa;
                if (a == "200")
                {
                    tft.fillRect(10, 163, 275, 144, BG);
                    page = 1;
                    for (int i = 0; i < 7; i++)
                    {
                        tft.setCursor(20, 182 + i * 20);
                        tft.setTextColor(TC);
                        tft.print((const char *)hrStatus["hourly"][i]["fxTime"]);
                        tft.setCursor(20 + 70, 182 + i * 20);
                        tft.print((const char *)hrStatus["hourly"][i]["text"]);
                        tft.setCursor(20 + 210, 182 + i * 20);
                        tft.print((const char *)hrStatus["hourly"][i]["temp"]);
                    }
                }
            }
        }
        else
        {
            if (JSON.typeof(dayStatus) == "undefined")
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            }
            else
            {
                const char *aa = dayStatus["code"];
                String a = aa;
                if (a == "200")
                {
                    tft.fillRect(10, 163, 275, 144, BG);
                    page = 0;
                    for (int i = 0; i < 7; i++)
                    {
                        tft.setCursor(20, 182 + i * 20);
                        tft.setTextColor(TC);
                        tft.print((const char *)dayStatus["daily"][i]["fxDate"]);
                        tft.setCursor(20 + 70, 182 + i * 20);
                        tft.print((const char *)dayStatus["daily"][i]["text"]);
                        tft.setCursor(20 + 180, 182 + i * 20);
                        tft.print((const char *)dayStatus["daily"][i]["temp"]);
                    }
                }
            }
        }
    }
    reload = 0;
}

int nowMode = 0;

// arduino 循环执行函数
void loop()
{
    // Crash detection
    File flagFile = LittleFS.open("/flag", "r");
    String successFlag = flagFile.readStringUntil('\n');
    flagFile.close();
    ArduinoOTA.handle();   // OTA monitoring
    server.handleClient(); // Web server
    if (successFlag != "-1")
    {
        int o = touchCommand();
        if (o == 2)
        {
            tft.fillScreen(BG);
            if (nowMode == 1)
            {
                nowMode = 0;
                reload = 2;
                mode = 1;
                showInfo();
            }
            else
            {
                mode = 2;
                reload = 1;
                nowMode = 1;
            }
        }
        if (nowMode == 0)
        {
            modeOne(o);
        }
        if (nowMode == 1)
        {
            updateTime();
            if (n % 10 == 0)
            {
                offline();
                if (reload == 1)
                {
                    reload = 0;
                }
            }
        }
        File flagFile = LittleFS.open("/flag", "w");
        flagFile.print("1\n");
        flagFile.close();
    }
    else
    {
        tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
        tft.setCursor(20, 60);
        tft.setFreeFont(FF17);
        tft.print("Oh, it crashed, please check the configuration");
    }
    if (touch == 2)
    {
        touch = 0;
    }
    n++;
    delay(100);
}

int wififlag = 1;

// 连接wifi函数
void conntionWIFI()
{
    WiFi.disconnect();
    wififlag = 1;
    WiFi.begin(config["ssid"], config["pass"]);
    tft.setCursor(20, tft.getCursorY());
    tft.print("[WIFI] Connecting to ");
    tft.println(config["ssid"]);
    int t = 0;
    tft.setCursor(20, tft.getCursorY());
    // 等待连接上
    while (WiFi.status() != WL_CONNECTED && t <= 30)
    {
        tft.print("=");
        delay(1000);
        t++;
    }
    tft.println("=");
    if (WiFi.status() == WL_CONNECTED)
    {
        wififlag = 0;
    }
}

// 监听 http 访问，返回首页
void handleRoot()
{
    server.send(200, "text/html", HTMLTEXT);
}

// 监听 http 访问，设置本地时间
void handleTime()
{
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_PINK);
    delay(500);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.sendHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    if (server.method() == HTTP_GET)
    {
        String oss;
        oss = nowTime;
        server.send(200, "text/plain", oss);
    }
    if (server.method() == HTTP_POST)
    {
        String getTime;
        getTime = server.arg("plain");
        nowTime = getTime.toInt() + (timeZone * 60 * 60);
        server.send(200);
    }
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
}

// 监听 http 更新配置文件
void handlewifi()
{
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_PINK);
    delay(500);
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Max-Age", "10000");
    server.sendHeader("Access-Control-Allow-Methods", "PUT,POST,GET,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
    if (server.method() == HTTP_GET)
    {
        server.send(200, "text/plain", JSON.stringify(config));
    }
    else if (server.method() == HTTP_POST)
    {
        tft.fillScreen(BG);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.setCursor(0, 0);
        server.send(200, "text/plain", "OK");
        File configFile = LittleFS.open("/config.json", "w");
        if (!configFile)
        {
            tft.println("[Config] Failed to open config file for writing");
            reload = 1;
        }
        else
        {
            configFile.print(server.arg("plain"));
        }
        configFile.close();
        File flagFile = LittleFS.open("/flag", "w");
        flagFile.print("2\n");
        flagFile.close();
        if (loadconfig())
        {
            conntionWIFI();
            if (wififlag == 0)
            {
                reload = 2;
            }
            else
            {
                server.close();
                wificonfig(true);
            }
        }
    }
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
}

// 加载配置文件
bool loadconfig()
{
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_YELLOW);
    File configFile = LittleFS.open("/config.json", "r");
    if (!configFile)
    {
        if (tft.getCursorY() == 0)
        {
            tft.setCursor(20, tft.getCursorY() + 20);
        }
        else
        {
            tft.setCursor(20, tft.getCursorY());
        }
        tft.println("[Config] Failed to open config file");
        return false;
    }
    if (tft.getCursorY() == 0)
    {
        tft.setCursor(20, tft.getCursorY() + 20);
    }
    else
    {
        tft.setCursor(20, tft.getCursorY());
    }
    tft.println("[Config] Parsing config.json...");
    size_t size = configFile.size();
    if (size > 1024)
    {
        tft.setCursor(20, tft.getCursorY());
        tft.println("[Config] Config file size is too large");
        return false;
    }
    std::unique_ptr<char[]> buf(new char[size]);
    configFile.readBytes(buf.get(), size);
    config = JSON.parse(buf.get());
    if (config["ssid"] == null || config["pass"] == null ||
        config["key"] == null || config["location"] == null ||
        config["proxy"] == null)
    {
        tft.setCursor(20, tft.getCursorY());
        tft.println(config);
        return false;
    }
    APIKEY = config["key"];
    LOCATION = config["location"];
    PROXYAPI = config["proxy"];
    if (config["hssid"] != null && config["hssid"] != "")
    {
        ssid = config["hssid"];
    }
    if (config["hpass"] != null && config["hpass"] != "")
    {
        password = config["hpass"];
    }
    if (config["hpass"] != null && config["hpass"] != "")
    {
        password = config["hpass"];
    }
    if (config["password"] != null && config["password"] != "")
    {
        configPass = config["password"];
    }
    if (config["timezone"] != null)
    {
        timeZone = atoi(config["timezone"]);
    }
    return true;
}

// 无网络显示空气质量
void offline()
{
    if (reload == 1)
    {
        tvoc = -1;
        eco2 = -1;
    }
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
    if (!sgp.IAQmeasure())
    {
        tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
    }
    else
    {
        if (sgp.TVOC != tvoc)
        {
            tft.setCursor(172, 150);
            tft.setFreeFont(FF17);
            tft.println("TVOC");
            tft.setCursor(172, tft.getCursorY() + 10);
            tft.setFreeFont(FF6);
            tft.fillRect(167, 160, 75, 30, BG);
            if (sgp.TVOC > 40 && sgp.TVOC <= 80)
            {
                tft.setTextColor(TFT_YELLOW);
            }
            else if (sgp.TVOC > 80)
            {
                tft.setTextColor(TFT_RED);
            }
            else
            {
                tft.setTextColor(TFT_GREEN);
            }
            tft.print(sgp.TVOC);
            tft.setTextColor(TC);
            tvoc = sgp.TVOC;
        }
        if (sgp.eCO2 != eco2)
        {
            tft.setCursor(268, 150);
            tft.setFreeFont(FF17);
            tft.println("CO2");
            tft.setCursor(268, tft.getCursorY() + 10);
            tft.setFreeFont(FF6);
            tft.fillRect(263, 160, 75, 30, BG);
            if (sgp.eCO2 > 800 && sgp.eCO2 <= 1000)
            {
                tft.setTextColor(TFT_YELLOW);
            }
            else if (sgp.eCO2 > 1000)
            {
                tft.setTextColor(TFT_RED);
            }
            else
            {
                tft.setTextColor(TFT_GREEN);
            }
            tft.print(sgp.eCO2);
            tft.setTextColor(TC);
            eco2 = sgp.eCO2;
        }
    }
}

// 配置wifi
void wificonfig(bool pass)
{
    if (loadconfig() && !pass)
    {
        conntionWIFI();
        if (wififlag == 0)
        {
            reload = 2;
            mode = 1;
            return;
        }
    }
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setCursor(20, tft.getCursorY());
    tft.println("[AcessPoint] Configuring access point...");
    WiFi.softAP(ssid, password);
    IPAddress myIP = WiFi.softAPIP();
    tft.setCursor(20, tft.getCursorY());
    tft.print("[AcessPoint] SSID: ");
    tft.println(ssid);
    tft.setCursor(20, tft.getCursorY());
    tft.print("[AcessPoint] Password: ");
    tft.println(password);
    tft.setCursor(20, tft.getCursorY());
    tft.print("[AcessPoint] AP IP address: ");
    tft.println(myIP);
    server.on("/", handleRoot);
    server.on("/config/" + configPass, handlewifi);
    server.on("/time/" + configPass, handleTime);
    server.begin();
    tft.setCursor(20, tft.getCursorY());
    tft.println("[WebConfig] HTTP server started");
    delay(1000);
    tft.fillScreen(BG);
    tft.setCursor(20, 20);
    tft.setTextColor(TC);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.print("[");
    tft.print(ssid);
    tft.print("]  ");
    tft.print(myIP);
    tft.print("(");
    tft.print(password);
    tft.print(")");
    tft.print("      v");
    tft.println(VERSION);
    nowTime = millis() + (timeZone * 60 * 60);
    offlineClock_lastUpdate = millis();
    mode = 0;
    while (wififlag)
    {
        server.handleClient();
        if (sgpmode == 1)
        {
            offline();
        }
        updateTime();
        delay(500);
    }
    server.close();
    WiFi.softAPdisconnect(true);
    mode = 1;
    server.on("/", handleRoot);
    server.on("/config/" + configPass, handlewifi);
    server.begin();
}

// 加载信息
void showInfo()
{
    tft.fillScreen(BG);
    tft.setCursor(20, 20);
    tft.setTextColor(TC);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.print("[");
    tft.print((const char *)config["ssid"]);
    tft.print("]  ");
    tft.print(WiFi.localIP());
    tft.print("           v");
    tft.println(VERSION);
    tft.drawRoundRect(300, 167, 170, 140, 10, TFT_WHITE);
    tft.fillRoundRect(350, 50, 90, 90, 5, BG);
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
    tft.drawRoundRect(10, 80, 460, 80, 10, TFT_YELLOW);
}

// 更新时间
void updateTime()
{
    if (mode != 0)
    {
        nowTime = ntp.epoch() + (timeZone * 60 * 60);
    }
    else
    {
        int m = millis();
        nowTime = nowTime + ((m - offlineClock_lastUpdate) / 1000);
        offlineClock_lastUpdate = m;
    }

    struct tm *ptr;
    ptr = localtime(&nowTime);
    char newT[80];
    strftime(newT, 100, "%F %T", ptr);
    //String newT = ntp.formattedTime();
    if (n % 60 == 0 && mode == 1)
    {
        if (n == 360)
        {
            n = 0;
        }
        String a = ntp.formattedTime("%H");
        if (((a[0] == '1' && a[1] >= '8') || a[0] >= '2' || (a[0] == '0' && a[1] <= '6')) && day == 0)
        {
            day = 1;
            tft.drawRoundRect(10, 80, 460, 80, 10, TFT_BLUE);
            TC = TFT_SILVER;
            reload = 1;
            mainw = "";
        }
        else if (((a[0] == '0' && a[1] > '6') || (a[0] == '1' && a[1] < '8')) && day == 1)
        {
            day = 0;
            tft.drawRoundRect(10, 80, 460, 80, 10, TFT_YELLOW);
            TC = TFT_WHITE;
            reload = 1;
            mainw = "";
        }
    }
    int x, y;
    if (mode == 0 || mode == 2)
    {
        tft.setFreeFont(FMB18);
        x = 30;
        y = 80;
    }
    else
    {
        tft.setFreeFont(FF6);
        x = 20;
        y = 60;
    }
    tft.setCursor(x, y);
    String newTs = newT;
    if (newTs != times)
    {
        for (int i = 0; i < 19; i++)
        {
            if (times[i] != newT[i])
            {
                int x = tft.getCursorX();
                tft.setTextColor(BG);
                tft.print(times[i]);
                tft.setCursor(x, y);
                tft.setTextColor(TC);
                tft.print(newT[i]);
            }
            else
            {
                tft.setTextColor(TC);
                tft.print(times[i]);
            }
        }
        times = newT;
    }
}

// rgb转换TFT颜色
uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue)
{
    red >>= 3;
    green >>= 2;
    blue >>= 3;
    return (red << 11) | (green << 5) | blue;
}

// 绘制图标
void changeIcon(String newI)
{
    tft.fillRoundRect(350, 50, 90, 90, 5, BG);
    if (newI == "Overcast" && day == 0)
    {
        tft.fillCircle(415, 80, 13, 0xFD20);
        tft.fillCircle(395, 77, 17, TFT_WHITE);
        tft.fillCircle(375, 92, 13, TFT_WHITE);
        tft.drawCircle(375, 92, 14, BG);
        tft.drawCircle(375, 92, 15, BG);
        tft.fillCircle(415, 94, 11, TFT_WHITE);
        tft.fillRect(375, 90, 39, 16, TFT_WHITE);
    }
    if (newI == "Overcast" && day == 1)
    {
        tft.fillCircle(415, 80, 13, TFT_WHITE);
        tft.fillCircle(395, 77, 17, 0x6B4D);
        tft.fillCircle(375, 92, 13, 0x6B4D);
        tft.drawCircle(375, 92, 14, BG);
        tft.drawCircle(375, 92, 15, BG);
        tft.fillCircle(415, 94, 11, 0x6B4D);
        tft.fillRect(375, 90, 39, 16, 0x6B4D);
    }
    if (newI == "Heavy Rain")
    {
        tft.fillCircle(395, 77, 17, TC);
        tft.fillCircle(375, 92, 13, TC);
        tft.drawCircle(375, 92, 14, BG);
        tft.drawCircle(375, 92, 15, BG);
        tft.fillCircle(415, 94, 11, TC);
        tft.fillRect(379, 90, 36, 16, TC);
        tft.drawLine(382, 111, 377, 131, rgb(30, 144, 255));
        tft.drawLine(383, 111, 378, 131, rgb(30, 144, 255));
        tft.drawLine(384, 111, 379, 131, rgb(30, 144, 255));
        tft.drawLine(387, 111, 382, 131, rgb(30, 144, 255));
        tft.drawLine(388, 111, 383, 131, rgb(30, 144, 255));
        tft.drawLine(389, 111, 384, 131, rgb(30, 144, 255));
        tft.drawLine(392, 111, 387, 131, rgb(30, 144, 255));
        tft.drawLine(393, 111, 388, 131, rgb(30, 144, 255));
        tft.drawLine(394, 111, 389, 131, rgb(30, 144, 255));
        tft.drawLine(397, 111, 392, 131, rgb(30, 144, 255));
        tft.drawLine(398, 111, 393, 131, rgb(30, 144, 255));
        tft.drawLine(399, 111, 394, 131, rgb(30, 144, 255));
        tft.drawLine(402, 111, 397, 131, rgb(30, 144, 255));
        tft.drawLine(403, 111, 398, 131, rgb(30, 144, 255));
        tft.drawLine(404, 111, 399, 131, rgb(30, 144, 255));
    }
    if (newI == "Light Rain")
    {
        tft.fillCircle(395, 77, 17, TC);
        tft.fillCircle(375, 92, 13, TC);
        tft.drawCircle(375, 92, 14, BG);
        tft.drawCircle(375, 92, 15, BG);
        tft.fillCircle(415, 94, 11, TC);
        tft.fillRect(379, 90, 36, 16, TC);
        tft.drawLine(397, 111, 392, 131, rgb(30, 144, 255));
        tft.drawLine(398, 111, 393, 131, rgb(30, 144, 255));
        tft.drawLine(399, 111, 394, 131, rgb(30, 144, 255));
    }
    if (newI == "Moderate Rain")
    {
        tft.fillCircle(395, 77, 17, TC);
        tft.fillCircle(375, 92, 13, TC);
        tft.drawCircle(375, 92, 14, BG);
        tft.drawCircle(375, 92, 15, BG);
        tft.fillCircle(415, 94, 11, TC);
        tft.fillRect(379, 90, 36, 16, TC);
        tft.drawLine(387, 111, 382, 131, rgb(30, 144, 255));
        tft.drawLine(388, 111, 383, 131, rgb(30, 144, 255));
        tft.drawLine(389, 111, 384, 131, rgb(30, 144, 255));
        tft.drawLine(402, 111, 397, 131, rgb(30, 144, 255));
        tft.drawLine(403, 111, 398, 131, rgb(30, 144, 255));
        tft.drawLine(404, 111, 399, 131, rgb(30, 144, 255));
    }
    if ((newI == "Clear" || newI == "Sunny") && day == 0)
    {
        tft.fillCircle(395, 85, 20, 0xFD20);
        tft.fillCircle(395, 55, 4, 0xFD20);
        tft.fillCircle(395, 115, 4, 0xFD20);
        tft.fillCircle(365, 85, 4, 0xFD20);
        tft.fillCircle(425, 85, 4, 0xFD20);
        tft.fillCircle(374, 64, 4, 0xFD20);
        tft.fillCircle(416, 64, 4, 0xFD20);
        tft.fillCircle(374, 106, 4, 0xFD20);
        tft.fillCircle(416, 106, 4, 0xFD20);
    }
    if ((newI == "Clear" || newI == "Sunny") && day == 1)
    {
        tft.fillCircle(395, 90, 30, TFT_WHITE);
        tft.fillCircle(380, 80, 24, TFT_BLACK);
    }
    if (newI == "Cloudy")
    {
        tft.fillCircle(410, 75, 10, TC);
        tft.fillCircle(395, 82, 8, TC);
        tft.fillCircle(422, 82, 6, TC);
        tft.fillRect(398, 80, 25, 10, TC);

        tft.fillCircle(385, 92, 15, TC);
        tft.drawCircle(385, 92, 16, BG);
        tft.drawCircle(385, 92, 17, BG);
        tft.fillCircle(365, 102, 12, TC);
        tft.drawCircle(365, 102, 13, BG);
        tft.drawCircle(365, 102, 14, BG);
        tft.fillCircle(400, 104, 10, TC);
        tft.fillRect(368, 100, 30, 15, TC);
    }
}

// http访问函数
JSONVar httpCom(String host, String path, bool https)
{
    delay(0);
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_BLUE);
    JSONVar data;
    HTTPClient http;
    if (https)
    {
        WiFiClientSecure client;
        client.setInsecure();
        client.connect("https://" + host + path, 443);
        if (http.begin(client, "https://" + host + path))
        {
            int httpCode = http.GET();
            if (httpCode > 0)
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
                {
                    String payload = http.getString();
                    // Serial.print(payload);
                    char char_array[payload.length() + 1];
                    int i = 0;
                    for (; i < payload.length(); i++)
                    {
                        char_array[i] = payload[i];
                    }
                    char_array[i] = '\0';
                    data = JSON.parse(char_array);
                }
            }
            else
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
                delay(500);
            }
            http.end();
        }
        tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
        return data;
    }
    else
    {
        WiFiClient client;

        if (http.begin(client, "http://" + host + path))
        {
            int httpCode = http.GET();
            if (httpCode > 0)
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
                if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
                {
                    String payload = http.getString();
                    // Serial.print(payload);
                    char char_array[payload.length() + 1];
                    int i = 0;
                    for (; i < payload.length(); i++)
                    {
                        char_array[i] = payload[i];
                    }
                    char_array[i] = '\0';
                    data = JSON.parse(char_array);
                }
            }
            else
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
                delay(500);
            }
            http.end();
        }
        tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
        return data;
    }
    delay(0);
}