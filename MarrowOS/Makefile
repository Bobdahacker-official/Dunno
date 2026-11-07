CC = gcc
CFLAGS = -O2 -Wall -Wextra -Werror
LDFLAGS = -static
BUILD ?= build
.PHONY: all clean kernel run image
all: $(BUILD)/marrow-train $(BUILD)/marrow-init $(BUILD)/marrow-wifi $(BUILD)/marrow-desktop $(BUILD)/marrow-cc
$(BUILD):
	mkdir -p $@
$(BUILD)/marrow-train: src/train.c src/dot.S | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ -lm
$(BUILD)/marrow-cc: src/cc.c | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
$(BUILD)/marrow-init: src/init.c | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
$(BUILD)/marrow-wifi: src/wifi.c | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
$(BUILD)/marrow-desktop: src/desktop.c assets/font.h | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -lm -lutil
kernel:
	test -n "$(KERNEL_SRC)"
	./scripts/kernel-config.sh "$(KERNEL_SRC)"
	$(MAKE) -C "$(KERNEL_SRC)" -j$${JOBS:-3} bzImage
run:
	./run.sh
clean:
	rm -rf $(BUILD)
