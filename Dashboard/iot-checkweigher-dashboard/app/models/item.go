package models

import "time"

// Item mewakili tabel "items" (mapping RFID tag ke kode barang).
type Item struct {
	ID          uint       `gorm:"primaryKey" json:"id"`
	ItemCode    string     `gorm:"size:20;not null;uniqueIndex" json:"item_code"`
	RfidUID     string     `gorm:"size:50;not null;uniqueIndex" json:"rfid_uid"`
	Name        string     `gorm:"size:100;not null" json:"name"`
	Description string     `gorm:"type:text" json:"description"`
	Weighings   []Weighing `gorm:"foreignKey:ItemID;constraint:OnUpdate:CASCADE,OnDelete:SET NULL;" json:"weighings,omitempty"`
	CreatedAt   time.Time  `json:"created_at"`
	UpdatedAt   time.Time  `json:"updated_at"`
}

