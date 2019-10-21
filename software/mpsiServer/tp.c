//********************************************************************************
//*	 	9830 MPSI Server - Magnetic Tape Drive
//* 
//* This module emulates a 9865 tape drive when connected to the MPSI.
//* This code has been arrived at in part by experiment, that is, without full details
//* of the semantics of the 9830 firmware tape operations.
//* There are some unknown and unexpected timing requirements. The operation logging has been
//* implemented as an in-memory record to be displayed at a later time to avoid delays during
//* operations. On the other hand, the explicit delay at the beginning of read-forward has been found
//* to be necessary. With no or too small a delay an apparent occasional dropping of bytes results in
//* checksum errors on reading file contents. With the delay, loads work but the log appears to show a
//* loss of a STOP and read-forward commands. Minimum delay is around 275uS.
//* 
//* The routines here set the MPSI_OR_ACK bit to keep the PR and GP devices happy, working on the
//* expectation that operations for the different devices do not overlap and the PR/GP devices always
//* leave the device in the ready state.
//*
//* 2014 Jan: V1 /bh
//* 
//********************************************************************************
#include <stdlib.h>
#include <stdio.h>
#include "../util/util.h"
#include "mpsi.h" 

extern int			Verbose;


#define	TP_CMD_MASK		0x0700
#define	TP_CMD_RFN		0x0000				// read forward, normal speed
#define	TP_CMD_RFH		0x0100				// read forward, high speed
#define	TP_CMD_RRN		0x0200				// read reverse, normal speed
#define	TP_CMD_RRH		0x0300				// read reverse, high speed
#define	TP_CMD_WFN		0x0400				// write forward, normal speed
#define	TP_CMD_STOP		0x0500				// stop 
#define	TP_CMD_CONT6		0x0600				// continue previous command
#define	TP_CMD_CONT		0x0700				// continue previous command
#define	TP_CMD_CTLBYTE		0x0800				// control-byte mode modifier

#define	TP_STAT_nCIN		0x0800				// no cassette in drive
#define	TP_STAT_LDR		0x0400				// clear leader (BOT/EOT)
#define	TP_STAT_nWPT		0x0200				// writing not permitted


#define LOG_SIZE		10000


typedef struct
 {	int			req;
	int			sent;
	int			bfIdx;
 }
	LOG_RECORD;


typedef struct
 {	MPSI*			mpsi;

	int			status;
	int			cmd;
	int			cmode;
	int			delayHack;

	char*			imgFname;
	BF*			imgBuf;
	int			imgIdx;
	int			imgModified;


	LOG_RECORD		logRecord[LOG_SIZE];
	int			logIdx;
 }
	TP;


//----------------------------------------------
// Logging Support

static void
TP__LogOp( self, req, sent, idx )
	TP*			self;
	int			req, sent, idx;
 {
	LOG_RECORD*		record;

	if( self->logIdx >= LOG_SIZE )
	 {	self->logRecord[LOG_SIZE-1].req = -1;
		return;
	 }

	record = &self->logRecord[self->logIdx++];
	record->req	= req;
	record->sent	= sent;
	record->bfIdx	= idx;
 }


// !!! printing hack
/*void
TP__LogPR( self, req )
	TP*			self;
	int			req;
 {
	LOG_RECORD*		record;

	if( self->logIdx >= LOG_SIZE )
	 {	self->logRecord[LOG_SIZE-1].req = -1;
		return;
	 }

	record = &self->logRecord[self->logIdx++];
	record->req	= req | 0x20000;
 }*/


static void
TP__LogDisplay( self )
	TP*			self;
 {
	LOG_RECORD*		record;
	int			i;
	char			*cmdS, modeC;

	for( i=0; i<self->logIdx; ++i )
	 {	record = &self->logRecord[i];

		// !!! printing hack
		/*if( record->req & 0x20000 )
		 {	printf( "  -- MPSI PR Log: %c %04X\n", record->req&0x10000?'Q':' ', record->req&0xFFFF );
			continue;
		 }*/

		modeC = record->req & TP_CMD_CTLBYTE ? 'M' : ' ';

		switch( record->req & TP_CMD_MASK )
		 {	case TP_CMD_RFN:	cmdS = "Rd-Fwd--";		break;
			case TP_CMD_RFH:	cmdS = "Rd-Fwd-H";		break;
			case TP_CMD_RRN:	cmdS = "Rd-Rev--";		break;
			case TP_CMD_RRH:	cmdS = "Rd-Rev-H";		break;
			case TP_CMD_WFN:	cmdS = "Wr-Fwd--";		break;
			case TP_CMD_STOP:	cmdS = "--STOP--";		break;
			case TP_CMD_CONT6:	cmdS = "contin-6"; modeC = ' ';	break;
			case TP_CMD_CONT:	cmdS = "continue"; modeC = ' ';	break;
			default:		cmdS = "bad     ";		break;
		 }
		printf( "  -- MPSI Tape Log:   Request:%c %4X %s%c   Response: ", record->req&0x10000?'Q':' ', record->req&0xFFFF, cmdS, modeC );

		if( record->sent >= 0 )
			printf( "%04X", record->sent );
		else
			printf( "  -  " );

		if( record->bfIdx >= 0 )
			printf( " @ %d\n", record->bfIdx );
		else
			printf( " @ -\n" );
	 }
 }


//** END Private
//********************************************************************************
//** Public


TP*
TP_Open( mpsi, imgFname, writePermit, cleanup, delay )
	MPSI*			mpsi;
	char*			imgFname;
	int			cleanup;
	int			delay;
 {
	TP*			self = r_alloc( TP );
	FILE*			fd;
	int			i;

	self->mpsi		= mpsi;

	self->status		= writePermit ? 0 : TP_STAT_nWPT;
	self->cmd		= TP_CMD_STOP;
	self->cmode		= 0;
	self->delayHack		= delay>=0 ? delay : 400;

	self->imgFname		= imgFname;
	self->imgBuf		= bf_alloc( 100000 );
	self->imgIdx		= -1;
	self->imgModified	= FALSE;

	if( self->imgFname )
	 {	fd = fopen( self->imgFname, "r" );
		if( !fd )
		 {	printf( "  -- MPSI Tape: error opening image file %s\n", self->imgFname );
			exit( -1 );
		 }

		XNS_Decode( self->imgBuf, fd );
		fclose( fd );

		printf( "  -- MPSI Tape: image mounted, %d bytes\n", self->imgBuf->l );
	 }
	else
		self->status |= TP_STAT_nCIN;

	if( self->imgBuf->l == 0 ) bf_put( self->imgBuf, 0 );				// ensure 1 byte in buffer so device can 'get out of leader'

	if( cleanup )									// Throw everything at the MPSI to unhang the 9830, which it
		for( i=10000; i; --i )							// can do under some circumstances of bad tape operations.
		 {	MPSI_Exec( self->mpsi, MPSI_OR_INT | MPSI_OR_CFI | MPSI_OR_ACK | TP_STAT_nCIN | TP_STAT_LDR | 0xFF );
			t_delay_uS( 100 );
		 }

	MPSI_Exec( self->mpsi, MPSI_OR_ACK | self->status | TP_STAT_LDR );		// clear SSI & CFI, set ACK

	return self;
 }


//----------------------------------------------

void
TP_Close( self )
	TP*			self;
 {
	FILE*			fd;
	int			n;

	TP__LogDisplay( self );

	if( self->imgModified )								// rewrite image file if buffer was modified
	 {	fd = fopen( self->imgFname, "w" );
		if( !fd )
			printf( "  -- MPSI Tape: error opening image file for write, %s\n", self->imgFname );
		else
		 {	n = XNS_Encode( fd, self->imgBuf );
			fclose( fd );		
			printf( "  -- MPSI Tape: image file updated, %d bytes\n", n );
		 }
	 }

	bf_free( self->imgBuf );
	r_free( self );
 }


//----------------------------------------------
// Primary operation routine for tape server

void
TP_Op( self, req )
	TP*			self;
	int			req;
 {
	int			cmd, send, idx, i;

	send = -1;
	idx  = -1;

	cmd = req & TP_CMD_MASK;							// extract command
	if( cmd!=TP_CMD_CONT && cmd!=TP_CMD_CONT6 )					// use previous if continue command
	 {	self->cmd   = cmd;
		self->cmode = req & TP_CMD_CTLBYTE;
	 }

	switch( self->cmd )								// select command
	 {	case TP_CMD_STOP:
			self->cmode = 0;
			break;

		case TP_CMD_RFN:							// read forward
		case TP_CMD_RFH:
			t_delay_uS( self->delayHack );					// !!! should figure out why this is needed to work

			if( self->cmode )
			 {	for( i=max(self->imgIdx,0); i<self->imgBuf->l; ++i )	// scan forward for next tape mark
					if( self->imgBuf->p[i] & 0xF00 )
						break;
				self->imgIdx = i;
			 }

			if( self->imgIdx >= self->imgBuf->l )
				send = MPSI_OR_INT | TP_STAT_LDR;
			else
			 {	self->imgIdx = max( self->imgIdx, 0 );
				send = self->imgBuf->p[idx=self->imgIdx++] & 0xFF;

				if( self->cmode && send==0x3C ) send |= MPSI_OR_INT;
			 }
			break;

		case TP_CMD_RRH:							// read reverse high-speed
			if( !self->cmode )
			 {	self->imgIdx = -1;					// rewind if high-speed without cmode
				break;
			 }
		case TP_CMD_RRN:							// read reverse
			if( self->cmode )
			 {	for( i=min(self->imgIdx,self->imgBuf->l-1); i>=0; --i )	// scan backward for previous tape mark
					if( self->imgBuf->p[i] & 0xF00 )
						break;
				self->imgIdx = i;
			 }

			if( self->imgIdx < 0 )
				send = MPSI_OR_INT | TP_STAT_LDR;
			else
			 {	self->imgIdx = min( self->imgIdx, self->imgBuf->l-1 );
				send = self->imgBuf->p[idx=self->imgIdx--] & 0xFF;

				if( self->cmode && send==0x3C ) send |= MPSI_OR_INT;
			 }
			break;

		case TP_CMD_WFN:							// writing
			if( self->status & TP_STAT_nWPT )
				send = MPSI_OR_INT;
			else
			 {	self->imgIdx = max( self->imgIdx, 0 );
				send = req & 0xFF;
				bf_put( self->imgBuf, idx=self->imgIdx++, self->cmode | send );
				self->imgModified = TRUE;
			 }
			break;

		default:
			printf( "  -- MPSI Tape: can't be here\n" );
			break;
	 }

	if( send >= 0 )
		send |= MPSI_OR_ACK | self->status | MPSI_OR_CFI;			// send response if set
	else
		send  = MPSI_OR_ACK | self->status | (self->imgIdx<0 || self->imgIdx>=self->imgBuf->l ? TP_STAT_LDR : 0); // otherwise restore ACK and status

	MPSI_Exec( self->mpsi, send );

	if( Verbose )
		TP__LogOp( self, req, send, idx );
 }


// END Tape Server
//********************************************************************************
