package requests

// ProductRequest menampung input form konfigurasi threshold dan setpoint RPM.
type ProductRequest struct {
	Name         string  `form:"name" label:"Nama konfigurasi" validate:"required,min=3,max=100"`
	TargetWeight float64 `form:"target_weight" label:"Berat target" validate:"required,gt=0"`
	ToleranceMin float64 `form:"tolerance_min" label:"Batas ringan (min)" validate:"required,gt=0"`
	ToleranceMax float64 `form:"tolerance_max" label:"Batas error (max)" validate:"required,gt=0"`
	SetpointRPM  float64 `form:"setpoint_rpm" label:"Setpoint RPM" validate:"required,gt=0"`
}

