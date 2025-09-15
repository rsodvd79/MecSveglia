/*
  Furgone - ESP8266 with OLED display and web server
  - Board: "Generic ESP8266 Module"
  - Framework: "Arduino"
  - Crystal Frequency: "26 MHz"
  - Debug port: "Disabled"
  - Flash Size: "1MB (FS:512KB OTA :~ 246KB)"
  - C++ Exceptions: "Disabled (new aborts on oom)"
  - Flash Frequency: "40MHz"
  - Flash Mode: "DOUT (compatible)"
  - IwlP Variant: "v2 Lower Memory"
  - Builtin Led: "2"
  - Debug Level: "None"
  - MMU: "32KB cache + 32KB IRAM (balanced)"
  - Non-32-Bit Access: "Use pgm_read macros for IRAM/PROGMEM"
  - Reset Method: "dtr (aka nodemcu)"
  - NONOS SDK Version: "nonos-sdk 2.2.1+100 (190703)"
  - SSL Support: "All SSL ciphers (most compatible)"
  - Stack Protection: "Disabled"
  - VTables: "Flash"
  - Erase Flash: "Only Sketch"
  - CPU Frequency: "80 MHz"
*/

#include <coredecls.h>                  // settimeofday_cb()
#include <PolledTimeout.h>
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval
#include <TZ.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#define SSD1306_NO_SPLASH
#include <Adafruit_SSD1306.h>
#include <ArduinoOTA.h>
#include <LittleFS.h> // LittleFS is declared
#include <math.h>
#include "eyes.h"
#include "meteo.h"
#include "Button.h"
#include "screen.h"
#include "GOL.h"

Adafruit_SSD1306 display(128, 32, &Wire, -1);

String ssid = "";
String password = "";
String mdns_name = "FURGONE";

static timeval tv;
static time_t xnow;

static esp8266::polledTimeout::periodicMs showTimeNow(250);
static esp8266::polledTimeout::periodicMs showSTimeNow(500);
static esp8266::polledTimeout::periodicMs showWifiNow(500);
static esp8266::polledTimeout::periodicMs showEyesNow(125);
static esp8266::polledTimeout::periodicMs showMeteoNow(1000 * 30);
static esp8266::polledTimeout::periodicMs showCicloScreenNow(1000 * 60);
static esp8266::polledTimeout::periodicMs showGOLNow(80);
static esp8266::polledTimeout::periodicMs showTriNow(60);
static esp8266::polledTimeout::periodicMs adjustBrightnessNow(1000 * 60);
static esp8266::polledTimeout::periodicMs invertPulseNow(1000 * 60 * 30);
static int time_machine_days = 0; // 0 = now
static bool time_machine_running = false;
static bool tsep = true;
//static bool keyp = false;

ESP8266WebServer server(80);
classEye eye = classEye();
classButton Bottone = classButton(12, ButtonType::PULLUP);
classMeteo meteo = classMeteo();
classScreen screen = classScreen(6);
bool CicloScreen = false;
classGOL GOL = classGOL();
bool mdnsStarted = false;

// HTTP handlers prototypes
void handleSetupGet();
void handleSetupPost();
void handleStatus();
void handleOled();
void handleFavicon();

void handleRoot() {
	digitalWrite(LED_BUILTIN, LOW);

	// Serve INDEX.HTM if present, otherwise 404 with a minimal informative page
	if (LittleFS.exists("/INDEX.HTM")) {
		File fhtm = LittleFS.open("/INDEX.HTM", "r");
		if (fhtm) {
			String message = fhtm.readString();
			fhtm.close();
			server.send(200, F("text/html"), message);
			digitalWrite(LED_BUILTIN, HIGH);
			return;
		}
	}

	// Minimal fallback page
	String body;
	body.reserve(512);
	body += F("<!doctype html><html lang=\"it\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>404 • INDEX.HTM mancante</title><style>body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:0;padding:24px;background:#0f172a;color:#e5e7eb}a{color:#93c5fd}</style></head><body>");
	body += F("<h1>Pagina non trovata</h1><p>Manca il file <code>INDEX.HTM</code> su LittleFS.</p><ul>");
	body += F("<li>Carica i file in <code>/data</code> con ‘Upload Filesystem Image’.</li>");
	body += F("<li>Oppure vai a <a href=\"/setup\">/setup</a> per configurare il Wi‑Fi.</li></ul></body></html>");
	server.send(404, F("text/html"), body);
	digitalWrite(LED_BUILTIN, HIGH);
}

void handleNotFound() {
	digitalWrite(LED_BUILTIN, LOW);

	String message = F("");

	if (LittleFS.exists("/M404.HTM")) {
		File fhtm = LittleFS.open("/M404.HTM", "r");
		if (fhtm) {
			message = fhtm.readString();
			fhtm.close();
		}
	}

	server.send(404, F("text/html"), message);
	digitalWrite(LED_BUILTIN, HIGH);
}

void time_is_set_scheduled() {
	// everything is allowed in this function

	if (time_machine_days == 0) {
		time_machine_running = !time_machine_running;
	}

	// time machine demo
	if (time_machine_running) {
		digitalWrite(LED_BUILTIN, LOW);

		xnow = time(nullptr);

		//const tm* tm = localtime(&xnow);

		gettimeofday(&tv, nullptr);
		constexpr int days = 30;
		time_machine_days += days;
		if (time_machine_days > 360) {
			tv.tv_sec -= (time_machine_days - days) * 60 * 60 * 24;
			time_machine_days = 0;
		}
		else {
			tv.tv_sec += days * 60 * 60 * 24;
		}
		settimeofday(&tv, nullptr);

		digitalWrite(LED_BUILTIN, HIGH);
	}

}

int dBmtoPercentage(int dBm) {
	int RSSI_MAX = 0;//-50 // define maximum strength of signal in dBm
	int RSSI_MIN = -100;// define minimum strength of signal in dBm
	int quality = 0;

	if (dBm <= RSSI_MIN)
	{
		quality = 0;
	}
	else if (dBm >= RSSI_MAX)
	{
		quality = 100;
	}
	else
	{
		quality = 2 * (dBm + 100);
	}

	return quality;
}

void showScreen0(bool cmp) {
	display.clearDisplay();
	display.setTextSize(1);
	display.setCursor(0, 0);
	display.print(F("Con. to ")); display.println(ssid);
	display.print(F("IP : ")); display.println(WiFi.localIP());
	if (cmp) {
		display.print(F("mDNS : ")); display.println(mdns_name);
		int rssi = WiFi.RSSI();
		display.print(F("RSSI : ")); display.print(rssi); display.print(F(" dBm ")); display.print(dBmtoPercentage(rssi)); display.println(F(" %"));
	}
	display.display();
}

void showScreen1() {
	gettimeofday(&tv, nullptr);
    // removed unused clock_gettime call
	xnow = time(nullptr);

	if (showSTimeNow) {
		tsep = tsep ? false : true;
	}

	char LCDTime[9];

	if (tsep == 0) {
		strftime(LCDTime, 9, "%H:%M:%S", localtime(&xnow));
	}
	else {
		strftime(LCDTime, 9, "%H %M %S", localtime(&xnow));
	}

	char LCDDate[11];

	strftime(LCDDate, 11, "%d/%m/%Y", localtime(&xnow));

	// Simple horizontal + vertical bounce of the time+date block
	static int timeX = 0;
	static int dirX = 1; // 1 right, -1 left
	static int timeY = 7; // starting Y similar to previous layout
	static int dirY = 1; // 1 down, -1 up

	// compute pixel widths with default font (6 px per char incl. spacing)
	int timeChars = 8; // HH:MM:SS
	int timeW = timeChars * 6 * 2; // textsize=2
	int dateChars = 10; // dd/mm/yyyy
	int dateW = dateChars * 6 * 1; // textsize=1

	int maxX = 128 - timeW;
	if (maxX < 0) maxX = 0;

	// advance X position each refresh
	timeX += dirX;
	if (timeX <= 0) { timeX = 0; dirX = 1; }
	else if (timeX >= maxX) { timeX = maxX; dirX = -1; }

	int dateX = timeX + (timeW - dateW) / 2;
	if (dateX < 0) dateX = 0;
	if (dateX > 128 - dateW) dateX = 128 - dateW;

	// advance Y position with constraints so both lines fit
	const int glyphH = 8; // default font height in pixels
	int timeH = glyphH * 2; // textsize=2
	int dateH = glyphH * 1; // textsize=1
	int gap = 1;            // 1 px spacing between lines
	int maxY = 32 - (timeH + gap + dateH);
	if (maxY < 0) maxY = 0;

	timeY += dirY;
	if (timeY <= 0) { timeY = 0; dirY = 1; }
	else if (timeY >= maxY) { timeY = maxY; dirY = -1; }

	int dateY = timeY + timeH + gap;

	display.clearDisplay();

	display.setTextSize(2);
	display.setCursor(timeX, timeY);
	display.print(LCDTime);

	display.setTextSize(1);
	display.setCursor(dateX, dateY);
	display.print(LCDDate);

	display.display();
}

void showScreen2() {

	display.clearDisplay();

	switch (eye.Next())
	{
	case 0:
		display.drawBitmap(0, 0, eye1, 128, 32, 1);
		break;
	case 1:
		display.drawBitmap(0, 0, eye2, 128, 32, 1);
		break;
	case 2:
		display.drawBitmap(0, 0, eye3, 128, 32, 1);
		break;
	case 3:
		display.drawBitmap(0, 0, eye4, 128, 32, 1);
		break;
	case 4:
		display.drawBitmap(0, 0, eye5, 128, 32, 1);
		break;
	case 5:
		display.drawBitmap(0, 0, eye6, 128, 32, 1);
		break;
	case 6:
		display.drawBitmap(0, 0, eye7, 128, 32, 1);
		break;
	case 7:
		display.drawBitmap(0, 0, eye8, 128, 32, 1);
		break;
	case 8:
		display.drawBitmap(0, 0, eye9, 128, 32, 1);
		break;
	case 9:
		display.drawBitmap(0, 0, eye10, 128, 32, 1);
		break;
	case 10:
		display.drawBitmap(0, 0, eye11, 128, 32, 1);
		break;
	case 11:
		display.drawBitmap(0, 0, eye12, 128, 32, 1);
		break;
	case 12:
		display.drawBitmap(0, 0, eye13, 128, 32, 1);
		break;
	case 13:
		display.drawBitmap(0, 0, eye14, 128, 32, 1);
		break;
		//case 14:
		//	display.drawBitmap(0, 0, eye15, 128, 32, 1);
		//	break;
		//case 15:
		//	display.drawBitmap(0, 0, eye16, 128, 32, 1);
		//	break;
	}

	display.display();

}

void showScreen3() {
	display.clearDisplay();
	display.setCursor(0, 0);
	display.setTextSize(1);
	display.print(F("TEMP. ")); display.printf("%.1f", meteo.temp); display.println(F(" C"));
	display.print(F("UMID. ")); display.printf("%d", meteo.humidity); display.println(F(" %"));
	display.print(F("VENTO ")); display.printf("%.1f", meteo.wind_speed); display.println(F(" m/s"));
	display.print(meteo.description);

	if (meteo.icon == F("01D")) { // clear sky
		display.drawBitmap(128 - 32, 0, Sun, 32, 32, 1);

	}
	else if (meteo.icon == F("01N")) { // clear sky
		display.drawBitmap(128 - 32, 0, Moon, 32, 32, 1);

	}
	else if (meteo.icon == F("02D")) { // few clouds
		display.drawBitmap(128 - 32, 0, CloudyDay, 32, 32, 1);

	}
	else if (meteo.icon == F("02N")) { // few clouds
		display.drawBitmap(128 - 32, 0, CloudyNight, 32, 32, 1);

	}
	else if (meteo.icon == F("03D") || meteo.icon == F("03N")) { // scattered clouds
		display.drawBitmap(128 - 32, 0, Clouds, 32, 32, 1);

	}
	else if (meteo.icon == F("04D") || meteo.icon == F("04N")) { // broken clouds
		display.drawBitmap(128 - 32, 0, Clouds, 32, 32, 1);

	}
	else if (meteo.icon == F("09D") || meteo.icon == F("09N")) { // shower rain
		display.drawBitmap(128 - 32, 0, Downpour, 32, 32, 1);

	}
	else if (meteo.icon == F("10D") || meteo.icon == F("10N")) { // rain
		display.drawBitmap(128 - 32, 0, Rain, 32, 32, 1);

	}
	else if (meteo.icon == F("11D") || meteo.icon == F("11N")) { // thunderstorm
		display.drawBitmap(128 - 32, 0, Storm, 32, 32, 1);

	}
	else if (meteo.icon == F("13D") || meteo.icon == F("13N")) { // snow
		display.print(F(" ")); display.printf("%.1f", meteo.snow); display.print(F(" mm/h"));
		display.drawBitmap(128 - 32, 0, Snow, 32, 32, 1);

	}
	else if (meteo.icon == F("50D")) { // mist
		display.drawBitmap(128 - 32, 0, FogDay, 32, 32, 1);

	}
	else if (meteo.icon == F("50N")) { // mist
		display.drawBitmap(128 - 32, 0, FogNight, 32, 32, 1);

	}
	else if (meteo.icon == F("ERR")) { // mist
		display.drawBitmap(128 - 32, 0, MeteoErr, 32, 32, 1);

	}

	display.display();

}

void showScreen4() {

	display.clearDisplay();

	for (byte X = 0; X < MondoLarghezza; X++) {
		for (byte Y = 0; Y < MondoAltezza; Y++) {
			if (GOL.Mondo[X][Y]) {
				byte x = map(X, 0, MondoLarghezza, 0, 128);
				display.drawPixel(x, Y, WHITE);
				display.drawPixel(x + 1, Y, WHITE);
			}
		}
	}

	display.display();
}

// Animated triangle: 3 independent vertices bounce with slight random angle change
void showScreen5() {
	const int W = display.width();
	const int H = display.height();
	static bool init = false;
	static float x[3], y[3], vx[3], vy[3];
    if (!init) {
        // seed PRNG for initial directions
        randomSeed(analogRead(0));
		// Initialize positions and velocities
		for (int i = 0; i < 3; i++) {
			x[i] = random(8, max(9, W - 8));
			y[i] = random(4, max(5, H - 4));
			float ang = (float)random(0, 360) * 0.01745329252f; // deg -> rad
			float spd = ((float)random(45, 110)) / 100.0f;     // 0.45..1.10 px/tick
            vx[i] = cos(ang) * spd;
            vy[i] = sin(ang) * spd;
        }
        init = true;
    }

	// Advance + bounce with slight random deflection on impact
	for (int i = 0; i < 3; i++) {
		float nx = x[i] + vx[i];
		float ny = y[i] + vy[i];

		bool bounced = false;
		bool hitVertical = false;
		bool hitHorizontal = false;

		if (nx < 0.0f) {
			nx = 0.0f; hitVertical = true; bounced = true;
		}
		else if (nx > (float)(W - 1)) {
			nx = (float)(W - 1); hitVertical = true; bounced = true;
		}
		if (ny < 0.0f) {
			ny = 0.0f; hitHorizontal = true; bounced = true;
		}
		else if (ny > (float)(H - 1)) {
			ny = (float)(H - 1); hitHorizontal = true; bounced = true;
		}

        if (bounced) {
            // Reflect the velocity and add a small random angular perturbation
            float speed = sqrt(vx[i] * vx[i] + vy[i] * vy[i]);
            if (speed < 0.05f) speed = 0.05f;
            float theta = atan2(vy[i], vx[i]);
            if (hitVertical) {
                // reflect around Y axis: theta' = pi - theta
                theta = 3.14159265f - theta;
            }
            if (hitHorizontal) {
                // reflect around X axis: theta' = -theta
                theta = -theta;
            }
            // Add jitter +/- ~6 degrees
            float jitter = ((float)random(-6, 7)) * 0.01745329252f;
            theta += jitter;
            vx[i] = cos(theta) * speed;
            vy[i] = sin(theta) * speed;
        }

		x[i] = nx;
		y[i] = ny;
	}

	// Draw triangle
	display.clearDisplay();
    int x0 = (int)(x[0] + 0.5f); int y0 = (int)(y[0] + 0.5f);
    int x1 = (int)(x[1] + 0.5f); int y1 = (int)(y[1] + 0.5f);
    int x2 = (int)(x[2] + 0.5f); int y2 = (int)(y[2] + 0.5f);
	// edges
	display.drawLine(x0, y0, x1, y1, SSD1306_WHITE);
	display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
	display.drawLine(x2, y2, x0, y0, SSD1306_WHITE);
	// vertices (emphasize corners)
	display.drawPixel(x0, y0, SSD1306_WHITE);
	display.drawPixel(x1, y1, SSD1306_WHITE);
	display.drawPixel(x2, y2, SSD1306_WHITE);
	
	display.display();
}

void showScreen6() {
	// Bouncing "CICLO" text
	static bool init = false;
	static int x = 0, y = 0;
	static int dx = 1, dy = 1; // pixels per tick
	const int W = display.width();
	const int H = display.height();
	const String txt = F("CICLO");
	const int tw = txt.length() * 6 * 2; // 6 px per char at size=1, times 2
	const int th = 8 * 2;               // 8 px glyph height at size=1, times 2
	const int maxX = (W > tw) ? (W - tw) : 0;
	const int maxY = (H > th) ? (H - th) : 0;

	if (!init) {
		// start near center
		x = maxX / 2;
		y = maxY / 2;
		dx = 1; dy = 1;
		init = true;
	}

	// advance
	x += dx; y += dy;
	if (x <= 0) { x = 0; dx = 1; }
	else if (x >= maxX) { x = maxX; dx = -1; }
	if (y <= 0) { y = 0; dy = 1; }
	else if (y >= maxY) { y = maxY; dy = -1; }

	display.clearDisplay();
	display.setTextSize(2);
	display.setCursor(x, y);
	display.println(txt);
	display.display();
}

void showScreenInit(unsigned int s) {

	switch (screen.Current(s))
	{
	case 0:
		showScreen0(true);
		break;
	case 1:
		showScreen1();
		break;
	case 2:
		showScreen2();
		break;
	case 3:
		showScreen3();
		break;
	case 4:
		// GOL.Genesi();
		showScreen4();
		break;
	case 5:
		showScreen5();
		break;
	case 6:
		showScreen6();
		break;
	}

}

void AddScreenRows(String r) {
	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(SSD1306_WHITE);
	display.setCursor(0, 0);

	screen.RowAdd(r);

	for (unsigned int i = 0; i < 4; i++) {
		display.println(screen.rows[i]);
	}

	display.display();
	delay(250);

}

void setup(void) {

	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, HIGH);

	if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
		digitalWrite(LED_BUILTIN, LOW);
		for (;;);
	}

	display.clearDisplay();
	display.setTextSize(1);
	display.setTextColor(SSD1306_WHITE);
	display.setCursor(0, 0);
	display.display();

    screen.RowClear();
    bool wifiConfigured = true;

	if (LittleFS.begin()) {
		AddScreenRows(F("FS OK"));
		if (LittleFS.exists("/SCREEN.TXT")) {
			File fscr = LittleFS.open("/SCREEN.TXT", "r");
			if (fscr) {
				AddScreenRows(F("FILE SCREEN.TXT OK"));
				String scr = fscr.readString();
				scr.trim();
				screen.Current(scr.toInt());
				fscr.close();
			}
			else {
				AddScreenRows(F("FILE SCREEN.TXT KO"));
			}
		}
		else {
			AddScreenRows(F("FILE SCREEN.TXT KO"));
		}
        if (LittleFS.exists("/WIFI_SID.TXT")) {
            File fsid = LittleFS.open("/WIFI_SID.TXT", "r");
            if (fsid) {
                AddScreenRows(F("FILE WIFI_SID.TXT OK"));
                ssid = fsid.readString();
                ssid.trim();
                fsid.close();
            }
            else {
                AddScreenRows(F("FILE WIFI_SID.TXT KO"));
                wifiConfigured = false;
            }
        }
        else {
            AddScreenRows(F("FILE WIFI_SID.TXT KO"));
            wifiConfigured = false;
        }
        if (LittleFS.exists("/WIFI_PWD.TXT")) {
            File fpwd = LittleFS.open("/WIFI_PWD.TXT", "r");
            if (fpwd) {
                AddScreenRows(F("FILE WIFI_PWD.TXT OK"));
                password = fpwd.readString();
                password.trim();
                fpwd.close();
            }
            else {
                AddScreenRows(F("FILE WIFI_PWD.TXT KO"));
                wifiConfigured = false;
            }
        }
        else {
            AddScreenRows(F("FILE WIFI_PWD.TXT KO"));
            wifiConfigured = false;
        }
        if (LittleFS.exists("/MDNS_NM.TXT")) {
            File fmdns = LittleFS.open("/MDNS_NM.TXT", "r");
            if (fmdns) {
                AddScreenRows(F("FILE MDNS_NM.TXT OK"));
                mdns_name = fmdns.readString();
                mdns_name.trim();
                fmdns.close();
            }
            else {
                AddScreenRows(F("FILE MDNS_NM.TXT KO"));
            }
        }
        else {
            AddScreenRows(F("FILE MDNS_NM.TXT KO"));
        }
		if (LittleFS.exists("/METEO_API.TXT")) {
			File fmta = LittleFS.open("/METEO_API.TXT", "r");
			if (fmta) {
				AddScreenRows(F("FILE METEO_API.TXT OK"));
				meteo.Api = fmta.readString();
				meteo.Api.trim();
				fmta.close();
			}
			else {
				AddScreenRows(F("FILE METEO_API.TXT KO"));
			}
		}
		else {
			AddScreenRows(F("FILE METEO_API.TXT KO"));
		}
	}
	else {
		AddScreenRows(F("FS KO"));
	}

	delay(500);

    // decide network mode
    if (wifiConfigured && ssid.length() > 0 && password.length() > 0) {
        WiFi.mode(WIFI_STA);
        // Avoid auto-reconnect loops while we manage fallback
        WiFi.setAutoConnect(false);
        WiFi.setAutoReconnect(false);
        WiFi.persistent(false);
        // Set DHCP hostname before connecting so router registers it
        WiFi.hostname(mdns_name.c_str());
        WiFi.begin(ssid, password);

        // Wait for connection with timeout
        int s = 100;
        const unsigned long connectTimeout = 20000; // 20s
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - start) < connectTimeout) {
            delay(500);
            if (s > 20) {
                display.clearDisplay();
                display.setCursor(0, 0);
                display.println(F("Connessione a"));
                display.println(ssid);
                s = 0;
            }
            s++;
            display.print("."); display.display();
        }

        if (WiFi.status() == WL_CONNECTED) {
            showScreen0(false);

            if (MDNS.begin(mdns_name)) {
                display.println(F("mDNS responder start"));
                MDNS.addService("http", "tcp", 80);
                mdnsStarted = true;
            }
            else {
                display.println(F("mDNS responder stop"));
                mdnsStarted = false;
            }

            display.display();
        } else {
            // Connection failed: fallback to AP mode with SSID = mdns_name
            // Ensure any previously stored STA credentials are cleared to avoid boot loops
            WiFi.persistent(true);
            WiFi.disconnect(true);
            WiFi.persistent(false);
            WiFi.mode(WIFI_AP);
            WiFi.softAP(mdns_name.c_str());
            delay(100);
            IPAddress apIP = WiFi.softAPIP();

            // Optionally start mDNS also in AP mode so MDNS.update() is safe
            if (MDNS.begin(mdns_name)) {
                MDNS.addService("http", "tcp", 80);
                mdnsStarted = true;
            } else {
                mdnsStarted = false;
            }

            display.clearDisplay();
            display.setCursor(0, 0);
            display.println(F("AP MODE"));
            display.print(F("SSID: ")); display.println(mdns_name);
            display.print(F("IP  : ")); display.println(apIP);
            display.display();
        }
    } else {
        // Fallback AP mode with SSID = mdns_name, open network
        // Proactively clear any saved STA credentials to avoid unintended reconnect attempts
        WiFi.persistent(true);
        WiFi.disconnect(true);
        WiFi.persistent(false);
        WiFi.mode(WIFI_AP);
        WiFi.softAP(mdns_name.c_str());
        delay(100);
        IPAddress apIP = WiFi.softAPIP();

        if (MDNS.begin(mdns_name)) {
            MDNS.addService("http", "tcp", 80);
            mdnsStarted = true;
        } else {
            mdnsStarted = false;
        }

        display.clearDisplay();
        display.setCursor(0, 0);
        display.println(F("AP MODE"));
        display.print(F("SSID: ")); display.println(mdns_name);
        display.print(F("IP  : ")); display.println(apIP);
        display.display();
    }

    server.on(F("/"), handleRoot);
    // favicon for browser tabs
    server.on(F("/favicon.ico"), HTTP_GET, handleFavicon);
    // setup page (GET to render, POST to save)
    server.on(F("/setup"), HTTP_GET, handleSetupGet);
    // current config as JSON (password intentionally omitted)
    server.on(F("/config"), HTTP_GET, []() {
        String json = F("{");
        json += F("\"ssid\":"); json += '"'; json += ssid; json += '"'; json += F(",");
        json += F("\"mdns\":"); json += '"'; json += mdns_name; json += '"';
        json += F("}");
        server.send(200, F("application/json"), json);
    });
    server.on(F("/setup"), HTTP_POST, handleSetupPost);

	server.onNotFound(handleNotFound);

	server.on(F("/wifi"), []() {
		showScreenInit(0);
		server.send(200, F("text/plain"), F(""));
		});

	server.on(F("/time"), []() {
		showScreenInit(1);
		server.send(200, F("text/plain"), F(""));
		});

	server.on(F("/eyes"), []() {
		showScreenInit(2);
		server.send(200, F("text/plain"), F(""));
		});

	server.on(F("/meteo"), []() {
		showScreenInit(3);
		server.send(200, F("text/plain"), F(""));
		});

		server.on(F("/tri"), []() {
			showScreenInit(5);
			server.send(200, F("text/plain"), F(""));
			});

		server.on(F("/gol"), []() {
			showScreenInit(4);
			server.send(200, F("text/plain"), F(""));
			});

        server.on(F("/cycle"), []() {
            // Enable auto cycle mode and show marker screen
            CicloScreen = true;
            showScreenInit(6);
            server.send(200, F("text/plain"), F(""));
            });

		// status endpoint
		server.on(F("/status"), handleStatus);

		// OLED buffer endpoint
		server.on(F("/oled"), handleOled);

	server.begin();
	display.println(F("HTTP server start")); display.display();

	display.setCursor(0, 0);
	display.display();

	ArduinoOTA.onStart([]() {
		digitalWrite(LED_BUILTIN, LOW);
		display.setTextSize(1);
		display.clearDisplay();
		display.setCursor(0, 0);
		display.println(F("Start updating "));

		if (ArduinoOTA.getCommand() == U_FLASH) {
			display.println(F("sketch"));
		}
		else { // U_FS
			display.println(F("filesystem"));
			LittleFS.end();
		}

		display.display();

		delay(1000);

		});

	ArduinoOTA.onEnd([]() {
		digitalWrite(LED_BUILTIN, HIGH);
		display.setTextSize(1);
		display.clearDisplay();
		display.setCursor(0, 0);
		display.println(F("End updating "));
		display.println(F("Reboot "));
		display.display();
		});

	ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
		display.setTextSize(1);
		display.setCursor(0, 0);
		display.clearDisplay();
		display.print(F("Progress "));
		display.print((progress / (total / 100)));
		display.println(F(" %"));
		display.println(progress);
		display.println(total);
		display.display();
		});

	ArduinoOTA.onError([](ota_error_t error) {
		digitalWrite(LED_BUILTIN, HIGH);
		display.setTextSize(1);
		display.clearDisplay();
		display.setCursor(0, 0);
		display.print(F("Error ")); display.println(error);

		if (error == OTA_AUTH_ERROR) {
			display.println(F("Auth Failed"));
		}
		else if (error == OTA_BEGIN_ERROR) {
			display.println(F("Begin Failed"));
		}
		else if (error == OTA_CONNECT_ERROR) {
			display.println(F("Connect Failed"));
		}
		else if (error == OTA_RECEIVE_ERROR) {
			display.println(F("Receive Failed"));
		}
		else if (error == OTA_END_ERROR) {
			display.println(F("End Failed"));
		}

		display.display();

		});

    // Make OTA service advertise with the same friendly hostname
    ArduinoOTA.setHostname(mdns_name.c_str());
    ArduinoOTA.begin();

	configTime(TZ_Europe_Rome, "pool.ntp.org");
	time_t rtc = 0;
	timeval tv = { rtc, 0 };
	settimeofday(&tv, nullptr);
	settimeofday_cb(time_is_set_scheduled);

	delay(500);

}

void loop(void) {
    ArduinoOTA.handle();
    server.handleClient();
    if (mdnsStarted) {
        MDNS.update();
    }
    Bottone.Update();

    // Periodic brightness adjustment based on local time (night dim)
    if (adjustBrightnessNow) {
        time_t nowt = time(nullptr);
        const tm* lt = localtime(&nowt);
        if (lt) {
            int h = lt->tm_hour;
            bool night = (h >= 22 || h < 7); // 22:00–06:59 dimmed
            display.dim(night);
        }
    }

    // Short inversion pulse every 30 minutes to mitigate burn-in
    if (invertPulseNow) {
        display.invertDisplay(true);
        display.display();
        delay(250);
        display.invertDisplay(false);
        display.display();
    }

	if (Bottone.Pressed()) {
		display.invertDisplay(true);
		display.display();
		delay(200);
		display.invertDisplay(false);
		display.display();

		if (!CicloScreen) {
			showScreenInit(screen.Next());
		}

		CicloScreen = false;
	}

    if (CicloScreen && showCicloScreenNow) {
        unsigned int sn = screen.Next();
        if (sn == 0 || sn == 6) {
            sn = screen.Current(1);
        }
        showScreenInit(sn);
    }

    switch (screen.Current())
    {
	case 0:
		if (showWifiNow) {
			showScreen0(true);
		}
		break;
	case 1:
		if (showTimeNow) {
			showScreen1();
		}
		break;
	case 2:
		if (showEyesNow) {
			showScreen2();
		}
		break;
	case 3:
		if (showMeteoNow) {
			digitalWrite(LED_BUILTIN, LOW);
			meteo.Update();
			digitalWrite(LED_BUILTIN, HIGH);
			showScreen3();
		}
		break;
    case 4:
        if (showGOLNow) {
            GOL.Update();
            showScreen4();
        }
        break;
    case 5:
        if (showTriNow) {
            showScreen5();
        }
        break;
    case 6:
        // Animate the bouncing text frequently
        if (showTriNow) {
            showScreen6();
        }
        // When not yet in auto-cycle, after a minute auto-enable it
        if (!CicloScreen && showCicloScreenNow) {
            CicloScreen = true;
        }
        break;
    }

}

// ================= HTTP Handlers (definitions) =================
void handleSetupGet() {
    digitalWrite(LED_BUILTIN, LOW);
    if (LittleFS.exists("/SETUP.HTM")) {
        File fhtm = LittleFS.open("/SETUP.HTM", "r");
        if (fhtm) {
            String message = fhtm.readString();
            fhtm.close();
            server.send(200, F("text/html"), message);
            digitalWrite(LED_BUILTIN, HIGH);
            return;
        }
    }

    // Minimal fallback page when SETUP.HTM is missing
    String body;
    body.reserve(512);
    body += F("<!doctype html><html lang=\"it\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width,initial-scale=1\"><title>404 • SETUP.HTM mancante</title><style>body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:0;padding:24px;background:#0f172a;color:#e5e7eb}label{color:#9ca3af}input{display:block;margin-top:6px}</style></head><body>");
    body += F("<h1>Setup non disponibile</h1><p>Manca il file <code>SETUP.HTM</code> su LittleFS.</p><p>Consigli:</p><ul>");
    body += F("<li>Carica i file in <code>/data</code> con ‘Upload Filesystem Image’.</li>");
    body += F("<li>Se sei in AP, collega a SSID <strong>"); body += mdns_name; body += F("</strong> e visita <a href=\"/\">Home</a>.</li></ul>");
    body += F("</body></html>");
    server.send(404, F("text/html"), body);
    digitalWrite(LED_BUILTIN, HIGH);
}

void handleSetupPost() {
    String nsid = server.hasArg("ssid") ? server.arg("ssid") : String(""); nsid.trim();
    String npwd = server.hasArg("pwd") ? server.arg("pwd") : String(""); npwd.trim();
    String nmdn = server.hasArg("mdns") ? server.arg("mdns") : String(""); nmdn.trim();
    bool ok = true;
    if (!LittleFS.begin()) ok = false;
    if (ok && nsid.length()) { File f = LittleFS.open("/WIFI_SID.TXT", "w"); if (f) { f.print(nsid); f.close(); } else ok = false; }
    if (ok && npwd.length()) { File f = LittleFS.open("/WIFI_PWD.TXT", "w"); if (f) { f.print(npwd); f.close(); } else ok = false; }
    if (ok && nmdn.length()) { File f = LittleFS.open("/MDNS_NM.TXT", "w"); if (f) { f.print(nmdn); f.close(); } else ok = false; }
    if (ok) {
        server.send(200, F("text/plain"), F("OK, riavvio in corso..."));
        delay(1000);
        ESP.restart();
    } else {
        server.send(500, F("text/plain"), F("Errore scrittura parametri"));
    }
}

void handleStatus() {
    IPAddress ip = WiFi.localIP();
    int rssi = WiFi.RSSI();
    int qual = dBmtoPercentage(rssi);
    WiFiMode_t mode = WiFi.getMode();
    bool apMode = (mode == WIFI_AP || mode == WIFI_AP_STA);
    IPAddress apIP = WiFi.softAPIP();
    String json = F("{");
    json += F("\"ip\":\""); json += ip.toString(); json += F("\",");
    json += F("\"mdns\":\""); json += mdns_name; json += F("\",");
    json += F("\"rssi\":"); json += rssi; json += F(",");
    json += F("\"quality\":"); json += qual; json += F(",");
    json += F("\"mode\":\""); json += (apMode ? F("AP") : F("STA")); json += F("\",");
    json += F("\"ap_ip\":\""); json += apIP.toString(); json += F("\"");
    json += F("}");
    server.send(200, F("application/json"), json);
}

void handleOled() {
    uint16_t w = display.width();
    uint16_t h = display.height();
    uint16_t pages = (h + 7) / 8;
    (void)pages; // unused but documents layout
    size_t sz = (size_t)w * ((h + 7) / 8);
    const uint8_t* buf = display.getBuffer();

    String json;
    json.reserve(32 + (sz * 2));
    json += F("{\"w\":"); json += w; json += F(",\"h\":"); json += h; json += F(",\"data\":\"");
    static const char hex[] = "0123456789ABCDEF";
    for (size_t i = 0; i < sz; i++) {
        uint8_t v = buf[i];
        json += hex[v >> 4];
        json += hex[v & 0x0F];
    }
    json += F("\"}");
    server.send(200, F("application/json"), json);
}

void handleFavicon() {
    // Serve /favicon.ico from LittleFS with proper content type and caching
    if (!LittleFS.begin()) {
        server.send(404, F("text/plain"), F(""));
        return;
    }
    if (LittleFS.exists("/favicon.ico")) {
        File f = LittleFS.open("/favicon.ico", "r");
        if (f) {
            server.sendHeader(F("Cache-Control"), F("public, max-age=604800, immutable"));
            server.streamFile(f, F("image/x-icon"));
            f.close();
            return;
        }
    }
    server.send(404, F("text/plain"), F(""));
}
