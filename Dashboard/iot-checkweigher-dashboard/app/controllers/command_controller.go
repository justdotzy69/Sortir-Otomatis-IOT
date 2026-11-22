package controllers

import (
	"strings"

	"checkweigher/app/services"

	"github.com/gofiber/fiber/v3"
)

type CommandController struct{}

// Execute POST /api/command — mengirim perintah START, STOP, TARE ke ESP32.
func (ctl *CommandController) Execute(c fiber.Ctx) error {
	var body struct {
		Command string `json:"command" form:"command"`
	}

	if err := c.Bind().Body(&body); err != nil {
		body.Command = c.FormValue("command")
	}

	cmd := strings.ToUpper(strings.TrimSpace(body.Command))
	if cmd != "START" && cmd != "STOP" && cmd != "TARE" {
		return c.Status(fiber.StatusBadRequest).JSON(fiber.Map{
			"success": false,
			"error":   "Perintah tidak valid. Gunakan START, STOP, atau TARE.",
		})
	}

	if services.MQTT == nil {
		return c.Status(fiber.StatusServiceUnavailable).JSON(fiber.Map{
			"success": false,
			"error":   "Layanan MQTT belum terhubung.",
		})
	}

	if err := services.MQTT.SendCommand(cmd); err != nil {
		return c.Status(fiber.StatusInternalServerError).JSON(fiber.Map{
			"success": false,
			"error":   "Gagal mengirim perintah: " + err.Error(),
		})
	}

	return c.JSON(fiber.Map{
		"success": true,
		"message": "Perintah " + cmd + " berhasil dikirim ke ESP32.",
	})
}

