#-------------------------------------------
# INIT

MPSIF    = mpsiServer/Makefile mpsiServer/*.[ch]

TP9830F  = tp9830/Makefile tp9830/*.[ch] tp9830/helpdoc.txt

UTILF    = util/Makefile util/*.[ch]

MACHINEF = machine/Makefile machine/mbb.s machine/mbb.f98 machine/blockscan.bas machine/blockscan.f98 machine/machine.t98


#-------------------------------------------
# TOP-LEVEL TARGETS

all:
	cd util; make
	cd mpsiServer; make
	cd tp9830; make
	cd machine; make

tar:
	tar cvzf pgm9830.tgz README.txt Makefile $(MPSIF) $(TP9830F) $(UTILF) $(MACHINEF)

clearout:
	rm -r util
	rm -r mpsiServer
	rm -r tp9830
	rm -r machine
	rm README.txt
