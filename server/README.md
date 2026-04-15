## Docker

Build image:

```bash
docker build -t pico-clock-green-server .
```

Run container with externally published TCP port:

```bash
docker run --rm \
  -e NATS_HOST=drift-dynamics.com:4222 \
  -e HOST=0.0.0.0:58001 \
  -p 58001:58001/tcp \
  pico-clock-green-server
```

After that the service accepts TCP connections on port `58001` of the Docker host.
