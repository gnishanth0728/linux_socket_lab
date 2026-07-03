# Packet Server Demo

This project includes a small C TCP server that shows a layered request flow in the terminal.
It simulates a request moving through a Docker-style stack:

- React frontend/client
- nginx-proxy
- spring-api
- controller
- service
- repository/DAO
- PostgreSQL/JDBC
- response back to the client

## Compile

Run this from the project folder:

```bash
gcc -o packet_server_demo packet_server_demo.c
```

## Run

Start the server:

```bash
./packet_server_demo
```

In another terminal, send a request:

```bash
curl "http://127.0.0.1:9090/user?name=alice"
```

## Notes

- The server listens on port 9090.
- If that port is already in use, change `PORT` in `packet_server_demo.c` and rebuild.
- This is a simulation for learning and tracing purposes; it does not connect to your real Tomcat/Spring/Postgres containers.
