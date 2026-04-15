package main

import (
	"bufio"
	"context"
	"errors"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"runtime"
	"strconv"
	"sync"
	"syscall"
	"time"

	"github.com/joho/godotenv"
	"github.com/nats-io/nats.go"
	"gitlab.com/nichalterego/drift-dynamics-bot/pkg/env"
	"gitlab.com/nichalterego/drift-dynamics-bot/pkg/logs"
	"gitlab.com/nichalterego/drift-dynamics-bot/pkg/protocol"
)

const service = "clock_server"
const version = "v1.0.1"

type Countdown struct {
	Minute int
	Second int
}

type connectionHub struct {
	mu   sync.RWMutex
	subs map[chan Countdown]struct{}
}

func main() {
	_ = godotenv.Load(".env")

	console := env.GetEnv("STAGE", "") == "prod"
	err := logs.InitLogs(service, logs.LogInfo, console)
	if err != nil {
		log.Panic(err)
	}
	log.Println("==========")
	log.Printf("[INFO] Name: %v, Version: %v", service, version)
	log.Printf("[INFO] GOOS: %v, GOARCH: %v", runtime.GOOS, runtime.GOARCH)

	natsHost := env.GetEnv("NATS_HOST", "localhost:4222")
	log.Printf("[INFO] Nats connect, host: %v", natsHost)
	nc, err := nats.Connect(natsHost, nats.Name(service))
	if err != nil {
		log.Panic(err)
	}
	log.Println("[INFO] Nats ", nc.Status().String())
	defer nc.Close()

	// host := env.GetEnv("HOST", "localhost:58001")
	host := env.GetEnv("HOST", "0.0.0.0:58001")
	listener, err := net.Listen("tcp", host)
	if err != nil {
		log.Fatalf("Error listening port: %v, %v", host, err)
	}
	defer listener.Close()
	log.Printf("[INFO] TCP server listening on: %v\n", host)

	hub := newConnectionHub()

	wg := sync.WaitGroup{}

	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	wg.Add(1)
	go acceptConnection(ctx, listener, hub, &wg)

	wg.Add(1)
	go handleNats(ctx, nc, "dd.clock.judge.in", hub, &wg)

	// Канал для получения сигналов
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)
	// Ожидаем сигнал завершения
	sig := <-sigChan
	log.Printf("[INFO] Got signal: %v. Finish working...\n", sig)

	// Отменяем контекст для остановки goroutine
	cancel()
	log.Println("[INFO] cancel done")

	wg.Wait()

	log.Println("[INFO] well done")
}

func newConnectionHub() *connectionHub {
	return &connectionHub{
		subs: make(map[chan Countdown]struct{}),
	}
}

func (h *connectionHub) subscribe() chan Countdown {
	ch := make(chan Countdown, 1)

	h.mu.Lock()
	h.subs[ch] = struct{}{}
	h.mu.Unlock()

	return ch
}

func (h *connectionHub) unsubscribe(ch chan Countdown) {
	h.mu.Lock()
	if _, ok := h.subs[ch]; ok {
		delete(h.subs, ch)
		close(ch)
	}
	h.mu.Unlock()
}

func (h *connectionHub) broadcast(cd Countdown) {
	h.mu.RLock()
	defer h.mu.RUnlock()

	for ch := range h.subs {
		select {
		case ch <- cd:
		default:
			log.Printf("[WARN] Countdown dropped for slow connection: %v", cd)
		}
	}
}

func acceptConnection(ctx context.Context, listener net.Listener, hub *connectionHub, wg *sync.WaitGroup) {
	defer wg.Done()

	go func() {
		<-ctx.Done()
		if err := listener.Close(); err != nil && !errors.Is(err, net.ErrClosed) {
			log.Printf("[ERROR] Error closing listener, %v", err)
		}
	}()

	for {
		conn, err := listener.Accept()
		if err != nil {
			if ctx.Err() != nil || errors.Is(err, net.ErrClosed) {
				log.Println("[INFO] acceptConnection stopped")
				return
			}
			log.Printf("[ERROR] Error accepting connection, %v", err)
			continue
		}

		log.Printf("[INFO] New connection: %v", conn.RemoteAddr())
		go handleConnection(ctx, conn, hub)
	}
}

func handleConnection(ctx context.Context, conn net.Conn, hub *connectionHub) {
	defer conn.Close()

	countdownCh := hub.subscribe()
	defer hub.unsubscribe(countdownCh)

	readDone := make(chan error, 1)

	go func() {
		<-ctx.Done()
		if err := conn.Close(); err != nil && !errors.Is(err, net.ErrClosed) {
			log.Printf("[ERROR] Error conn.Close, %v", err)
		}
	}()

	go func() {
		scanner := bufio.NewScanner(conn)
		for scanner.Scan() {
			line := scanner.Text()
			log.Printf("%s", line)
		}

		readDone <- scanner.Err()
	}()

	pingTicker := time.NewTicker(10 * time.Second)
	defer pingTicker.Stop()

	for {
		select {
		case <-ctx.Done():
			log.Printf("[INFO] Connection closed by context: %v", conn.RemoteAddr())
			return
		case err := <-readDone:
			if err != nil && !errors.Is(err, net.ErrClosed) {
				log.Printf("[ERROR] Error scanner.Scan, %v", err)
			}
			log.Printf("[INFO] Connection reader stopped: %v", conn.RemoteAddr())
			return
		case cd := <-countdownCh:
			if err := writeMessage(conn, fmt.Sprintf("COUNTDOWN ON %d %d\n", cd.Minute, cd.Second)); err != nil {
				log.Printf("[ERROR] Error conn.Write nats countdown, %v", err)
				return
			}
		case <-pingTicker.C:
			if err := writeMessage(conn, "PING\n"); err != nil {
				log.Printf("[ERROR] Error conn.Write ping, %v", err)
				return
			}
		}
	}
}

func handleNats(ctx context.Context, nc *nats.Conn, subj string, hub *connectionHub, wg *sync.WaitGroup) {
	defer wg.Done()

	sub, err := nc.Subscribe(subj, func(msg *nats.Msg) {
		name, _, kwargs, err := protocol.ReadCmd(msg.Data)
		if err != nil {
			log.Printf("[ERROR] Error protocol.ReadCmd, %v", err)
			return
		}

		switch name {
		case "countdown":
			minute, err := countdownValue(kwargs, "minute")
			if err != nil {
				log.Printf("[ERROR] Invalid countdown minute, %v", err)
				return
			}

			second, err := countdownValue(kwargs, "second")
			if err != nil {
				log.Printf("[ERROR] Invalid countdown second, %v", err)
				return
			}

			cd := Countdown{Minute: minute, Second: second}
			hub.broadcast(cd)
			log.Printf("[INFO] Countdown: %v", cd)
		default:
			log.Printf("[DEBUG] Ignore NATS command: %s", name)
		}
	})
	if err != nil {
		log.Printf("[ERROR] Error nc.Subscribe, %v", err)
		return
	}
	defer func() {
		if err := sub.Unsubscribe(); err != nil && !errors.Is(err, nats.ErrBadSubscription) && !errors.Is(err, nats.ErrConnectionClosed) {
			log.Printf("[ERROR] Error sub.Unsubscribe, %v", err)
		}
	}()

	if err := nc.Flush(); err != nil {
		log.Printf("[ERROR] Error nc.Flush, %v", err)
		return
	}

	log.Printf("[INFO] NATS subscription ready: %s", subj)

	<-ctx.Done()
	log.Println("[INFO] handleNats stopped")
}

func countdownValue(kwargs map[string]interface{}, key string) (int, error) {
	value, ok := kwargs[key]
	if !ok {
		return 0, fmt.Errorf("key %q is missing", key)
	}

	switch v := value.(type) {
	case string:
		value, _ := strconv.Atoi(v)
		return value, nil
	case float64:
		return int(v), nil
	case float32:
		return int(v), nil
	case int:
		return v, nil
	case int64:
		return int(v), nil
	default:
		return 0, fmt.Errorf("key %q has unsupported type %T", key, value)
	}
}

func writeMessage(conn net.Conn, message string) error {
	if err := conn.SetWriteDeadline(time.Now().Add(5 * time.Second)); err != nil {
		return err
	}

	if _, err := conn.Write([]byte(message)); err != nil {
		return err
	}

	// log.Printf("[DEBUG] %s", message)
	return nil
}
