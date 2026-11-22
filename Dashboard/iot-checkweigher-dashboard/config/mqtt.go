package config

import (
	"context"
	"crypto/tls"
	"log"
	"net"
	"net/url"
	"strings"
	"time"

	mqtt "github.com/eclipse/paho.mqtt.golang"
)

var MQTTClient mqtt.Client

// ConnectMQTT menghubungkan server ke broker MQTT (mendukung Public HiveMQ & HiveMQ Cloud TLS).
func ConnectMQTT() (mqtt.Client, error) {
	broker := Env("MQTT_BROKER", "tcp://broker.hivemq.com:1883")
	clientID := Env("MQTT_CLIENT_ID", "checkweigher-server")
	username := Env("MQTT_USERNAME", "")
	password := Env("MQTT_PASSWORD", "")

	opts := mqtt.NewClientOptions()
	opts.AddBroker(broker)
	opts.SetClientID(clientID)
	opts.SetAutoReconnect(true)
	opts.SetMaxReconnectInterval(10 * time.Second)
	opts.SetKeepAlive(30 * time.Second)

	if username != "" {
		opts.SetUsername(username)
		opts.SetPassword(password)
	}

	// Gunakan TLS jika skema adalah ssl://, tls://, atau tcps://
	isTLS := strings.HasPrefix(broker, "ssl://") || strings.HasPrefix(broker, "tls://") || strings.HasPrefix(broker, "tcps://")
	if isTLS {
		tlsConfig := &tls.Config{
			InsecureSkipVerify: false,
			MinVersion:         tls.VersionTLS12,
		}
		opts.SetTLSConfig(tlsConfig)
	}

	// Dial menggunakan IPv4 (tcp4) untuk mencegah koneksi terputus (wsarecv) pada jaringan dual-stack (IPv6)
	opts.SetCustomOpenConnectionFn(func(uri *url.URL, options mqtt.ClientOptions) (net.Conn, error) {
		dialer := &net.Dialer{Timeout: 5 * time.Second}
		host := uri.Host
		if strings.HasPrefix(uri.Scheme, "ssl") || strings.HasPrefix(uri.Scheme, "tls") || strings.HasPrefix(uri.Scheme, "tcps") {
			return tls.DialWithDialer(dialer, "tcp4", host, options.TLSConfig)
		}
		return dialer.DialContext(context.Background(), "tcp4", host)
	})

	opts.SetOnConnectHandler(func(c mqtt.Client) {
		log.Println("[MQTT] Terhubung ke broker HiveMQ:", broker)
	})

	opts.SetConnectionLostHandler(func(c mqtt.Client, err error) {
		log.Println("[MQTT] Koneksi terputus:", err)
	})

	client := mqtt.NewClient(opts)
	token := client.Connect()
	if !token.WaitTimeout(5 * time.Second) {
		log.Println("[MQTT] Timeout menghubungkan ke broker HiveMQ (server tetap lanjut)")
		return nil, nil
	}

	if token.Error() != nil {
		log.Printf("[MQTT] Gagal terhubung ke broker: %v", token.Error())
		return nil, token.Error()
	}

	MQTTClient = client
	log.Println("[MQTT] Berhasil terhubung ke broker HiveMQ")
	return client, nil
}

