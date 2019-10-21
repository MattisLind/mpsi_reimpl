//********************************************************************************
//* 	9830 MPSI Server - Printer
//*
//* Printer emulator for use with the MPSI.
//*
//* 2014 Jan: V1 /bh
//*
//********************************************************************************
#include <stdlib.h>
#include <stdio.h>
#include "../util/util.h"
#include "mpsi.h" 


typedef struct
 {	MPSI*			mpsi;
 }
	PR;


//** END Private
//********************************************************************************
//** Public


PR*
PR_Open( mpsi )
	MPSI*			mpsi;
 {
	PR*			self = r_alloc( PR );

	self->mpsi = mpsi;

	return self;
 }


//----------------------------------------------

void
PR_Close( self )
	PR*			self;
 {
	r_free( self );
 }


//----------------------------------------------

void
PR_Op( self, req )
	PR*			self;
	int			req;
 {
	int			c;

	MPSI_Exec( self->mpsi, MPSI_OR_ACK );				// ack to the 9830

	c = req & 0x7F;							// ensure 7-bit ASCII

	if( c>=' ' && c<='~' )						// print visibles
	 {	fputc( c, stdout );
		fflush( stdout );
	 }
	else if( c == 10 )						// print LF
		fputc( '\n', stdout );
	else if( c == 13 )						// ignore CR
		;
	else								// anything else in hex
	 {	fprintf( stdout, "\\x%02X", c );
		fflush( stdout );
	 }
 }


//** END Printer Server
//********************************************************************************
