package config

import (
	"fmt"
	"log"

	"gorm.io/driver/mysql"
	"gorm.io/gorm"
	"gorm.io/gorm/logger"
)

// DB adalah koneksi database global (mirip facade DB di Laravel).
var DB *gorm.DB

// ConnectDatabase membuka koneksi ke MySQL berdasarkan nilai di .env.
func ConnectDatabase() {
	dsn := fmt.Sprintf("%s:%s@tcp(%s:%s)/%s?charset=utf8mb4&parseTime=True&loc=Local",
		Env("DB_USERNAME", "root"),
		Env("DB_PASSWORD", ""),
		Env("DB_HOST", "127.0.0.1"),
		Env("DB_PORT", "3306"),
		Env("DB_DATABASE", "checkweigher"),
	)

	logLevel := logger.Silent
	if Env("APP_ENV", "local") == "local" {
		logLevel = logger.Info // tampilkan query SQL di terminal saat development
	}

	db, err := gorm.Open(mysql.Open(dsn), &gorm.Config{
		Logger: logger.Default.LogMode(logLevel),
	})
	if err != nil {
		log.Fatalf("Gagal terhubung ke database: %v", err)
	}

	DB = db
	log.Println("Berhasil terhubung ke database MySQL")
}

