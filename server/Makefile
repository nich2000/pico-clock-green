IMAGE ?= pico-clock-green-server
NATS_HOST ?= drift-dynamics.com:4222
HOST ?= 0.0.0.0:58001
PORT ?= 58001

## build
build:
	#clear
	go fmt ./...
	go build -o ./clock_server ./main.go

## mod
mod:
	#clear
	go get -u ./...
	go mod tidy -compat=1.25
	go mod vendor
	go fmt ./...

## lint
lint:
	#clear
	go fmt ./...
	golangci-lint run

## test
test:
	#clear
	go fmt ./...
	go test -coverprofile cover.out ./...
	go tool cover -func=cover.out

## docker-build
docker-build:
	docker build -t $(IMAGE) .

## docker-run
docker-run:
	docker run --rm \
		-e NATS_HOST=$(NATS_HOST) \
		-e HOST=$(HOST) \
		-p $(PORT):58001/tcp \
		$(IMAGE)
