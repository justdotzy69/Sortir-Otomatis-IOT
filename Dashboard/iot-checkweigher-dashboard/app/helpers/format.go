package helpers

import (
	"fmt"
	"time"
)

// Tanggal memformat waktu menjadi "12-11-2026 14:05".
func Tanggal(t time.Time) string {
	if t.IsZero() {
		return "-"
	}
	return t.Format("02-01-2006 15:04:05")
}

// FormatWeight memformat nilai berat float menjadi "245.5 g".
func FormatWeight(w float64) string {
	return fmt.Sprintf("%.1f g", w)
}

