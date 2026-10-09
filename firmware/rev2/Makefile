CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
BUILD := ../../build/rev2
.PHONY: test
test: $(BUILD)/test_firmware
	$<
$(BUILD)/test_firmware: src/mpsi_storage.c src/mpsi_tape.c tests/test_firmware.c include/mpsi_protocol.h include/mpsi_storage.h include/mpsi_tape.h
	mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -Iinclude src/mpsi_storage.c src/mpsi_tape.c tests/test_firmware.c -o $@
