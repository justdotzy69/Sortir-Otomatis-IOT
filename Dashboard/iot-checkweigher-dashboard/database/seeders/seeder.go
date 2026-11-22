package seeders

import (
	"log"
	"time"

	"checkweigher/app/models"
	"gorm.io/gorm"
)

// Run mengisi data awal (seeder) ke database (mirip "php artisan db:seed").
func Run(db *gorm.DB) {
	// 1. Seed Default Product Config
	product := models.Product{
		Name:         "Default Checkweigher Config",
		TargetWeight: 300.0,
		ToleranceMin: 300.0,
		ToleranceMax: 1000.0,
		SetpointRPM:  60.0,
		IsActive:     true,
	}
	db.Where(models.Product{Name: product.Name}).FirstOrCreate(&product)

	// 2. Seed Device Status
	devices := []models.DeviceStatus{
		{DeviceName: "LoadCell HX711", DeviceType: "loadcell", Status: "OFFLINE", LastValue: "0.0 g"},
		{DeviceName: "Rotary Encoder", DeviceType: "encoder", Status: "OFFLINE", LastValue: "0.0 RPM"},
		{DeviceName: "Servo SG90", DeviceType: "servo", Status: "OFFLINE", LastValue: "0 deg"},
		{DeviceName: "IR Sensor", DeviceType: "infrared", Status: "OFFLINE", LastValue: "IDLE"},
		{DeviceName: "DC Motor", DeviceType: "motor", Status: "OFFLINE", LastValue: "PWM: 0"},
		{DeviceName: "RFID Reader", DeviceType: "rfid", Status: "OFFLINE", LastValue: "IDLE"},
	}
	for _, dev := range devices {
		db.Where(models.DeviceStatus{DeviceName: dev.DeviceName}).FirstOrCreate(&dev)
	}

	// 3. Seed Sample Registered Items
	items := []models.Item{
		{ItemCode: "BRG-01", RfidUID: "A1B2C3D4", Name: "Kardus Sampel A", Description: "Barang uji coba bobot ringan"},
		{ItemCode: "BRG-02", RfidUID: "E5F6A7B8", Name: "Kardus Sampel B", Description: "Barang uji coba bobot standar"},
		{ItemCode: "BRG-03", RfidUID: "C9D0E1F2", Name: "Kardus Sampel C", Description: "Barang uji coba bobot berat"},
	}
	for i := range items {
		db.Where(models.Item{ItemCode: items[i].ItemCode}).FirstOrCreate(&items[i])
	}

	// 4. Seed Sample Weighings
	now := time.Now()
	sampleWeighings := []models.Weighing{
		{ItemID: &items[0].ID, RfidUID: items[0].RfidUID, Weight: 245.0, Category: "RINGAN", ConveyorSpeed: 58.7, MeasuredAt: now.Add(-10 * time.Minute)},
		{ItemID: &items[1].ID, RfidUID: items[1].RfidUID, Weight: 450.5, Category: "BERAT", ConveyorSpeed: 59.8, MeasuredAt: now.Add(-5 * time.Minute)},
		{ItemID: &items[2].ID, RfidUID: items[2].RfidUID, Weight: 310.2, Category: "BERAT", ConveyorSpeed: 60.1, MeasuredAt: now.Add(-1 * time.Minute)},
	}
	for _, w := range sampleWeighings {
		db.Create(&w)
	}

	// 5. Seed Daily Stats for today
	todayStr := now.Format("2006-01-02")
	stats := models.DailyStats{
		StatDate:     todayStr,
		TotalWeighed: 3,
		TotalRingan:  1,
		TotalBerat:   2,
		TotalError:   0,
		AvgWeight:    335.2,
		MinWeight:    245.0,
		MaxWeight:    450.5,
	}
	db.Where(models.DailyStats{StatDate: todayStr}).FirstOrCreate(&stats)

	log.Println("Seeder selesai: Konfigurasi default, status perangkat, dan sampel barang berhasil dimasukkan")
}

