#include <Wire.h>
#include "DHT.h"
#include <BH1750FVI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

/*  Change 
    DEV_ID
    WIFI password
    RL 135,137
    RO 135,137
 */

/* ===================== DEVICE ===================== */
#define DEV_ID "EnviBox2"

/* ===================== PIN ======================== */
#define MQ137_PIN 35
#define MQ135_PIN 34
#define MIC_PIN   32
#define DHTPIN    23

/* ===================== ADC ======================== */
#define ADC_VOLTAGE 3.3
#define ADC_MAX     4095.0
#define ADC_MID     2048

/* ===================== WIFI ======================= */
const char* ssid     = "Fahmui-IOT";
const char* password = "6Kbe4yhY";  //6Kbe4yhY For Dev 2  ;  4bgtuRUT For Dev 1

/* ===================== SERVER ===================== */
const char* serverURL = "http://10.201.30.244:3000/api/v1/device_sensor_data_raw";

/* ===================== MQ RL (วัดจริง) ============ */
// Box 1
// #define RL_MQ137 2230.0   // 2.23 kΩ
// #define RL_MQ135 820.0    // 0.82 kΩ

// Box 2
#define RL_MQ137 3810.0   // 3.81 kΩ
#define RL_MQ135 950.0    // 0.95 kΩ


/* ===================== MQ-137 NH3 ================= */
#define SLOPE_137  -0.268
#define A_137       0.624
#define R0_137      7984.06   // Box 2



/* ===================== MQ-135 AIR ================= */
#define SLOPE_135  -0.42
#define A_135       0.77
#define R0_135      8567.21  // Box 2

/* ===================== DHT ======================== */
#define DHTTYPE DHT21
DHT dht(DHTPIN, DHTTYPE);

/* ===================== BH1750 ===================== */
BH1750FVI LightSensor(BH1750FVI::k_DevModeContLowRes);

/* ===================== FILTER ===================== */
const int NUM_SAMPLES = 15;
float alpha = 0.2;

/* ===================== MIC ======================== */
#define MIC_SAMPLES 400
#define MIC_A       24.17f   // calibration slope
#define MIC_B       63.96f   // calibration intercept (dB)
float micRefRMS = 50.0;   // ค่าเงียบ (cal ครั้งเดียว)

/* ===================== TIMER ===================== */
unsigned long lastMillis = 0;
const unsigned long interval = 30000;  //30 sec

/* ================================================= */
/* ===================== ADC ======================= */
int readFilteredADC(int pin) {
  static float ema135 = 0;
  static float ema137 = 0;
  float *ema = (pin == MQ135_PIN) ? &ema135 : &ema137;

  long sum = 0;
  for (int i = 0; i < NUM_SAMPLES; i++) {
    sum += analogRead(pin);
    delay(5);
  }

  float avg = sum / (float)NUM_SAMPLES;
  *ema = (*ema == 0) ? avg : alpha * avg + (1 - alpha) * (*ema);
  return (int)*ema;
}

/* ===================== Rs ========================= */
float calcRs(int adc, float rl) {
  float vout = (adc * ADC_VOLTAGE) / ADC_MAX;
  if (vout < 0.02) vout = 0.02;
  return ((ADC_VOLTAGE - vout) / vout) * rl;
}

/* ===================== PPM ======================== */
float getPPM137(int adc) {
  float Rs = calcRs(adc, RL_MQ137);
  float ratio = Rs / R0_137;

  ratio = constrain(ratio, 0.02, 50);
  float ppm = pow(10, (log10(ratio / A_137) / SLOPE_137));

  return ppm;   // ไม่มี hard limit
  // float Rs = calcRs(adc, RL_MQ137);
  // float ratio = Rs / R0_137;

  // ratio = constrain(ratio, 0.1, 20);
  // float ppm = pow(10, (log10(ratio / A_137) / SLOPE_137));
  // return constrain(ppm, 0, 300);
}

float getPPM135(int adc) {
  float Rs = calcRs(adc, RL_MQ135);
  float ratio = Rs / R0_135;

  ratio = constrain(ratio, 0.1, 30);
  float ppm = pow(10, (log10(ratio / A_135) / SLOPE_135));
  return constrain(ppm, 0, 1000);
}

/* ===================== MIC ======================== */
float readMicDB() {
  double sumSq = 0;
  for (int i = 0; i < MIC_SAMPLES; i++) {
    float v = analogRead(MIC_PIN) - ADC_MID;
    sumSq += v * v;
  }
  float rms = sqrt(sumSq / MIC_SAMPLES);
  return MIC_A * log10(max(rms / micRefRMS, 0.001f)) + MIC_B;
}

/* ===================== WIFI ======================= */
void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected");
}

/* ===================== SETUP ====================== */
void setup() {
  Serial.begin(115200);

  dht.begin();
  LightSensor.begin();

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  WiFi.mode(WIFI_STA);
  connectWiFi();
}

/* ===================== LOOP ======================= */
void loop() {
  if (millis() - lastMillis < interval) return;
  lastMillis = millis();

  float temp = dht.readTemperature();
  float hum  = dht.readHumidity();
  float L_temp;
  float L_hum ;
  if (isnan(temp) || isnan(hum)) {
    Serial.println("DHT read failed!");
    temp = L_temp;
    hum =  L_hum;
  }else{
    L_temp = temp;
    L_hum = hum;
  }

  int adc137 = readFilteredADC(MQ137_PIN);
  int adc135 = readFilteredADC(MQ135_PIN);

  float ppm137 = getPPM137(adc137);
  float ppm135 = getPPM135(adc135);


  uint16_t lux = LightSensor.GetLightIntensity();
  float soundDB = readMicDB();

  int aqi = getAQIfromMQ135(ppm135);
  String AQILvl = getAQILevel(aqi);

  Serial.println("\n==============================");
  Serial.printf("MQ137 (NH3): %.2f ppm\n", ppm137);
  Serial.printf("MQ135 (AIR): %.2f ppm | AQI: %s\n", ppm135, AQILvl);

  Serial.printf("Temp: %.2f °C | Hum: %.2f %%\n", temp, hum);
  Serial.printf("Light: %u lux\n", lux);
  Serial.printf("Sound: %.2f dB (relative)\n", soundDB);

  sendJSON( ppm137, ppm135, AQILvl, temp, hum, lux, soundDB);
}

void sendJSON(float ppm137, float ppm135,String AQILvl,
              float temp, float hum, uint16_t lux, float soundDB) {

  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    return;
  }

  HTTPClient http;
  http.begin(serverURL);
  http.addHeader("Content-Type", "application/json");
  
  StaticJsonDocument<256> doc;

  doc["device_name"] = DEV_ID;    //String

  JsonObject values = doc.createNestedObject("values");
  values["nh3_ppm"]     = ppm137;   //float
  values["air_ppm"]     = ppm135;   //float
  values["aqi_lvl"]     = AQILvl;   //String
  values["temperature"] = temp;     //float
  values["humidity"]    = hum;      //float
  values["light_lux"]   = lux;      //uint16_t
  values["sound_db"]    = soundDB;  //float
  
  String payload;
  serializeJson(doc, payload);

  Serial.println("JSON Payload:");
  Serial.println(payload);

  int httpCode = http.POST(payload);

  Serial.print("HTTP Response Code: ");
  Serial.println(httpCode);

  if (httpCode > 0) {
    Serial.println("Server response:");
    Serial.println(http.getString());
  }

  http.end();
}

int mapFloat(float x, float in_min, float in_max,
             int out_min, int out_max) {
  return (int)((x - in_min) * (out_max - out_min) /
               (in_max - in_min) + out_min);
}

int getAQIfromMQ135(float ppm) {
  if (ppm <= 50)
    return mapFloat(ppm, 0, 50, 0, 50);

  else if (ppm <= 100)
    return mapFloat(ppm, 50, 100, 51, 100);

  else if (ppm <= 200)
    return mapFloat(ppm, 100, 200, 101, 150);

  else if (ppm <= 400)
    return mapFloat(ppm, 200, 400, 151, 200);

  else if (ppm <= 600)
    return mapFloat(ppm, 400, 600, 201, 300);

  else
    return mapFloat(ppm, 600, 1000, 301, 500);
}

String getAQILevel(int aqi) {
  if (aqi <= 50) return "Good";
  else if (aqi <= 100) return "Moderate";
  else if (aqi <= 150) return "Unhealthy (Sensitive)";
  else if (aqi <= 200) return "Unhealthy";
  else if (aqi <= 300) return "Very Unhealthy";
  else return "Hazardous";
}