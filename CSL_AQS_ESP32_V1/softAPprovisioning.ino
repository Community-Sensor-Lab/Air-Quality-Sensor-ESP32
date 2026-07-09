#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>

#include "CSL_AQS_ESP32_V1.h"
// HTTPS provisioning support:
// The normal HTTP provisioning page is still kept for compatibility.
// This adds a local HTTPS version at https://192.168.4.1 for phones, browsers, or networks that block interaction with plain HTTP pages.
// The browser will show a certificate warning because the ESP32 uses a self-signed certificate, which is expected for this local setup page.
#include "cert.h"
#include "private_key.h"


// Convenience wrapper for the CSL-edited HTTPS server library.
// Sketches include this unique header so Arduino selects this forked copy instead of another installed HTTPS server library with generic header names.
#include <CSLedited_ESP32_IDF5_HTTPS_Server.h>
#include <string>

using namespace httpsserver;

/**
* Starts a wifi access point with a unique name 'csl-xxxx' and starts a server.
* When client connects with a browser at specified ip (192.168.4.1), serves a  provisioning page to client with fields for ssid, passcode and gsid entry
* Parses the response and decodes the string for any %-encoded characters, and saves to memory
*/

// ----------------------
// SoftAP config

// Local self-signed certificate for the ESP32 setup page.
// This enables HTTPS on the ESP32 access point, but browsers will still warn because the certificate is not signed by a public certificate authority.
static SSLCert httpsCert = SSLCert(
  example_crt_DER,
  example_crt_DER_len,
  example_key_DER,
  example_key_DER_len
);
// HTTPS provisioning fallback. The existing WebServer on port 80 still handles HTTP;
// This server adds port 443 for browsers/devices that block plain HTTP setup pages.
static HTTPSServer secureServer = HTTPSServer(&httpsCert);

String httpsQueryArg(httpsserver::HTTPRequest *req, const char *name);

void handleRootHttps(httpsserver::HTTPRequest *req, httpsserver::HTTPResponse *res);
void handleGetHttps(httpsserver::HTTPRequest *req, httpsserver::HTTPResponse *res);
void handleNotFoundHttps(httpsserver::HTTPRequest *req, httpsserver::HTTPResponse *res);
// These ResourceNodes are registered when provisioning starts.
// If provisioning can restart in the same boot, avoid registering duplicates.
void setupHttpsServer();

// Decode URL form values from GET query strings.
// Spaces and special characters in SSIDs/passwords may arrive as + or %XX.
static String decodeUrl(const String& in) {
  // Decodes application/x-www-form-urlencoded for query strings
  String out;
  out.reserve(in.length());

  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < in.length()) {
      char h1 = in[i + 1];
      char h2 = in[i + 2];
      auto hexVal = [](char h) -> int {
        if (h >= '0' && h <= '9') return h - '0';
        if (h >= 'A' && h <= 'F') return 10 + (h - 'A');
        if (h >= 'a' && h <= 'f') return 10 + (h - 'a');
        return -1;
      };
      int v1 = hexVal(h1);
      int v2 = hexVal(h2);
      if (v1 >= 0 && v2 >= 0) {
        out += char((v1 << 4) | v2);
        i += 2;
      } else {
        out += c;  // leave as-is
      }
    } else {
      out += c;
    }
  }
  return out;
}

// updated webpage to show avaliable networks:
String buildProvisioningPage() {
  int n = WiFi.scanNetworks();                                                       //scans avaliable wifi using the built in function and stores it in n variablr
  String page = "<!DOCTYPE HTML><html><head><title>Provision</title></head><body>";  //sets up the title of the page
  page += "<form action=\"/get\">";                                                  //adds a form to the page and users get request

  //uses the select function  that creates a dropdown list
  //the option value iterates through n with the networks and add it to the select column as an option
  page += "SSID: <select name=\"SSID\">";
  for (int i = 0; i < n; i++) {
    page += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)" + "</option>";
  }
  page += "</select><br>";
  //text input for passcode
  page += "Passcode: <input type=\"password\" name=\"passcode\"><br>";
  //text input for GSID
  page += "GSID: <input type=\"text\" name=\"GSID\" placeholder=\"leave blank to reuse saved\"><br>"; // keeps old GSID if not filled in by user
  //adds a submit button
  page += "<input type=\"submit\" value=\"Submit\">";
  page += "</form></body></html>";
  return page;
}
// Build the confirmation page shared by HTTP and HTTPS after credentials submit.
String buildProvisioningSuccessPage(const String& ssid, const String& gsid) {
  String resp = "<!DOCTYPE html><html><body>";
  resp += "<h3>Received provisioning info</h3>";
  resp += "<p><b>SSID:</b> " + ssid + "</p>";
  resp += "<p><b>GSID:</b> " + gsid + "</p>";
  resp += "<p>You can now close this page.</p>";
  resp += "</body></html>";
  return resp;
}
// Shared provisioning save path used by both HTTP and HTTPS handlers to prevent the two setup routes from drifting apart.
void applyProvisioningInfo(const String& ssid, const String& pass, const String& gsid) {
  // Preserve the previous Google Script ID when the GSID field is left blank
  char savedGsid[sizeof(provisionInfo.gsid)];
  strlcpy(savedGsid, provisionInfo.gsid, sizeof(savedGsid));

  String cleanedGsid = gsid;
  cleanedGsid.trim();

  memset(&provisionInfo, 0, sizeof(provisionInfo));

  strlcpy(provisionInfo.ssid, ssid.c_str(), sizeof(provisionInfo.ssid));
  strlcpy(provisionInfo.passcode, pass.c_str(), sizeof(provisionInfo.passcode));

  if (cleanedGsid.length() > 0) {
    // Blank GSID means reuse the last saved value
    strlcpy(provisionInfo.gsid, cleanedGsid.c_str(), sizeof(provisionInfo.gsid));
  } else {
    strlcpy(provisionInfo.gsid, savedGsid, sizeof(provisionInfo.gsid));
  }

  Serial.println("\nProvisioning received:");
  Serial.print("  SSID: ");
  Serial.println(provisionInfo.ssid);
  Serial.print("  PASS length: ");
  Serial.println(strlen(provisionInfo.passcode));
  Serial.print("  GSID: ");
  Serial.println(strlen(provisionInfo.gsid) > 0 ? "present" : "missing");

  provisionInfo.valid = true;
  provisionInfo.WiFiPresent = true;
  saveProvisioningInfoToEEPROM(provisionInfo);
}
// ----------------------
// Handlers
// ----------------------
void handleRoot() {
  // If you want faster page load, you can avoid scanning every time and cache results.
  Serial.println("handleRoot");

  String page = buildProvisioningPage();
  server.send(200, "text/html", page);
}

void handleGet() {
  Serial.println("handleGet");

  String ssidRaw = server.hasArg("SSID") ? server.arg("SSID") : "";
  String passRaw = server.hasArg("passcode") ? server.arg("passcode") : "";
  String gsidRaw = server.hasArg("GSID") ? server.arg("GSID") : "";

  String ssid = decodeUrl(ssidRaw);
  String pass = decodeUrl(passRaw);
  String gsid = decodeUrl(gsidRaw);

  applyProvisioningInfo(ssid, pass, gsid);

  String resp = buildProvisioningSuccessPage(ssid, gsid);
  server.send(200, "text/html", resp);
}


void handleNotFound() {
  Serial.println("handleNotFound");
  server.send(404, "text/plain", "Not found");
}
// HTTPS query helper.
// Arduino WebServer uses server.arg(...), but ESP32_IDF5_HTTPS_Server exposes query values through ResourceParameters instead.
String httpsQueryArg(httpsserver::HTTPRequest *req, const char *name) {
  ResourceParameters *params = req->getParams();

  std::string paramName = name;
  std::string value;

  if (params->getQueryParameter(paramName, value)) {
    return decodeUrl(String(value.c_str()));
  }

  return "";
}

void handleRootHttps(httpsserver::HTTPRequest *req, httpsserver::HTTPResponse *res) {
  Serial.println("handleRootHttps");

  String page = buildProvisioningPage();

  res->setHeader("Content-Type", "text/html");
  res->print(page);
}

void handleGetHttps(httpsserver::HTTPRequest *req, httpsserver::HTTPResponse *res) {
  Serial.println("handleGetHttps");

  String ssid = httpsQueryArg(req, "SSID");
  String pass = httpsQueryArg(req, "passcode");
  String gsid = httpsQueryArg(req, "GSID");

  applyProvisioningInfo(ssid, pass, gsid);

  String resp = buildProvisioningSuccessPage(ssid, gsid);

  res->setHeader("Content-Type", "text/html");
  res->print(resp);
}

void handleNotFoundHttps(httpsserver::HTTPRequest *req, httpsserver::HTTPResponse *res) {
  Serial.println("handleNotFoundHttps");

  req->discardRequestBody();

  res->setStatusCode(404);
  res->setStatusText("Not Found");
  res->setHeader("Content-Type", "text/plain");
  res->print("Not found");
}
// Register HTTPS routes equivalent to the existing HTTP routes:
// "/" shows the setup page, "/get" receives submitted provisioning values
void setupHttpsServer() {
  ResourceNode *httpsRootNode = new ResourceNode("/", "GET", &handleRootHttps);
  ResourceNode *httpsGetNode = new ResourceNode("/get", "GET", &handleGetHttps);
  ResourceNode *https404Node = new ResourceNode("", "GET", &handleNotFoundHttps);

  secureServer.registerNode(httpsRootNode);
  secureServer.registerNode(httpsGetNode);
  secureServer.setDefaultNode(https404Node);

  Serial.println("Starting HTTPS server on port 443...");
  secureServer.start();

  if (secureServer.isRunning()) {
    Serial.println("HTTPS server started.");
  } else {
    Serial.println("HTTPS server failed to start.");
  }
}
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {

    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf(
        "📲 Client connected: %02X:%02X:%02X:%02X:%02X:%02X, AID=%d\n",
        info.wifi_ap_staconnected.mac[0],
        info.wifi_ap_staconnected.mac[1],
        info.wifi_ap_staconnected.mac[2],
        info.wifi_ap_staconnected.mac[3],
        info.wifi_ap_staconnected.mac[4],
        info.wifi_ap_staconnected.mac[5],
        info.wifi_ap_staconnected.aid);
      break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.printf(
        "📴 Client disconnected: %02X:%02X:%02X:%02X:%02X:%02X, AID=%d\n",
        info.wifi_ap_stadisconnected.mac[0],
        info.wifi_ap_stadisconnected.mac[1],
        info.wifi_ap_stadisconnected.mac[2],
        info.wifi_ap_stadisconnected.mac[3],
        info.wifi_ap_stadisconnected.mac[4],
        info.wifi_ap_stadisconnected.mac[5],
        info.wifi_ap_stadisconnected.aid);
      break;

    default:
      break;
  }
}

void printMac(const char* label, uint8_t mac[6]) {
  Serial.printf("%s: %02X:%02X:%02X:%02X:%02X:%02X\n",  //Writes in Hex w/ zero padding
                label,
                mac[0], mac[1], mac[2],
                mac[3], mac[4], mac[5]);
}

void displayMac(const char* label, uint8_t mac[6]) {
  char buf[32];

  snprintf(buf, sizeof(buf),  //Less chance of buffer overflowing
           "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);

  display.printf("%s\n%s\n", label, buf);
}

void softAPprovision() {
  static const IPAddress AP_IP(192, 168, 4, 1);
  static const IPAddress AP_GW(192, 168, 4, 1);
  static const IPAddress AP_MASK(255, 255, 255, 0);
  // Allow scanning while also running SoftAP
  WiFi.mode(WIFI_AP_STA);

  //mac_ssid = "csl-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);

  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  WiFi.onEvent(onWiFiEvent);  //
  display.clearDisplay();
  display.setCursor(0, 0);

  if (!WiFi.softAP(mac_ssid.c_str())) {
    Serial.println("❌ softAP start failed");
    display.printf("SoftAP start failed\n");
  } else {
    Serial.printf("✅ Started Provisioning Wifi: %s\n", mac_ssid);
    display.printf("Started Provisioning Wifi:%s\n", mac_ssid);
  }
  display.display();

  // Web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/get", HTTP_GET, handleGet);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("✅ HTTP server started (port 80)");
  setupHttpsServer();
  Serial.printf("Open webpage to %s on device connected to the WiFi\n", WiFi.softAPIP().toString());

  uint64_t chipid = ESP.getEfuseMac();  // Writes from LSB to MSB and Byte order is LSB to MSB. Results in a backwards Mac Address

  uint8_t baseMac[6];                               //MacAddress lies in 48bits Writes MSB to LSB
  for (int i = 0; i < 6; i++) {                     //Shift from most significant to least. This returns the same order which is the wrong order
    baseMac[i] = (chipid >> (8 * (5 - i))) & 0xFF;  //Mask each byte.
  }

  printMac("Base MAC (eFuse)", baseMac);

  uint8_t staMac[6];
  WiFi.macAddress(staMac);  //Writes MSB to LSB. Correct Mac Address
  printMac("STA MAC", staMac);

  uint8_t apMac[6];
  WiFi.softAPmacAddress(apMac);

  printMac("AP MAC", apMac);


  display.printf("Open webpage at\n%s\n", WiFi.softAPIP().toString());
  //  displayMac("Base MAC", baseMac);
  displayMac("STA MAC", staMac);
  displayMac("AP MAC", apMac);

// Service both setup servers while waiting for valid credentials.
// HTTP helps older/simple clients; HTTPS helps browsers that block plain HTTP.
  display.display();
  while (!provisionInfo.valid) {
    server.handleClient();

  if (secureServer.isRunning()) {
    secureServer.loop();
  }
    delay(1);
      if (!provisionInfo.WiFiPresent) {
        Serial.println("Provisioning canceled. Continue without WiFi");
        display.printf("Canceled. No WiFi");
        display.display();
        break;
    }
  }

server.stop();

if (secureServer.isRunning()) {
  secureServer.stop();
}

WiFi.softAPdisconnect(true);
}

void connectToWiFi() {

  delay(1000);
  Serial.printf("\nWill try to connect to WiFi: %s\n", provisionInfo.ssid);
  display.clearDisplay();
  display.setCursor(0, 0);
  display.printf("(A) Provisioning\n");
  display.printf("(B) No WiFi\n");
  display.printf("Trying:%s\n", provisionInfo.ssid);
  display.display();

  WiFi.mode(WIFI_STA);
  WiFi.begin(provisionInfo.ssid, provisionInfo.passcode);

  unsigned long st = millis();
  while (WiFi.status() != WL_CONNECTED && provisionInfo.WiFiPresent && provisionInfo.valid) {
    Serial.print(".");
    delay(100);
    if ((millis() - st) > WIFI_TIMEOUT) {
      Serial.println("wifi connect timeout");
      provisionInfo.valid = false;
      break;
    }
  }

  if (WiFi.status() == WL_CONNECTED) {
    delay(1000);

    uint64_t mac = ESP.getEfuseMac();
    char macBuf[20];
    sprintf(macBuf, "%02x:%02x:%02x:%02x:%02x:%02x",
            (uint8_t)(mac)&0xFF,
            (uint8_t)(mac >> 8) & 0xFF,
            (uint8_t)(mac >> 16) & 0xFF,
            (uint8_t)(mac >> 24) & 0xFF,
            (uint8_t)(mac >> 32) & 0xFF,
            (uint8_t)(mac >> 40) & 0xFF);
    FullmacStr = String(macBuf);

    Serial.printf("\nConnected to WiFi: %s\n", provisionInfo.ssid);
    display.printf("\nConnected to WiFi: \n\n%s", provisionInfo.ssid);
    display.display();
  }
}
