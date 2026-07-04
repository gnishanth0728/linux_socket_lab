# =============================================================================
# Request Observer
#
# Build:
#   make
#
# Clean:
#   make clean
# =============================================================================

CLANG      := clang
CC         := gcc

BPFTOOL    := bpftool

ARCH       := x86

LIBS       := -lbpf -lelf -lz

CFLAGS     := -O2 -g -Wall

BPF_CFLAGS := -O2 -g -target bpf -D__TARGET_ARCH_$(ARCH)

OUTPUT      := build
BPF_OBJ     := $(OUTPUT)/tcp_monitor.bpf.o
SKEL_HEADER := $(OUTPUT)/tcp_monitor.skel.h

APP         := request-observer

all: directories $(SKEL_HEADER) $(APP)

directories:
	mkdir -p $(OUTPUT)

###############################################################################
# Compile eBPF Program
###############################################################################

$(BPF_OBJ): tcp-monitor.bpf.c
	$(CLANG) \
		$(BPF_CFLAGS) \
		-c $< \
		-o $@

###############################################################################
# Generate Skeleton
###############################################################################

$(SKEL_HEADER): $(BPF_OBJ)
	$(BPFTOOL) gen skeleton $< > $@

###############################################################################
# Build User Program
###############################################################################

$(APP): loader.c $(SKEL_HEADER)
	$(CC) \
		$(CFLAGS) \
		-I$(OUTPUT) \
		-o $@ \
		loader.c \
		$(LIBS)

###############################################################################

clean:
	rm -rf build
	rm -f request-observer

.PHONY: all clean directories