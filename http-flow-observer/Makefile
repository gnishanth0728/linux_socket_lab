# ============================================================================
# HTTP Flow Observer
# ============================================================================

APP := http-flow-observer

CC      := gcc
CLANG   := clang
BPFTOOL := bpftool

CFLAGS  := -O2 -g -Wall
BPF_CFLAGS := -O2 -g -target bpf

LIBS := -lbpf -lelf -lz

SRC := \
main.c \
collector/event_queue.c \
collector/collector.c

OBJ := $(SRC:.c=.o)

BPF_SRC := \
kernel/nic.bpf.c \
kernel/irq.bpf.c \
kernel/ip.bpf.c \
kernel/tcp.bpf.c \
kernel/socket.bpf.c \
kernel/send.bpf.c

BPF_OBJ := $(BPF_SRC:.bpf.c=.bpf.o)

SKEL := \
kernel/nic.skel.h \
kernel/irq.skel.h \
kernel/ip.skel.h \
kernel/tcp.skel.h \
kernel/socket.skel.h \
kernel/send.skel.h

# ============================================================================

all: vmlinux $(SKEL) $(APP)

# ============================================================================

vmlinux:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

# ============================================================================

kernel/%.bpf.o: kernel/%.bpf.c
	$(CLANG) $(BPF_CFLAGS) -I. -c $< -o $@

# ============================================================================

kernel/%.skel.h: kernel/%.bpf.o
	$(BPFTOOL) gen skeleton $< > $@

# ============================================================================

$(APP): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LIBS)

# ============================================================================

%.o: %.c
	$(CC) $(CFLAGS) -I. -c $< -o $@

# ============================================================================

clean:
	rm -f $(OBJ)
	rm -f $(BPF_OBJ)
	rm -f $(SKEL)
	rm -f $(APP)

# ============================================================================

run: all
	sudo ./$(APP)

# ============================================================================

.PHONY: all clean run