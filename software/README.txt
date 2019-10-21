pgm9830 README		2014 May / bhilpert
===========================================

The pgm9830 file structure includes the source to produce 3 executables and 1 tape image file:

	tp9830		- Tape image manipulation and control for the 9830 internal tape drive
			  via the MPSI I/O server.
			  For use, see help command when running, or tp9830/helpdoc.txt.

	tp9865		- Tape image manipulation and control for the 9865 tape drive
			  via the 9865 I/O adapter.
			  For use, see help command when running, or tp9830/helpdoc.txt.

	mpsiServer	- cassette-tape/printer/paper-tape-reader server for use with
			  MPSI I/O device.
			  For use, see comment in mpsiServer/main.c

	machine.t98	- A tape image with the machine support binary block
			  and blockscan BASIC program.

Subdirectories:
	tp9830		- source for the tp9830 & tp9865 programs.

	mpsiServer	- source for the mpsiServer program.

	util		- source for a few C utility modules.

	machine		- source and tape files to produce the machine.t98 tape image.

These sources are currently targeted primarily for compilation on an RPi, although the tp9830/65 programs
may be compiled on other suitable unix systems using:
	'make mode=standalone'
to produce a program without the tape control but still provides for tape image manipulation.

The machine.t98 may also be produced on other unix systems. Assembly requires the asm9830 assembler.
