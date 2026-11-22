package requests

import (
	"errors"
	"fmt"
	"reflect"
	"strings"

	"github.com/go-playground/validator/v10"
)

var validate = validator.New()

// Validate memvalidasi struct request dan mengembalikan map pesan error berbahasa Indonesia.
func Validate(req any) map[string]string {
	errs := map[string]string{}
	err := validate.Struct(req)
	if err == nil {
		return errs
	}

	var validationErrors validator.ValidationErrors
	if !errors.As(err, &validationErrors) {
		errs["_form"] = err.Error()
		return errs
	}

	t := reflect.TypeOf(req)
	if t.Kind() == reflect.Pointer {
		t = t.Elem()
	}

	for _, fe := range validationErrors {
		field, _ := t.FieldByName(fe.StructField())
		formName := field.Tag.Get("form")
		label := field.Tag.Get("label")
		if label == "" {
			label = fe.StructField()
		}
		errs[formName] = message(label, fe)
	}
	return errs
}

func message(label string, fe validator.FieldError) string {
	switch fe.Tag() {
	case "required":
		return fmt.Sprintf("%s wajib diisi.", label)
	case "min":
		if fe.Kind() == reflect.String {
			return fmt.Sprintf("%s minimal %s karakter.", label, fe.Param())
		}
		return fmt.Sprintf("%s minimal %s.", label, fe.Param())
	case "max":
		if fe.Kind() == reflect.String {
			return fmt.Sprintf("%s maksimal %s karakter.", label, fe.Param())
		}
		return fmt.Sprintf("%s maksimal %s.", label, fe.Param())
	case "gt":
		return fmt.Sprintf("%s harus lebih besar dari %s.", label, fe.Param())
	case "gte":
		return fmt.Sprintf("%s tidak boleh kurang dari %s.", label, fe.Param())
	default:
		return fmt.Sprintf("%s tidak valid (%s).", label, strings.ToLower(fe.Tag()))
	}
}

