#include <TFT_eSPI.h> // https://github.com/Bodmer/TFT_eSPI
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <Arduino_JSON.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <NTP.h>
#include "fonts.h"

#define VERSION "2.7 OTA"

#define WIFINAME ""
#define WIFIPW ""
#define NTPADDRESS "pool.ntp.org"

const String APIKEY = ""; // https://dev.qweather.com/docs/start/get-api-key
const String LOCATION = ""; // https://dev.qweather.com/docs/api/geo/
const String PROXYAPI = ""; // See README

TFT_eSPI tft = TFT_eSPI();
String mainw = "";
String desc = "";
String times = "";
String temp = "";
String load[] = {"|", "/", "-", "\\"};
WiFiUDP wifiUdp;
NTP ntp(wifiUdp);
int n = 0, page = 1;

JSONVar hrStatus;
JSONVar dayStatus;

void updateTime();
void changeIcon(String newI);
JSONVar httpsCom(String host, String path, int port);
JSONVar httpCom(String host, String path, String port);

void setup(void)
{
    Serial.begin(9600);
    tft.begin();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.setTextFont(2);
    tft.setTextSize(1);
    tft.println("[Chip] ESP8266EX");
    tft.println("[Screen] ST8896S");
    tft.print("[Firmware] v");
    tft.println(VERSION);
    WiFi.begin(WIFINAME, WIFIPW);
    tft.print("[WIFI] Connecting to ");
    tft.println(WIFINAME);
    while (WiFi.status() != WL_CONNECTED)
    {

        for (int i = 0; i < 4; i++)
        {
            tft.setCursor(tft.width() / 2, tft.height() / 2);
            tft.setTextColor(TFT_WHITE);
            tft.print(load[i]);
            delay(250);
            tft.setCursor(tft.width() / 2, tft.height() / 2);
            tft.setTextColor(TFT_BLACK);
            tft.print(load[i]);
        }
    }
    tft.fillScreen(TFT_BLACK);
    tft.setCursor(0, 0);
    tft.setTextColor(TFT_WHITE);
    tft.println("[OTA] Setup");
    ArduinoOTA.setHostname("ESP8266");
    ArduinoOTA.begin();
    tft.print("[NTP] Begin -> ");
    tft.print(NTPADDRESS);
    ntp.ntpServer(NTPADDRESS);
    ntp.begin();
    ntp.timeZone(8);
    delay(1000);
    tft.setTextSize(1);
    tft.setCursor(20, 20);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE);
    tft.print(WIFINAME);
    tft.print("-");
    tft.print(WiFi.localIP());
    tft.print("      v");
    tft.println(VERSION);
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_GREEN);
    tft.drawRoundRect(10, 80, 460, 80, 10, TFT_YELLOW);
    tft.drawRoundRect(295, 165, 175, 145, 10, TFT_WHITE);
    tft.fillRoundRect(350, 50, 90, 90, 5, TFT_BLACK);
}

void loop()
{
    updateTime();
    ArduinoOTA.handle();
    if (n == 180 || n == 0)
    {
        if (n == 180)
        {
            n = 1;
        }
        tft.fillRoundRect(300, 20, 160, 20, 0, TFT_BLACK);
        tft.setCursor(320, 20);
        tft.setTextFont(2);
        tft.setTextSize(1);
        tft.println(times);
        String a, b, c;
        JSONVar nowStatus = httpsCom("devapi.qweather.com", "/v7/weather/now?location=" + LOCATION + "&key=" + APIKEY + "&lang=en&gzip=n", 443);
        if (JSON.typeof(nowStatus) == "undefined" || nowStatus["code"] != "200")
        {
            tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
        }
        else
        {
            if (mainw != nowStatus["now"]["text"])
            {
                tft.setFreeFont(FF36);
                tft.setCursor(20, 120);
                tft.setTextColor(TFT_BLACK);
                tft.print(mainw);
                mainw = nowStatus["now"]["text"];
                tft.setCursor(20, 120);
                tft.setTextColor(TFT_WHITE);
                tft.println(mainw);
                changeIcon(mainw);
            }
            a = nowStatus["now"]["temp"];
            b = nowStatus["now"]["humidity"];
            if (a + "°C/" + b + "%" != temp)
            {
                tft.setFreeFont(FF6);
                tft.setCursor(240, 120);
                tft.setTextColor(TFT_BLACK);
                tft.print(temp);
                temp = a + "°C/" + b + "%";
                tft.setCursor(240, 120);
                tft.setTextColor(TFT_WHITE);
                tft.print(temp);
            }
        }

        JSONVar airStatus = httpsCom("devapi.qweather.com", "/v7/air/now?location=" + LOCATION + "&key=" + APIKEY + "&lang=en&gzip=n", 443);
        if (JSON.typeof(airStatus) == "undefined" || airStatus["code"] != "200")
        {
            tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
        }
        else
        {
            a = airStatus["now"]["category"];
            b = airStatus["now"]["aqi"];
            c = airStatus["now"]["pm2p5"];
            if (a + "  AQI " + b + "  PM2.5 " + c != desc)
            {
                tft.setFreeFont(FF17);
                tft.setCursor(20, 150);
                tft.setTextColor(TFT_BLACK);
                tft.print(desc);
                desc = a + "  AQI " + b + "  PM2.5 " + c;
                tft.setCursor(20, 150);
                tft.setTextColor(TFT_WHITE);
                tft.println(desc);
            }
        }
        hrStatus = httpCom(PROXYAPI, "/v7/weather/24h/" + LOCATION + "/" + APIKEY + "/en", "5000");
        dayStatus = httpCom(PROXYAPI, "/v7/weather/7d/" + LOCATION + "/" + APIKEY + "/en", "5000");
    }

    if (n % 15 == 0)
    {
        tft.setFreeFont(FF33);
        tft.fillRect(10, 163, 270, 144, TFT_BLACK);
        if (page == 0)
        {
            if (JSON.typeof(hrStatus) == "undefined" || hrStatus["code"] != "200")
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            }
            else
            {
                page = 1;
                for (int i = 0; i < 7; i++)
                {
                    tft.setCursor(20, 182 + i * 20);
                    tft.setTextColor(TFT_WHITE);
                    tft.print((const char *)hrStatus["hourly"][i]["fxTime"]);
                    tft.setCursor(20 + 70, 182 + i * 20);
                    tft.print((const char *)hrStatus["hourly"][i]["text"]);
                    tft.setCursor(20 + 210, 182 + i * 20);
                    tft.print((const char *)hrStatus["hourly"][i]["temp"]);
                }
            }
        }
        else
        {
            if (JSON.typeof(dayStatus) == "undefined" || dayStatus["code"] != "200")
            {
                tft.drawRoundRect(5, 5, 470, 310, 10, TFT_RED);
            }
            else
            {
                page = 0;
                for (int i = 0; i < 7; i++)
                {
                    tft.setCursor(20, 182 + i * 20);
                    tft.setTextColor(TFT_WHITE);
                    tft.print((const char *)dayStatus["daily"][i]["fxDate"]);
                    tft.setCursor(20 + 70, 182 + i * 20);
                    tft.print((const char *)dayStatus["daily"][i]["text"]);
                    tft.setCursor(20 + 180, 182 + i * 20);
                    tft.print((const char *)dayStatus["daily"][i]["temp"]);
                }
            }
        }
    }
    n++;
    delay(1000);
}

void updateTime()
{
    ntp.update();
    String newT = ntp.formattedTime("%Y-%m-%d %H:%M:%S");
    tft.setFreeFont(FF6);
    tft.setCursor(20, 60);
    for (int i = 0; i < 19; i++)
    {
        if (times[i] != newT[i])
        {
            int x = tft.getCursorX();
            tft.setTextColor(TFT_BLACK);
            tft.print(times[i]);
            tft.setCursor(x, 60);
            tft.setTextColor(TFT_WHITE);
            tft.print(newT[i]);
        }
        else
        {
            tft.setTextColor(TFT_WHITE);
            tft.print(times[i]);
        }
    }
    times = newT;
}

void changeIcon(String newI)
{
    tft.fillRoundRect(350, 50, 90, 90, 5, TFT_BLACK);
    tft.drawRoundRect(350, 50, 90, 90, 5, TFT_BLACK);
    if (newI == "Overcast")
    {
        tft.drawRoundRect(350, 50, 90, 90, 5, TFT_GREEN);
        tft.drawFastHLine(360, 120, 70, TFT_WHITE);
        tft.drawFastHLine(360, 110, 70, TFT_WHITE);
        tft.drawFastHLine(360, 100, 70, TFT_WHITE);
        tft.drawFastHLine(360, 90, 70, TFT_WHITE);
        tft.drawFastHLine(360, 80, 70, TFT_WHITE);
        tft.drawFastHLine(360, 70, 70, TFT_WHITE);
    }
    if (newI == "Light Rain")
    {
        tft.fillCircle(395, 77, 17, TFT_WHITE);
        tft.fillCircle(375, 92, 13, TFT_WHITE);
        tft.drawCircle(375, 92, 14, TFT_BLACK);
        tft.drawCircle(375, 92, 15, TFT_BLACK);
        tft.fillCircle(415, 94, 11, TFT_WHITE);
        tft.fillRect(379, 90, 36, 16, TFT_WHITE);
        tft.drawLine(397, 111, 392, 131, TFT_BLUE);
        tft.drawLine(398, 111, 393, 131, TFT_BLUE);
    }
    if (newI == "Clear" || newI == "Sunny")
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
    if (newI == "Cloudy")
    {
        tft.fillCircle(415, 80, 13, 0xFD20);
        tft.fillCircle(395, 77, 17, TFT_WHITE);
        tft.fillCircle(375, 92, 13, TFT_WHITE);
        tft.drawCircle(375, 92, 14, TFT_BLACK);
        tft.drawCircle(375, 92, 15, TFT_BLACK);
        tft.fillCircle(415, 94, 11, TFT_WHITE);
        tft.fillRect(379, 90, 36, 16, TFT_WHITE);
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
    delay(500);
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
        delay(0);
        String line = httpsClient.readStringUntil('\n');
        Serial.print(line);
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

JSONVar httpCom(String host, String path, String port)
{
    tft.drawRoundRect(5, 5, 470, 310, 10, TFT_BLUE);
    HTTPClient http;
    WiFiClient client;
    JSONVar data;
    if (http.begin(client, "http://" + host + ":" + port + path))
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