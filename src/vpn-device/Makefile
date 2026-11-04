# Cross-compile standalone vpn.device; freestanding RC-oriented SANA-II.
CC := m68k-amigaos-gcc
DEVFLAGS = -nostdlib -nostartfiles -O1 -Wall -Wextra -Werror \
           -Wno-unused-parameter -fomit-frame-pointer -m68000 -msoft-float
SANA_INCLUDE ?=
SANA_CPPFLAGS = $(if $(strip $(SANA_INCLUDE)),-I"$(SANA_INCLUDE)")

TOOLFLAGS = -O1 -m68020 -Wall -Wextra -Werror=implicit-function-declaration \
            -Werror=int-conversion

.PHONY: all test clean
all: build/vpn.device build/AOVPN-VPNProbe build/AOVPN-Diag build/AOVPN-Trace build/AOVPN-MiamiPeer

build/vpn.device: src/vpn_device.c src/vpn_ring.h src/vpn_diag.h src/vpn_trace.h src/sana2_core.inc
	mkdir -p build
	$(CC) $(CPPFLAGS) $(SANA_CPPFLAGS) $(DEVFLAGS) -o $@ src/vpn_device.c

build/AOVPN-Diag: tools/aovpn_diag.c src/vpn_diag.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(SANA_CPPFLAGS) $(TOOLFLAGS) -o $@ tools/aovpn_diag.c

build/AOVPN-Trace: tools/aovpn_trace.c src/vpn_trace.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(SANA_CPPFLAGS) $(TOOLFLAGS) -o $@ tools/aovpn_trace.c

build/AOVPN-MiamiPeer: tools/vpn_miami_peer.c src/icmp_echo_core.h
	mkdir -p build
	$(CC) $(CPPFLAGS) $(SANA_CPPFLAGS) $(TOOLFLAGS) -o $@ tools/vpn_miami_peer.c

build/AOVPN-VPNProbe: tools/vpn_probe.c
	mkdir -p build
	$(CC) $(CPPFLAGS) $(SANA_CPPFLAGS) $(TOOLFLAGS) -o $@ tools/vpn_probe.c

test:
	mkdir -p build
	cc -std=c99 -Wall -Wextra -Werror -O2 -o build/ring-test tests/ring_test.c
	./build/ring-test
	gcc -std=gnu99 -DVPN_HOST_MODEL=1 '-Dasm(x)=' -Wall -Wextra -Werror \
	  -Wno-unused-parameter -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
	  -I tests/mock_ndk -o build/host-device-test tests/host_device_test.c
	./build/host-device-test
	cc -std=c99 -Wall -Wextra -Werror -O2 -o build/test-icmp tests/test_icmp.c
	./build/test-icmp
	python3 tests/verify_reuse.py
	gcc -std=gnu99 -fsyntax-only '-Dasm(x)=' -Wall -Wextra -Werror \
	  -Wno-unused-parameter -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast \
	  -I tests/mock_ndk src/vpn_device.c tools/vpn_probe.c tools/aovpn_diag.c tools/aovpn_trace.c tools/vpn_miami_peer.c

clean:
	rm -rf build
