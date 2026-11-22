package config

import (
	"log"
	"os"

	"github.com/joho/godotenv"
)

// LoadEnv membaca file .env (mirip Laravel yang membaca .env saat boot).
func LoadEnv() {
	if err := godotenv.Load(); err != nil {
		log.Println("File .env tidak ditemukan, memakai environment variable sistem")
	}
}

// Env mengambil nilai environment variable, jika kosong pakai nilai default.
// Mirip helper env('KEY', 'default') di Laravel.
func Env(key, fallback string) string {
	if value, ok := os.LookupEnv(key); ok && value != "" {
		return value
	}
	return fallback
}

