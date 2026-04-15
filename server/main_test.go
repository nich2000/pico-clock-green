package main

import (
	"log"
	"os"
	"testing"

	"github.com/nats-io/nats.go"
	"gitlab.com/nichalterego/drift-dynamics-bot/pkg/protocol"
)

func TestSendCountDown(t *testing.T) {
	natsHost := os.Getenv("NATS_INTEGRATION_HOST")
	if natsHost == "" {
		t.Skip("set NATS_INTEGRATION_HOST to run integration test")
	}

	log.Printf("[INFO] Nats connect, host: %v", natsHost)
	nc, err := nats.Connect(natsHost, nats.Name(service))
	if err != nil {
		t.Fatalf("nats.Connect failed: %v", err)
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
	t.Run("numeric string", func(t *testing.T) {
		value, err := countdownValue(map[string]interface{}{"minute": "12"}, countdownMinute)
		if err != nil {
			t.Fatalf("countdownValue returned error: %v", err)
		}
		if value != 12 {
			t.Fatalf("countdownValue returned %d, want 12", value)
		}
	})

	t.Run("float64", func(t *testing.T) {
		value, err := countdownValue(map[string]interface{}{"minute": float64(12)}, countdownMinute)
		if err != nil {
			t.Fatalf("countdownValue returned error: %v", err)
		}
		if value != 12 {
			t.Fatalf("countdownValue returned %d, want 12", value)
		}
	})

	t.Run("int", func(t *testing.T) {
		value, err := countdownValue(map[string]interface{}{"second": 34}, countdownSecond)
		if err != nil {
			t.Fatalf("countdownValue returned error: %v", err)
		}
		if value != 34 {
			t.Fatalf("countdownValue returned %d, want 34", value)
		}
	})

	t.Run("missing key", func(t *testing.T) {
		_, err := countdownValue(map[string]interface{}{}, countdownSecond)
		if err == nil {
			t.Fatal("countdownValue returned nil error for missing key")
		}
	})

	t.Run("invalid numeric string", func(t *testing.T) {
		_, err := countdownValue(map[string]interface{}{"second": "3x"}, countdownSecond)
		if err == nil {
			t.Fatal("countdownValue returned nil error for invalid numeric string")
		}
	})

	t.Run("unsupported type", func(t *testing.T) {
		_, err := countdownValue(map[string]interface{}{"second": true}, countdownSecond)
		if err == nil {
			t.Fatal("countdownValue returned nil error for unsupported type")
		}
	})
}

func TestValidateCountdown(t *testing.T) {
	t.Run("valid", func(t *testing.T) {
		if err := validateCountdown(Countdown{Minute: 1, Second: 59}); err != nil {
			t.Fatalf("validateCountdown returned error: %v", err)
		}
	})

	t.Run("negative minute", func(t *testing.T) {
		if err := validateCountdown(Countdown{Minute: -1, Second: 10}); err == nil {
			t.Fatal("validateCountdown returned nil error for negative minute")
		}
	})

	t.Run("second out of range", func(t *testing.T) {
		if err := validateCountdown(Countdown{Minute: 1, Second: 60}); err == nil {
			t.Fatal("validateCountdown returned nil error for invalid second")
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
