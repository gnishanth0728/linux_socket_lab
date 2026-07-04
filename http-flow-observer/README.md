# HTTP Flow Observer

Traces one HTTP request across the **complete OS + JVM stack**:

```
[Client machine]                       [Server machine]
HTTP_Request_flow.c ──── TCP/IP ──────► http-flow-observer (eBPF)
                                             │
                                        STEP 1  NIC receives Ethernet frame
                                        STEP 2  CPU enters IRQ handler
                                        STEP 3  SoftIRQ NET_RX_SOFTIRQ
                                        STEP 4  IPv4 validates header
                                        STEP 5  TCP finds destination socket
                                        STEP 6  Socket receive queue
                                        STEP 7  recv() wakes up
                                             │
                                        STEP 8  Tomcat worker thread
                                        STEP 9  DispatcherServlet
                                        STEP 10 Controller
                                        STEP 11 Service
                                        STEP 12 Repository
                                        STEP 13 JDBC / PostgreSQL
                                             │
                                        output/merged_requests.jsonl
                                        output/index.html  ← Live UI
```

---

## Prerequisites

Install once on the server machine (Ubuntu/Debian):

```bash
sudo apt update
sudo apt install -y \
  build-essential clang llvm bpftool \
  libbpf-dev libelf-dev zlib1g-dev \
  linux-headers-$(uname -r) \
  openjdk-17-jdk maven
```

---

## Server Setup  (run all three terminals on the server)

### Terminal 1 — Build and run the kernel observer

```bash
cd /home/udz1kor/linux-network-lab/http-flow-observer
make
sudo ./http-flow-observer
```

Shortcut:

```bash
make run
```

This attaches eBPF probes to the kernel network path.
It must stay running while requests are made.

---

### Terminal 2 — Build and attach the Java agent to your Spring Boot app

Build the agent once:

```bash
cd /home/udz1kor/linux-network-lab/http-flow-observer/agent
make build
```

Start your Spring Boot app with the agent attached:

```bash
java \
  -javaagent:/home/udz1kor/linux-network-lab/http-flow-observer/agent/target/http-flow-agent-1.0.0.jar=com.yourcompany.yourapp \
  -jar app.jar
```

Replace `com.yourcompany.yourapp` with your application's root package.
The agent instruments Tomcat, Spring MVC, Controller, Service, Repository, and JDBC automatically.

---

### Terminal 3 — Serve the live UI

```bash
cd /home/udz1kor/linux-network-lab/http-flow-observer
make serve
```

Default port is **8090**.  Override:

```bash
make serve UI_PORT=9000
```

Open in browser:

```
http://localhost:8090/index.html
```

The page auto-refreshes every second and shows the complete numbered step list
for the latest request (kernel steps + JVM spans merged together).

---

## Client — Send a request

### Option A — HTTP_Request_flow.c (C client in this repo)

```bash
cd /home/udz1kor/linux-network-lab/HTTP_Request_flow
make
./socket_lab --host <server-ip> --port 8080 --path /students/1051110001
```

For local testing (client and server on same machine):

```bash
./socket_lab --host localhost --port 8080 --path /students/1051110001
```

### Option B — curl

```bash
curl http://<server-ip>:8080/students/1051110001
```

### Option C — browser

```
http://<server-ip>:8080/students/1051110001
```

The observer tracks **any** TCP connection that arrives on the server — it does not care
which client sent it.

---

## What you see per request

### Terminal 1 (kernel observer)

```
====================================================================
KERNEL NETWORK PATH  socket=0x<ptr>
Client: <india-ip>:54021  →  Server: <server-ip>:8080
====================================================================

STEP 1  NIC receives Ethernet frame
        ↓ NET_RX

STEP 2  CPU enters IRQ handler
        ↓ IRQ_ENTRY               Δ 12.341 us
...
```

### Terminal 2 (Java agent)

```
====================================================================
HTTP Request #1  GET /students/1051110001
Client: <india-ip>:54021       Server: <server-ip>:8080
====================================================================

STEP 7   Socket marked readable, recv() wakes up
         ↓ SOCK_READABLE           Δ 1.002 us

STEP 8   Tomcat worker thread processes connection
         ↓ StandardWrapperValve#invoke   2341.000 us

STEP 9   DispatcherServlet matches and dispatches handler
         ↓ DispatcherServlet#doDispatch  2200.000 us
...
====================================================================
Total latency: 4.21 ms  (7 kernel steps + 6 JVM spans)
====================================================================
```

### Live UI (browser)

Shows the same merged numbered list with KERNEL / JVM badges,
auto-refreshing from `output/merged_requests.jsonl`.

---

## Output files

All written to `output/` next to the observer binary:

| File | Contents |
|---|---|
| `merged_requests.jsonl` | Full merged timeline per request (kernel + JVM) |
| `requests.jsonl` | Kernel-only event timelines |
| `metrics.json` | Aggregated counts and latency stats |
| `flame.folded` | Flame graph input (`flamegraph.pl < flame.folded > flame.svg`) |
| `sequence.mmd` | Mermaid sequence diagram (`mermaid -i sequence.mmd`) |
| `index.html` | Live UI page |
| `kernel_request.txt` | Handoff file read by Java agent for merge |

---

## Notes

- The kernel observer and Spring Boot app must run on the **same machine**.
- The C client (`HTTP_Request_flow.c`) can run anywhere — any machine, any network.
- eBPF attach requires root (`sudo`) or `CAP_BPF + CAP_NET_ADMIN`.
- If `make` fails with `bpftool: not found`, install `bpftool` and retry.
- If BTF generation fails, ensure `/sys/kernel/btf/vmlinux` exists.
- Set `HTTP_FLOW_OUTPUT_DIR=/path/to/dir` to change where output files are written.
