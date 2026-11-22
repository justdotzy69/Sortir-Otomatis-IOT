package main

import (
	"log"
	"os"

	"checkweigher/app/services"
	"checkweigher/bootstrap"
	"checkweigher/config"
	"checkweigher/database/migrations"
	"checkweigher/database/seeders"
	"checkweigher/routes"

	"github.com/gofiber/fiber/v3"
)

func main() {
	config.LoadEnv()
	config.ConnectDatabase()

	// Perintah CLI ala "php artisan":
	// go run . migrate -> buat tabel
	// go run . migrate:fresh -> hapus semua tabel lalu buat ulang
	// go run . seed -> isi data contoh
	if len(os.Args) > 1 {
		switch os.Args[1] {
		case "migrate":
			migrations.Migrate(config.DB)
		case "migrate:fresh":
			migrations.Fresh(config.DB)
		case "seed":
			seeders.Run(config.DB)
		default:
			log.Fatalf("Perintah tidak dikenal: %s (gunakan: migrate | migrate:fresh | seed)", os.Args[1])
		}
		return
	}

	// 1. Jalankan WebSocket Hub di background
	go services.Hub.Run()

	// 2. Hubungkan ke Broker HiveMQ (Public / Cloud)
	mqttClient, _ := config.ConnectMQTT()
	services.InitMQTT(mqttClient)

	// 3. Jalankan Web Server Fiber
	app := bootstrap.NewApp()
	routes.Web(app)

	// 404 fallback untuk route yang tidak terdaftar (harus paling akhir)
	app.Use(func(c fiber.Ctx) error { return fiber.ErrNotFound })

	port := config.Env("APP_PORT", "3000")
	log.Println("[SERVER] Checkweigher Fullstack MVC berjalan di http://localhost:" + port)
	log.Fatal(app.Listen(":" + port))
}

