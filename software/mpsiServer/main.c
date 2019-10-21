//********************************************************************************
//*	 	9830 MPSI Server
//* 
//* Server program for use with the MPSI I/O Interface.
//* This program implements emulation of 4 devices:
//*	- a 9866 printer,
//*	- a secondary printer,
//*	- a paper tape reader,
//*	- a 9865 cassette tape drive.
//*
//* Use: mpsi [-u] [-v] [-k] [-w] [-p <paperTapeFile>] [-t <tapeImagefile>] [-d <tapeDelay>]
//*	-u			: display use message and exit
//*	-v			: verbose, display tape log at termination
//*	-k			: tape cleanup at startup to unhang the 9830
//*	-w			: writing permitted on tape image file
//*	-p <paperTapeFile>	: source text file for paper tape reader emulator
//*	-t <tapeImagefile>	: cassette tape image file in T98 XNS format for 9865 emulator
//*	-c <clockWidth>		: sets the clock-pulse width for the MPSI I/O transfers
//*				  see MPSI_Transfer() in mpsi.c
//*	-d <tapeDelay>		: see tape server code
//*
//* 2014 Jan: V1 /bh
//*
//********************************************************************************
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include "../util/util.h"
#include "../util/attn.h"
#include "mpsi.h" 

typedef void			TP;
extern TP*			TP_Open();
extern void			TP_Close();
extern void			TP_Cleanup();
extern void			TP_Op();

typedef void			PR;
extern PR*			PR_Open();
extern void			PR_Close();
extern void			PR_Op();

typedef void			PTR;
extern PTR*			PTR_Open();
extern void			PTR_Close();
extern void			PTR_Op();

//--------------------------------------------------------------------------------

static void
Fcheck( fname )
	char*			fname;
{
	FILE*			fd;

	if( fname )
	 {	fd = fopen( fname, "r" );
		if( !fd )
		 {	printf( "  -- Error opening file %s\n", fname );
			exit( -1 );
		 }
		fclose( fd );
	 }
 }


//* END Private
//********************************************************************************
//* Main execution

int				Verbose;


int
main( argc, argv )
	int			argc;
	char**			argv;
 {
	MPSI*			mpsi;
	int			clockWidth;
	char*			magTpFname;
	int			magTpWritePermit, magTpCleanup, magTpDelay;
	TP*			magTp;
	PR*			printer;
	PTR*			paperTp;
	char*			paperTpFname;
	int			req, i;

	Attn_Init();

	//------------------------------
	// Command line configuration

	Verbose			= FALSE;
	clockWidth		= -1;
	paperTpFname		= NULL;
	magTpFname		= NULL;
	magTpWritePermit	= FALSE;
	magTpCleanup		= FALSE;
	magTpDelay		= -1;

	for( i=1; i<argc; ++i )
		if( strcmp(argv[i],"-p") == 0 )
		 {	if( ++i == argc ) goto useFail;
			paperTpFname = argv[i];
		 }
		else if( strcmp(argv[i],"-t") == 0 )
		 {	if( ++i == argc ) goto useFail;
			magTpFname = argv[i];
		 }
		else if( strcmp(argv[i],"-d") == 0 )
		 {	if( ++i == argc ) goto useFail;
			if( sscanf( argv[i], "%d", &magTpDelay ) != 1 ) goto useFail;
		 }
		else if( strcmp(argv[i],"-c") == 0 )
		 {	if( ++i == argc ) goto useFail;
			if( sscanf( argv[i], "%d", &clockWidth ) != 1 ) goto useFail;
		 }
		else if( strcmp(argv[i],"-w") == 0 )
			magTpWritePermit = TRUE;
		else if( strcmp(argv[i],"-k") == 0 )
			magTpCleanup = TRUE;
		else if( strcmp(argv[i],"-v") == 0 )
			Verbose = TRUE;
		else
		 {useFail:
			printf( "  -- Use: %s [-u] [-v] [-k] [-w] [-p <paperTapeFile>] [-t <tapeImagefile>] [-d <tapeDelay>] [-c <clockWidth>]\n", argv[0] );
			exit( -1 );
		 }

	Fcheck( paperTpFname );
	Fcheck( magTpFname );

	//------------------------------
	// Initialisation

	printf( "  ** 9830 MPSI Server: start, ctl-C to terminate\n" );

	mpsi = MPSI_Open( clockWidth );

	magTp   = TP_Open( mpsi, magTpFname, magTpWritePermit, magTpCleanup, magTpDelay );
	printer = PR_Open( mpsi );
	paperTp = PTR_Open( mpsi, paperTpFname );

	//------------------------------
	// Activity loop

	repeat
	 {	req = MPSI_WaitForRequest( mpsi );				// wait for activity from 9830
		if( Attn )
		 {	printf( "  -- MPSI Server: terminated by user\n" );
			break;
		 }

		switch( req & MPSI_IR_DEVMASK )					// distribute to appropriate device server
		 {	case MPSI_IR_TAPE:
				TP_Op(  magTp,   req );
				break;

			case MPSI_IR_PRINTER:
				PR_Op(  printer, req );
				break;

			case MPSI_IR_GENPURP:
				PTR_Op( paperTp, req );
				break;

			default:
				printf( "  -- MPSI Server: request with bad device spec (req:%04X)\n", req );
				break;
		 }
	 }

	//------------------------------
	// Termination

	PTR_Close( paperTp );
	PR_Close( printer );
	TP_Close( magTp );
	MPSI_Close( mpsi );

	m_check();
	return( 0 );
 }


//* EOF
//********************************************************************************
