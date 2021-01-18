#include <TFT_eSPI.h>     // https://github.com/Bodmer/TFT_eSPI
#include <SPI.h>          // https://www.arduino.cc/en/Reference/SPI
#include <Arduino_JSON.h> // https://github.com/arduino-libraries/Arduino_JSON
// https://github.com/sstaub/NTP
#include <WiFiUdp.h>
#include <NTP.h>
// https://github.com/adafruit/Adafruit_SGP30
#include "Adafruit_SGP30.h"
#include <Wire.h>
#include <stdlib.h>
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

#define VERSION "7.0"

#define NTPADDRESS "ntp.aliyun.com"
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

JSONVar hrStatus;
JSONVar dayStatus;
JSONVar config;
Adafruit_SGP30 sgp;
ESP8266WebServer server(80);

bool loadconfig();
void conntionWIFI();
void handleRoot();
void handlewifi();
void wificonfig(bool pass);
void showInfo();
void updateTime();
void changeIcon(String newI);
void offline();
JSONVar httpCom(String host, String path);
JSONVar httpsCom(String host, String path, int port);

void setup(void)
{
    Serial.begin(115200);
    tft.init();
    tft.begin();
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
    if (!LittleFS.begin())
    {
        tft.println("[LittleFS] Failed to mount file system");
        tft.setCursor(20, tft.getCursorY());
        while (1)
        {
            delay(0);
        }
    }
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

    wificonfig(false);
    tft.setCursor(20, tft.getCursorY());
    tft.println("[Web Config] HTTP server started");
    ArduinoOTA.setHostname("ESP8266");
    ArduinoOTA.begin();
    showInfo();
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

void loop()
{
    File flagFile = LittleFS.open("/flag", "r");
    String successFlag = flagFile.readStringUntil('\n');
    flagFile.close();
    ArduinoOTA.handle();
    server.handleClient();
    if (successFlag != "-1")
    {
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
        if (n % 300 == 0 || reload >= 1)
        {
            ntp.update();
            tft.fillRoundRect(300, 20, 160, 20, 0, BG);
            tft.setCursor(320, 20);
            tft.setTextFont(2);
            tft.setTextSize(1);
            tft.println(times);
            String a, b, c, d;
            ESP.wdtFeed();
            JSONVar nowStatus = httpsCom("devapi.qweather.com", "/v7/weather/now?location=" + LOCATION + "&key=" + APIKEY + "&lang=en&gzip=n", 443);
            if (JSON.typeof(nowStatus) == "undefined")
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            }
            else
            {
                const char *aa = nowStatus["code"];
                String a = aa;
                if (a == "200")
                {
                    if (mainw != nowStatus["now"]["text"])
                    {
                        tft.setFreeFont(FF36);
                        tft.setCursor(20, 120);
                        tft.setTextColor(BG);
                        tft.print(mainw);
                        mainw = nowStatus["now"]["text"];
                        tft.setCursor(20, 120);
                        tft.setTextColor(TC);
                        tft.println(mainw);
                        changeIcon(mainw);
                    }
                    a = nowStatus["now"]["temp"];
                    b = nowStatus["now"]["humidity"];
                    if (a + "°C/" + b + "%" != temp)
                    {
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
            ESP.wdtFeed();
            JSONVar StatusNew;
            JSONVar airStatus = httpsCom("devapi.qweather.com", "/v7/air/now?location=" + LOCATION + "&key=" + APIKEY + "&lang=en&gzip=n", 443);
            if (JSON.typeof(airStatus) == "undefined")
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            }
            else
            {
                String warn = "";
                uint32_t WC = TFT_WHITE; // wearning color
                StatusNew = httpCom(PROXYAPI, "/v7/warning/now/" + LOCATION + "/" + APIKEY + "/en");
                if (JSON.typeof(StatusNew) == "undefined")
                {
                    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
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
                const char *aa = airStatus["code"];
                String a = aa;
                if (a == "200")
                {
                    a = airStatus["now"]["category"];
                    b = airStatus["now"]["aqi"];
                    c = airStatus["now"]["pm2p5"];
                    d = airStatus["now"]["pm10"];
                    if (desc != a + b + c + d + warn)
                    {
                        tft.fillRoundRect(16, 135, 450, 22, 5, BG);
                        tft.setFreeFont(FF17);
                        desc = a + b + c + warn;
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
                        tft.setTextColor(WC);
                        tft.print("    ");
                        tft.print(warn);
                    }
                }
            }
            ESP.wdtFeed();
            StatusNew = httpCom(PROXYAPI, "/v7/weather/24h/" + LOCATION + "/" + APIKEY + "/en");
            if (JSON.typeof(StatusNew) == "undefined")
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
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
            StatusNew = httpCom(PROXYAPI, "/v7/weather/7d/" + LOCATION + "/" + APIKEY + "/en");
            if (JSON.typeof(StatusNew) == "undefined")
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
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
        }

        if (n % 3 == 0 || reload == 1)
        {
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

        if (n % 15 == 0 || reload == 1)
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
        n++;
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

    delay(1000);
}

int wififlag = 1;

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

void handleRoot()
{
    server.send(200, "text/html", HTMLTEXT);
}

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

void offline()
{
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
    if (!sgp.IAQmeasure())
    {
        tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
    }
    else
    {
        if (sgp.TVOC != tvoc)
        {
            tft.setCursor(192, 150);
            tft.setFreeFont(FF17);
            tft.println("TVOC");
            tft.setCursor(192, tft.getCursorY() + 10);
            tft.setFreeFont(FF6);
            tft.fillRect(187, 160, 75, 30, BG);
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
            tft.setCursor(288, 150);
            tft.setFreeFont(FF17);
            tft.println("CO2");
            tft.setCursor(288, tft.getCursorY() + 10);
            tft.setFreeFont(FF6);
            tft.fillRect(283, 160, 75, 30, BG);
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
        delay(1000);
    }
    server.close();
    WiFi.softAPdisconnect(true);
    mode = 1;
    server.on("/", handleRoot);
    server.on("/config/" + configPass, handlewifi);
    server.begin();
}

void showInfo()
{
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.setCursor(20, tft.getCursorY());
    tft.println("[OTA] Start");
    tft.setCursor(20, tft.getCursorY());
    tft.print("[NTP] Begin -> ");
    tft.println(NTPADDRESS);
    ntp.ntpServer(NTPADDRESS);
    ntp.begin();
    ntp.timeZone(TIMEZONE);
    delay(1000);
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
            reload = 1;
            mainw = "";
        }
        else if (((a[0] == '0' && a[1] > '6') || (a[0] == '1' && a[1] < '8')) && day == 1)
        {
            day = 0;
            tft.drawRoundRect(10, 80, 460, 80, 10, TFT_YELLOW);
            reload = 1;
            mainw = "";
        }
    }
    int y = 0;
    if (mode == 0)
    {
        tft.setFreeFont(FMB18);
        y = 80;
    }
    else
    {
        tft.setFreeFont(FF6);
        y = 60;
    }
    tft.setCursor(20, y);

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
    if (newI == "Light Rain")
    {
        tft.fillCircle(395, 77, 17, TC);
        tft.fillCircle(375, 92, 13, TC);
        tft.drawCircle(375, 92, 14, BG);
        tft.drawCircle(375, 92, 15, BG);
        tft.fillCircle(415, 94, 11, TC);
        tft.fillRect(379, 90, 36, 16, TC);
        tft.drawLine(397, 111, 392, 131, TFT_BLUE);
        tft.drawLine(398, 111, 393, 131, TFT_BLUE);
    }
    if ((newI == "Clear" || newI == "Sunny") && day == 0)
    {
        tft.fillCircle(395, 95, 20, 0xFD20);
        tft.fillCircle(395, 65, 4, 0xFD20);
        tft.fillCircle(395, 125, 4, 0xFD20);
        tft.fillCircle(365, 95, 4, 0xFD20);
        tft.fillCircle(425, 95, 4, 0xFD20);
        tft.fillCircle(374, 74, 4, 0xFD20);
        tft.fillCircle(416, 74, 4, 0xFD20);
        tft.fillCircle(374, 116, 4, 0xFD20);
        tft.fillCircle(416, 116, 4, 0xFD20);
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

JSONVar httpsCom(String host, String path, int port)
{
    JSONVar data;
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_BLUE);
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    httpsClient.setCiphersLessSecure();
    httpsClient.setTimeout(15000);
    int r = 0;
    while ((!httpsClient.connect(host, port)) && (r < 30))
    {
        delay(100);
        r++;
    }
    if (r == 30)
    {
        return data;
    }
    String request = String("GET ") + path + " HTTP/1.1\r\n" +
                     "Host: " + host + "\r\n" +
                     "Connection: close\r\n" +
                     "\r\n";
    httpsClient.print(request);
    int flag = 0;
    String payload = "";
    while (httpsClient.connected())
    {
        String line = httpsClient.readStringUntil('\n');
        if (line == "\r")
        {
            flag = 1;
        }
        if (flag == 1)
        {
            payload += line;
        }
    }
    char char_array[payload.length() + 1];
    int i = 0;
    for (; i < payload.length(); i++)
    {
        char_array[i] = payload[i];
    }
    char_array[i] = '\0';
    data = JSON.parse(char_array);
    httpsClient.stop();
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
    delay(500);
    return data;
}

JSONVar httpCom(String host, String path)
{
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_BLUE);
    HTTPClient http;
    WiFiClient client;
    JSONVar data;

    if (http.begin(client, "http://" + host + path))
    {
        int httpCode = http.GET();
        if (httpCode > 0)
        {
            tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
            {
                String payload = http.getString();
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