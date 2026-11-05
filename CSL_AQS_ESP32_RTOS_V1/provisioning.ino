#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <Preferences.h>
#include "CSL_AQS_ESP32_RTOS_V1.h"

/**
* Starts a wifi access point with a unique name 'csl-xxxx' and starts a server.
* When client connects with a browser at specified ip (192.168.4.1), serves a 
* provisioning page to client with fields for ssid, passcode and gsid entry
* (gsid is the unique identifier for google script). Parses the response and decodes 
* the string for any %-encoded characters, and saves to memory via Preferences.
*/

// ----------------------
// Preferences Storage Functions
// ----------------------

// Save provisioning info using individual Preferences keys
bool saveProvisioningInfoToPreferences(const Secrets& s) {
  Preferences prefs;
  if (!prefs.begin("wifi", false)) {
    return false;
  }

  prefs.putString("ssid", s.ssid);
  prefs.putString("pass", s.passcode);
  prefs.putString("gsid", s.gsid);
  
  prefs.end();
  return true;
}

// Load provisioning info using individual Preferences keys
bool loadProvisioningInfoFromPreferences(Secrets& out) {
  Preferences prefs;
  if (!prefs.begin("wifi", true)) {
    return false;
  }

  // If the SSID key doesn't exist, provisioning hasn't happened yet
  if (!prefs.isKey("ssid")) {
    prefs.end();
    return false;
  }

  String ssidStr = prefs.getString("ssid", "");
  String passStr = prefs.getString("pass", "");
  String gsidStr = prefs.getString("gsid", "");

  strlcpy(out.ssid, ssidStr.c_str(), sizeof(out.ssid));
  strlcpy(out.passcode, passStr.c_str(), sizeof(out.passcode));
  strlcpy(out.gsid, gsidStr.c_str(), sizeof(out.gsid));

  // Defaults since valid/WiFiPresent flags were dropped
  out.valid = true;
  out.WiFiPresent = true;

  prefs.end();
  return true;
}

// Read and verify info from Preferences
void provisioningFromPreferences() {
  if (!loadProvisioningInfoFromPreferences(provisionInfo)) {
    safePrintf("No valid ProvisioningInfo in Preferences");
    provisionInfo.valid = false;
    provisionInfo.WiFiPresent = true;
  } else {
    safePrintf("Loaded ProvisioningInfo from Preferences.");
    safePrintf("ssid:%s, pw:%s, gsid:%s\nvalid:%i, wifipresent:%i\n",
               provisionInfo.ssid, provisionInfo.passcode, provisionInfo.gsid, 
               (int)provisionInfo.valid, (int)provisionInfo.WiFiPresent);
  }  
}

// ----------------------
// SoftAP config & Web Server
// ----------------------

// Decodes %-encoded strings (it's a thing)
static String decodeUrl(const String& in) {
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

// updated webpage to show available networks:
String buildProvisioningPage() {
  int n = WiFi.scanNetworks();                                                   
  String page = "<!DOCTYPE HTML><html><head><title>Provision</title></head><body>";  
  page += "<form action=\"/get\">";                                            

  page += "SSID: <select name=\"SSID\">";
  for (int i = 0; i < n; i++) {
    page += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + " dBm)" + "</option>";
  }
  page += "</select><br>";
  page += "Passcode: <input type=\"password\" name=\"passcode\"><br>";
  page += "GSID: <input type=\"text\" name=\"GSID\"><br>";
  page += "<input type=\"submit\" value=\"Submit\">";
  page += "</form></body></html>";
  return page;
}

void handleRoot() {
  safePrintf("handleRoot");
  String page = buildProvisioningPage();
  server.send(200, "text/html", page);
}

void handleGet() {
  safePrintf("handleGet");
  String ssidRaw = server.hasArg("SSID") ? server.arg("SSID") : "";
  String passRaw = server.hasArg("passcode") ? server.arg("passcode") : "";
  String gsidRaw = server.hasArg("GSID") ? server.arg("GSID") : "";

  String ssid = decodeUrl(ssidRaw);
  String pass = decodeUrl(passRaw);
  String gsid = decodeUrl(gsidRaw);

  memset(&provisionInfo, 0, sizeof(provisionInfo));

  strlcpy(provisionInfo.ssid, ssid.c_str(), sizeof(provisionInfo.ssid));
  strlcpy(provisionInfo.passcode, pass.c_str(), sizeof(provisionInfo.passcode));
  strlcpy(provisionInfo.gsid, gsid.c_str(), sizeof(provisionInfo.gsid));

  safePrintf("\n✅ Provisioning received:");
  Serial.print("  SSID: ");
  safePrintf(provisionInfo.ssid);
  Serial.print("  PASS: ");
  safePrintf(provisionInfo.passcode);  
  Serial.print("  GSID: ");
  safePrintf(provisionInfo.gsid);

  String resp = "<!DOCTYPE html><html><body>";
  resp += "<h3>Received provisioning info</h3>";
  resp += "<p><b>SSID:</b> " + ssid + "</p>";
  resp += "<p><b>GSID:</b> " + gsid + "</p>";
  resp += "<p>You can now close this page.</p>";
  resp += "</body></html>";

  server.send(200, "text/html", resp);

  provisionInfo.valid = true;
  provisionInfo.WiFiPresent = true;
  saveProvisioningInfoToPreferences(provisionInfo);
}

void handleNotFound() {
  safePrintf("handleNotFound");
  server.send(404, "text/plain", "Not found");
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf(
        "📲 Client connected: %02X:%02X:%02X:%02X:%02X:%02X, AID=%d\n",
        info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
        info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
        info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5],
        info.wifi_ap_staconnected.aid);
      break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.printf(
        "📴 Client disconnected: %02X:%02X:%02X:%02X:%02X:%02X, AID=%d\n",
        info.wifi_ap_stadisconnected.mac[0], info.wifi_ap_stadisconnected.mac[1],
        info.wifi_ap_stadisconnected.mac[2], info.wifi_ap_stadisconnected.mac[3],
        info.wifi_ap_stadisconnected.mac[4], info.wifi_ap_stadisconnected.mac[5],
        info.wifi_ap_stadisconnected.aid);
      break;

    default:
      break;
  }
}

void printMac(const char* label, uint8_t mac[6]) {
  Serial.printf("%s: %02X:%02X:%02X:%02X:%02X:%02X\n",  
                label, mac[0], mac[1], mac[2],
                mac[3], mac[4], mac[5]);
}

void displayMac(const char* label, uint8_t mac[6]) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
  display.printf("%s\n%s\n", label, buf);
}

void softAPprovision() {
  static const IPAddress AP_IP(192, 168, 4, 1);
  static const IPAddress AP_GW(192, 168, 4, 1);
  static const IPAddress AP_MASK(255, 255, 255, 0);
  
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(AP_IP, AP_GW, AP_MASK);
  WiFi.onEvent(onWiFiEvent);  
  display.clearDisplay();
  display.setCursor(0, 0);

  if (!WiFi.softAP(mac_ssid.c_str())) {
    safePrintf("❌ softAP start failed");
    display.printf("SoftAP start failed\n");
  } else {
    Serial.printf("✅ Started Provisioning Wifi: %s\n", mac_ssid.c_str());
    display.printf("Started Provisioning Wifi:%s\n", mac_ssid.c_str());
  }
  display.display();

  server.on("/", HTTP_GET, handleRoot);
  server.on("/get", HTTP_GET, handleGet);
  server.onNotFound(handleNotFound);

  server.begin();
  safePrintf("✅ HTTP server started (port 80)");
  Serial.printf("Open webpage to %s on device connected to the WiFi\n", WiFi.softAPIP().toString().c_str());

  uint64_t chipid = ESP.getEfuseMac();  
  uint8_t baseMac[6];                       
  for (int i = 0; i < 6; i++) {                 
    baseMac[i] = (chipid >> (8 * (5 - i))) & 0xFF;  
  }

  printMac("Base MAC (eFuse)", baseMac);

  uint8_t staMac[6];
  WiFi.macAddress(staMac);  
  printMac("STA MAC", staMac);

  uint8_t apMac[6];
  WiFi.softAPmacAddress(apMac);
  printMac("AP MAC", apMac);

  display.printf("Open webpage at\n%s\n", WiFi.softAPIP().toString().c_str());
  displayMac("STA MAC", staMac);
  displayMac("AP MAC", apMac);
  display.display();

  while (!provisionInfo.valid) {
    server.handleClient();
    if (!provisionInfo.WiFiPresent) {
      safePrintf("Provisioning canceled. Continue without WiFi");
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
      safePrintf("wifi connect timeout");
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
  else 
  {
    safePrintf("\n Failed to reconnect...");
  }
}

void provisioning() {
  Serial.printf("To force provisioning press button A\n");
  Serial.printf("To continue without WiFi press button B\n");
  display.printf("Provisioning: bttn A\n");
  display.printf("No WiFi: bttn B\n");
  display.display();

  provisioningFromPreferences();  // get Preferences info

  Serial.printf("10s to decide\n");
  unsigned long ts = millis();

  if (provisionInfo.valid) {  
    int i = 10;
    while (millis() - ts < WIFI_TIMEOUT && provisionInfo.valid && provisionInfo.WiFiPresent) {
      Serial.printf("%d ", i--);
      delay(1000);
    }
    Serial.println();
  }

  if (provisionInfo.valid && provisionInfo.WiFiPresent) {
    connectToWiFi();
  }

  while (!provisionInfo.valid && provisionInfo.WiFiPresent) {  
    uint32_t mac_reversed = (uint32_t)(ESP.getEfuseMac() >> 24) & 0xFFFFFF;
    uint32_t mac_original = ((mac_reversed & 0xFF) << 16) | ((mac_reversed & 0xFF00)) | ((mac_reversed & 0xFF0000) >> 16);  

    mac_ssid = "csl-" + String(mac_original, HEX);

    Serial.println("going to softAPprovision");
    softAPprovision();  
    if (provisionInfo.valid && provisionInfo.WiFiPresent) {
      connectToWiFi();
    }
  }

  if (!provisionInfo.WiFiPresent) {
    Serial.println("No WiFi present. Continuing without WiFi.");
    display.printf("No WiFi\n");
    display.display();
  }
}