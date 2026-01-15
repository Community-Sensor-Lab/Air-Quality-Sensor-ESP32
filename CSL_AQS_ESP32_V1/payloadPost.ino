/*
  Wifi secure connection example for ESP32
  Running on TLS 1.2 using mbedTLS
  Suporting the following chipersuites:
*/
//#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "CSL_AQS_ESP32_V1.h"

// #define PRE_PAYLOAD_APPEND_ROW "{\"command\":\"appendRow\",\"sheet_name\":\"Sheet1\",\"values\":\""
// #define PRE_PAYLOAD_ADD_HEADER "{\"command\":\"addHeader\",\"sheet_name\":\"Sheet1\",\"values\":\""
#define POST_PAYLOAD "\"}"
// #define SERVER "script.google.com"  // Server URL

#define SERVER_GOOGLE_SCRIPT  "script.google.com"
#define SERVER_GOOGLE_USERCONTENT  "script.googleusercontent.com"

// google
const char* test_root_ca = R"string_literal(
-----BEGIN CERTIFICATE-----
MIIFVzCCAz+gAwIBAgINAgPlk28xsBNJiGuiFzANBgkqhkiG9w0BAQwFADBHMQsw
CQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZpY2VzIExMQzEU
MBIGA1UEAxMLR1RTIFJvb3QgUjEwHhcNMTYwNjIyMDAwMDAwWhcNMzYwNjIyMDAw
MDAwWjBHMQswCQYDVQQGEwJVUzEiMCAGA1UEChMZR29vZ2xlIFRydXN0IFNlcnZp
Y2VzIExMQzEUMBIGA1UEAxMLR1RTIFJvb3QgUjEwggIiMA0GCSqGSIb3DQEBAQUA
A4ICDwAwggIKAoICAQC2EQKLHuOhd5s73L+UPreVp0A8of2C+X0yBoJx9vaMf/vo
27xqLpeXo4xL+Sv2sfnOhB2x+cWX3u+58qPpvBKJXqeqUqv4IyfLpLGcY9vXmX7w
Cl7raKb0xlpHDU0QM+NOsROjyBhsS+z8CZDfnWQpJSMHobTSPS5g4M/SCYe7zUjw
TcLCeoiKu7rPWRnWr4+wB7CeMfGCwcDfLqZtbBkOtdh+JhpFAz2weaSUKK0Pfybl
qAj+lug8aJRT7oM6iCsVlgmy4HqMLnXWnOunVmSPlk9orj2XwoSPwLxAwAtcvfaH
szVsrBhQf4TgTM2S0yDpM7xSma8ytSmzJSq0SPly4cpk9+aCEI3oncKKiPo4Zor8
Y/kB+Xj9e1x3+naH+uzfsQ55lVe0vSbv1gHR6xYKu44LtcXFilWr06zqkUspzBmk
MiVOKvFlRNACzqrOSbTqn3yDsEB750Orp2yjj32JgfpMpf/VjsPOS+C12LOORc92
wO1AK/1TD7Cn1TsNsYqiA94xrcx36m97PtbfkSIS5r762DL8EGMUUXLeXdYWk70p
aDPvOmbsB4om3xPXV2V4J95eSRQAogB/mqghtqmxlbCluQ0WEdrHbEg8QOB+DVrN
VjzRlwW5y0vtOUucxD/SVRNuJLDWcfr0wbrM7Rv1/oFB2ACYPTrIrnqYNxgFlQID
AQABo0IwQDAOBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4E
FgQU5K8rJnEaK0gnhS9SZizv8IkTcT4wDQYJKoZIhvcNAQEMBQADggIBAJ+qQibb
C5u+/x6Wki4+omVKapi6Ist9wTrYggoGxval3sBOh2Z5ofmmWJyq+bXmYOfg6LEe
QkEzCzc9zolwFcq1JKjPa7XSQCGYzyI0zzvFIoTgxQ6KfF2I5DUkzps+GlQebtuy
h6f88/qBVRRiClmpIgUxPoLW7ttXNLwzldMXG+gnoot7TiYaelpkttGsN/H9oPM4
7HLwEXWdyzRSjeZ2axfG34arJ45JK3VmgRAhpuo+9K4l/3wV3s6MJT/KYnAK9y8J
ZgfIPxz88NtFMN9iiMG1D53Dn0reWVlHxYciNuaCp+0KueIHoI17eko8cdLiA6Ef
MgfdG+RCzgwARWGAtQsgWSl4vflVy2PFPEz0tv/bal8xa5meLMFrUKTX5hgUvYU/
Z6tGn6D/Qqc6f1zLXbBwHSs09dR2CQzreExZBfMzQsNhFRAbd03OIozUhfJFfbdT
6u9AWpQKXCBfTkBdYiJ23//OYb2MI3jSNwLgjt7RETeJ9r/tSQdirpLsQBqvFAnZ
0E6yove+7u7Y/9waLd64NnHi/Hm3lCXRSHNboTXns5lndcEZOitHTtNCjv0xyBZm
2tIMPNuzjsmhDYAPexZ3FL//2wmUspO8IFgV6dtxQ/PeEMMA3KgqlbbC1j+Qa3bb
bP6MvPJwNQzcmRk13NfIRmPVNnGuV/u3gm3c
-----END CERTIFICATE-----
)string_literal";

WiFiClientSecure client;

void initializeClient() {
  client.setCACert(test_root_ca);
}

/*!
* @brief Makes an http get request and handles the response message by calling handleResponse()
**/
void httpGet(String url) {
  Serial.println("Calling httpGet...");
  client.println("GET " + url + " HTTP/1.1");
  client.println("Host: "  SERVER_GOOGLE_USERCONTENT);
  client.println("Connection: close");
  client.println();

  response = "";
  unsigned long lastRead = millis();
  const unsigned long timeout = 1700;  // 3 seconds

  while (client.connected() || client.available()) {
    if (client.available()) {
      char c = client.read();
      response += c;
      lastRead = millis();  // reset timer on data received
    }

    if (millis() - lastRead > timeout) {
      Serial.println("Response timeout. Exiting read loop.");
      break;
    }
  }

  delay(200);  // Allow TLS setup
  handleResponse();
}

void doPost(String outstr) {
  Serial.println("Calling httpPost...");

  String payload = outstr + POST_PAYLOAD;
  Serial.println(payload);

  Serial.print("\n\nStarting connection to server... ");
  if (!client.connect(SERVER_GOOGLE_SCRIPT, 443))
    Serial.println("Connection failed");
  else {
    Serial.println("Connected to server");
    client.println("POST /macros/s/" + String(provisionInfo.gsid) + "/exec? HTTP/1.0");  //value=Hello HTTP/1.0");
    client.println("Host: " SERVER_GOOGLE_SCRIPT);
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(payload.length());
    client.println();
    //Serial.println(String(PRE_PAYLOAD) + String(payload) + String(POST_PAYLOAD));
    client.print(payload);
    client.println();
    delay(200);
    Serial.println("\nResponse from client: ");

    unsigned long lastRead = millis();
    const unsigned long timeout = 3000;  // 3 seconds

    while (client.connected() || client.available()) {
      if (client.available()) {
        char c = client.read();
        response += c;
        lastRead = millis();  // reset timer on data received
      }

      if (millis() - lastRead > timeout) {
        Serial.println("Response timeout. Exiting read loop.");
        break;
      }
    }

    delay(200);  // Allow TLS setup
    handleResponse();
  }
}

/*!
* @brief Handles response from httpGet() and httPost()
**/
void handleResponse() {
  if (response.indexOf("200 OK") != -1) {
    //Serial.println("Successfully fetched data from google AppScript");
    int srateIndex = response.indexOf("response:");
    Serial.println("RESPONSE: ");
    Serial.println(response);
    if (srateIndex != -1) {
      int valueStart = srateIndex + 9; // Skip past "srate:"
      int valueEnd = response.indexOf('\n', valueStart); // Find the end of the line
      String rateStr = response.substring(valueStart, valueEnd);
      samplingPeriod = rateStr.toInt();
     
      Serial.print("Parsed sampling rate: ");
      Serial.println(samplingPeriod);
    }

    // Future application: Remotely turning off the screen
    
  }
 // Handle Redirect (302 Moved Temporarily)
  else if (response.indexOf("302 Moved Temporarily") != -1) {
    // Extract the "Location" header (URL to redirect to)
    int locIndex = response.indexOf("Location: ");
    if (locIndex != -1) {
      // Extract the URL after "Location: "
      int endIndex = response.indexOf('\n', locIndex);
      if (endIndex == -1) endIndex = response.length();  // Fallback if no newline found
      String locationURL = response.substring(locIndex + 9, endIndex);
      locationURL.trim();  // Clean up the URL
      int pathIndex = locationURL.indexOf(".com");
      if (pathIndex == -1) {
         Serial.println("Invalid URL format");
        return;
      }

      // Handle the redirect with a GET request
      String pathAndQuery = locationURL.substring(pathIndex + 4);  // Skip ".com"
      
    
      initializeClient();
      httpGet(pathAndQuery);
    } else {
      Serial.println("No Location header found.");
    }
  }
  else{
    Serial.println("Unrecognized response code");
    Serial.print(response);
  }
}
