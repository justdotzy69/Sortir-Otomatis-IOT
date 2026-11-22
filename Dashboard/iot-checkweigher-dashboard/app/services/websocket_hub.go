package services

import (
	"encoding/json"
	"log"
	"sync"

	"github.com/gofiber/contrib/v3/websocket"
)

type WebSocketHub struct {
	clients    map[*websocket.Conn]bool
	mu         sync.RWMutex
	broadcast  chan interface{}
	register   chan *websocket.Conn
	unregister chan *websocket.Conn
}

var Hub = NewWebSocketHub()

func NewWebSocketHub() *WebSocketHub {
	return &WebSocketHub{
		clients:    make(map[*websocket.Conn]bool),
		broadcast:  make(chan interface{}, 256),
		register:   make(chan *websocket.Conn),
		unregister: make(chan *websocket.Conn),
	}
}

func (h *WebSocketHub) Run() {
	for {
		select {
		case conn := <-h.register:
			h.mu.Lock()
			h.clients[conn] = true
			h.mu.Unlock()
			log.Printf("[WS] Klien terhubung. Total klien: %d", len(h.clients))

		case conn := <-h.unregister:
			h.mu.Lock()
			if _, ok := h.clients[conn]; ok {
				delete(h.clients, conn)
				conn.Close()
			}
			h.mu.Unlock()
			log.Printf("[WS] Klien terputus. Total klien: %d", len(h.clients))

		case data := <-h.broadcast:
			msg, err := json.Marshal(data)
			if err != nil {
				continue
			}

			h.mu.RLock()
			for conn := range h.clients {
				if err := conn.WriteMessage(websocket.TextMessage, msg); err != nil {
					conn.Close()
					go func(c *websocket.Conn) {
						h.unregister <- c
					}(conn)
				}
			}
			h.mu.RUnlock()
		}
	}
}

func (h *WebSocketHub) Broadcast(data interface{}) {
	h.broadcast <- data
}

func (h *WebSocketHub) Handle(conn *websocket.Conn) {
	h.register <- conn
	defer func() {
		h.unregister <- conn
	}()

	for {
		_, _, err := conn.ReadMessage()
		if err != nil {
			break
		}
	}
}

