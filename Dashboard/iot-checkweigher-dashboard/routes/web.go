package routes

import (
	"checkweigher/app/controllers"
	"checkweigher/app/services"

	"github.com/gofiber/contrib/v3/websocket"
	"github.com/gofiber/fiber/v3"
)

// Web mendaftarkan semua route aplikasi (mirip routes/web.php di Laravel).
func Web(app *fiber.App) {
	dashboard := &controllers.DashboardController{}
	item := &controllers.ItemController{}
	history := &controllers.HistoryController{}
	setting := &controllers.SettingController{}
	command := &controllers.CommandController{}

	// 1. Dashboard Utama
	app.Get("/", dashboard.Index).Name("dashboard")

	// 2. Resource route: Daftar Barang (RFID)
	app.Get("/items", item.Index).Name("items.index")
	app.Get("/items/create", item.Create).Name("items.create")
	app.Post("/items", item.Store).Name("items.store")
	app.Get("/items/:id/edit", item.Edit).Name("items.edit")
	app.Put("/items/:id", item.Update).Name("items.update")
	app.Delete("/items/:id", item.Destroy).Name("items.destroy")

	// 3. Riwayat Penimbangan & Export CSV
	app.Get("/history", history.Index).Name("history.index")
	app.Get("/history/export", history.ExportCSV).Name("history.export")

	// 4. Pengaturan (Threshold & Setpoint)
	app.Get("/settings", setting.Index).Name("settings.index")
	app.Post("/settings", setting.Update).Name("settings.update")

	// 5. API Kontrol (Start, Stop, Tare dari panel web)
	app.Post("/api/command", command.Execute).Name("command.execute")

	// 6. WebSocket untuk Push Data Real-time ke Dashboard
	app.Get("/ws", websocket.New(func(c *websocket.Conn) {
		services.Hub.Handle(c)
	}))
}

