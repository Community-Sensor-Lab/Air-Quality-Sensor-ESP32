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
  example_key_DER_len);
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
static String decodeUrl(const String &in) {
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
  page += "GSID: <input type=\"text\" name=\"GSID\" placeholder=\"leave blank to reuse saved\"><br>";  // keeps old GSID if not filled in by user
  //adds a submit button
  page += "<input type=\"submit\" value=\"Submit\">";
  page += "</form>";
  page += "<p><a href=\"/status\">View device status</a></p>";
  page += "</body></html>";
  return page;
}
// Build the confirmation page shared by HTTP and HTTPS after credentials submit.
String buildProvisioningSuccessPage(const String &ssid, const String &gsid) {
  String resp = "<!DOCTYPE html><html><body>";
  resp += "<h3>Received provisioning info</h3>";
  resp += "<p><b>SSID:</b> " + ssid + "</p>";
  resp += "<p><b>GSID:</b> " + gsid + "</p>";
  resp += "<p>You can now close this page.</p>";
  resp += "<p><a href=\"/status\">View device status</a></p>";
  resp += "</body></html>";
  return resp;
}
// builds a read only webpage with device, wifi, google, and sensor status
String buildStatusPage() {
  String page = "<!DOCTYPE html><html><head>";
  page += "<title>AQS Status</title>";
  page += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  // Refresh the status page when a new sample should be available
  page += "<meta http-equiv=\"refresh\" content=\"" + String(sampleIntervalMs / 1000) + "\">";
  page += "<style>";
  page += "body{font-family:Arial,sans-serif;margin:20px;line-height:1.4;}";
  page += "table{border-collapse:collapse;width:100%;max-width:520px;}";
  page += "td{border-bottom:1px solid #ddd;padding:6px;}";
  page += "td:first-child{font-weight:bold;width:45%;}";
  page += "</style>";
  page += "</head><body>";

  page += "<h2>Air Quality Sensor Status</h2>";
  page += "<table>";

  page += "<tr><td>Device MAC</td><td>" + FullmacStr + "</td></tr>";
  page += "<tr><td>AP SSID</td><td>" + mac_ssid + "</td></tr>";
  page += "<tr><td>AP IP</td><td>" + apIpText + "</td></tr>";
  page += "<tr><td>STA IP</td><td>" + staIpText + "</td></tr>";
  page += "<tr><td>WiFi SSID</td><td>" + String(provisionInfo.ssid) + "</td></tr>";
  page += "<tr><td>WiFi RSSI</td><td>" + String(lastWifiRssi) + " dBm</td></tr>";
  page += "<tr><td>WiFi Status</td><td>" + wifiStatusText + "</td></tr>";
  page += "<tr><td>Google Status</td><td>" + googleStatusText + "</td></tr>";
  page += "<tr><td>GSID</td><td>";
  page += strlen(provisionInfo.gsid) > 0 ? "present" : "missing";
  page += "</td></tr>";

  page += "<tr><td>PM2.5</td><td>" + String(sensorData.mPm2_5, 1) + " ug/m3</td></tr>";
  page += "<tr><td>CO2</td><td>" + String(sensorData.CO2) + " ppm</td></tr>";
  page += "<tr><td>Temperature</td><td>" + String(sensorData.Tbme, 1) + " C</td></tr>";
  page += "<tr><td>Humidity</td><td>" + String(sensorData.RHbme, 0) + " %</td></tr>";
  page += "<tr><td>Battery</td><td>" + String(sensorData.Vbat, 2) + " V</td></tr>";
  page += "<tr><td>Sample Mode</td><td>" + sampleModeText + "</td></tr>";
  page += "<tr><td>Sample Interval</td><td>" + String(sampleIntervalMs / 1000) + " sec</td></tr>";
  page += "<tr><td>WiFi Upload Interval</td><td>" + String(wifiUploadIntervalMs / 1000) + " sec</td></tr>";
  page += "<tr><td>Cell Upload Interval</td><td>" + String(cellUploadIntervalMs / 1000) + " sec</td></tr>";
  page += "<tr><td>GPS Fix</td><td>" + latestFixValidText + "</td></tr>";
  page += "<tr><td>Lat</td><td>" + latestLatText + "</td></tr>";
  page += "<tr><td>Lon</td><td>" + latestLonText + "</td></tr>";
  page += "<tr><td>Uptime</td><td>" + String(millis() / 1000) + " sec</td></tr>";

  page += "</table>";
  page += "<p><a href=\"/\">Provisioning page</a></p>";
  page += "</body></html>";

  return page;
}

// Serve current device status without changing provisioning settings.
void handleStatus() {
  Serial.println("handleStatus");

  String page = buildStatusPage();
  server.send(200, "text/html", page);
}
// Shared provisioning save path used by both HTTP and HTTPS handlers to prevent the two setup routes from drifting apart.
void applyProvisioningInfo(const String &ssid, const String &pass, const String &gsid) {
  // Reuse the saved Google Script ID when the provisioning GSID field is blank
  char savedGsid[sizeof(provisionInfo.gsid)];
  strlcpy(savedGsid, provisionInfo.gsid, sizeof(savedGsid));

  String cleanedGsid = gsid;
  cleanedGsid.trim();

  memset(&provisionInfo, 0, sizeof(provisionInfo));

  strlcpy(provisionInfo.ssid, ssid.c_str(), sizeof(provisionInfo.ssid));
  strlcpy(provisionInfo.passcode, pass.c_str(), sizeof(provisionInfo.passcode));

  if (cleanedGsid.length() > 0) {
    strlcpy(provisionInfo.gsid, cleanedGsid.c_str(), sizeof(provisionInfo.gsid));
  } else {
    // Blank GSID means reuse the last saved value
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

void printMac(const char *label, uint8_t mac[6]) {
  Serial.printf("%s: %02X:%02X:%02X:%02X:%02X:%02X\n",  //Writes in Hex w/ zero padding
                label,
                mac[0], mac[1], mac[2],
                mac[3], mac[4], mac[5]);
}

void displayMac(const char *label, uint8_t mac[6]) {
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
  // Use AP+STA so the ESP32 can host phone access while connected to the router
  WiFi.mode(WIFI_AP_STA);

  //mac_ssid = "csl-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFF), HEX);

  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  WiFi.onEvent(onWiFiEvent);  //
  display.clearDisplay();
  display.setCursor(0, 0);

  if (!WiFi.softAP(mac_ssid.c_str())) {
    Serial.println("[AP] SoftAP start failed");
    display.printf("SoftAP start failed\n");

    apActive = false;
    apIpText = "";
  } else {
    Serial.printf("[AP] Started: %s\n", mac_ssid.c_str());
    display.printf("Started AP:%s\n", mac_ssid.c_str());

    apActive = true;
    apIpText = WiFi.softAPIP().toString();
  }
  display.display();

  // Web routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/get", HTTP_GET, handleGet);
  // Status route works from the ESP32 AP IP and the router-assigned STA IP.
  server.on("/status", HTTP_GET, handleStatus);
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
  char apMacBuf[20];
  snprintf(apMacBuf, sizeof(apMacBuf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           apMac[0], apMac[1], apMac[2],
           apMac[3], apMac[4], apMac[5]);

  apMacText = String(apMacBuf);
  apMacShort = apMacText.substring(9);

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

  // Keep HTTP active after provisioning; phones use HTTP because local HTTPS certs are unreliable
  // server.stop();

  // Stop HTTPS after setup to reduce overhead; laptops can still use HTTPS during provisioning
  if (secureServer.isRunning()) {
    secureServer.stop();
  }
  // Keep SoftAP active so phones can connect after provisioning.
  // WiFi.softAPdisconnect(true);
}

void connectToWiFi() {
  // Brief pause lets WiFi mode changes settle before joining the router
  delay(1000);
  Serial.printf("\nWill try to connect to WiFi: %s\n", provisionInfo.ssid);
  display.clearDisplay();
  display.setCursor(0, 0);

  display.printf("T:%.1f P:%.0f\n", sensorData.Tbme, sensorData.Pbme);
  display.printf("RH:%.0f CO2:%d\n", sensorData.RHbme, sensorData.CO2);
  display.printf("PM25:%.1f VOC:%.1f\n", sensorData.mPm2_5, sensorData.VOCs);
  display.printf("Bat:%.2fV\n", sensorData.Vbat);

  display.printf("STA:%s %d\n", staIpText.c_str(), lastWifiRssi);
  display.printf("AP:%s\n", apIpText.c_str());
  display.printf("%s\n", googleStatusText.c_str());

  display.printf("S:%s A:%s\n", staMacShort.c_str(), apMacShort.c_str());
  display.display();

  // Keep AP alive while joining WiFi so phone access and uploads can coexist
  WiFi.mode(WIFI_AP_STA);
  wifiStatusText = "WiFi:JOIN";
  staConnected = false;
  staIpText = "";
  lastWifiRssi = 0;
  WiFi.begin(provisionInfo.ssid, provisionInfo.passcode);

  uint8_t staMac[6];
  WiFi.macAddress(staMac);

  char staMacBuf[20];
  snprintf(staMacBuf, sizeof(staMacBuf),
           "%02X:%02X:%02X:%02X:%02X:%02X",
           staMac[0], staMac[1], staMac[2],
           staMac[3], staMac[4], staMac[5]);

  staMacText = String(staMacBuf);
  staMacShort = staMacText.substring(9);

  unsigned long st = millis();
  while (WiFi.status() != WL_CONNECTED && provisionInfo.WiFiPresent && provisionInfo.valid) {
    Serial.print(".");
    delay(100);
    if ((millis() - st) > WIFI_TIMEOUT) {
      Serial.println("[WIFI] Connect timeout");
      provisionInfo.valid = false;
      staConnected = false;
      wifiStatusText = "WiFi:FAIL";
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
    staConnected = true;
    staIpText = WiFi.localIP().toString();
    lastWifiRssi = WiFi.RSSI();
    wifiStatusText = "WiFi:OK";

    Serial.printf("\nConnected to WiFi: %s\n", provisionInfo.ssid);
    display.printf("\nConnected to WiFi: \n\n%s", provisionInfo.ssid);
    display.display();
  }
}
