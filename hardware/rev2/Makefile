IVERILOG ?= iverilog
VVP ?= vvp
BUILD := ../../build/rev2
.PHONY: test
test: $(BUILD)/tb_mpsi
	$(VVP) $<
$(BUILD)/tb_mpsi: rtl/mpsi_cpld.v tb/hct165_pair.v tb/tb_mpsi.sv
	mkdir -p $(BUILD)
	$(IVERILOG) -g2012 -Wall -s tb_mpsi -o $@ $^
