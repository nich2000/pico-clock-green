FROM golang:1.25.3-alpine AS builder

WORKDIR /src

COPY go.mod go.sum ./
COPY vendor ./vendor
COPY main.go main_test.go ./

RUN CGO_ENABLED=0 GOOS=linux GOARCH=amd64 go build -mod=vendor -o /out/clock_server ./main.go

FROM alpine:3.22

WORKDIR /app

COPY --from=builder /out/clock_server ./clock_server

ENV HOST=0.0.0.0:58001

EXPOSE 58001/tcp

CMD ["./clock_server"]
