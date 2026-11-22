# IoT Checkweigher Dashboard (Go Fiber MVC)

Sistem IoT monitoring dan kendali penimbangan barang (Checkweigher) secara real-time berbasis ESP32, Go Fiber v3, MySQL, HiveMQ Cloud MQTT, dan Tailwind CSS. Dibangun dengan arsitektur Monolithic Model-View-Controller (MVC) ala Laravel mengacu pada standar modul praktikum Pemrograman Web II Teknik Komputer Universitas Jenderal Soedirman.

---

## Fitur Utama

1. **Monitoring Real-time (Dashboard)**
   - Status Bar sistem: Status Sistem (AKTIF/NONAKTIF), Status ESP32 (Online/Offline), Status Broker (OK/Disconnected).
   - Info Barang Terakhir: ID Barang (BRG-XX dari lookup RFID), Berat (gram), Kategori (RINGAN/BERAT/ERROR), dan Kecepatan Konveyor (RPM).
   - Grafik Berat terhadap Waktu: Line chart Chart.js real-time dengan batas toleransi 300g dan 1000g.
   - Grafik RPM vs Setpoint & PWM: Pemantauan telemetri PID motor konveyor secara real-time.
   - Distribusi Kategori: Grafik batang jumlah penimbangan hari ini per kategori.
   - Panel Kontrol: Tombol kontrol konveyor (Start, Stop) dan Tare Loadcell via HTTP API & MQTT.

2. **Klasifikasi 3 Kategori Berat**
   - **RINGAN (< 300g)**: Motor servo aktif membelokkan barang ke jalur reject.
   - **BERAT (300g - 1000g)**: Bobot standar lolos, konveyor jalan lurus.
   - **ERROR (> 1000g)**: Peringatan kelebihan beban, konveyor otomatis berhenti.

3. **Integrasi RFID Reader & Manajemen Barang**
   - Setiap kemasan ditempel tag RFID UID fisik.
   - Pendaftaran barang baru otomatis meng-generate kode urut (`BRG-01`, `BRG-02`, dst.).
   - Dilengkapi validasi form, pagination, pencarian, ubah data, dan modal konfirmasi hapus.

4. **Kontrol PID Konveyor Berbasis Encoder**
   - Rotary Encoder mengukur kecepatan aktual motor (RPM).
   - ESP32 menjalankan algoritma PID untuk menjaga kecepatan konveyor sesuai setpoint target.
   - Nilai setpoint RPM dan batas toleransi threshold dapat disesuaikan langsung dari halaman Pengaturan dan disinkronkan ke ESP32.

5. **Riwayat Penimbangan & Export CSV**
   - Tabel catatan penimbangan dengan filter kategori (RINGAN, BERAT, ERROR), rentang tanggal mulai, dan tanggal selesai.
   - Tombol unduh laporan ke format spreadsheet CSV.

---

## Arsitektur Sistem

```
[ Hardware Layer (ESP32) ]
  - LoadCell + HX711 (Sensor Berat)
  - RFID RC522 (Identifikasi Barang)
  - Sensor Infrared (Deteksi Kehadiran Objek)
  - Rotary Encoder (Feedback Kecepatan PID)
  - DC Motor (Penggerak Konveyor)
  - Servo SG90 (Penyortir Jalur)
         |
         | (MQTT TLS - Port 8883)
         v
[ Message Broker: HiveMQ Cloud ]
         ^
         | (MQTT Subscriber & Publisher)
         v
[ Fullstack Monolith: Go Fiber MVC (Port 3000) ]
  - Model: GORM (Item, Product, Weighing, ConveyorLog, DeviceStatus, DailyStats)
  - View: HTML Template Engine (resources/views/) + Tailwind CSS CDN + Chart.js
  - Controller: DashboardController, ItemController, HistoryController, SettingController, CommandController
  - WebSocket Hub: Push telemetri real-time ke browser client (/ws)
  - MySQL Database: Port 3306
```

---

## Struktur Direktori

```
Checkweigher Dashboard/
|-- app/
|   |-- controllers/              # Controller MVC (Dashboard, Item, History, Setting, Command)
|   |-- helpers/                  # View helper (CSRF, Shared data), Pagination, Formatters
|   |-- middleware/               # MethodOverride (_method=PUT/DELETE)
|   |-- models/                   # GORM Struct (Item, Product, Weighing, ConveyorLog, DeviceStatus, DailyStats)
|   |-- requests/                 # Form validation request validator
|   |-- services/                 # WebSocket Hub & MQTT Ingestion / Publishing Service
|
|-- bootstrap/
|   |-- app.go                    # Konfigurasi Fiber v3, View Engine, CSRF, Recover, Logger
|
|-- config/
|   |-- config.go                 # Load .env
|   |-- database.go               # GORM MySQL Connection
|   |-- mqtt.go                   # Paho MQTT Client (HiveMQ TLS)
|
|-- database/
|   |-- migrations/               # AutoMigrate & Fresh table runner
|   |-- seeders/                  # Seeder awal default config, devices, sample items & weighings
|
|-- routes/
|   |-- web.go                    # Route web (Dashboard, Items CRUD, History, Settings, WS)
|
|-- resources/
|   |-- views/
|       |-- layouts/              # Master layout (app.html)
|       |-- partials/             # Navbar, Flash alert, Delete modal, Pagination
|       |-- dashboard/            # Dashboard utama (index.html)
|       |-- items/                # Index, Create, Edit, _form partial
|       |-- history/              # Riwayat penimbangan & filter
|       |-- settings/             # Konfigurasi threshold & status hardware
|       |-- errors/               # Halaman 404 & 500 error
|
|-- public/
|   |-- css/app.css               # Styling custom
|   |-- js/app.js                 # Delete modal handler
|
|-- esp32/
|   |-- checkweigher.ino          # Firmware ESP32 Arduino
|
|-- .env                          # Konfigurasi database MySQL & HiveMQ
|-- .env.example
|-- go.mod
|-- go.sum
|-- main.go                       # Entrypoint aplikasi & CLI migrate/seed
```

---

## Topik MQTT

| Topik | Arah | Fungsi |
|---|---|---|
| `checkweigher/weighing` | ESP32 -> Server | Hasil penimbangan (`rfid_uid`, `weight`, `conveyor_speed`) |
| `checkweigher/conveyor/rpm` | ESP32 -> Server | Telemetri PID (`actual_rpm`, `pwm_value`) setiap 500ms |
| `checkweigher/device/status` | ESP32 -> Server | Heartbeat status sensor/aktuator setiap 5 detik |
| `checkweigher/command` | Server -> ESP32 | Perintah kontrol: `START`, `STOP`, `TARE` |
| `checkweigher/config/threshold` | Server -> ESP32 | Sinkronisasi batas `tolerance_min` dan `tolerance_max` |
| `checkweigher/config/setpoint` | Server -> ESP32 | Sinkronisasi target `setpoint_rpm` konveyor |

---

## Panduan Instalasi & Menjalankan

### 1. Database MySQL

Pastikan MySQL (XAMPP / Laragon / MySQL Service) telah aktif di port 3306 dan database `checkweigher` telah dibuat:
```sql
CREATE DATABASE checkweigher;
```

### 2. Konfigurasi Environment (.env)

Periksa file `.env` di folder utama:
```ini
APP_NAME=Checkweigher IoT
APP_ENV=local
APP_PORT=3000

DB_HOST=127.0.0.1
DB_PORT=3306
DB_USER=root
DB_PASSWORD=753951aziz
DB_NAME=checkweigher

MQTT_BROKER=tcp://broker.hivemq.com:1883
MQTT_CLIENT_ID=checkweigher-server
MQTT_USERNAME=
MQTT_PASSWORD=
```

### 3. Migrasi & Seeder Database

Jalankan perintah migrasi dan pengisian data awal:
```bash
# Migrasi tabel (AutoMigrate GORM)
go run . migrate

# Atau reset tabel dan buat ulang dari nol
go run . migrate:fresh

# Masukkan data awal (profil pengaturan, daftar sensor, sampel barang, sampel riwayat)
go run . seed
```

*(Atau bisa juga langsung mengeksekusi binary `.\checkweigher.exe migrate` dan `.\checkweigher.exe seed`)*.

### 4. Menjalankan Web Server

Jalankan aplikasi utama:
```bash
go run .
```
atau:
```bash
.\checkweigher.exe
```

Aplikasi langsung siap diakses melalui browser di alamat:
`http://localhost:3000` (atau `http://127.0.0.1:3000`)

---

## Firmware ESP32

1. Buka file `esp32/checkweigher.ino` pada Arduino IDE.
2. Pasang library via Arduino Library Manager:
   - `PubSubClient` (oleh Nick O'Leary)
   - `ArduinoJson` (oleh Benoit Blanchon)
   - `HX711` (oleh Rob Tillaart atau Bogde)
   - `MFRC522` (oleh GithubCommunity)
   - `ESP32Servo` (oleh Kevin Harrington)
3. Sesuaikan konfigurasi WiFi (`WIFI_SSID`, `WIFI_PASS`) serta kredensial HiveMQ Cloud.
4. Upload program ke modul ESP32.
