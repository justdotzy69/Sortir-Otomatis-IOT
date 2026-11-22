package requests

// ItemRequest menampung input form pendaftaran barang & RFID tag.
type ItemRequest struct {
	RfidUID     string `form:"rfid_uid" label:"RFID UID" validate:"required,min=4,max=50"`
	Name        string `form:"name" label:"Nama barang" validate:"required,min=3,max=100"`
	Description string `form:"description" label:"Deskripsi" validate:"max=255"`
}

