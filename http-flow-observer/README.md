# HTTP Flow Observer

This module traces an HTTP request across:
- Linux kernel network path (eBPF)
- Userspace collector
- Java app internals via Java Agent (Tomcat/Spring/JDBC)

Build 6 outputs are generated in `output/`.

## Prerequisites

Ubuntu/Debian example:

```bash
sudo apt update
sudo apt install -y \
  build-essential clang llvm bpftool \
  libbpf-dev libelf-dev zlib1g-dev \
  linux-headers-$(uname -r) \
  openjdk-17-jdk maven
```

## 1) Build Kernel Observer

```bash
cd /home/udz1kor/linux-network-lab/http-flow-observer
make
```

## 2) Run Kernel Observer

eBPF attach requires root:

```bash
cd /home/udz1kor/linux-network-lab/http-flow-observer
sudo ./http-flow-observer
```

Or:

```bash
make run
```

## 3) Build Java Agent

```bash
cd /home/udz1kor/linux-network-lab/http-flow-observer/agent
make build
```

## 4) Run Spring Boot App With Agent

```bash
java \
  -javaagent:/home/udz1kor/linux-network-lab/http-flow-observer/agent/target/http-flow-agent-1.0.0.jar=com.yourcompany.yourapp \
  -jar app.jar
```

Replace `com.yourcompany.yourapp` with your application base package.

## 5) Generate Traffic

Send requests (curl, browser, load test) to your app while observer + agent are running.

## 6) Build 6 Outputs

Generated under `/home/udz1kor/linux-network-lab/http-flow-observer/output`:

- `requests.jsonl` (request timelines)
- `metrics.json` (aggregated performance metrics)
- `flame.folded` (flame graph folded input)
- `sequence.mmd` (Mermaid sequence diagram)
- `index.html` (live UI page)

## 7) Open Live UI

```bash
cd /home/udz1kor/linux-network-lab/http-flow-observer/output
python3 -m http.server 8080
```

Open in browser:

- http://localhost:8080/index.html

## Notes

- If `make` fails with `bpftool: not found`, install `bpftool` and retry.
- If BTF generation fails, ensure `/sys/kernel/btf/vmlinux` exists and kernel headers are installed.
