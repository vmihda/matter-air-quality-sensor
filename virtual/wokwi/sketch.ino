// Throwaway virtual bench for checking the wiring and threshold behavior in Wokwi.
// Production firmware uses ESP-IDF and aq_core. Timing here is shortened so the
// behavior is visible; thresholds match the spec.
#include <Wire.h>

// Pin numbers are chip GPIOs on the XIAO ESP32-C6.
constexpr int PIN_SDA = 22;     // D4
constexpr int PIN_SCL = 23;     // D5
constexpr int PIN_PMS_TX = 16;  // D6: ESP TX -> PMS5003 RX
constexpr int PIN_PMS_RX = 17;  // D7: PMS5003 TX -> ESP RX
// Simulation-only indicators; the real device reports these states through Matter.
constexpr int PIN_LED_VENTILATE = 0;  // D0
constexpr int PIN_LED_DUST = 1;       // D1
constexpr int PIN_LED_FAULT = 2;      // D2
constexpr uint8_t SCD41_ADDRESS = 0x62;

constexpr float CO2_MODERATE = 800, CO2_POOR = 1000, CO2_OFF = 800;
constexpr float PM25_MODERATE = 15, PM25_POOR = 35, PM25_OFF = 15;
constexpr uint32_t HOLD_MS = 10000;        // 5 minutes in the real device
constexpr uint32_t CO2_STALE_MS = 6000;    // 60 seconds in the real device
constexpr uint32_t PM25_STALE_MS = 9000;   // 5 minutes in the real device
constexpr uint32_t READ_PERIOD_MS = 2000;  // 5 s and 2 min in the real device

struct Alert {
  float on, off;
  bool active;
  bool timing;
  uint32_t since;
};
Alert ventilate = {CO2_POOR, CO2_OFF, false, false, 0};
Alert dust = {PM25_POOR, PM25_OFF, false, false, 0};

float co2 = 0, pm25 = 0, temperature = 0, humidity = 0;
bool haveCo2 = false, havePm25 = false;
uint32_t lastCo2 = 0, lastPm25 = 0, lastRead = 0;

uint8_t crc8(const uint8_t *data, size_t length) {
  uint8_t crc = 0xFF;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int bit = 0; bit < 8; bit++) {
      crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
    }
  }
  return crc;
}

bool sendScd41Command(uint16_t command) {
  Wire.beginTransmission(SCD41_ADDRESS);
  Wire.write((uint8_t)(command >> 8));
  Wire.write((uint8_t)(command & 0xFF));
  return Wire.endTransmission() == 0;
}

bool readScd41() {
  if (!sendScd41Command(0xEC05)) return false;
  delay(2);
  if (Wire.requestFrom(SCD41_ADDRESS, (uint8_t)9) != 9) return false;
  uint8_t raw[9];
  for (uint8_t &byteValue : raw) byteValue = Wire.read();
  for (int word = 0; word < 3; word++) {
    if (crc8(raw + word * 3, 2) != raw[word * 3 + 2]) return false;
  }
  uint16_t rawCo2 = (raw[0] << 8) | raw[1];
  if (rawCo2 == 0) return false;
  co2 = rawCo2;
  temperature = -45.0f + 175.0f * ((raw[3] << 8) | raw[4]) / 65535.0f;
  humidity = 100.0f * ((raw[6] << 8) | raw[7]) / 65535.0f;
  return true;
}

bool readPms5003() {
  // Parse every complete frame in the buffer and keep the latest valid value.
  static uint8_t frame[32];
  static int length = 0;
  bool valid = false;
  while (Serial1.available()) {
    uint8_t value = Serial1.read();
    if ((length == 0 && value != 0x42) || (length == 1 && value != 0x4D)) {
      length = 0;
      continue;
    }
    frame[length++] = value;
    if (length < 32) continue;
    length = 0;
    uint16_t sum = 0;
    for (int i = 0; i < 30; i++) sum += frame[i];
    if (sum == ((frame[30] << 8) | frame[31]) && frame[2] == 0x00 && frame[3] == 0x1C) {
      pm25 = (frame[12] << 8) | frame[13];
      valid = true;
    }
  }
  return valid;
}

void updateAlert(Alert &alert, float value, uint32_t now) {
  if (alert.active) {
    if (value < alert.off) {
      alert.active = false;
      alert.timing = false;
    }
    return;
  }
  if (value < alert.on) {
    alert.timing = false;
    return;
  }
  if (!alert.timing) {
    alert.timing = true;
    alert.since = now;
  }
  if (now - alert.since >= HOLD_MS) alert.active = true;
}

const char *levelName(int level) {
  static const char *const names[] = {"Good", "Moderate", "Poor"};
  return level < 0 ? "Unknown" : names[level];
}

int levelFor(float value, float moderate, float poor) {
  return value >= poor ? 2 : (value >= moderate ? 1 : 0);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_VENTILATE, OUTPUT);
  pinMode(PIN_LED_DUST, OUTPUT);
  pinMode(PIN_LED_FAULT, OUTPUT);
  Wire.begin(PIN_SDA, PIN_SCL);
  Serial1.begin(9600, SERIAL_8N1, PIN_PMS_RX, PIN_PMS_TX);
  Serial.println(sendScd41Command(0x21B1) ? "SCD41: start OK" : "SCD41: no reply, check SDA and SCL");
  lastCo2 = lastPm25 = millis();
}

void loop() {
  uint32_t now = millis();
  if (now - lastRead < READ_PERIOD_MS) {
    delay(20);
    return;
  }
  lastRead = now;

  if (readScd41()) {
    haveCo2 = true;
    lastCo2 = now;
    updateAlert(ventilate, co2, now);
  }
  if (readPms5003()) {
    havePm25 = true;
    lastPm25 = now;
    updateAlert(dust, pm25, now);
  }

  bool co2Stale = now - lastCo2 >= CO2_STALE_MS;
  bool pm25Stale = now - lastPm25 >= PM25_STALE_MS;
  if (co2Stale) {
    ventilate.active = false;
    ventilate.timing = false;
  }
  if (pm25Stale) {
    dust.active = false;
    dust.timing = false;
  }
  bool fault = co2Stale || pm25Stale;

  int level = -1;
  if (!fault && haveCo2 && havePm25) {
    level = max(levelFor(co2, CO2_MODERATE, CO2_POOR), levelFor(pm25, PM25_MODERATE, PM25_POOR));
  }

  digitalWrite(PIN_LED_VENTILATE, ventilate.active);
  digitalWrite(PIN_LED_DUST, dust.active);
  digitalWrite(PIN_LED_FAULT, fault);

  Serial.printf("CO2=%.0f ppm PM2.5=%.0f ug/m3 T=%.1f C RH=%.0f %% | level=%s ventilate=%d dust=%d fault=%d\n",
                co2, pm25, temperature, humidity, levelName(level), ventilate.active, dust.active, fault);
}
