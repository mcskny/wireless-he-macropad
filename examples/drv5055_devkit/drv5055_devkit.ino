// DRV5055 OUT -> GPIO34, VCC -> 3V3, GND -> GND (klasik ESP32)
const int SENSOR_PIN = 34;
const int SAMPLE_COUNT = 32;

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  analogSetPinAttenuation(SENSOR_PIN, ADC_11db);
}

void loop() {
  uint32_t total = 0;
  for (int i = 0; i < SAMPLE_COUNT; ++i) {
    total += analogRead(SENSOR_PIN);
    delayMicroseconds(200);
  }
  float raw = total / (float)SAMPLE_COUNT;
  Serial.print("ADC (0-4095): ");
  Serial.println(raw, 1);
  delay(100);
}
