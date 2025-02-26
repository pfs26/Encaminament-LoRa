#include <Arduino.h>  

#include <AsyncTCP.h>           //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>	//https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <esp_wifi.h>			//Used for mpdu_rx_disable android workaround

#include "webpage.h"
#include "utils.h"
#include "scheduler.h"

// Pre reading on the fundamentals of captive portals https://textslashplain.com/2022/06/24/captive-portals/

void _setUpDNSServer();
void _stopDNSServer();
void _setUpSoftAP();
void _stopSoftAP();
void _setUpWebServer();
void _stopWebServer();

const IPAddress localIP(1, 1, 1, 1);		   // the IP address the web server, Samsung requires the IP to be in public space
const IPAddress gatewayIP(1, 1, 1, 1);		   // IP address of the network should be the same as the local IP for captive portals
const IPAddress subnetMask(255, 255, 255, 0);  // no need to change: https://avinetworks.com/glossary/subnet-mask/

const String localIPURL = "http://1.1.1.1";	 // a string version of the local IP with http, used for redirecting clients to your webpage

const char index_html[] PROGMEM = R"=====(
  <!DOCTYPE html>
  <html>
    <head>
      <title>ESP32 Captive Portal</title>
    </head>
    <body>
      <h2>This is a captive portal example. All requests will be redirected here </h2>
      <button onclick="sendRequest()">Click to Send Request to /q</button>
      
      <script>
        function sendRequest() {
          // Sending GET request to /q endpoint
          fetch('/q')
            .then(response => response.text())
            .then(data => {
              alert("Response from server: " + data); // Display server response
            })
            .catch(error => {
              console.error('Error:', error);
            });
        }
      </script>
    </body>
  </html>
)=====";


DNSServer dnsServer;
AsyncWebServer server(80);

void _setUpDNSServer() {
	dnsServer.setTTL(3600);
	dnsServer.start(53, "*", localIP);
}

void _stopDNSServer() {
    dnsServer.stop();
}

void _setUpSoftAP() {
	// Set the WiFi mode to access point and station
	WiFi.mode(WIFI_MODE_AP);

	// Configure the soft access point with a specific IP and subnet mask
	WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

	// Start the soft access point with the given ssid, password, channel, max number of clients
	WiFi.softAP(SSID, PASSWORD, WIFI_CHANNEL, 0, MAX_CLIENTS);

	// Disable AMPDU RX on the ESP32 WiFi to fix a bug on Android
	esp_wifi_stop();
	esp_wifi_deinit();
	wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
	my_config.ampdu_rx_enable = false;
	esp_wifi_init(&my_config);
	esp_wifi_start();
	vTaskDelay(100 / portTICK_PERIOD_MS);  // Add a small delay
}

void _stopSoftAP() {
    WiFi.mode(WIFI_MODE_NULL);
    WiFi.softAPdisconnect(true);
    WiFi.enableAP(false);
    esp_wifi_stop();
	esp_wifi_deinit();
}

void _setUpWebServer() {
	// Required
	server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });	// windows 11 captive portal workaround
	server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });								// Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

	// Background responses: Probably not all are Required, but some are. Others might speed things up?
	// A Tier (commonly used by modern systems)
	server.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });		   // android captive portal redirect
	server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // microsoft redirect
	server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });  // apple call home
	server.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });	   // firefox captive portal call home
	server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					   // firefox captive portal call home
	server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // windows call home

	// return 404 to webpage icon
	server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });	// webpage icon

	// Serve Basic HTML Page
	server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
		response->addHeader("Cache-Control", "public,max-age=31536000");  // save this file to cache for 1 year (unless you refresh)
		request->send(response);
		Serial.println("Served Basic HTML Page");
	});

    server.on("/q", HTTP_ANY, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
		response->addHeader("Cache-Control", "public,max-age=31536000");  // save this file to cache for 1 year (unless you refresh)
		request->send(response);
		Serial.println("Quit received");
        webpage_stop();
	});

	// the catch all
	server.onNotFound([](AsyncWebServerRequest *request) {
		request->redirect(localIPURL);
		Serial.print("onnotfound ");
		Serial.print(request->host());	// This gives some insight into whatever was being requested on the serial monitor
		Serial.print(" ");
		Serial.print(request->url());
		Serial.print(" sent redirect to " + localIPURL + "\n");
	});

    server.begin();
}

void _stopWebServer() {
    server.end();
    server.reset();
}

Task* dnsReqTask;
void _processNextDNSReq() {
	_PM("[WEB] DNS PROC");
	dnsServer.processNextRequest();
}

bool webpage_start() {
	// Print a welcome message to the Serial port.
	_PL("\n\nCaptive Test, V0.5.0 compiled " __DATE__ " " __TIME__ " by CD_FER");  //__DATE__ is provided by the platformio ide

    _PL("[WEB] Starting AP");
	_setUpSoftAP();
    _PL("[WEB] Starting DNS");
	_setUpDNSServer();
    _PL("[WEB] Starting WEB");
	_setUpWebServer();

	// Comprovacions cada 30 MS utilitzant les tasques, per no bloquejar el fil principal
	// Cal que LOOP executi scheduler_run().
	dnsReqTask = scheduler_infinite(DNS_INTERVAL, &_processNextDNSReq);
	_PM("[WEB] STARTED");	// should be somewhere between 270-350 for Generic ESP32 (D0WDQ6 chip, can have a higher startup time on first boot)
	return true;
}

void webpage_stop() {
	_PL("[WEB] Stopping DNS task");
	scheduler_stop(dnsReqTask);
    _PL("[WEB] Stoping WEB");
	_stopWebServer();
    _PL("[WEB] Stoping DNS");
	_stopDNSServer();
    _PL("[WEB] Stoping AP");
	_stopSoftAP();
}