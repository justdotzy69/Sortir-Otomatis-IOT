package controllers

import (
	"strings"

	"checkweigher/app/helpers"
	"checkweigher/app/models"
	"checkweigher/app/requests"
	"checkweigher/app/services"
	"checkweigher/config"

	"github.com/gofiber/fiber/v3"
)

type SettingController struct{}

// Index GET /settings — menampilkan form konfigurasi threshold & status perangkat.
func (ctl *SettingController) Index(c fiber.Ctx) error {
	var product models.Product
	if err := config.DB.Where("is_active = ?", true).First(&product).Error; err != nil {
		product = models.Product{
			Name:         "Default Config",
			TargetWeight: 300.0,
			ToleranceMin: 300.0,
			ToleranceMax: 1000.0,
			SetpointRPM:  60.0,
			IsActive:     true,
		}
	}

	var devices []models.DeviceStatus
	config.DB.Order("id asc").Find(&devices)

	return helpers.View(c, "settings/index", fiber.Map{
		"Title":   "Pengaturan",
		"Product": product,
		"Devices": devices,
		"Form": requests.ProductRequest{
			Name:         product.Name,
			TargetWeight: product.TargetWeight,
			ToleranceMin: product.ToleranceMin,
			ToleranceMax: product.ToleranceMax,
			SetpointRPM:  product.SetpointRPM,
		},
	})
}

// Update POST/PUT /settings — simpan perubahan threshold & publish ke ESP32.
func (ctl *SettingController) Update(c fiber.Ctx) error {
	var req requests.ProductRequest
	if err := c.Bind().Form(&req); err != nil {
		return c.Redirect().With("error", "Format input tidak valid.").Back("/settings")
	}

	req.Name = strings.TrimSpace(req.Name)
	errs := requests.Validate(req)

	if req.ToleranceMin >= req.ToleranceMax {
		errs["tolerance_min"] = "Batas ringan (min) harus lebih kecil dari batas error (max)."
	}

	if len(errs) > 0 {
		var devices []models.DeviceStatus
		config.DB.Order("id asc").Find(&devices)

		c.Status(fiber.StatusUnprocessableEntity)
		return helpers.View(c, "settings/index", fiber.Map{
			"Title":   "Pengaturan",
			"Devices": devices,
			"Form":    req,
			"Errors":  errs,
		})
	}

	var product models.Product
	config.DB.Where("is_active = ?", true).First(&product)

	product.Name = req.Name
	product.TargetWeight = req.TargetWeight
	product.ToleranceMin = req.ToleranceMin
	product.ToleranceMax = req.ToleranceMax
	product.SetpointRPM = req.SetpointRPM
	product.IsActive = true

	config.DB.Save(&product)

	// Kirim konfigurasi baru ke ESP32 via MQTT
	if services.MQTT != nil {
		_ = services.MQTT.SendThreshold(product.ToleranceMin, product.ToleranceMax)
		_ = services.MQTT.SendSetpoint(product.SetpointRPM)
	}

	return c.Redirect().With("success", "Pengaturan berhasil disimpan dan dikirimkan ke ESP32.").To("/settings")
}

