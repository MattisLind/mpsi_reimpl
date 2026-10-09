GHDL ?= ghdl
YOSYS ?= yosys
BUILD := $(abspath ../../build/rev2/vhdl)
FIT := $(BUILD)/fit
POSTFIT := $(BUILD)/postfit
ATF_ROOT ?= /atf15xx_yosys
VHDL := rtl/mpsi_cpld.vhdl tb/hct165_pair.vhdl tb/tb_mpsi.vhdl
HP_PINS := $(addprefix hp_n_di_,0 1 2 3 4 5 6 7 8 9 10 11) hp_n_cfi hp_n_ssi
FIT_OPTIONS ?=
.PHONY: test test-local synth synth-local fit fit-local postfit postfit-local build-image build-sim-image
test:
	@if command -v $(GHDL) >/dev/null 2>&1; then $(MAKE) test-local; \
	else python3 ../../scripts/rev2_hdl.py test; fi
test-local:
	mkdir -p $(BUILD)
	$(GHDL) -a --std=08 --workdir=$(BUILD) $(VHDL)
	$(GHDL) -e --std=08 --workdir=$(BUILD) -o $(BUILD)/tb_mpsi tb_mpsi
	cd $(BUILD) && for rate in 9000000 18000000 24000000; do \
	  $(GHDL) -r --std=08 --workdir=$(BUILD) tb_mpsi -gSPI_HZ=$$rate \
	    --assert-level=error --vcd=$(BUILD)/mpsi-$$rate.vcd || exit $$?; done
build-image:
	python3 ../../scripts/rev2_hdl.py build-image
build-sim-image:
	python3 ../../scripts/rev2_hdl.py build-sim-image
synth:
	python3 ../../scripts/rev2_hdl.py synth
fit:
	python3 ../../scripts/rev2_hdl.py fit
postfit:
	python3 ../../scripts/rev2_hdl.py postfit
synth-local:
	mkdir -p $(FIT)
	cp rtl/mpsi_cpld.vhdl $(FIT)/mpsi_cpld.vhdl
	python3 ../../scripts/rev2_synthesis.py $(ATF_ROOT) $(FIT)
	cd $(FIT) && $(YOSYS) -m ghdl -s mpsi.ys > synthesis.log
fit-local: synth-local
	cd $(FIT) && $(ATF_ROOT)/run_fitter.sh -d ATF1504AS -p PLCC44 -s 10 mpsi_cpld \
	  -preassign keep -strategy JTAG on -strategy pin_keep off \
	  -strategy open_collector = $(HP_PINS) -strategy VHDL_sim on $(FIT_OPTIONS) > fitter.log 2>&1
	python3 ../../scripts/check_rev2_pins.py --fit $(FIT)/mpsi_cpld.pin
postfit-local: fit-local
	python3 ../../scripts/rev2_postfit.py $(abspath ../..) $(FIT) $(POSTFIT)
	$(GHDL) -a --std=08 --workdir=$(POSTFIT) $(POSTFIT)/mpsi_fitted.vhdl $(POSTFIT)/mpsi_adapter.vhdl tb/hct165_pair.vhdl tb/tb_mpsi.vhdl
	$(GHDL) -e --std=08 --workdir=$(POSTFIT) -o $(POSTFIT)/tb_mpsi tb_mpsi
	cd $(POSTFIT) && for rate in 9000000 18000000 24000000; do \
	  $(GHDL) -r --std=08 --workdir=$(POSTFIT) tb_mpsi -gSPI_HZ=$$rate \
	    --assert-level=error --vcd=$(POSTFIT)/mpsi-$$rate.vcd || exit $$?; done
