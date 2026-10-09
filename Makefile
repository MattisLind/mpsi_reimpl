.PHONY: test test-hdl test-firmware
test: test-hdl test-firmware
test-hdl:
	$(MAKE) -C hardware/rev2 test
test-firmware:
	$(MAKE) -C firmware/rev2 test
