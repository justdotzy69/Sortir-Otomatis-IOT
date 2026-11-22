package models

import "time"

// Product mewakili tabel "products" (konfigurasi threshold toleransi berat dan kecepatan setpoint).
type Product struct {
	ID           uint      `gorm:"primaryKey" json:"id"`
	Name         string    `gorm:"size:100;not null" json:"name"`
	TargetWeight float64   `gorm:"type:float;default:300" json:"target_weight"`
	ToleranceMin float64   `gorm:"type:float;default:300" json:"tolerance_min"`
	ToleranceMax float64   `gorm:"type:float;default:1000" json:"tolerance_max"`
	SetpointRPM  float64   `gorm:"type:float;default:60" json:"setpoint_rpm"`
	IsActive     bool      `gorm:"default:true" json:"is_active"`
	CreatedAt    time.Time `json:"created_at"`
	UpdatedAt    time.Time `json:"updated_at"`
}

