char* HelpSrc="\
#	int*roduction\n\
--------------------------------------------------\n\
--	tp9830 & tp9865\n\
--	User Documentation\n\
--------------------------------------------------\n\
\n\
tp9830 and tp9865 are programs providing for manipulation of 9830\n\
tape images and for controlling a 9830 or 9865 cassette tape drive.\n\
tp9830 provides control of the internal 9830 tape drive through the MPSI I/O interface.\n\
tp9865 provides control of a 9865 drive with a 9865 I/O adapter installed.\n\
\n\
Running tp9830/65 on RPi/linux will generally require sudo to access the GPIO pins to\n\
control the tape drive. A version of tp9830/65 may be compiled that does not access the GPIO,\n\
useful for stand-alone manipulation of tape images.\n\
\n\
In the following:\n\
	- a 'tape image' is a byte-level copy of the contents of a 9830 cassette tape.\n\
	- a 'tape image file' (.t98) is a file version of the above.\n\
	- a 'tape file' (.f98) is a 9830 BASIC, data, key or bin file normally contained\n\
	  within a tape or tape image. That is, there may be multiple tape files on a tape,\n\
	  or within a tape image. Tape files are 16-bit-word-oriented.\n\
\n\
	- Number arguments may be in standard decimal, octal or hexadecimal syntax.\n\
	- Characters after a '*' in a keyword are optional.\n\
\n\
tp9830/65 maintains and works upon a buffer of one tape image in memory.\n\
\n\
\n\
#	images\n\
Tape Images:\n\
------------\n\
\n\
Due to the characteristics of the 9830 tape drives, the control-byte modifier of BOF bytes\n\
is not recovered when reading, although the 0x3C BOF byte itself is.\n\
The BOF bytes then, must be determined by assuming that a 0x3C byte 'outside' a tape file\n\
header or contents - as determined sequentially by header data - is such a BOF byte.\n\
A recovered image must have the BOFs marked as control bytes before using the image.\n\
See the clean command.\n\
\n\
There is no explicit end-of-tape indicator for tapes.\n\
Generally, tapes will have an 'EOT' file - an empty file after the last 'used' file.\n\
The 9830 firmware marks an extra file of the same size as the last file to this end.\n\
tp9830/65 marks a file of size 13 words for EOT files.\n\
\n\
\n\
#	use\n\
Use:\n\
----\n\
\n\
tp9830 [-u] [<tapeFile>.. > <tapeImageFile>]\n\
tp9865 [-u] [<tapeFile>.. > <tapeImageFile>]\n\
\n\
	-u:		Display use message and exit.\n\
	<tapeFile>:	Tape files to be processed into a tape image file.\n\
\n\
tp9830 & tp9865 execute in either interactive mode or in single-execution mode.\n\
If no arguments are specified at invokation, the interactive mode is entered.\n\
\n\
If any tape files are specified at invokation the tape file(s) are inserted into a\n\
tape image in the order they appear on the command line, the image is then output to\n\
the standard output in XNS format.\n\
\n\
When starting tp9830 in interactive mode it will attempt to communicate with the\n\
MTAPE program running on the 9830, waiting for MTAPE to respond.\n\
Ctl-C/interrupt may be used to terminate the wait and get to the tp9830 prompt.\n\
\n\
\n\
#\n\
Commands:\n\
--------\n\
\n\
#C	o*pen <fname> [dbb]\n\
\n\
	Read the tape image file <fname> into the image buffer.\n\
	The image file is interpreted as a sequence of bytes in XNS format,\n\
	with BOF-bytes having a value greater than 255.\n\
	If 'dbb' is specified, the image file is interpreted as double-byte binary.\n\
\n\
\n\
#C	sa*ve <fname>\n\
\n\
	Save the image buffer to file <fname>.\n\
\n\
\n\
#C	b*uf [<addr> [<count>]]\n\
\n\
	Display a range of bytes in the image buffer.\n\
\n\
	The bytes are displayed in 2 forms:\n\
		- hex bytes,\n\
		- ASCII characters.\n\
\n\
	Bytes that are marked as BOF-bytes will appear with a '*' preceding the hex value.\n\
\n\
\n\
#C	l*ist [<fnum>]\n\
\n\
	List the tape files in the image buffer, or display a tape file.\n\
	Some analysis is done, such as checking checksums.\n\
\n\
	If <fnum> is specified, the header and contents of tape file <fnum> will be displayed.\n\
	The contents are displayed in 3 forms:\n\
		- hex bytes,\n\
		- octal words,\n\
		- ASCII characters, byte-swapped such that they are in readable order.\n\
\n\
\n\
#C	mark [<numFiles> [<fSpace> [<fNum>] ] ]\n\
\n\
	Mark the image buffer with <numFiles> empty tape files, each of size <fSpace> words,\n\
	starting at file <fNum>.\n\
\n\
	If <fNum> is not specified, marking starts with the last file in the image (overwritten).\n\
	If <fNum> is specified, the file must be present in the image.\n\
\n\
	If the image buffer was empty, 300 bytes of zero-padding are marked at the start of the buffer,\n\
	before marking the new tape files.\n\
\n\
	<fspace> defaults to 1000.\n\
\n\
	An additional EOT tape file of size 13 words is marked after the last of the specified tape files.\n\
\n\
	If <numFiles> is not specified an EOT file is appended to the end of the image buffer,\n\
	to provide for situations where the EOT file is missing.\n\
\n\
	Zero-padding of 25% of the file size is included after each tape file.\n\
\n\
\n\
#C	load <fname> [<fnum>]\n\
\n\
	Read the external file <fname> into the image buffer as the contents of tape file <fnum>.\n\
	If <fnum> is not specified, the last tape file in the image buffer is remarked and\n\
	overwritten and a new EOT file marked. If there were no tape files in the image buffer,\n\
	a new tape file of the requisite size is marked and written and an EOT file marked.\n\
\n\
	File <fname> should be a tape file in XNS format as produced by the store command or\n\
	the asm9830 assembler with the -x flag.\n\
\n\
	The tape file checksums and content length are updated.\n\
	The tape file space header value is updated if it was less than the content length,\n\
	but the actual file space in the image is not checked or increased (*fix!!!*).\n\
\n\
\n\
#C	store <fnum> <fname>\n\
\n\
	Write the tape file <fnum> from the image buffer to the file <fname>.\n\
	The tape file will be written as 16-bit words in XNS format,\n\
	and will include the header and checksums.\n\
\n\
\n\
#C	clean*up [z*eroes] [bof*s] [seq*uence] [ch*ecksums] [pad*ding] [tr*uncate]\n\
\n\
	Cleanup various aspects of the image buffer.\n\
	If no option keywords are specified all options are performed.\n\
\n\
	Options:\n\
		zeroes		- Non-zero bytes that are 'outside' tape files are cleared.\n\
		BOFs		- 0x3C bytes that are outside tape file header and content\n\
				  area will be marked as BOF bytes.\n\
		sequence	- File numbers are sequenced starting from 0.\n\
		checksums	- All checksums are recalculated.\n\
		padding		- Insert zero bytes as necessary to ensure at least 300 bytes\n\
				  padding at BOT, at least 25% padding for each tape file,\n\
				  and 300 bytes at EOT.\n\
		truncate	- The image buffer is truncated after the last byte of the\n\
				  last file, although if padding is specified the EOT padding\n\
				  is still ensured.\n\
\n\
\n\
#C	tr*uncate [<length>]\n\
\n\
	Truncate the image  buffer to <length> bytes.\n\
	<length> defaults to 0.\n\
\n\
\n\
#C	z*eroes <addr> <count>\n\
\n\
	Zero the specified range of bytes in the image buffer.\n\
	<addr> starts at 0.\n\
\n\
\n\
#C	bs*et <addr> <byte> [<byte>]\n\
\n\
	Set the value of one or two bytes in the image buffer.\n\
	<addr> starts at 0.\n\
\n\
\n\
#C	n*umber <n>\n\
\n\
	Display <n> in decimal, hex, and octal.\n\
	Provided for quick base conversion.\n\
\n\
\n\
#C	q*uit / EOF\n\
\n\
	Terminate the program.\n\
\n\
\n\
#\n\
\n\
The following commands control the tape drive:\n\
\n\
#C	rim*age [full] [fast] [<maxBytes>]\n\
\n\
	Read bytes from the tape drive into the image buffer.\n\
\n\
	The read terminates if no bytes are seen for several seconds.\n\
	*** timeout not implemented yet in tp9830.\n\
	If 'full' is specified the timeout is extended so reading will continue until leader\n\
	or some other exception occurs.\n\
\n\
	if <maxBytes> is specified, reading will terminate when that number of bytes are read.\n\
\n\
	'fast' does a high-speed read (generally not reliable).\n\
\n\
\n\
#C	wimage\n\
\n\
	Write the image buffer to the tape drive.\n\
\n\
\n\
#C	sta*tus\n\
\n\
	Display the tape drive status.\n\
	Actually executes a 'continue' command on the drive.\n\
\n\
#C	rew*ind\n\
\n\
	Send a rewind command (RRH) to the tape drive.\n\
\n\
	Note that if the tape is already rewound, the drive will still attempt to rewind the tape\n\
	because the drive cannot distinguish leader at the EOT versus BOT.\n\
	To avoid having the motor left jammed on, in this command if the tape is in leader and \n\
	does not move out of leader within 3 seconds, a stop command will be issued.\n\
\n\
\n\
#C	sto*p\n\
\n\
	Send a stop command to the tape drive.\n\
\n\
\n\
#C	dr*ive rfn|rfh|rrn|rrh|wfn|stop|cont*inue [control]\n\
\n\
	Send the specified low-level command to the tape drive.\n\
	If control is specified, the control-mode bit is set in the command.\n\
\n\
\n\
";
