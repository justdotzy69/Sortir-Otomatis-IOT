#include <WiFi.h>
#include <PubSubClient.h>   // Library: "PubSubClient" by Nick O'Leary
#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <HX711.h>          // Library: "HX711" by Bogdan Necula (bogde)
#include <ArduinoJson.h>    // Library: "ArduinoJson" by Benoit Blanchon
#include <time.h>
#include <math.h>

// ================== WIFI & MQTT ==================
const char* WIFI_SSID = "YTAA";
const char* WIFI_PASS = "123456789";
const int   WIFI_CHANNEL = 6;            // Hapus parameter WIFI_CHANNEL di WiFi.begin jika WiFi asli

const char* MQTT_HOST = "broker.hivemq.com";
const int   MQTT_PORT = 1883;

const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET = 25200;  // WIB = UTC+7
bool ntpSynced = false;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ================== KONFIGURASI PIN ==================
const int ENA = 25;   // PWM motor (L298N)
const int IN1 = 26;
const int IN2 = 33;

const int ENC_A = 4;
const int ENC_B = 32;

const int PIN_IR_SENSOR = 27;
const int IR_ACTIVE_STATE = LOW;  // Ganti HIGH jika modul IR aktif-HIGH

const int PIN_SERVO = 13;

const int PIN_RFID_SS   = 5;
const int PIN_RFID_SCK  = 18;
const int PIN_RFID_MISO = 19;
const int PIN_RFID_MOSI = 23;
const int PIN_RFID_RST  = 22;

const int PIN_HX711_DT  = 16;
const int PIN_HX711_SCK = 17;

// ================== PARAMETER LOADCELL ==================
// Salin angka hasil kalibrasi ('k') dari Serial Monitor ke baris ini agar permanen:
float CALIBRATION_FACTOR = 428.90;       
long  zero_offset        = 0;
const float MIN_ITEM_G        = 5.0;   
const unsigned long SETTLE_MS = 500;
const int WEIGHT_SAMPLES      = 5;    // 5x bacaRawStabil() (total 35 sampel median-filtered)

// ================== THRESHOLD TOLERANSI DEFAULT (SATUAN GRAM) ==================
float toleranceMin = 300.0;     // Di bawah ini = RINGAN (disortir via servo ke wadah reject/ringan)
float toleranceMax = 1000.0;    // Di atas ini  = ERROR  (motor konveyor berhenti)
                                // Di antara    = BERAT  (pass, jalur lurus konveyor)

// ================== PARAMETER SERVO & WAKTU ==================
const int SERVO_STRAIGHT = 0;
const int SERVO_DIVERT   = 90;

const unsigned long TIMEOUT_ITEM_ON_SCALE = 15000;
const unsigned long TIMEOUT_TRAVEL        = 20000;
const unsigned long TIMEOUT_CLEAR         = 15000;
const unsigned long CLEAR_DELAY_MS        = 600;
const unsigned long SERVO_MIN_HOLD_MS     = 2000;

// ================== ENCODER & MOTOR ==================
volatile long encoderValue = 0;
const int PULSES_PER_MOTOR_REV = 11;
const float GEAR_RATIO = 90.0;
const float PULSES_PER_SHAFT_REV = PULSES_PER_MOTOR_REV * GEAR_RATIO;

const int freq = 5000;
const int resolution = 8;
const int PWM_MAX = 255;

// ================== PID KECEPATAN ==================
float setpointRPM = 60.0;
const float MAX_RPM_LIMIT = 200.0;

float Kp = 2.0;
float Ki = 5.0;
float Kd = 0.05;

const unsigned long PID_INTERVAL_MS = 100;
const float RPM_FILTER_ALPHA = 0.3;
const float PWM_START = 120.0;

float rpmFiltered = 0;
float pidIntegral = 0;
float pidPrevMeas = 0;
int   pwmOut = 0;

// ================== OBJEK ==================
MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);
Servo sortingServo;
HX711 scale;

// ================== STATE MACHINE ==================
enum State {
  WAIT_RFID,
  WAIT_ITEM_ON_SCALE,
  WEIGHING,
  CONVEYOR_RUN,
  SORTING,
  WAIT_CLEAR
};
const char* STATE_NAMES[] = {
  "WAIT_RFID", "WAIT_ITEM_ON_SCALE", "WEIGHING",
  "CONVEYOR_RUN", "SORTING", "WAIT_CLEAR"
};

State state = WAIT_RFID;
unsigned long stateStart = 0;

String currentUID = "";
float currentWeightG = 0;
String currentCategory = "";
bool conveyorRunning = false;

float weightSum = 0;
int weightCount = 0;

bool irClearSeen = false;
unsigned long irClearTime = 0;

bool tareRequested = false;

// ================== FORWARD DECLARATIONS ==================
bool irDetected();
void startConveyor();
void stopConveyor();
void resetCycle();

// ================== FUNGSI LOADCELL ANTI-SPIKE ==================
long bacaRawStabil() {
  const int jumlahSampel = 7;
  long data[jumlahSampel];

  for (int i = 0; i < jumlahSampel; i++) {
    unsigned long waitStart = millis();
    while (!scale.is_ready()) {
      if (millis() - waitStart > 200) return zero_offset; // Cegah hang jika HX711 terputus
      delay(1);
    }
    data[i] = scale.read();
  }

  // Urutkan sampel (Bubble Sort) untuk membuang nilai ekstrem atas & bawah
  for (int i = 0; i < jumlahSampel - 1; i++) {
    for (int j = i + 1; j < jumlahSampel; j++) {
      if (data[i] > data[j]) {
        long temp = data[i];
        data[i] = data[j];
        data[j] = temp;
      }
    }
  }

  // Ambil rata-rata dari 3 nilai tengah
  return (data[2] + data[3] + data[4]) / 3;
}

float getWeightGram() {
  if (fabs(CALIBRATION_FACTOR) < 0.0001) return 0.0;
  long rawSekarang = bacaRawStabil();
  float beratGram = (float)(rawSekarang - zero_offset) / CALIBRATION_FACTOR;

  // Deadzone di sekitar 0 gram untuk meredam fluktuasi mikro
  if (fabs(beratGram) < 0.2) {
    beratGram = 0.0;
  }
  return beratGram;
}

void prosesTare() {
  Serial.println("[LOADCELL] Melakukan Tare (Nol-kan timbangan), pastikan kosong...");
  delay(500);
  zero_offset = bacaRawStabil();
  Serial.printf("[LOADCELL] Tare selesai! Zero Offset: %ld\n", zero_offset);
}

// ================== KALIBRASI INTERAKTIF VIA SERIAL ==================
void runInteractiveCalibration() {
  stopConveyor();
  Serial.println("\n==================================================");
  Serial.println("[KALIBRASI] Mode Kalibrasi LoadCell HX711");
  Serial.println("Langkah 1: KOSONGKAN platform timbangan sekarang!");
  Serial.println("Menunggu 3 detik untuk mengunci titik nol (Tare)...");
  delay(3000);

  zero_offset = bacaRawStabil();
  Serial.printf("[KALIBRASI] Tare selesai! Zero Offset terkunci di: %ld\n", zero_offset);
  Serial.println("\nLangkah 2: Letakkan barang uji di atas load cell,");
  Serial.println("lalu KETIK berat asli barang dalam gram (contoh: 65 atau 314.7) dan tekan ENTER:");

  while (Serial.available()) Serial.read();

  String input = "";
  while (true) {
    if (Serial.available() > 0) {
      input = Serial.readStringUntil('\n');
      input.trim();
      if (input.length() > 0) break;
    }
    delay(20);
  }

  float knownWeight = input.toFloat();
  if (knownWeight <= 0) {
    Serial.println("[KALIBRASI] Input tidak valid (harus > 0), kalibrasi dibatalkan.\n");
    return;
  }

  Serial.printf("[KALIBRASI] Mengukur beban %.1f g, jangan sentuh load cell...\n", knownWeight);
  delay(1000);

  long rawBeban = bacaRawStabil();
  long selisihRaw = rawBeban - zero_offset;

  if (abs(selisihRaw) < 100) {
    Serial.println("[KALIBRASI] ERROR: Perubahan raw terlalu kecil! Pastikan benda sudah diletakkan.\n");
    return;
  }

  CALIBRATION_FACTOR = (float)selisihRaw / knownWeight;

  Serial.println("--------------------------------------------------");
  Serial.printf("Raw Kosong (Offset) : %ld\n", zero_offset);
  Serial.printf("Raw Dengan Beban    : %ld\n", rawBeban);
  Serial.printf("Berat Referensi     : %.1f g\n", knownWeight);
  Serial.printf("CALIBRATION_FACTOR  : %.4f\n", CALIBRATION_FACTOR);
  Serial.printf("Hasil Cek Timbang   : %.1f g\n", getWeightGram());
  Serial.println("--------------------------------------------------");
  Serial.println("SALIN baris berikut ke kode Anda agar tersimpan permanen:");
  Serial.printf("float CALIBRATION_FACTOR = %.4f;\n", CALIBRATION_FACTOR);
  Serial.println("==================================================\n");
}

// ================== TIMESTAMP HELPER ==================
String getTimestamp() {
  if (!ntpSynced) return "";
  struct tm t;
  if (!getLocalTime(&t)) return "";
  char buf[30];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S+07:00", &t);
  return String(buf);
}

// ================== MQTT PUBLISH ==================
void mqttPublish(const char* topic, const char* payload, bool retain = false) {
  if (mqtt.connected()) {
    mqtt.publish(topic, payload, retain);
  }
}

void publishWeighing() {
  JsonDocument doc;
  doc["rfid_uid"] = currentUID;
  doc["weight"] = currentWeightG;
  doc["conveyor_speed"] = rpmFiltered;
  String ts = getTimestamp();
  if (ts.length() > 0) doc["timestamp"] = ts;

  char buf[256];
  serializeJson(doc, buf, sizeof(buf));
  mqttPublish("checkweigher/weighing", buf);
  Serial.printf("[MQTT] Publish weighing: %s\n", buf);
}

void publishConveyorRPM() {
  JsonDocument doc;
  doc["actual_rpm"] = rpmFiltered;
  doc["pwm_value"] = pwmOut;
  String ts = getTimestamp();
  if (ts.length() > 0) doc["timestamp"] = ts;

  char buf[200];
  serializeJson(doc, buf, sizeof(buf));
  mqttPublish("checkweigher/conveyor/rpm", buf);
}

void publishDeviceStatus() {
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  JsonObject d1 = arr.add<JsonObject>();
  d1["device_name"] = "LoadCell HX711";
  d1["device_type"] = "loadcell";
  d1["status"] = scale.is_ready() ? "ONLINE" : "OFFLINE";
  char wBuf[16];
  snprintf(wBuf, sizeof(wBuf), "%.1f g", currentWeightG);
  d1["last_value"] = wBuf;

  JsonObject d2 = arr.add<JsonObject>();
  d2["device_name"] = "Rotary Encoder";
  d2["device_type"] = "encoder";
  d2["status"] = "ONLINE";
  char rBuf[16];
  snprintf(rBuf, sizeof(rBuf), "%.1f RPM", rpmFiltered);
  d2["last_value"] = rBuf;

  JsonObject d3 = arr.add<JsonObject>();
  d3["device_name"] = "Servo SG90";
  d3["device_type"] = "servo";
  d3["status"] = "ONLINE";
  char sBuf[16];
  snprintf(sBuf, sizeof(sBuf), "%d deg", sortingServo.read());
  d3["last_value"] = sBuf;

  JsonObject d4 = arr.add<JsonObject>();
  d4["device_name"] = "IR Sensor";
  d4["device_type"] = "infrared";
  d4["status"] = "ONLINE";
  d4["last_value"] = irDetected() ? "DETECTED" : "IDLE";

  JsonObject d5 = arr.add<JsonObject>();
  d5["device_name"] = "DC Motor";
  d5["device_type"] = "motor";
  d5["status"] = "ONLINE";
  char mBuf[16];
  snprintf(mBuf, sizeof(mBuf), "PWM: %d", pwmOut);
  d5["last_value"] = mBuf;

  JsonObject d6 = arr.add<JsonObject>();
  d6["device_name"] = "RFID Reader";
  d6["device_type"] = "rfid";
  d6["status"] = "ONLINE";
  d6["last_value"] = currentUID.length() > 0 ? currentUID.c_str() : "IDLE";

  char out[800];
  serializeJson(doc, out, sizeof(out));
  mqttPublish("checkweigher/device/status", out);
}

// ================== MQTT CALLBACK ==================
void mqttCallback(char* topicName, byte* payload, unsigned int len) {
  char msg[256];
  unsigned int n = len < sizeof(msg) - 1 ? len : sizeof(msg) - 1;
  memcpy(msg, payload, n);
  msg[n] = '\0';

  String t = String(topicName);
  Serial.printf("[MQTT] Pesan masuk topik=%s payload=%s\n", topicName, msg);

  if (t == "checkweigher/command") {
    JsonDocument doc;
    if (deserializeJson(doc, msg) != DeserializationError::Ok) return;

    String cmd = doc["command"].as<String>();
    cmd.toUpperCase();

    if (cmd == "TARE") {
      tareRequested = true;
      Serial.println("[CMD] TARE diterima dari dashboard");
    } else if (cmd == "START") {
      Serial.println("[CMD] START diterima dari dashboard");
      startConveyor();
    } else if (cmd == "STOP") {
      Serial.println("[CMD] STOP diterima dari dashboard");
      resetCycle();
    }
  }
  else if (t == "checkweigher/config/threshold") {
    JsonDocument doc;
    if (deserializeJson(doc, msg) != DeserializationError::Ok) return;

    if (!doc["tolerance_min"].isNull()) toleranceMin = doc["tolerance_min"].as<float>();
    if (!doc["tolerance_max"].isNull()) toleranceMax = doc["tolerance_max"].as<float>();
    Serial.printf("[CMD] Threshold update dari web: min=%.1f max=%.1f\n", toleranceMin, toleranceMax);
    publishDeviceStatus();
  }
  else if (t == "checkweigher/config/setpoint") {
    JsonDocument doc;
    if (deserializeJson(doc, msg) != DeserializationError::Ok) return;

    if (!doc["setpoint_rpm"].isNull()) {
      setpointRPM = constrain(doc["setpoint_rpm"].as<float>(), 0.0, MAX_RPM_LIMIT);
      Serial.printf("[CMD] Setpoint RPM update dari web: %.2f\n", setpointRPM);
      publishDeviceStatus();
    }
  }
}

// ================== MQTT CONNECT ==================
void connectMqtt() {
  String clientId = "esp32-checkweigher-" + String((uint32_t)ESP.getEfuseMac(), HEX);

  Serial.print("[MQTT] Menghubungkan ke broker... ");
  if (mqtt.connect(clientId.c_str())) {
    Serial.println("terhubung.");

    mqtt.subscribe("checkweigher/command");
    mqtt.subscribe("checkweigher/config/threshold");
    mqtt.subscribe("checkweigher/config/setpoint");

    Serial.println("[MQTT] Subscribe: command, config/threshold, config/setpoint");
    publishDeviceStatus();
  } else {
    Serial.printf("gagal, rc=%d\n", mqtt.state());
  }
}

void mqttLoop() {
  static unsigned long lastTry = 0;
  if (WiFi.status() != WL_CONNECTED) return;

  if (!mqtt.connected()) {
    if (millis() - lastTry > 5000) {
      lastTry = millis();
      connectMqtt();
    }
  } else {
    mqtt.loop();
  }
}

void telemetryLoop() {
  static unsigned long lastRPM = 0;
  unsigned long rpmInterval = conveyorRunning ? 1000 : 5000;
  if (millis() - lastRPM >= rpmInterval) {
    lastRPM = millis();
    publishConveyorRPM();
  }

  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    publishDeviceStatus();
  }
}

// ================== INTERRUPT ENCODER ==================
void IRAM_ATTR handleEncoder() {
  if (digitalRead(ENC_B) > 0) encoderValue++;
  else encoderValue--;
}

// ================== FUNGSI BANTU ==================
void changeState(State s) {
  state = s;
  stateStart = millis();
  Serial.printf("[STATE] -> %s\n", STATE_NAMES[s]);
}

bool irDetected() {
  return digitalRead(PIN_IR_SENSOR) == IR_ACTIVE_STATE;
}

void startConveyor() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  pidIntegral = PWM_START;
  pidPrevMeas = rpmFiltered;
  pwmOut = (int)PWM_START;
  ledcWrite(ENA, pwmOut);
  conveyorRunning = true;
}

void stopConveyor() {
  ledcWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  pwmOut = 0;
  pidIntegral = 0;
  conveyorRunning = false;
}

String determineCategory(float weight) {
  if (weight < toleranceMin) return "RINGAN";
  if (weight > toleranceMax) return "ERROR";
  return "BERAT";
}

// ================== PID KECEPATAN ==================
void updatePID() {
  static unsigned long lastT = 0;
  static long lastPulses = 0;

  unsigned long now = millis();
  if (now - lastT < PID_INTERVAL_MS) return;
  float dt = (now - lastT) / 1000.0;
  lastT = now;

  noInterrupts();
  long pulses = encoderValue;
  interrupts();
  long delta = pulses - lastPulses;
  lastPulses = pulses;

  float rpmRaw = ((float)abs(delta) / PULSES_PER_SHAFT_REV) / dt * 60.0;
  rpmFiltered = RPM_FILTER_ALPHA * rpmRaw + (1.0 - RPM_FILTER_ALPHA) * rpmFiltered;

  if (!conveyorRunning) return;

  float error = setpointRPM - rpmFiltered;
  float pTerm = Kp * error;

  pidIntegral += Ki * error * dt;
  pidIntegral = constrain(pidIntegral, 0.0, (float)PWM_MAX);

  float dTerm = -Kd * (rpmFiltered - pidPrevMeas) / dt;
  pidPrevMeas = rpmFiltered;

  float out = pTerm + pidIntegral + dTerm;
  pwmOut = (int)constrain(out, 0.0, (float)PWM_MAX);

  ledcWrite(ENA, pwmOut);
}

void resetCycle() {
  stopConveyor();
  sortingServo.write(SERVO_STRAIGHT);
  currentUID = "";
  currentWeightG = 0;
  currentCategory = "";
  weightSum = 0;
  weightCount = 0;
  irClearSeen = false;
  changeState(WAIT_RFID);
  Serial.println("\n[INFO] Siap! Silakan scan RFID barang (atau ketik 'k' kalibrasi / 't' tare).");
}

void handleSerialCommand() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("t")) {
      tareRequested = true;
    } else if (input.equalsIgnoreCase("k")) {
      runInteractiveCalibration();
      resetCycle();
    } else if (input.equalsIgnoreCase("w")) {
      // Cek cepat pembacaan raw & gram saat ini via Serial Monitor
      long r = bacaRawStabil();
      float g = getWeightGram();
      Serial.printf("[CEK] Raw: %ld | Berat: %.1f g | CalFactor: %.4f\n", r, g, CALIBRATION_FACTOR);
    }
  }
}

// ================== SETUP ==================
void setup() {
  Serial.begin(115200);
  while (!Serial);

  ESP32PWM::allocateTimer(0);
  sortingServo.setPeriodHertz(50);
  sortingServo.attach(PIN_SERVO, 500, 2400);
  sortingServo.write(SERVO_STRAIGHT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  ledcAttachChannel(ENA, freq, resolution, 8);
  stopConveyor();

  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), handleEncoder, CHANGE);

  pinMode(PIN_IR_SENSOR, INPUT);

  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  rfid.PCD_Init();

  scale.begin(PIN_HX711_DT, PIN_HX711_SCK);
  while (!scale.is_ready()) {
    Serial.println("[LOADCELL] Menunggu modul HX711 siap...");
    delay(500);
  }
  Serial.println("[LOADCELL] Pastikan timbangan KOSONG saat boot untuk tare (nol)...");
  prosesTare();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
  Serial.print("[WIFI] Menghubungkan");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
    delay(250);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(" terhubung, IP: ");
    Serial.println(WiFi.localIP());

    configTime(GMT_OFFSET, 0, NTP_SERVER);
    struct tm t;
    if (getLocalTime(&t, 5000)) {
      ntpSynced = true;
      Serial.println("[NTP] Waktu tersinkronisasi.");
    } else {
      Serial.println("[NTP] Gagal sinkronisasi.");
    }
  } else {
    Serial.println(" gagal (lanjut tanpa MQTT).");
  }

  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(1024);
  mqtt.setSocketTimeout(3);

  Serial.println("==================================================");
  Serial.println(" Sistem Checkweigher IoT (Anti-Spike Filter)");
  Serial.println(" - Ketik 'k' + Enter : Kalibrasi berat barang");
  Serial.println(" - Ketik 't' + Enter : Tare (nol-kan) ulang");
  Serial.println(" - Ketik 'w' + Enter : Cek Raw ADC & Berat saat ini");
  Serial.println("==================================================");

  resetCycle();
}

// ================== LOOP ==================
void loop() {
  mqttLoop();
  telemetryLoop();
  updatePID();
  handleSerialCommand();

  if (tareRequested) {
    tareRequested = false;
    prosesTare();
    publishDeviceStatus();
  }

  switch (state) {

    case WAIT_RFID: {
      if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        currentUID = "";
        for (byte i = 0; i < rfid.uid.size; i++) {
          if (rfid.uid.uidByte[i] < 0x10) currentUID += "0";
          currentUID += String(rfid.uid.uidByte[i], HEX);
        }
        currentUID.toUpperCase();
        rfid.PICC_HaltA();
        rfid.PCD_StopCrypto1();

        Serial.print("\n[RFID] Barang terdaftar. UID: ");
        Serial.println(currentUID);

        if (scale.is_ready() && fabs(getWeightGram()) > MIN_ITEM_G) {
          Serial.println("[LOADCELL] Barang sudah terdeteksi di timbangan, langsung menimbang...");
          weightSum = 0;
          weightCount = 0;
          changeState(WEIGHING);
        } else {
          Serial.println("[INFO] Silakan letakkan barang di loadcell...");
          changeState(WAIT_ITEM_ON_SCALE);
        }
      }
      break;
    }

    case WAIT_ITEM_ON_SCALE: {
      static unsigned long lastDbg = 0;

      if (scale.is_ready()) {
        float w = fabs(getWeightGram());

        if (millis() - lastDbg > 500) {
          lastDbg = millis();
          Serial.printf("[DEBUG] Berat terbaca: %.1f g\n", w);
        }

        if (w > MIN_ITEM_G) {
          Serial.println("[LOADCELL] Barang terdeteksi, menunggu stabil...");
          weightSum = 0;
          weightCount = 0;
          changeState(WEIGHING);
          break;
        }
      } else if (millis() - lastDbg > 500) {
        lastDbg = millis();
        Serial.println("[DEBUG] HX711 belum ready, cek wiring DT/SCK");
      }

      if (millis() - stateStart > TIMEOUT_ITEM_ON_SCALE) {
        Serial.println("[WARNING] Barang tidak diletakkan di loadcell (timeout).");
        resetCycle();
      }
      break;
    }

    case WEIGHING: {
      if (millis() - stateStart < SETTLE_MS) break;

      if (scale.is_ready()) {
        weightSum += fabs(getWeightGram());
        weightCount++;
      }

      if (weightCount >= WEIGHT_SAMPLES) {
        currentWeightG = weightSum / weightCount;

        if (currentWeightG < MIN_ITEM_G) {
          Serial.println("[WARNING] Barang diangkat saat penimbangan. Ulangi.");
          resetCycle();
          break;
        }

        currentCategory = determineCategory(currentWeightG);

        Serial.printf("[LOADCELL] Berat: %.1f g -> Kategori: %s\n",
                      currentWeightG, currentCategory.c_str());

        publishWeighing();
        publishDeviceStatus();

        if (currentCategory == "ERROR") {
          Serial.println("[ERROR] Berat melebihi toleransi maksimum! Konveyor TIDAK dijalankan.");
          Serial.println("[INFO] Angkat barang, lalu siklus akan reset.");
          resetCycle();
          break;
        }

        Serial.println("[CONVEYOR] Conveyor dijalankan...");
        startConveyor();
        changeState(CONVEYOR_RUN);
      }
      break;
    }

    case CONVEYOR_RUN: {
      if (irDetected()) {
        Serial.println("[IR] Barang tiba di tempat sortir.");
        changeState(SORTING);
        break;
      }
      if (millis() - stateStart > TIMEOUT_TRAVEL) {
        Serial.println("[WARNING] Barang tidak sampai ke sensor IR (timeout).");
        resetCycle();
      }
      break;
    }

    case SORTING: {
      if (currentCategory == "RINGAN") {
        Serial.println("[SERVO] RINGAN -> servo 90 derajat (belok ke wadah sortir).");
        sortingServo.write(SERVO_DIVERT);
      } else {
        Serial.println("[SERVO] BERAT -> servo tetap (jalur lurus).");
        sortingServo.write(SERVO_STRAIGHT);
      }
      irClearSeen = false;
      changeState(WAIT_CLEAR);
      break;
    }

    case WAIT_CLEAR: {
      if (!irDetected()) {
        if (!irClearSeen) {
          irClearSeen = true;
          irClearTime = millis();
        }
        if (millis() - irClearTime >= CLEAR_DELAY_MS &&
            millis() - stateStart >= SERVO_MIN_HOLD_MS) {
          Serial.println("[INFO] Barang sudah melewati tempat sortir.");
          resetCycle();
        }
      } else {
        irClearSeen = false;
      }

      if (millis() - stateStart > TIMEOUT_CLEAR) {
        Serial.println("[INFO] Sensor IR masih aktif terlalu lama, siklus diakhiri paksa.");
        resetCycle();
      }
      break;
    }
  }
}