# Linux Network Lab

Interactive C lab that demonstrates the lifecycle of a TCP client socket on Linux:
- socket creation
- DNS resolution
- TCP connect
- HTTP request send
- HTTP response receive loop
- TCP/IP/Ethernet header visibility using tcpdump and AF_PACKET

## Build

```bash
gcc socket_lab.c -o socket_lab
```

## Run

Default run (uses built-in host and port):

```bash
./socket_lab
```

Run with your own target host and port:

```bash
./socket_lab --host example.com --port 80
```

Packet output mode (compact table is default):

```bash
./socket_lab --packet-compact
./socket_lab --packet-detail
```

Show command help:

```bash
./socket_lab --help
```

## Important Permission Note

The lab includes AF_PACKET raw socket decoding of Ethernet/IP/TCP headers.
Raw sockets require root (or CAP_NET_RAW).

If you run without permissions, the app continues, but raw decode will be skipped.

To run with permissions:

```bash
sudo ./socket_lab --host example.com --port 80 --packet-detail
```

Optional capability-based approach (avoid full sudo run):

```bash
sudo setcap cap_net_raw+ep ./socket_lab
./socket_lab --host example.com --port 80
```

To remove capability:

```bash
sudo setcap -r ./socket_lab
```

## Runtime Flow

The program is step-by-step and pauses for ENTER between stages.

Current runtime options:
- `--host NAME` target hostname
- `--port N` target TCP port (1-65535)
- `--packet-compact` compact one-line packet table
- `--packet-detail` verbose packet decode
- `--help` usage information

## Suggested External Observability Commands

Run in another terminal while lab is running:

```bash
sudo watch -n 1 'ss -tanpi'
sudo tcpdump -i any -nn host example.com
watch -n 1 'ls -l /proc/$(pgrep -n socket_lab)/fd'
watch -n 1 'ps -p $(pgrep -n socket_lab) -f'
```
