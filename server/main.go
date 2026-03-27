package main

import (
	"bufio"
	"fmt"
	"log"
	"math/rand"
	"net"
	"time"
)

func main() {
	address := ":45678"
	listener, err := net.Listen("tcp", address)
	if err != nil {
		log.Fatal("Ошибка при открытии порта:", err)
	}
	defer listener.Close()

	fmt.Printf("TCP-сервер запущен на %v\n", address)

	for {
		// Принимаем входящее соединение
		conn, err := listener.Accept()
		if err != nil {
			log.Println("Ошибка при принятии соединения:", err)
			continue
		}

		// Обрабатываем соединение в отдельной горутине
		fmt.Printf("Новое соединение: %v\n", conn.RemoteAddr())
		go handleConnection(conn)
	}
}

func handleConnection(conn net.Conn) {
	defer conn.Close()

	done := make(chan struct{})
	go func() {
		random := rand.New(rand.NewSource(time.Now().UnixNano()))

		ticker := time.NewTicker(300 * time.Second)
		defer ticker.Stop()

		ticker1 := time.NewTicker(10 * time.Second)
		defer ticker1.Stop()

		for {
			select {
			case <-ticker.C:
				totalSeconds := random.Intn(111) + 10
				minutes := totalSeconds / 60
				seconds := totalSeconds % 60

				message := fmt.Sprintf("COUNTDOWN ON %d %d\n", minutes, seconds)
				if _, err := conn.Write([]byte(message)); err != nil {
					log.Println("Ошибка записи:", err)
					return
				}
			case <-ticker1.C:
				if _, err := conn.Write([]byte("PING\n")); err != nil {
					log.Println("Ошибка записи:", err)
					return
				}
			case <-done:
				return
			}
		}
	}()

	scanner := bufio.NewScanner(conn)
	for scanner.Scan() {
		line := scanner.Text()

		// Выводим полученные данные в консоль
		fmt.Printf("Получено: %s\n", line)
	}
	close(done)

	if err := scanner.Err(); err != nil {
		log.Println("Ошибка чтения:", err)
	}
}
