package middleware

import (
	"strings"

	"github.com/gofiber/fiber/v3"
)

// MethodOverride membaca field tersembunyi "_method" pada form HTML (mirip @method di Laravel).
func MethodOverride() fiber.Handler {
	return func(c fiber.Ctx) error {
		if c.Method() == fiber.MethodPost {
			method := strings.ToUpper(c.FormValue("_method"))
			switch method {
			case fiber.MethodPut, fiber.MethodPatch, fiber.MethodDelete:
				c.Method(method)
				return c.RestartRouting()
			}
		}
		return c.Next()
	}
}

