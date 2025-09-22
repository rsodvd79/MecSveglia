#include "meteo.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

classMeteo::classMeteo() {
	temp = 0;
	humidity = 0;
	wind_speed = 0;
	description = "";
	icon = "";
	snow = 0;
}

void classMeteo::Update() {
	WiFiClient client;
	HTTPClient http;

	String URL = F("http://api.openweathermap.org/data/2.5/weather?");

	if (URL.concat(Api) && http.begin(client, URL)) {
		int httpCode = http.GET();

		if (httpCode > 0) {
			if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {

				DynamicJsonDocument jsonBuffer(2048);
				// Filter only required fields to reduce memory usage
				DynamicJsonDocument filter(256);
				filter[F("main")][F("temp")] = true;
				filter[F("main")][F("humidity")] = true;
				filter[F("wind")][F("speed")] = true;
				filter[F("weather")][0][F("description")] = true;
				filter[F("weather")][0][F("icon")] = true;
				filter[F("snow")][F("1h")] = true;
				DeserializationError error = deserializeJson(jsonBuffer, http.getString(), DeserializationOption::Filter(filter));

				if (error) {
					description = error.c_str();

				}
				else {
					temp = (float)(jsonBuffer[F("main")][F("temp")]);
					humidity = jsonBuffer[F("main")][F("humidity")];
					wind_speed = jsonBuffer[F("wind")][F("speed")];
					
					String d = jsonBuffer[F("weather")][0][F("description")];
					description = d; 
					description.toUpperCase();

					String i = jsonBuffer[F("weather")][0][F("icon")];
					icon = i; 
					icon.toUpperCase();

					if (icon == "13D" || icon == "13N") { // snow
						snow = (float)jsonBuffer[F("snow")][F("1h")];
					}
					else {
						snow = 0;
					}
				}

			}
			else {
				icon = String(F("ERR"));
			}
		}
		else {
			icon = String(F("ERR"));
		}

		http.end();

	}
	else {
		icon = String(F("ERR"));
	}

}
