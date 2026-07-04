APP := http-flow-observer

CLANG := clang
CC := gcc
BPFTOOL := bpftool

LIBS := -lbpf -lelf -lz

all: vmlinux tracer

vmlinux:
	bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h

tracer:
	$(CLANG) -O2 -g -target bpf -I. -c kernel/tracer.bpf.c -o kernel/tracer.bpf.o