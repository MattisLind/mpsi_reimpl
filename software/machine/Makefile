#-------------------------------------------
# TOP-LEVEL TARGETS

all: machine.t98

clean: 
	rm mbb.f98 mbb.lst machine.t98

tar:
	tar cvzf mbb.s blockscan.bas blockscan.f98 Makefile


#-------------------------------------------

machine.t98: mbb.f98 blockscan.f98
	tp9830 mbb.f98 blockscan.f98 >machine.t98

mbb.f98: mbb.s
	asm9830 -x -a mbb.lst -o mbb.f98 mbb.s
