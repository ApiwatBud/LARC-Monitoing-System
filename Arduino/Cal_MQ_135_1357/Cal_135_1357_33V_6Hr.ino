/******************************************************
 * MQ-135 & MQ-137 Calibration (6 hours)
 * ESP32 ADC 12bit, 3.3V
 * Update R0 ทุก 30 นาที
 ******************************************************/

#define MQ135_PIN 34
#define MQ137_PIN 35

#define ADC_MAX 4095.0
#define ADC_VOLTAGE 3.3

// Box 1
#define RL_MQ135 820.0    
#define RL_MQ137 2230.0

// Box 2
// #define RL_MQ135 950.0    
// #define RL_MQ137 3810.0

// ================= TIMER =================
const unsigned long SAMPLE_INTERVAL = 1800000UL; // 30 Min
const int TOTAL_SAMPLES = 12; // 6 Hr = 12 Times

unsigned long lastSampleTime = 0;
int sampleCount = 0;

// ================= R0 ====================
float R0_135_sum = 0;
float R0_137_sum = 0;

// ================= ADC FILTER ============
#define NUM_SAMPLES 20
float alpha = 0.2;

int readFilteredADC(int pin) {
  static float ema34 = 0;
  static float ema35 = 0;
  float *ema = (pin == MQ135_PIN) ? &ema34 : &ema35;

  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(pin);
    delay(5);
  }

  float avg = sum / (float)NUM_SAMPLES;
  *ema = (*ema == 0) ? avg : alpha * avg + (1 - alpha) * (*ema);

  return (int)(*ema);
}

// ================= RS ====================
float calcRs(int adc, float rl) {
  float vout = (adc / ADC_MAX) * ADC_VOLTAGE;
  if (vout <= 0.01) vout = 0.01;
  return ((ADC_VOLTAGE - vout) / vout) * rl;
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); // 0–3.3V

  Serial.println("=== MQ135 & MQ137 Calibration START ===");
  Serial.println("Place sensors in CLEAN AIR");
}

// ================= LOOP ==================
void loop() {
  if (sampleCount >= TOTAL_SAMPLES) {
    float R0_135 = R0_135_sum / TOTAL_SAMPLES;
    float R0_137 = R0_137_sum / TOTAL_SAMPLES;

    Serial.println("\n=== CALIBRATION COMPLETE (6 HOURS) ===");
    Serial.printf("Final R0 MQ-135 : %.2f ohm\n", R0_135);
    Serial.printf("Final R0 MQ-137 : %.2f ohm\n", R0_137);
    Serial.println(">>> COPY THESE VALUES INTO MAIN CODE <<<");

    while (1); // stop
  }

  if (millis() - lastSampleTime >= SAMPLE_INTERVAL) {
    lastSampleTime = millis();
    sampleCount++;

    int adc135 = readFilteredADC(MQ135_PIN);
    int adc137 = readFilteredADC(MQ137_PIN);

    float Rs135 = calcRs(adc135, RL_MQ135);
    float Rs137 = calcRs(adc137, RL_MQ137);

    R0_135_sum += Rs135;
    R0_137_sum += Rs137;

    Serial.println("\n-----------------------------");
    Serial.printf("Sample %d / %d\n", sampleCount, TOTAL_SAMPLES);

    Serial.printf("MQ135 ADC: %d | Rs: %.2f ohm\n", adc135, Rs135);
    Serial.printf("MQ137 ADC: %d | Rs: %.2f ohm\n", adc137, Rs137);

    Serial.printf("R0_135 (avg): %.2f ohm\n", R0_135_sum / sampleCount);
    Serial.printf("R0_137 (avg): %.2f ohm\n", R0_137_sum / sampleCount);
  }
}
