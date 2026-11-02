# Makefile for weron-signaler-c (C implementation)
# Minimal WebSocket relay server for weron VPN signaling
#
# --- Targets ---------------------------------------------------------------
# make TARGET=linux              Linux native (x86_64, statically linked)
# make TARGET=win                Windows x86_64 (MinGW-w64, libwinpthread static)
# make TARGET=mipsel             OpenWrt MIPS little-endian (glibc hard-float)
# make TARGET=mipsel-musl-sf     OpenWrt MIPS little-endian soft-float (musl)
# make TARGET=mipsisa64r6        OpenWrt MIPS ISA 64-bit r6
# make TARGET=armhf              OpenWrt ARMv7 hard-float
#
# --- All 14 targets via build-all.sh ----------------------------------------
# Static libwebsockets for all targets are prebuilt in /tmp/lws-build-weron/
# (built with CMake: LWS_WITH_STATIC=ON, SSL/OFF, zlib/OFF, CGI/OFF, HTTP2/OFF, CAP/OFF)

TARGET ?= linux

# Linux targets use prebuilt LWS in /tmp/lws-build-weron
# Windows target uses LWS built with __MINGW_USE_STATIC_CRT=1 in /tmp/libwebsockets-4.1.6
LWS_LINUX = /tmp/lws-build-weron
LWS_WIN   = /tmp/libwebsockets-4.1.6

ifeq ($(TARGET),linux)
  CC       = x86_64-linux-musl-gcc
  LWS_CFLAGS = -I$(LWS_LINUX)/linux_musl_static/include
  LWS_LIBS  = $(LWS_LINUX)/linux_musl_static/lib/libwebsockets.a -lpthread
  OUT_DIR  = build-linux
  OUT_NAME = weron-signaler-c
  LDFLAGS  = $(LWS_LIBS) -static
endif

ifeq ($(TARGET),win)
  CC       = x86_64-w64-mingw32-gcc
  LWS_CFLAGS = -I$(LWS_WIN)/build-win-static-crt/include
  LWS_LIBS  = $(LWS_WIN)/build-win-static-crt/lib/libwebsockets_static.a               -lws2_32 -lbcrypt -liphlpapi -lole32               $(shell $(CC) -print-file-name=libwinpthread.a)
  OUT_DIR  = build-win
  OUT_NAME = weron-signaler-c.exe
  LDFLAGS  = $(LWS_LIBS) -static-libgcc -Wl,--allow-multiple-definition
endif

ifeq ($(TARGET),mipsel)
  CC       = mipsel-linux-gnu-gcc
  LWS_CFLAGS = -I$(LWS_LINUX)/linux_mipsel_musl_static/include
  LWS_LIBS  = $(LWS_LINUX)/linux_mipsel_musl_static/lib/libwebsockets.a -lpthread
  OUT_DIR  = build-mipsel
  OUT_NAME = weron-signaler-c-mipsel
  LDFLAGS  = $(LWS_LIBS) -static
endif

ifeq ($(TARGET),mipsel-musl-sf)
  CC       = /opt/musl/mipsel_le/mipsel-linux-muslsf-cross/bin/mipsel-linux-muslsf-gcc
  LWS_CFLAGS = -I$(LWS_LINUX)/linux_mipsel_musl_static/include
  LWS_LIBS  = $(LWS_LINUX)/linux_mipsel_musl_static/lib/libwebsockets.a -lpthread
  OUT_DIR  = build-mipsel-musl-sf
  OUT_NAME = weron-signaler-c-mipsel-sf
  LDFLAGS  = $(LWS_LIBS) -static
endif

ifeq ($(TARGET),mipsisa64r6)
  CC       = mipsisa64r6-linux-gnuabi64-gcc
  LWS_CFLAGS = -I$(LWS_LINUX)/linux_mips_musl_static/include
  LWS_LIBS  = $(LWS_LINUX)/linux_mips_musl_static/lib/libwebsockets.a -lpthread
  OUT_DIR  = build-mipsisa64r6
  OUT_NAME = weron-signaler-c-mipsisa64r6
  LDFLAGS  = $(LWS_LIBS) -static
endif

ifeq ($(TARGET),armhf)
  CC       = arm-linux-gnueabihf-gcc
  LWS_CFLAGS = -I$(LWS_LINUX)/linux_armv7_musleabihf_static/include
  LWS_LIBS  = $(LWS_LINUX)/linux_armv7_musleabihf_static/lib/libwebsockets.a -lpthread
  OUT_DIR  = build-armhf
  OUT_NAME = weron-signaler-c-armhf
  LDFLAGS  = $(LWS_LIBS) -static
endif

CFLAGS  = -Wall -Wextra -O2 -g $(LWS_CFLAGS)

SRCS    = main.c relay.c
OBJS    = $(addprefix $(OUT_DIR)/,$(SRCS:.c=.o))

.PHONY: all clean

all: $(OUT_DIR)/$(OUT_NAME)

$(OUT_DIR)/$(OUT_NAME): $(OBJS)
	mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(OUT_DIR)/%.o: %.c relay.h
	mkdir -p $(OUT_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf build-linux build-win build-mipsel build-mipsel-musl-sf build-mipsisa64r6 build-armhf
