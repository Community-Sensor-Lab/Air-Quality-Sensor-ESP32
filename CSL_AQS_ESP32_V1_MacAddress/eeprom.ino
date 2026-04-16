
#include <Arduino.h>
#include "EEPROM.h"
#include "CSL_AQS_ESP32_V1.h"

// crc helper function
static uint32_t crc32_bytes(const uint8_t* data, size_t len) {
  uint32_t crc = 0xFFFFFFFF;
  while (len--) {
    crc ^= *data++;
    for (int i = 0; i < 8; i++) {
      uint32_t mask = -(crc & 1u);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc; // ~ is bitwise NOT  
}

// Save to EEPROM
bool saveProvisioningInfoToEEPROM(const Secrets& s) {
  SecretsRecord rec{};
  rec.magic   = SECRETS_MAGIC;
  rec.version = SECRETS_VERSION;
  rec.length  = sizeof(Secrets);
  rec.payload = s;

  rec.crc32 = crc32_bytes(reinterpret_cast<const uint8_t*>(&rec.payload),sizeof(rec.payload));

  const size_t need = EEPROM_ADDR + sizeof(SecretsRecord);
  if (!EEPROM.begin(need)) return false;

  EEPROM.put(EEPROM_ADDR, rec);
  return EEPROM.commit();
}

// Load from EEPROM
bool loadProvisioningInfoFromEEPROM(Secrets& out) {
  const size_t need = EEPROM_ADDR + sizeof(SecretsRecord);
  if (!EEPROM.begin(need)) return false;

  SecretsRecord rec;
  EEPROM.get(EEPROM_ADDR, rec);

  if (rec.magic != SECRETS_MAGIC) return false;
  if (rec.version != SECRETS_VERSION) return false;
  if (rec.length != sizeof(Secrets)) return false;

  const uint32_t crc = crc32_bytes(reinterpret_cast<const uint8_t*>(&rec.payload),sizeof(rec.payload));
  if (crc != rec.crc32) return false;

  out = rec.payload;
  return true;
}

// read and verify info
void provisioningFromEEPROM() {

  if (!loadProvisioningInfoFromEEPROM(provisionInfo)) {
    Serial.println("No valid ProvisioningInfo in EEPROM");
    memset(&provisionInfo, 0, sizeof(provisionInfo));
    provisionInfo.valid = false;
    provisionInfo.WiFiPresent = false;

  } else {
    Serial.println("Loaded ProvisioningInfo from EEPROM.");
    //Serial.printf("ssid:%s, pw:%s, gsid:%s\nvalid:%i, wifipresent:%i\n",provisionInfo.ssid,provisionInfo.passcode,provisionInfo.gsid,(int)provisionInfo.valid,(int)provisionInfo.WiFiPresent);
  }  
}