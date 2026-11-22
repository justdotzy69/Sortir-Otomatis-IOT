package helpers

import (
	"checkweigher/config"

	"github.com/gofiber/fiber/v3"
	"github.com/gofiber/fiber/v3/middleware/csrf"
)

// View merender template dengan data bersama (shared data) yang selalu tersedia di semua halaman.
func View(c fiber.Ctx, name string, data fiber.Map) error {
	if data == nil {
		data = fiber.Map{}
	}
	data["AppName"] = config.Env("APP_NAME", "Checkweigher IoT")
	data["CsrfToken"] = csrf.TokenFromContext(c)
	data["CurrentPath"] = c.Path()
	if _, ok := data["Errors"]; !ok {
		data["Errors"] = map[string]string{}
	}
	// Flash message dari redirect sebelumnya
	if msg := c.Redirect().Message("success"); msg.Value != "" {
		data["FlashSuccess"] = msg.Value
	}
	if msg := c.Redirect().Message("error"); msg.Value != "" {
		data["FlashError"] = msg.Value
	}
	return c.Render(name, data)
}

