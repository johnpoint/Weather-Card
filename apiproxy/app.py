from flask import Flask
import json
import requests
import time
import iso8601
import datetime
app = Flask(__name__)


@app.route('/v7/<string:ftype>/<string:dtype>/<string:location>/<string:key>/<string:lang>')
def get_data(ftype, dtype, location, key, lang):
    try:
        if ftype == "weather":
            if dtype == "24h":
                data = requests.get(
                    "https://devapi.qweather.com/v7/weather/24h?location="+location+"&key="+key+"&lang=en").text
                data = json.loads(data)
                if data["code"] != "200":
                    return data
                rdata = {"code": "200", "hourly": []}
                j = 0
                for i in range(0, 7):
                    d = {"fxTime": iso8601.parse_date(
                        data["hourly"][i]["fxTime"]).strftime('%H:%M'),
                        "temp": data["hourly"][i]["temp"]+"C",
                        "text": data["hourly"][i]["text"]}
                    rdata["hourly"].append(d)
                return rdata
            if dtype == "7d":
                data = requests.get(
                    "https://devapi.qweather.com/v7/weather/7d?location="+location+"&key="+key+"&lang=en").text
                data = json.loads(data)
                if data["code"] != "200":
                    return data
                rdata = {"code": "200", "daily": []}
                for i in range(0, 7):
                    d = {"fxDate": datetime.datetime.strptime(data["daily"][i]["fxDate"], '%Y-%m-%d').strftime('%m-%d'),
                         "temp": data["daily"][i]["tempMin"]+"C ~ "+data["daily"][i]["tempMax"]+"C",
                         "text": data["daily"][i]["textDay"]}
                    rdata["daily"].append(d)
                return rdata
        elif ftype == "warning":
            data = requests.get(
                "https://devapi.qweather.com/v7/warning/now?location="+location+"&key="+key+"&lang=en").text
            data = json.loads(data)
            if data["code"] != "200":
                return data
            rdata = {"code": "200", "warning": {}}
            if len(data["warning"]) != 0:
                rdata["warning"] = {
                    "level": data["warning"][0]["level"],
                    "typeName": data["warning"][0]["typeName"]
                }
            else:
                rdata = {"code": "404"}
            return rdata
    except BaseException:
        return {"code": "500"}
