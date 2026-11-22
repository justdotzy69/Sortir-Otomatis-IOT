package controllers

import (
	"encoding/json"
	"time"

	"checkweigher/app/helpers"
	"checkweigher/app/models"
	"checkweigher/config"

	"github.com/gofiber/fiber/v3"
)

type DashboardController struct{}

// Index menampilkan halaman utama Dashboard Checkweigher sesuai desain referensi.
func (ctl *DashboardController) Index(c fiber.Ctx) error {
	// 1. Ambil data penimbangan terakhir
	var latestWeighing models.Weighing
	config.DB.Preload("Item").Order("measured_at desc").First(&latestWeighing)

	// 2. Ambil data statistik hari ini
	todayStr := time.Now().Format("2006-01-02")
	var todayStats models.DailyStats
	config.DB.Where("stat_date = ?", todayStr).First(&todayStats)

	// 3. Ambil data status perangkat
	var devices []models.DeviceStatus
	config.DB.Find(&devices)

	// Cek status sistem & ESP32
	allOnline := len(devices) > 0
	esp32Online := false
	for _, d := range devices {
		if d.Status != "ONLINE" {
			allOnline = false
		}
		if d.DeviceType == "loadcell" && d.Status == "ONLINE" {
			esp32Online = true
		}
	}

	// 4. Ambil 30 log penimbangan terakhir untuk grafik berat
	var recentWeighings []models.Weighing
	config.DB.Order("measured_at desc").Limit(30).Find(&recentWeighings)
	if recentWeighings == nil {
		recentWeighings = []models.Weighing{}
	}

	// 5. Ambil 30 log telemetri RPM terakhir untuk grafik PID
	var recentConveyorLogs []models.ConveyorLog
	config.DB.Order("recorded_at desc").Limit(30).Find(&recentConveyorLogs)
	if recentConveyorLogs == nil {
		recentConveyorLogs = []models.ConveyorLog{}
	}

	// 6. Ambil konfigurasi aktif (threshold & setpoint)
	var activeProduct models.Product
	config.DB.Where("is_active = ?", true).First(&activeProduct)
	if activeProduct.ID == 0 {
		activeProduct = models.Product{
			Name:         "Default Config",
			TargetWeight: 300.0,
			ToleranceMin: 300.0,
			ToleranceMax: 1000.0,
			SetpointRPM:  60.0,
			IsActive:     true,
		}
	}

	// Serialize data chart untuk inisialisasi awal di browser menggunakan template.JS agar tidak di-escape
	weightChartJSON, _ := json.Marshal(recentWeighings)
	rpmChartJSON, _ := json.Marshal(recentConveyorLogs)

	return helpers.View(c, "dashboard/index", fiber.Map{
		"Title":           "Dashboard",
		"LatestWeighing":  latestWeighing,
		"TodayStats":      todayStats,
		"Devices":         devices,
		"AllOnline":       allOnline,
		"Esp32Online":     esp32Online,
		"ActiveProduct":   activeProduct,
		"WeightChartJSON": string(weightChartJSON),
		"RPMChartJSON":    string(rpmChartJSON),
		"BrokerConnected": config.MQTTClient != nil && config.MQTTClient.IsConnected(),
	})
}

