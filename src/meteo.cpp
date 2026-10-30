#include "meteo.h"
#include "network_io.h"
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <math.h>

classMeteo::classMeteo() {
  temp = 0;
  humidity = 0;
  wind_speed = 0;
  description = "";
  icon = "";
  snow = 0;
  _mutex = xSemaphoreCreateMutex();
}

bool classMeteo::Update() {
  WiFiClientSecure client;
  client.setInsecure();
  client.setHandshakeTimeout(5);
  HTTPClient http;
  String url = F("https://api.openweathermap.org/data/2.5/weather?");
  if (!url.concat(Api) || !http.begin(client, url)) {
    return false;
  }

  configureFeedRequest(http);
  if (http.GET() != HTTP_CODE_OK || !feedResponseIsReadable(http)) {
    http.end();
    return false;
  }

  StaticJsonDocument<512> json;
  StaticJsonDocument<JSON_OBJECT_SIZE(4) + JSON_OBJECT_SIZE(2) +
                     JSON_OBJECT_SIZE(1) + JSON_ARRAY_SIZE(1) +
                     JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(1)> filter;
  filter["main"]["temp"] = true;
  filter["main"]["humidity"] = true;
  filter["wind"]["speed"] = true;
  filter["weather"][0]["description"] = true;
  filter["weather"][0]["icon"] = true;
  filter["snow"]["1h"] = true;
  FeedReader reader(http);
  DeserializationError error =
      deserializeJson(json, reader, DeserializationOption::Filter(filter));
  http.end();
  if (error || !json["main"]["temp"].is<float>() ||
      !json["main"]["humidity"].is<int>() ||
      !json["wind"]["speed"].is<float>() ||
      !json["weather"][0]["description"].is<const char *>() ||
      !json["weather"][0]["icon"].is<const char *>()) {
    return false;
  }

  const float newTemp = json["main"]["temp"];
  const int newHumidity = json["main"]["humidity"];
  const float newWind = json["wind"]["speed"];
  String newDescription = json["weather"][0]["description"].as<const char *>();
  String newIcon = json["weather"][0]["icon"].as<const char *>();
  float newSnow = 0;
  if (!isfinite(newTemp) || !isfinite(newWind) || newWind < 0 ||
      newHumidity < 0 || newHumidity > 100 || newIcon.length() != 3 ||
      (newIcon[2] != 'd' && newIcon[2] != 'n') ||
      !isDigit(newIcon[0]) || !isDigit(newIcon[1])) {
    return false;
  }
  if (!json["snow"]["1h"].isNull()) {
    if (!json["snow"]["1h"].is<float>()) return false;
    newSnow = json["snow"]["1h"];
    if (!isfinite(newSnow) || newSnow < 0) return false;
  }
  newDescription.toUpperCase();
  newIcon.toUpperCase();

  if (!lock()) return false;
  temp = newTemp;
  humidity = newHumidity;
  wind_speed = newWind;
  description = newDescription;
  icon = newIcon;
  snow = newSnow;
  unlock();
  return true;
}
