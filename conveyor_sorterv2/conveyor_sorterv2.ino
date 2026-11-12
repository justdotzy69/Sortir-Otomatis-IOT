#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>

// --- KONFIGURASI PIN (BEBAS KONFLIK) ---
const int ENA   = 25;   // Pin PWM untuk kecepatan motor (L298N)
const int IN1   = 26;   // Kontrol arah 1 (L298N)
const int IN2   = 33;   // Kontrol arah 2 (L298N)

const int ENC_A = 4;    // Sinyal Encoder A (Pull-up internal aktif)
const int ENC_B = 32;   // Sinyal Encoder B (Pull-up internal aktif)

const int PIN_ITEM_SENSOR = 27; // Sensor IR/PIR pendeteksi barang (OUT)
const int PIN_SERVO       = 13; // Servo pemilah jalur

// Pin RFID RC522 (Standard Hardware SPI ESP32)
const int PIN_RFID_SS     = 5;
const int PIN_RFID_SCK    = 18;
const int PIN_RFID_MISO   = 19;
const int PIN_RFID_MOSI   = 23;
const int PIN_RFID_RST    = 22;

// --- VARIABEL ENCODER & SPESIFIKASI MOTOR JGY-370 ---
volatile long encoderValue = 0;   
long lastEncoderValue = 0;        

const int PULSES_PER_MOTOR_REV = 11; // Pulsa per putaran shaft motor
const float GEAR_RATIO = 90.0;     // Sesuaikan dengan rasio gearbox Anda
const float PULSES_PER_SHAFT_REV = PULSES_PER_MOTOR_REV * GEAR_RATIO; 

// --- OBJEK ---
MFRC522 rfid(PIN_RFID_SS, PIN_RFID_RST);
Servo sortingServo;

// Konfigurasi PWM ESP32 (Kompatibel Core v3.x)
const int freq = 30000;
const int resolution = 8;

// Posisi Sudut Servo
const int SERVO_STRAIGHT = 0;   // Jalur lurus (Posisi Awal)
const int SERVO_DIVERT   = 90;  // Jalur belok ke wadah sortir

bool itemProcessed = false;

// Fungsi Interrupt untuk membaca pulsa encoder (Channel A)
void IRAM_ATTR handleEncoder() {
  int b = digitalRead(ENC_B);
  if (b > 0) {
    encoderValue++;
  } else {
    encoderValue--;
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // 1. Konfigurasi Pin Motor
  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  // 2. Konfigurasi Pin Encoder (Pull-up Internal)
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_A), handleEncoder, CHANGE);

  // 3. Konfigurasi Sensor & Servo
  pinMode(PIN_ITEM_SENSOR, INPUT);
  sortingServo.attach(PIN_SERVO);
  sortingServo.write(SERVO_STRAIGHT); // Pastikan posisi awal lurus saat boot

  // 4. Inisialisasi RFID (SPI)
  SPI.begin(PIN_RFID_SCK, PIN_RFID_MISO, PIN_RFID_MOSI, PIN_RFID_SS);
  rfid.PCD_Init();

  // 5. Inisialisasi PWM Motor & Jalankan Konveyor Kontinu
  ledcAttach(ENA, freq, resolution);
  setConveyorSpeed(200, HIGH, LOW);

  Serial.println("==========================================");
  Serial.println(" Sistem Sortir & Konveyor Siap Berjalan!");
  Serial.println("==========================================");
}

void loop() {
  // Pantau performa putaran konveyor secara berkala di latar belakang
  monitorConveyorRPM();

  // Membaca status sensor barang (Asumsi HIGH saat barang terdeteksi)
  bool itemDetected = digitalRead(PIN_ITEM_SENSOR);

  // Ketika barang terdeteksi tiba di area sensor/pemindaian
  if (itemDetected && !itemProcessed) {
    Serial.println("\n[INFO] Barang terdeteksi di konveyor! Menunggu pembacaan RFID...");

    unsigned long startTime = millis();
    bool rfidReadSuccess = false;
    String targetUID = "";

    // Beri batas waktu (timeout) 2 detik untuk membaca stiker RFID
    while (millis() - startTime < 2000) {
      if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
        for (byte i = 0; i < rfid.uid.size; i++) {
          if (rfid.uid.uidByte[i] < 0x10) targetUID += "0";
          targetUID += String(rfid.uid.uidByte[i], HEX);
        }
        rfidReadSuccess = true;
        break;
      }
    }

    // --- LOGIKA UTAMA: APA PUN KARTU YANG DI-SCAN, SERVO LANGSUNG GERAK ---
    if (rfidReadSuccess) {
      Serial.print("-> RFID Terbaca! UID: ");
      Serial.println(targetUID);

      Serial.println("-> KEPUTUSAN: Belokkan ke Wadah (Divert)");
      sortingServo.write(SERVO_DIVERT);   // 1. Servo LANGSUNG GERAK ke wadah
      delay(1200);                        // 2. Durasi pendorongan barang
      sortingServo.write(SERVO_STRAIGHT); // 3. KEMBALI KE POSISI AWAL (Lurus)
    } 
    // Jika waktu habis (Timeout) dan RFID tidak terbaca
    else {
      Serial.println("[WARNING] RFID tidak terbaca (Timeout). Melewatkan jalur lurus.");
      sortingServo.write(SERVO_STRAIGHT);
      delay(800);
    }

    itemProcessed = true;
  }

  // Reset status ketika barang sudah lewat dan sensor kembali bebas (LOW)
  if (!itemDetected && itemProcessed) {
    itemProcessed = false;
    Serial.println("[INFO] Konveyor siap menerima barang berikutnya.\n");
  }
}

// Fungsi untuk mengatur kecepatan dan arah putaran conveyor
void setConveyorSpeed(int speed, int in1State, int in2State) {
  ledcWrite(ENA, speed);
  digitalWrite(IN1, in1State);
  digitalWrite(IN2, in2State);
}

// Fungsi untuk menghentikan conveyor
void stopConveyor() {
  ledcWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
}

// Fungsi untuk memantau RPM conveyor secara real-time setiap 1 detik
void monitorConveyorRPM() {
  static unsigned long lastPrint = 0;
  unsigned long currentMillis = millis();
  
  if (currentMillis - lastPrint >= 1000) {
    lastPrint = currentMillis;

    noInterrupts();
    long currentPulses = encoderValue;
    interrupts();

    long deltaPulses = currentPulses - lastEncoderValue;
    lastEncoderValue = currentPulses;

    float rpm = ((float)abs(deltaPulses) / PULSES_PER_SHAFT_REV) * 60.0;

    Serial.print("[Monitor] Total Pulsa: ");
    Serial.print(currentPulses);
    Serial.print(" | RPM Roller: ");
    Serial.println(rpm);
  }
}