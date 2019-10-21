//********************************************************************************
//*	 	9830 MPSI Server - Paper Tape Reader
//*
//* Paper Tape Reader emulator to allow use of the 9830 PTAPE command with the MPSI,
//* using the MPSI GP device.
//* This module also implements a secondary printer on the same device.
//*
//* 2014 Jan: V1 /bh
//*
//********************************************************************************
#include <stdlib.h>
#include <stdio.h>
#include "../util/util.h"
#include "mpsi.h" 

typedef void			PR;
extern PR*			PR_Open();
extern void			PR_Close();
extern void			PR_Op();


typedef struct
 {	MPSI*			mpsi;
	FILE*			dataFd;
	PR*			pr;
 }
	PTR;


//** END Private
//********************************************************************************
//** Public


PTR*
PTR_Open( mpsi, fname )
	MPSI*			mpsi;
	char*			fname;
 {
	PTR*			self = r_alloc( PTR );

	self->mpsi	= mpsi;
	self->pr	= PR_Open( mpsi );					// create secondary printer

	if( fname )								// open paper tape source file
	 {	self->dataFd = fopen( fname, "r" );
		if( !self->dataFd )
		 {	printf( "  -- MPSI Paper Tape: Error opening file %s\n", fname );
			exit( -1 );
		 }

		fseek( self->dataFd, 0, SEEK_END );
		printf( "  -- MPSI Paper Tape: file mounted, ~ %d bytes\n", (int)ftell(self->dataFd) );
		fseek( self->dataFd, 0, SEEK_SET );
	 }
	else
		self->dataFd = NULL;

	return self;
 }


//----------------------------------------------

void
PTR_Close( self )
	PTR*			self;
 {
	if( self->dataFd ) fclose( self->dataFd );
	PR_Close( self->pr );
	r_free( self );
 }


//----------------------------------------------

void
PTR_Op( self, req )
	PTR*			self;
	int			req;
 {
	int			c;

	if( req & MPSI_IR_INPUT )						// distribute input vs output request
	 {	if( !self->dataFd )						// send nulls after EOF
		 {	MPSI_Exec( self->mpsi, MPSI_OR_ACK | 0 );
			return;
		 }

		if( ftell(self->dataFd) == 0 )					// is this really proper for text files?
		 {	fprintf( stdout, "  -- MPSI Paper Tape: read start " );
			fflush( stdout );
		 }

		c = fgetc( self->dataFd );
		if( c == EOF )
		 {	c = 0;							// send null at EOF
			fclose( self->dataFd );
			self->dataFd = NULL;

			fprintf( stdout, "\n  -- MPSI Paper Tape: read EOF\n" );
		 }
		else if( c == '\n' )
		 {	c = 0x0A;						// 9830 expects LF for end-of-line
			fputc( '.', stdout );
			fflush( stdout );
		 }

		MPSI_Exec( self->mpsi, MPSI_OR_ACK | c );			// send to the 9830
	 }
	else
		PR_Op( self->pr, req );
 }


//** END Paper Tape Reader Server
//********************************************************************************
