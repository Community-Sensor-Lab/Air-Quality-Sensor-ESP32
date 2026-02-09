#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include "CSL_AQS_ESP32_V1.h"

/**
* Starts a wifi access point with a unique name 'csl-xxxx' and starts a server.
* When client connects with a browser at specified ip (192.168.4.1), serves a 
* provisioning page to client with fields for ssid, passcode and gsid entry
* (gsid is the unique identifier for google script). Parses the response and decodes 
* the string for any %-encoded characters, and saves to memory
*/

// ----------------------
// SoftAP config

// Decodes %-encoded strings (it's a thing)
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
  page += "<form action=\"/get\">";                                                  //adds a form to the page anf uses get request

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
  page += "GSID: <input type=\"text\" name=\"GSID\"><br>";
  //adds a submit button
  page += "<input type=\"submit\" value=\"Submit\">";
  page += "</form></body></html>";
  return page;
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
  // Read raw args (these are URL-encoded for GET)
  String ssidRaw = server.hasArg("SSID") ? server.arg("SSID") : "";
  String passRaw = server.hasArg("passcode") ? server.arg("passcode") : "";
  String gsidRaw = server.hasArg("GSID") ? server.arg("GSID") : "";

  // Decode
  String ssid = decodeUrl(ssidRaw);
  String pass = decodeUrl(passRaw);
  String gsid = decodeUrl(gsidRaw);

  // Store into struct safely
  memset(&provisionInfo, 0, sizeof(provisionInfo));

  strlcpy(provisionInfo.ssid, ssid.c_str(), sizeof(provisionInfo.ssid));
  strlcpy(provisionInfo.passcode, pass.c_str(), sizeof(provisionInfo.passcode));
  strlcpy(provisionInfo.gsid, gsid.c_str(), sizeof(provisionInfo.gsid));

  // Print for debugging
  Serial.println("\n✅ Provisioning received:");
  Serial.print("  SSID: ");
  Serial.println(provisionInfo.ssid);
  Serial.print("  PASS: ");
  Serial.println(provisionInfo.passcode);  // consider not printing in production
  Serial.print("  GSID: ");
  Serial.println(provisionInfo.gsid);

  // Respond to client
  String resp = "<!DOCTYPE html><html><body>";
  resp += "<h3>Received provisioning info</h3>";
  resp += "<p><b>SSID:</b> " + ssid + "</p>";
  resp += "<p><b>GSID:</b> " + gsid + "</p>";
  resp += "<p>You can now close this page.</p>";
  resp += "</body></html>";

  server.send(200, "text/html", resp);

  // From here you would typically:
  //  - persist to NVS/EEPROM
  //  - stop AP
  //  - attempt WiFi.begin(ssid, pass)
  provisionInfo.valid = true;
  provisionInfo.WiFiPresent = true;
  saveProvisioningInfoToEEPROM(provisionInfo);
}

void handleNotFound() {
  Serial.println("handleNotFound");
  server.send(404, "text/plain", "Not found");
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
  Serial.printf("Open webpage to %s on device connected to the WiFi\n", WiFi.softAPIP().toString());
  display.printf("Open webpage at\n%s\n", WiFi.softAPIP().toString());
  display.display();

  while (!provisionInfo.valid) {
    server.handleClient();
    if (!provisionInfo.WiFiPresent) {
      Serial.println("Provisioning canceled. Continue without WiFi");
      display.printf("Canceled. No WiFi");
      display.display();
      break;
    }
  }

  server.stop();
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
    Serial.printf("\nConnected to WiFi: %s\n", provisionInfo.ssid);
    display.printf("\nConnected to WiFi: \n\n%s", provisionInfo.ssid);
    display.display();
  }
}
