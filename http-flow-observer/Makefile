APP := http-flow-observer

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

# ============================================================
# Build Userspace
# ============================================================

$(APP): main.c kernel/tracer.skel.h
	$(CC) \
	$(CFLAGS) \
	-I. \
	main.c \
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

.PHONY: all clean run