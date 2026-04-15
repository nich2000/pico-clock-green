package main

import (
	"log"
	"testing"

	"github.com/nats-io/nats.go"
	"gitlab.com/nichalterego/drift-dynamics-bot/pkg/env"
	"gitlab.com/nichalterego/drift-dynamics-bot/pkg/protocol"
)

func TestSendCountDown(t *testing.T) {
	natsHost := env.GetEnv("NATS_HOST", "drift-dynamics.com:4222")
	log.Printf("[INFO] Nats connect, host: %v", natsHost)
	nc, err := nats.Connect(natsHost, nats.Name(service))
	if err != nil {
		log.Panic(err)
	}
	log.Println("[INFO] Nats ", nc.Status().String())
	defer nc.Close()

	kwargs := make(map[string]string)
	kwargs["minute"] = "0"
	kwargs["second"] = "20"

	cmd := protocol.NewCmd("countdown", &kwargs)

	_ = nc.Publish("dd.clock.judge.in", cmd)
}

func TestCountdownValue(t *testing.T) {
	t.Run("float64", func(t *testing.T) {
		value, err := countdownValue(map[string]interface{}{"minute": float64(12)}, "minute")
		if err != nil {
			t.Fatalf("countdownValue returned error: %v", err)
		}
		if value != 12 {
			t.Fatalf("countdownValue returned %d, want 12", value)
		}
	})

	t.Run("int", func(t *testing.T) {
		value, err := countdownValue(map[string]interface{}{"second": 34}, "second")
		if err != nil {
			t.Fatalf("countdownValue returned error: %v", err)
		}
		if value != 34 {
			t.Fatalf("countdownValue returned %d, want 34", value)
		}
	})

	t.Run("missing key", func(t *testing.T) {
		_, err := countdownValue(map[string]interface{}{}, "second")
		if err == nil {
			t.Fatal("countdownValue returned nil error for missing key")
		}
	})

	t.Run("unsupported type", func(t *testing.T) {
		_, err := countdownValue(map[string]interface{}{"second": "34"}, "second")
		if err == nil {
			t.Fatal("countdownValue returned nil error for unsupported type")
		}
	})
}

func TestConnectionHubBroadcast(t *testing.T) {
	hub := newConnectionHub()

	ch1 := hub.subscribe()
	ch2 := hub.subscribe()

	t.Cleanup(func() {
		hub.unsubscribe(ch1)
		hub.unsubscribe(ch2)
	})

	want := Countdown{Minute: 1, Second: 23}
	hub.broadcast(want)

	got1 := <-ch1
	if got1 != want {
		t.Fatalf("first subscriber got %+v, want %+v", got1, want)
	}

	got2 := <-ch2
	if got2 != want {
		t.Fatalf("second subscriber got %+v, want %+v", got2, want)
	}
}
