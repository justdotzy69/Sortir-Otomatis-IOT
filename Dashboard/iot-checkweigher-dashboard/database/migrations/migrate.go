package migrations

import (
	"log"

	"checkweigher/app/models"
	"gorm.io/gorm"
)

// Migrate membuat / memperbarui tabel sesuai struct model GORM (mirip "php artisan migrate").
func Migrate(db *gorm.DB) {
	err := db.AutoMigrate(
		&models.Item{},
		&models.Product{},
		&models.Weighing{},
		&models.ConveyorLog{},
		&models.DeviceStatus{},
		&models.DailyStats{},
	)
	if err != nil {
		log.Fatalf("Migrasi gagal: %v", err)
	}
	log.Println("Migrasi tabel database selesai")
}

// Fresh menghapus semua tabel lalu migrasi ulang (mirip "php artisan migrate:fresh").
func Fresh(db *gorm.DB) {
	db.Exec("SET FOREIGN_KEY_CHECKS = 0")
	if err := db.Migrator().DropTable(
		&models.Weighing{},
		&models.ConveyorLog{},
		&models.DailyStats{},
		&models.DeviceStatus{},
		&models.Product{},
		&models.Item{},
	); err != nil {
		log.Fatalf("Gagal menghapus tabel: %v", err)
	}
	db.Exec("SET FOREIGN_KEY_CHECKS = 1")
	log.Println("Semua tabel berhasil dihapus")
	Migrate(db)
}

