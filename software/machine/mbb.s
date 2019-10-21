
/************************************************************************
*
*		HP9830 Machine Binary-Block
*
*		This assembly module is a 9830 Binary-Block (machine language extension)
*		providing a few commands and functions for machine level support.
*
*		!! Note: MDUMP command currently uses in-line variables and so will
*		only run from RAM. Variables must be moved to RAM temps for this
*		block to run from ROM.
*
*		2013 Oct: created / bh
*
*		2013 Oct: SPUTNIK command
*		2013 Nov: MDUMP command
*
*		2013 Nov: MARG(n) function
*		2013 Nov: MPEEK(addr) function
*		2013 Nov: MPOKE(addr,n) function
*
*		2013 Nov: Converted to new asm syntax / rf
*		2013 Nov: Bitwise AND & OR functions
*
*		2014 Feb: MARG function removed
*		2014 Feb: MTAPE command added (MPSI Tape Relay Program)
*
************************************************************************/

					// System Imports
FLTFA		equ 000226
FXFLA		equ 000227
XFAR2		equ 000444

FLGBT		equ 000062
//FLTRA		equ 000225
E5		equ 000274
FETCH		equ 001217
MODE		equ 001416
TEMPS		equ 001472	
T1		equ 001644		//  ST area, syntax temps

FSCA		equ 000202
RETN3		equ 000356
RETN2		equ 000357
COMCE		equ 000510
LPCKE		equ 000521
RPCKE		equ 000534
GNEXT		equ 001055	
ST		equ 001641		//  ST area, syntax temps
SBPTR		equ 001706	
RSPTR		equ 001777

					// Configuration

		org 057777+1 -(28+12+3+10+6) -5 -(27+13+10+14+5) -24 -5 -5 -5 -5 -177	// align end of program to end of page, 57000 for easy reference in RAM block
		//org 057777+1-(BlockID-.)	// in principle, manual calc if the assembler can't, 057777+1 to keep out of zero-page


/*************************************************************
* MPSI Tape Drive Relay Program (9830 Command)
*
* This program, operating in conjunction with an external control program communicating through the MPSI,
* allows the external control program to perform operations with the internal 9830 tape drive.
* This program relays commands and responses back and forth between the controller and the tape drive:
* waiting for directives received from the controller through the MPSI, executing an according command on the tape drive,
* and sending a reply back through the MPSI.
* Timing issues and limitations of the 9830 IO system dictate that writing be treated differently than other drive commands.
*
* Commands from the controller are referred to as directives here for the sake of distinguishing them from tape drive commands.
* Directives from the controller are received in 11 bits:
*
*	Command execute:	---- 001k axxx cccc
*	Write:			---- 100k xxxx x321
*	Push-Byte:		---- 010k wwww wwww
*
*		c = tape drive command
*		a = noWait option (don't wait for command completion (flag) by drive)
*		w = write data
*		1 = clear leader & write: wait to get out of leader, then write buffer
*		2 = find BOF & write: back up over BOF, then read forward to BOF and write buffer
*		3 = write next BOF mark: write a BOF control-byte followed by a bunch of zeroes
*		k = IO ACK bit
*		u = select code
*		x = not used
*
* The Command directive executes the specified command on the tape drive:
*
*				uuuu cccc 0000 0000
*
* Setting the 'a' bit indicates to wait for command completion (CEO/flag) before replying to the controller.
*
* The Write directive executes optional sub-directives in sequence from 1 to 3.
* The options are used in appropriate combinations to produce a tape image from segments.
*
* The Push-Byte directive saves a byte in a stack buffer and decrements the stack pointer.
* The stored bytes are written to tape in the opposite directon to which they were stored, so the push-bytes must be
* sequenced accordingly.
* The stack pointer is reset to the stack base after any other directive.
*
* The stack buffer is the 9830 RAM, starting at a location specified in this code and growing down.
* Currently there is no limit check, it is up to the controller to be aware of how many bytes can be pushed.
* A BASIC program or other data in the 9830 memory may be overwritten by the stack buffer, it is a good idea to
* execute a SCRATCH after using MTAPE. SCRATCH is essentially a reset of the 9830, it restarts the firmware very near
* the machine reset.
*
* The 11-bit response from the tape drive is sent back in one 12-bit transfer.
*
*	Reply to controller:	---- dsss rrrr rrrr
*
*		d = IO direction bit
*		s = tape drive status
*		r = tape drive read data
*
* The 4-bit status from the drive is reduced to 3 as bit SI0/DI8 from the internal drive is always 1.
* To avoid conflict with the GP direction bit, the nCIN status bit is moved down to SI0/DI8.
*
* Log:
*	V1,2: 2-word-dir/1-word reply, too slow for write
*	V3: 1 word exchange, still too slow for write
*	V4: buffered write
*
**************************************************************/
						// init
Cmd_MTapeRelay		RAR 16				// clear reply
						// reset stack pointer to stack base (high address)
_DirLoopCLRSTK		LDB MTR_STACKBASE
			SBL 1				// make into byte address
			STB MTR_stackPtr
						// adjust tape status in A for reply to controller
_DirLoop		AND HexEFF			// mask out superfluous drive status bit
			LDB A				// save briefly
			SAR 3				// move nCIN bit down
			AND Hex100			// leave nCIN bit
			IOR B				// add everything else back in
						// call for directive from controller, A contains reply
			IOR MPSI_GPSC_INPUT		// add in select code and input call
			STF 1				// interrupts off
			OTA 1				// set call and reply on bus
			STC 1,C				// initiate op (pulse assert on CEO), interrupts on
						// wait for controller
_waitForDir		SFC 0				// check stop key
			JMP _Stopped			// stopped
			STF 1				// interrupts off
			OTA 1				// select device
			STF 1				// load IO reg from DI bus
			LIB 1,C				// IO reg into B, interrupts on
			RBR 9				// move ack bit (0x0100) to sign
			SBP _waitForDir			// 0=busy, loop while busy
			RBR 7				// rotate directive through into place
						// dispatch received directive
			LDA B				// use A, whole directive saved in B
			AND MTC_DIR_MASK		// mask to get directive bits
			CPA MTC_DIR_CMD			// check for command directive
			JMP _DirCmd
			CPA MTC_DIR_WRITE		// check for write directive
			JMP _DirWrite
			CPA MTC_DIR_PUSHBYT		// check for push byte directive
			JMP _DirPushByt
			JMP _Error			// bad directive

			//-----------------------------
			// Command Directive
			// Execute a specified tape drive command

						// form tape drive command
_DirCmd			LDA B				// get back directive
			AND HexF			// mask to get 4-bit tape command
			SAL 8				// shift up, clear low bits
						// send command to tape drive
			IOR TD_SC			// add in drive select code
			STF 1				// interrupts off
			OTA 1				// send to DO bus
			STC 1				// and initiate operation (assert CEO)
						// if noWait, just send status to MPSI
			LDA B				// get back directive
			AND MTC_C_WAIT
			RZA _cmd_waitLoop		// if wait modifier bit was 1, go wait for completion
						// just get the status from the drive
			STF 1				// DI bus to IO reg
			JMP _cmd_getSR
						// wait for drive operation completion
_cmd_waitLoop		SFC 0				// check for stop key
			JMP _Stopped
			SFS 1				// loop until drive de-asserts CEO flag (and loads IO reg from DI bus)
			JMP _cmd_waitLoop
						// completed, get drive status and read-byte
_cmd_getSR		LIA 1,C				// load drive data from IO reg into A
			//STF 1				// load IO reg from DI bus again to get status
			//MIA 1,C				// merge status, deassert CEO, deselect device, enable interrupts
						// go send reply and wait for next command from controller
			JMP _DirLoopCLRSTK

			//-----------------------------
			// Push-Byte Directive
			// Push the byte received from controller onto the buffer stack.
			// The stack grows down in memory.
			// Bytes are pushed in the reverse order from which they will be popped and written to tape.
			// Bytes will be written to tape low-byte first, so they are pushed here high-byte first.

_DirPushByt		LDA B				// get back directive
			AND HexFF			// mask to get byte to push onto buffer stack
						// figure out address
			DSZ MTR_stackPtr		// decrement stack pointer
			LDB MTR_stackPtr		// (safety, should not be executed)
			LDB MTR_stackPtr		// byte address into B
			RBR 1				// make word address, and LSB to sign
			SBP _pb_second			// select on bit 15, which was LSB of byte address
			SBL 1				// get rid of bit 15
			SBR 1
						// byte address was odd, first in receipt, save in high byte
			SAL 8				// move to high byte, clear low byte
			JMP _pb_push
						// byte address was even, second in receipt, save in low byte
_pb_second		IOR B,I				// combine with high byte from stack
						// save in buffer stack and go wait for next directive
_pb_push		STA B,I				// save on stack
			JMP _DirLoop

			//-----------------------------
			// Write Directive
			// Check the sub-directive bits in sequence and perform if set

_DirWrite		STB MTR_directive		// we will need this
						//-----------------------------
						// Clear-Leader Sub-directive
			LDA MTR_directive
			AND MTC_W_CLRLDR		// check for sub-directive
			RZA _w_clrLdr
			JMP _w_CFFBOF
						// start tape motion, loop until out of leader
_w_clrLdr		LDA TD_SC_RFN			// start drive motion
			JSM Td_ExecCmd
			CLF 1				// deassert CEO
_w_ldrLoop		JSM Td_Status			// get drive status in A
			LDB A				// hang on to status
			AND TD_ST_nCIN_nWPT
			SZA _w_ldrChk
			JMP _w_FIN			// no cassette or no write, get out of here
_w_ldrChk		LDA B				// get status back
			AND TD_ST_LDR
			RZA _w_ldrLoop			// loop while leader bit set
			JMP _w_WBUFFER			// tape motion continues
						//-----------------------------
						// Find-BOF Sub-directive
_w_CFFBOF		LDA MTR_directive
			AND MTC_W_FBOF			// check for sub-directive
			RZA _w_fbof
			JMP _w_CFWBOF
						// seek backwards over BOF, stop, then forward to BOF
_w_fbof			LDA TD_SC_RRNC			// read reverse normal speed looking for control byte
			JSM Td_ExecCmd
_w_revLoop		SFC 1				// loop till control byte seen
			JMP _w_delay			// found
			SFC 0				// check for stop key
			JMP _Stopped
			JMP _w_revLoop
							// delay (65mS by HP) and stop, check status
_w_delay		LDA MTR_REV_DELAY		// back up before the BOF
_w_delayLoop		RIA _w_delayLoop		// so drive can get up to speed in forward
			JSM Td_ExecStop			// stop
			AND TD_ST_FATAL			// check status
			SZA _w_fwd
			JMP _w_FIN			// no good, get out of here
							// now read forward
_w_fwd			LDA TD_SC_RFNC	
			JSM Td_ExecCmd
_w_fwdLoop		SFS 1				// loop till control byte seen again
			JMP _w_fwdLoop
							// tape motion continues, switch to writing in next section
						//-----------------------------
						// Write-Buffer Sub-directive
						// write the stack buffer, code adapted from HP firmware listing, UKP-pg??
_w_WBUFFER		LDB MTR_stackPtr		// get buffer starting address
			SBR 1				// convert byte address to word address
			SEC .+1,C			// clear E, E is byte-of-word flag
			LDA TD_SC_WFN			// set up write command
_w_prepLoop		SFS 1				// ensure ready, normally already is
			JMP _w_prepLoop
			OTA 1				// select-code and command to DO bus (set high byte of IOR for rest of write)
_w_wordLoop		CPB MTR_STACKBASE		// check for end of stack buffer
			JMP _w_CFWBOF			// write-buffer is finished, on to next sub-directive
			LDA B,I				// load two bytes to write
_w_waitLoop		SFS 1				// loop till ready
			JMP _w_waitLoop
			OTA 0				// low byte to DO bus, start command (CEO), and rotate A 8 bits
			SEC _w_waitLoop,S		// if E clear, set E, loop for 2nd byte
			SEC .+1,C			// clear E
			RIB _w_wordLoop			// increment to next word
			JMP _w_wordLoop			// (pedantic safety, should never be here)
							// tape motion continues
						//-----------------------------
						// Write-BOF Sub-directive
_w_CFWBOF		LDA MTR_directive
			AND MTC_W_WBOF			// check for sub-directive
			RZA _w_wbof
			JMP _w_FIN
						// write BOF and bunch of zeroes
_w_wbof			LDB MTR_BOF_ZEROES		// count of BOF and zeroes
			LDA TD_SC_WFNC_3C		// start with BOF
_w_zLoop		SFS 1				// wait ready
			JMP _w_zLoop
			OTA 1				// output command & data
			STC 1				// and execute (assert CEO)
			LDA TD_SC_WFN			// rest are zeroes
			SIB _w_FIN			// fin when B increments to 0
			JMP _w_zLoop
						//-----------------------------
						// Write Directive Finish
_w_FIN			JSM Td_ExecStop			// stop the drive, drive status returned in A
			JMP _DirLoopCLRSTK		// and go wait for next directive

			//-----------------------------
			// Termination

_Error			STC 2				// beep for error indication to user
_Stopped		JSM Td_ExecStop			// a little safety
			RET				// return to system


			//------------------------------------------------------------
			// Quick tape drive subroutines

Td_ExecStop		LDA TD_SC_STOP
			JSM Td_ExecCmd			// execute command
			CLF 1				// deassert CEO, deselect device, interrupts on
						// flow through to get status
Td_Status		LDA TD_SC
			STF 1				// interrupts off
			OTA 1				// select device
			STF 1				// DI bus to IO reg
			LIA 1,C				// IO reg to A, deselect device, interrupts on
			RET


			//------------------------------------------------------------
			// Execute tape command, leave CEO asserted, interrupts off

Td_ExecCmd		STF 1
			OTA 1
			STC 1
			RET


			//------------------------------------------------------------
			// Variables *** NOTE: won't work in ROM of course
						
MTR_stackPtr		dw var				// byte address
MTR_directive		dw var


			//------------------------------------------------------------
			// Static Data			
						// configuration
MTR_STACKBASE		dw 050000			// address of high end of buffer stack, stack will go down from the specified address

MTR_REV_DELAY		dw -4934			// loop counter for reverse-BOF-seek delay

MTR_BOF_ZEROES		dw -50				// count of zeroes to output after writing BOF

TAPE_SC			equ (10<<12)			// select code of internal tape drive

						// MPSI Tape Controller directives
MTC_DIR_MASK		dw 0x0E00			// mask for directive
MTC_DIR_CMD		dw 0x0200
MTC_DIR_PUSHBYT		dw 0x0400
MTC_DIR_WRITE		dw 0x0800

MTC_C_WAIT		dw 0x0080			// command-directive option to wait for flag
						// write sub-directives
MTC_W_CLRLDR		dw 0x0001			// get clear of leader & write buffer to tape
MTC_W_FBOF		dw 0x0002			// find BOF & write buffer to tape
MTC_W_WBOF		dw 0x0004			// write BOF and trailer

						// tape drive commands with select code
TD_SC			dw TAPE_SC
TD_SC_RFN		dw TAPE_SC | 0x000
TD_SC_RFNC		dw TAPE_SC | 0x000|0x800
TD_SC_RRNC		dw TAPE_SC | 0x200|0x800
TD_SC_WFN		dw TAPE_SC | 0x400
TD_SC_WFNC_3C		dw TAPE_SC | 0x400|0x800 | 0x3C	// and with BOF control byte
TD_SC_STOP		dw TAPE_SC | 0x500

						// tape drive status
TD_ST_nCIN_nWPT		dw 0x800|0x200
TD_ST_LDR		dw 0x400
TD_ST_nWPT		dw 0x200
TD_ST_FATAL		dw 0x800|0x400|0x200

MPSI_GPSC_INPUT		dw (8<<12) | 0x0800		// select code for MPSI GP device, with input call

HexFF			dw 0xFF
HexEFF			dw 0xEFF
Hex100			dw 0x100


/*************************************************************
* Peek Function
*
* In BASIC:	<data> = MPEEK( <address> )
* Return the contents of the memory location specified by <address>.
* <address>: range [0..32767]
* <data>: range [-32768..32767]
*
**************************************************************/

Fct1_MPeek	JSM XFAR2+1		// arg to AR2
		JSM FLTFA,I		// convert AR2 float to int in B
		LDA A			// NoOP for error return for now

		LDB B,I			// get memory contents

		JMP FXFLA,I		// convert int B to AR2 float for result, return in called routine


/*************************************************************
* Poke Function
*
* In BASIC:	<previousData> = MPOKE(<address>,<data>)
* Set the memory location specified by <address> to <data>.
* Return the previous contents.
* <address>: range [0..32767]
* <data>: range [-32768..32767]
*
**************************************************************/

Fct2_MPoke	JSM ParseDblParam	// get parameters 1st->T1, 2nd->B

		LDA T1,I		// hang on to about-to-be previous memory contents
		STB T1,I		// the actual POKE: set memory contents

		LDB A			// return previous contents
		JMP FXFLA,I		// convert int B to AR2 float for result, return in called routine


/*************************************************************
* Binary AND Function
*
* In BASIC:	<result> = MAND( <val1>, <val2> )
* Return the 16 bit AND of <val1> and <val2>.
* <val1> <val2> <result>: range [-32768..32767]
*
**************************************************************/

Fct2_MBAND	JSM ParseDblParam	// get parameters 1st->T1, 2nd->B

		LDA B
		AND T1			// do the AND

		LDB A			// return previous contents
		JMP FXFLA,I		// convert int B to AR2 float for result, return in called routine


/*************************************************************
* Binary OR Function
*
* In BASIC:	<result> = MOR( <val1>, <val2> )
* Return the 16 bit OR of <val1> and <val2>.
* <val1> <val2> <result>: range [-32768..32767]
*
**************************************************************/

Fct2_MBOR	JSM ParseDblParam	// get parameters 1st->T1, 2nd->B

		LDA B
		IOR T1			// do the OR

		LDB A			// return previous contents
		JMP FXFLA,I		// convert int B to AR2 float for result, return in called routine


/*************************************************************
* Double Parameter Support Routine
*
* Syntax checking and parameter fetching for functions with two parameters.
* Code obtained from Extended I/O Option, UKP-pg550.
* FLTRA changed to FLTFA.
*
* Return: T1 = 1st param, B = 2nd param
*
**************************************************************/

ParseDblParam	LDA MODE		// fork for syntax vs execution
		SAM _syntaxChk
		JSM FETCH		// get 1st param
		JSM FLTFA,I		// convert to int in B
		LDB FLGBT		// return+1: overflow
		STB T1			// return+2: save in tmp
		JSM FETCH		// get 2nd param
		JSM FLTFA,I		// convert to int in B
		JMP E5-1,I		// return1: err
		ISZ TEMPS		// ?
		RET		

_syntaxChk	DSZ RSPTR		// this fork returns directly, remove stack level.
		LDA TEMPS,I		// save left-bracket count
		STA ST+1		// "
		JSM GNEXT		// get next non-blank character
		ISZ SBPTR		// ?
		JSM LPCKE		// left-paren check
		JSM FSCA,I		// parse formula (1st param)
		JSM COMCE		// comma check
		JSM FSCA,I		// parse formula (2nd param)
		LDB ST+1		// restore left-bracket count
		STB TEMPS,I		// "
		JSM RPCKE		// right-paren check
		JMP RETN3		// return+3


/*************************************************************
* Memory Dump Command
*
* Dump the internal memory of the 9830 to the printer device.
* The dump starts at 0 and continues to MemDump_endAddress-1.
* The output is hex PBF format with a linefeed and address every 16 words.
*
**************************************************************/

Cmd_MDump	SAR 16			// start at address 0
		STA _memPtr

_loopWord	SFC 0			// check stop key		
		JMP _fin		// stop pressed, get out

		LDA _memPtr		// check for end
		CPA _endAddress
		JMP _fin

		AND HexF		// print LF/addr every 16 words
		RZA _printMem

		LDA AscLF		// print LF
		JSM PutByte

		LDA _memPtr		// print address
		JSM PrintHex4
		LDA AscColon
		JSM PutByte

_printMem	LDA _memPtr
		LDA A,I			// get memory contents
		JSM PrintHex4		// and print
		LDA AscTilde		// with separator
		JSM PutByte

		ISZ _memPtr		// on to next word, 'skip' not used
		JMP _loopWord

_fin		LDA AscLF		// print LF
		JSM PutByte
		RET			// return to system

_memPtr		dw var
_endAddress	dw 060000


		/*------------------------------------------------------------
		* Print A as 4 hex digits
		*/

PrintHex4	STA _word		// save in local temporary

		RAR 12			// move bits down
		JSM PrintHex1		// print as hex digit

		LDA _word		// .. 3 more times
		RAR 8
		JSM PrintHex1

		LDA _word
		RAR 4
		JSM PrintHex1

		LDA _word
		JSM PrintHex1

		RET

_word		dw var


		/*------------------------------------------------------------
		* Print low 4 bits of A as 1 hex digit
		*/

PrintHex1	AND HexF		// mask out upper bits
		ADA _M10		// subtract 10
		SAP _alpha		// if still +, then A..F
		ADA _N09		// convert to ascii 0..9
_alpha		ADA _NAF		// convert to ascii
		JSM PutByte		// and output
		RET

_M10		dw -10
_N09		dw 10+'0'-'A'
_NAF		dw 'A'


		/*------------------------------------------------------------
		* Output low-byte of A to printer device
		*/

PutByte		SFC 0			// check stop key
		RET			// stop pressed, get out
					// *** check if device busy
		LDB PrinterSelCd
		STF 1			// disable interrupt
		OTB 1			// select device
		STF 1			// load dev status
		LIB 1,C			// with interrupt back on
		RBR 9			// move dev busy bit to sign
		SBP PutByte		// 0=busy, go round again
					// *** output byte
		IOR PrinterSelCd	// form device code with byte
		STF 1			// interrupt off
		OTA 1			// output byte & code
		STC 1,C			// pulse CEO, interrupt back on

		RET


		/*------------------------------------------------------------
		* Misc / Util
		*/

PrinterSelCd	dw 15<<12		// print-device select code in upper 4 bits
		
AscColon	dw ':'
AscTilde	dw '~'
AscLF		dw 10

HexF		dw 0xF

var		equ 0			// symbolic def to indicate variable data rather than static


/*************************************************************
* Sputnik Command
*
**************************************************************/

Cmd_Sputnik	STC 2			// beep the 9830, beeper is control bit on channel 2
_delayLoop	SFC 0			// check for STOP key
		RET			// STOP key is pressed, return to sys
		RIA _delayLoop		// loop until zero for delay, inc delay counter (A)
		JMP Cmd_Sputnik		// go beep again


/*************************************************************
* Block Tables & Linkage
*
**************************************************************/
					// *** Function Table
ftMBOR		dw Fct2_MBOR-.
ftMBAND		dw Fct2_MBAND-.
ftMPoke		dw Fct2_MPoke-.
		bss  030			// move Poke,etc to opcode >=30 so we can do syntax checking for 2 params
ftMPeek		dw Fct1_MPeek-.
FctTbl		db 'M', 'P', 'E', 'E', 'K',		0x80 | (FctTbl-ftMPeek),	\
		   'M', 'P', 'O', 'K', 'E',		0x80 | (FctTbl-ftMPoke),	\
		   'M', 'B', 'A', 'N', 'D',		0x80 | (FctTbl-ftMBAND),	\
		   'M', 'B', 'O', 'R',			0xC0 | (FctTbl-ftMBOR)

					// *** Command Table
ctMDump		dw Cmd_MDump-.
ctMTapeRelay	dw Cmd_MTapeRelay-.
ctSputnik	dw Cmd_Sputnik-.
CmdTbl		db 'M', 'D', 'U', 'M', 'P',		0x80 | (CmdTbl-ctMDump),	\
		   'M', 'T', 'A', 'P', 'E',		0x80 | (CmdTbl-ctMTapeRelay),	\
		   'S', 'P', 'U', 'T', 'N', 'I', 'K',	0xC0 | (CmdTbl-ctSputnik)

					// *** An Empty Table
nullTbl		dw 0xFF00
					// *** Block Linkage
		dw nullTbl-.		// non-formula operator table offset
		dw FctTbl-.		// function table offset
		dw CmdTbl-.		// command table offset
		dw nullTbl-.		// statement table offset
BlockID		dw 050000		// semi-random choice


/*****************************************************************************
*		END
*****************************************************************************/
