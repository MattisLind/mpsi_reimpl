//********************************************************************************
//*
//*			9830/65 Tape Controller Program
//*
//* 2013 Jul   : V1
//* 2013 Jul 12: V2 updated for nEXCP bit added to adapter
//* 2013 Jul 13: V3 updated CTL, EXEC, AUTOR
//* 2013 Nov 02: V4 interface/command split, buffer mod commands, pbf
//* 2014 Feb:	 V5 converted to C for use with MPSI
//*
//********************************************************************************
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include "../util/util.h"
#include "../util/cp.h"
#include "t98.h"
#include "tpf.h"

extern void			LoadDBImage();
extern void			habDisplay();

extern char*			HelpSrc;


//--------------------------------------------------------------------------------
// Command Table


#define CMD_OPEN		1
#define CMD_SAVE		2
#define CMD_BDISPLAY		3
#define CMD_LIST		4
#define CMD_MARK		5
#define CMD_LOAD		6
#define CMD_STORE		17
#define CMD_CLEANUP		7
#define CMD_BTRUNCATE		8
#define CMD_BZEROES		9
#define CMD_BSET		10

#define CMD_RIMAGE		11
#define CMD_WIMAGE		12
#define CMD_STATUS		13
#define CMD_REWIND		14
#define CMD_STOP		15
#define CMD_DRIVE		16


static CP_TBL			cmdTbl[] =
 {	{ "o*pen",	"<fname> [dbb]",		"read tape image file into buffer",				CMD_OPEN	},
	{ "sa*ve",	"<fname>",			"save image buffer to file",					CMD_SAVE	},
	{ "b*uf",	"[<addr> [<count>]]",		"display buffer",						CMD_BDISPLAY	},
	{ "l*ist",	"[<fnum>]",			"list tape files in the buffer",				CMD_LIST	},
	{ "mark",	"[<numFiles> [<fSpace> [<fNum>]]]",	"mark empty tape files in buffer",			CMD_MARK	},
	{ "load",	"<fname> [<fnum>]",		"load a file into a tape file in image buffer",			CMD_LOAD	},
	{ "store",	"<fnum> <fname>",		"store a tape file from the image buffer in file <fname>",	CMD_STORE	},
	{ "clean*up",	"[z*eroes] [bof*s] [seq*uence] [ch*ecksums] [pad*ding] [tr*uncate]", "cleanup the image",	CMD_CLEANUP	},
	{ "tr*uncate",	"[<length>]",			"truncate buffer",						CMD_BTRUNCATE	},
	{ "z*eroes",	"<addr> <count>",		"zero range in buffer",						CMD_BZEROES	},
	{ "bs*et", 	"<addr> <byte> [<byte>]",	"change bytes in buffer",					CMD_BSET	},

	{ "rim*age",	"[full] [fast] [<maxBytes>]",	"read tape image into buffer",					CMD_RIMAGE	},
	{ "wimage",	"",				"write image buffer to tape",					CMD_WIMAGE	},
	{ "sta*tus",	"",				"display drive status",						CMD_STATUS	},
	{ "rew*ind",	"",				"rewind the drive",						CMD_REWIND	},
	{ "sto*p",	"",				"stop the drive",						CMD_STOP	},
	{ "dr*ive",	"rfn|rfh|rrn|rrh|wfn|stop|cont [control]", "command to drive",					CMD_DRIVE	},
	{ NULL,		NULL,				NULL,								0		}
 };


//* END Command Table
//********************************************************************************
//* Interactive mode

void
Interactive()
 {
	CP*			cp = Cp_Open(HelpSrc);
	T98*			tp = T98_Open();
	BF*			bf = bf_alloc( 100000 );
	FILE*			fd;
	int			cmd;
	int			dbi, n, i;
	int			fNum, fSpace, fbi, bi;
	int			addr, byte1, byte2;
	int			rdCmd, timeoutSec, maxBytes;
	int			doZeroes, doBOFs, doSeq, doSums, doPad, doTrunc;
	int			sr, endTm;

	while( (cmd=Cp_Cmd(cp,"TP9830> ",cmdTbl)) ) switch( cmd )			// command loop
	 {	case CMD_OPEN:								// CMD: read image from file
			fd  = Cp_File( cp, "r" );
			dbi = Cp_Kw( cp, "dbb" );
			if( !Cp_Check(cp) )
			 {	if( fd ) fclose( fd );
				continue;
			 }

			bf->l = 0;								// empty the buffer before loading

			if( dbi )
				LoadDBImage( bf, fd );
			else
				XNS_Decode( bf, fd );

			fclose( fd );
			printf( "  buffer length = %d\n", bf->l );
			continue;

		case CMD_SAVE:								// CMD: write image to file
			fd = Cp_File( cp, "w" );
			if( !Cp_Check(cp) )
			 {	if( fd ) fclose( fd );
				continue;
			 }

			fprintf( fd, "HP9830 Tape Image\n" );
			XNS_Encode( fd, bf );

			fclose( fd );
			continue;

		case CMD_BDISPLAY:							// CMD: display image buffer
			addr = Cp_Num( cp, 0 );
			n    = Cp_Num( cp, bf->l );
			if( !Cp_Check(cp) )
				continue;

			printf( "  buffer length = %d\n", bf->l );
			habDisplay( bf, addr, n );
			continue;

		case CMD_LIST:								// CMD: analyse the buffer contents
			fNum = Cp_Num( cp, -1 );						// display the specified file
			if( !Cp_Check(cp) )
				continue;

			if( fNum < 0 )
				Tpf_AnaImage( bf );						// list files in image
			else
			 {	fbi = Tpf_FindFile( bf, fNum );					// display file contents
				if( fbi < 0 )
				 {	printf( "  -- file fNum not found in image\n" );
					continue;
				 }
				Tpf_AnaFile( bf, fbi );
			 }
			continue;

		case CMD_MARK:								// CMD: mark empty files
			n	= Cp_Num( cp, -1 );
			fSpace	= Cp_Num( cp, 1000 );
			fNum	= Cp_Num( cp, -1 );
			if( !Cp_Check(cp) )
				continue;

			Tpf_Mark( bf, n, fSpace, fNum );
			continue;

		case CMD_LOAD:								// CMD: read file into a tape file in buffer
			fd    = Cp_File( cp, "r" );
			fNum  = Cp_Num( cp, -1 );						// -1 implies append (overwrite last file)
			if( !Cp_Check(cp) )
			 {	if( fd ) fclose( fd );
				continue;
			 }

			fbi = Tpf_LoadFile( bf, fNum, fd );
			fclose( fd );
			if( fbi < 0 )
				continue;

			printf( "  words = %d\n", Tpf_getWord(bf,fbi,TPF_OF_LEN) );
			continue;

		case CMD_STORE:								// CMD: store tape file from buffer in external file
			fNum  = Cp_Num( cp, -1 );						// fNum must be specified
			if( fNum < 0 ) cp->err = CP_BADARG;
			fd    = Cp_File( cp, "w" );
			if( !Cp_Check(cp) )
			 {	if( fd ) fclose( fd );
				continue;
			 }

			fbi = Tpf_FindFile( bf, fNum );
			if( fbi < 0 )
			 {	printf( "  -- file fNum not found in image\n" );
				fclose( fd );
				continue;
			 }

			n = TPF_HDR_LEN+1 + Tpf_getWord(bf,fbi+TPF_OF_LEN)+1;			// number of words to extract
			i = 0;
			bi = fbi + 1;								// skip BOF
			while( i < n )
			 {	fprintf( fd, "%X~", Tpf_getWord(bf,bi) );			// write as XNS
				bi += 2;
				if( (++i % 16) == 0 ) fprintf( fd, "\n" );
			 }
			fprintf( fd, "!\n" );
			fclose( fd );

			printf( "  words = %d\n", n );
			continue;


		case CMD_CLEANUP:							// CMD: cleanup aspects of the image
			doZeroes = doBOFs = doSeq = doSums = doPad = doTrunc = FALSE;
			if( Cp_Kw(cp,"z*eros")		) doZeroes	= TRUE;
			if( Cp_Kw(cp,"bof*s")		) doBOFs	= TRUE;
			if( Cp_Kw(cp,"seq*uence")	) doSeq		= TRUE;
			if( Cp_Kw(cp,"ch*ecksums")	) doSums	= TRUE;
			if( Cp_Kw(cp,"pad*ding")	) doPad		= TRUE;
			if( Cp_Kw(cp,"tr*uncate")	) doTrunc	= TRUE;
			if( !Cp_Check(cp) )
				continue;

			if( !doZeroes && !doBOFs && !doSeq && !doSums && !doPad && !doTrunc )	// if no options specified do everything
				doZeroes = doBOFs = doSeq = doSums = doPad = doTrunc = TRUE;

			Tpf_Cleanup( bf, doZeroes, doBOFs, doSeq, doSums, doPad, doTrunc );
			continue;

		case CMD_BTRUNCATE:							// CMD: truncate the buffer to specified length
			n = Cp_Num( cp, 0 );
			if( !Cp_Check(cp) )
				continue;

			bf->l = min( max(n,0), bf->size );

			printf( "  buffer len = %d\n", bf->l );
			continue;

		case CMD_BZEROES:							// CMD: append zeroes to buffer
			addr = Cp_Num( cp, -1 );
			n    = Cp_Num( cp, -1 );
			if( addr<0 || n<0 ) cp->err = CP_BADARG;
			if( !Cp_Check(cp) )
				continue;

			bf_fill( bf, addr, n, 0 );
			continue;

		case CMD_BSET:								// CMD: append empty files to buffer
			addr  = Cp_Num( cp, -1 );
			byte1 = Cp_Num( cp, -1 );
			byte2 = Cp_Num( cp, -1 );
			if( addr<0 || byte1<0 ) cp->err = CP_BADARG;
			if( !Cp_Check(cp) )
				continue;

			bf_put( bf, addr, byte1 );
			if( byte2 >= 0 ) bf_put( bf, addr+1, byte2 );
			continue;

		//---------------------------------------
		// Tape Drive Control Commands 

		case CMD_RIMAGE:							// CMD: read tape image into buffer
			rdCmd	   = Cp_Kw( cp, "fast" ) ? T98_CMD_RFH : T98_CMD_RFN;		// high or normal speed
			timeoutSec = Cp_Kw( cp, "full" ) ? 10*60 : 5;				// long timeout for full image
			maxBytes   = Cp_Num( cp, 90000 );
			if( !Cp_Check(cp) )
				continue;

			bf_resize( bf, maxBytes );

			sr = T98_Read( tp, rdCmd, bf, timeoutSec );
			T98_printStat( sr );
			printf( "  bytes read = %d\n", bf->l );
			continue;

		case CMD_WIMAGE:							// CMD: write image buffer to tape
			if( !Cp_Check(cp) )
				continue;

			sr = T98_Write( tp, bf );
			T98_printStat( sr );
			continue;

		case CMD_STATUS:							// CMD: just get and display the drive status
			if( !Cp_Check(cp) )
				continue;

			sr = T98_Cmd( tp, T98_CMD_CONT );
			T98_printStat( sr );
			continue;

		case CMD_STOP:								// CMD: stop command to drive
			if( !Cp_Check(cp) )
				continue;

			sr = T98_Cmd( tp, T98_CMD_STOP );
			T98_printStat( sr );
			continue;

		case CMD_REWIND:							// CMD: rewind command to drive
			if( !Cp_Check(cp) )
				continue;

			endTm = clock() + 3 * CLOCKS_PER_SEC;
			repeat
			 {	sr = T98_Cmd( tp, T98_CMD_RRH );				// if the rewind began while in leader,
				if( !T98_leader(sr) )						// loop till out of leader
					break;
				if( clock() > endTm )						// if not out of leader in time then already rewound
				 {	sr = T98_Cmd( tp, T98_CMD_STOP );
					break;
				 }
			 }
			T98_printStat( sr );
			continue;

		case CMD_DRIVE:								// CMD: other commands to drive
			cmd = -1;
			if( Cp_Kw(cp,"rfn") )		 cmd = T98_CMD_RFN;
			else if( Cp_Kw(cp,"rfh") )	 cmd = T98_CMD_RFH;
			else if( Cp_Kw(cp,"rrn") )	 cmd = T98_CMD_RRN;
			else if( Cp_Kw(cp,"rrh") )	 cmd = T98_CMD_RRH;
			else if( Cp_Kw(cp,"wfn") )	 cmd = T98_CMD_WFN;
			else if( Cp_Kw(cp,"stop") )	 cmd = T98_CMD_STOP;
			else if( Cp_Kw(cp,"cont*inue") ) cmd = T98_CMD_CONT;
			if( cmd < 0 ) cp->err = CP_BADARG;
			if( Cp_Kw( cp, "control" ) ) cmd |= T98_CMD_CTLBYTE;			// add control bit if requested
			if( !Cp_Check(cp) )
				continue;

			sr = T98_Cmd( tp, cmd );
			T98_printStat( sr );
			continue;
	 }

	bf_free( bf );
	T98_Close( tp );
	Cp_Close( cp );
 }

//* END Interactive
//********************************************************************************
//* Main

int
main( argc, argv )
	int			argc;
	char**			argv;
 {
	BF*			bf;
	FILE*			fd;
	int			i;

	//------------------------------
	// Command line configuration

	for( i=1; i<argc; ++i )
		if( strcmp(argv[i],"-u") == 0 )
		 {	printf( "  -- Use: %s [-u] [<tapeFile>... > <tapeImageFile>]\n", argv[0] );
			exit( -1 );
		 }

	//------------------------------
	// Execution modes

	if( argc == 1 )						// Interactive Mode
	 {	Interactive();
	 }
	else							// Command Line Mode - append files into an mage
	 {	bf = bf_alloc( 0 );
		
		for( i=1; i<argc; ++i )
		 {	fd = fopen( argv[i], "r" );
			if( !fd )
			 {	printf( "  -- Error opening file %s\n", argv[i] );
				exit( -1 );
			 }
			Tpf_LoadFile( bf, -1, fd );		// append file to image
			fclose( fd );
		 }

		Tpf_Cleanup( bf, FALSE, FALSE, FALSE, FALSE, TRUE, TRUE );	// add padding

		fprintf( fd, "HP9830 Tape Image\n" );		// write the image
		XNS_Encode( stdout, bf );

		bf_free( bf );
	 }

	//------------------------------
	// Termination

	m_check();
	return( 0 );
}


//** END Main
//********************************************************************************
