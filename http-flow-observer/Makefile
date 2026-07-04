APP := http-flow-observer
AGENT_DIR := agent
AGENT_JAR := $(AGENT_DIR)/target/http-flow-agent-1.0.0.jar

CC := gcc
CLANG := clang
BPFTOOL := bpftool

CFLAGS := -O2 -g -Wall
BPF_CFLAGS := -O2 -g -target bpf

LIBS := -lbpf -lelf -lz

# ============================================================
# Default Target
# ============================================================

all: vmlinux kernel/tracer.skel.h $(APP)

all-with-agent: all agent-build

# ============================================================
# Generate vmlinux.h
# ============================================================

vmlinux:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

# ============================================================
# Build BPF Object
# ============================================================

kernel/tracer.bpf.o: kernel/tracer.bpf.c
	$(CLANG) \
	$(BPF_CFLAGS) \
	-I. \
	-c kernel/tracer.bpf.c \
	-o kernel/tracer.bpf.o

# ============================================================
# Generate Skeleton
# ============================================================

kernel/tracer.skel.h: kernel/tracer.bpf.o
	$(BPFTOOL) gen skeleton kernel/tracer.bpf.o > kernel/tracer.skel.h

COLLECTOR_SRCS := \
	collector/collector.c \
	collector/connection_table.c \
	collector/timeline.c \
	collector/event_queue.c

# ============================================================
# Build Userspace
# ============================================================

$(APP): main.c $(COLLECTOR_SRCS) kernel/tracer.skel.h
	$(CC) \
	$(CFLAGS) \
	-I. \
	main.c \
	$(COLLECTOR_SRCS) \
	-o $(APP) \
	$(LIBS)

# ============================================================
# Clean
# ============================================================

clean:
	rm -f kernel/tracer.bpf.o
	rm -f kernel/tracer.skel.h
	rm -f $(APP)

# ============================================================
# Run
# ============================================================

run: all
	sudo ./$(APP)

# ============================================================
# Live UI  (Build 6)
# Serves output/ on http://localhost:$(UI_PORT)/index.html
# Override port: make serve UI_PORT=9090
# ============================================================

UI_PORT ?= 8090
UI_DIR  := output

serve: $(UI_DIR)
	@echo "Live UI → http://localhost:$(UI_PORT)/index.html"
	@cd $(UI_DIR) && python3 -m http.server $(UI_PORT)

$(UI_DIR):
	@mkdir -p $(UI_DIR)

agent-build:
	$(MAKE) -C $(AGENT_DIR) build

agent-clean:
	$(MAKE) -C $(AGENT_DIR) clean

.PHONY: all all-with-agent clean run serve agent-build agent-clean
