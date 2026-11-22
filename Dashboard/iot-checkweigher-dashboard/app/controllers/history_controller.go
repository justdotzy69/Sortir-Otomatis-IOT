package controllers

import (
	"encoding/csv"
	"fmt"
	"strconv"
	"strings"
	"time"

	"checkweigher/app/helpers"
	"checkweigher/app/models"
	"checkweigher/config"

	"github.com/gofiber/fiber/v3"
)

type HistoryController struct{}

// Index GET /history — riwayat penimbangan dengan filter tanggal, kategori, dan pagination.
func (ctl *HistoryController) Index(c fiber.Ctx) error {
	category := strings.TrimSpace(c.Query("category"))
	startDate := strings.TrimSpace(c.Query("start_date"))
	endDate := strings.TrimSpace(c.Query("end_date"))
	page := fiber.Query[int](c, "page", 1)

	query := config.DB.Model(&models.Weighing{})

	if category != "" {
		query = query.Where("category = ?", category)
	}
	if startDate != "" {
		query = query.Where("measured_at >= ?", startDate+" 00:00:00")
	}
	if endDate != "" {
		query = query.Where("measured_at <= ?", endDate+" 23:59:59")
	}

	var total int64
	query.Count(&total)

	pagination := helpers.Paginate(page, 15, total).WithQuery(map[string]string{
		"category":   category,
		"start_date": startDate,
		"end_date":   endDate,
	})

	var weighings []models.Weighing
	query.Preload("Item").
		Order("measured_at desc").
		Limit(pagination.PerPage).
		Offset(pagination.Offset()).
		Find(&weighings)

	return helpers.View(c, "history/index", fiber.Map{
		"Title":      "Riwayat Penimbangan",
		"Weighings":  weighings,
		"Pagination": pagination,
		"Category":   category,
		"StartDate":  startDate,
		"EndDate":    endDate,
	})
}

// ExportCSV GET /history/export — mengunduh file CSV hasil penimbangan.
func (ctl *HistoryController) ExportCSV(c fiber.Ctx) error {
	category := strings.TrimSpace(c.Query("category"))
	startDate := strings.TrimSpace(c.Query("start_date"))
	endDate := strings.TrimSpace(c.Query("end_date"))

	query := config.DB.Model(&models.Weighing{})
	if category != "" {
		query = query.Where("category = ?", category)
	}
	if startDate != "" {
		query = query.Where("measured_at >= ?", startDate+" 00:00:00")
	}
	if endDate != "" {
		query = query.Where("measured_at <= ?", endDate+" 23:59:59")
	}

	var weighings []models.Weighing
	query.Preload("Item").Order("measured_at desc").Find(&weighings)

	filename := fmt.Sprintf("checkweigher_history_%s.csv", time.Now().Format("20060102_150405"))
	c.Set("Content-Type", "text/csv")
	c.Set("Content-Disposition", "attachment; filename="+filename)

	writer := csv.NewWriter(c)
	defer writer.Flush()

	header := []string{"ID", "Kode Barang", "RFID UID", "Nama Barang", "Berat (g)", "Kategori", "Kecepatan (RPM)", "Waktu Ukur"}
	if err := writer.Write(header); err != nil {
		return err
	}

	for _, w := range weighings {
		code, name := "-", "-"
		if w.Item != nil {
			code = w.Item.ItemCode
			name = w.Item.Name
		}
		row := []string{
			strconv.Itoa(int(w.ID)),
			code,
			w.RfidUID,
			name,
			fmt.Sprintf("%.1f", w.Weight),
			w.Category,
			fmt.Sprintf("%.1f", w.ConveyorSpeed),
			w.MeasuredAt.Format("2006-01-02 15:04:05"),
		}
		if err := writer.Write(row); err != nil {
			return err
		}
	}

	return nil
}

