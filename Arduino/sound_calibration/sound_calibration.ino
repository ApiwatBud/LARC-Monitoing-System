
// ── Configuration ──────────────────────────────────────────────
const int    MIC_PIN     = 34;     // Analog mic pin (ADC)
const float  RMS_REF     = 50.0;   // Reference RMS (same as production)
const int    SAMPLE_N    = 1000;   // number of sample  (~50ms ที่ 20kHz)
const int    WINDOW_MS   = 1000;   // record window (every 1 min)
const int    AVG_COUNT   = 5;     

// ── Globals ────────────────────────────────────────────────────
unsigned long lastPrint  = 0;
int           readingIdx = 0;
float         rmsBuffer[AVG_COUNT];
unsigned long startTime  = 0;

// ── Setup ──────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  analogReadResolution(12);       // ESP32: 12-bit ADC (0–4095)
  analogSetAttenuation(ADC_11db); 

  startTime = millis();

  // Excel Header  
  Serial.println("=== Sound Calibration Log ===");
  Serial.println("Time_s,RMS_ratio,RMS_raw,Note");
  Serial.println("--- START RECORDING ---");
}

// ── Measure RMS_ratio  ──────────────────────────────
float measureRMS() {
  long   sumSq  = 0;
  int    dcSum  = 0;

  // read SAMPLE_N samples to calculate RMS
  for (int i = 0; i < SAMPLE_N; i++) {
    int raw = analogRead(MIC_PIN);
    dcSum += raw;
  }
  int dcOffset = dcSum / SAMPLE_N;  // Find DC offset

  for (int i = 0; i < SAMPLE_N; i++) {
    int raw    = analogRead(MIC_PIN);
    int sample = raw - dcOffset;     // DC offset removal
    sumSq += (long)sample * sample;
  }

  float rmsRaw   = sqrt((float)sumSq / SAMPLE_N);
  float rmsRatio = rmsRaw / RMS_REF;

  return rmsRatio;
}

// ── Loop ───────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  if (now - lastPrint >= WINDOW_MS) {
    lastPrint = now;

    float rmsRatio = measureRMS();
    float rmsRaw   = rmsRatio * RMS_REF;
    float timeSec  = (now - startTime) / 1000.0;

    //  buffer for moving average
    rmsBuffer[readingIdx % AVG_COUNT] = rmsRatio;
    readingIdx++;


    Serial.print(timeSec, 1);
    Serial.print(",");
    Serial.print(rmsRatio, 4);
    Serial.print(",");
    Serial.print(rmsRaw, 2);
    Serial.print(",");
    Serial.println("");
  }
}
