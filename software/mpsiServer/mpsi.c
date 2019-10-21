//********************************************************************************
//*	 	MPSI Device Interface
//*
//*	Low-level interface to the MPSI hardware.
//*	Note the delay loops in the Transfer routine. The loop count used is valid for
//*	running on an RPi.
//*	The Linux nanosleep routine results in an overly long minimum delay, so is not used here.
//*
//*	2014 Jan: V1 /bh
//*
//********************************************************************************
#include <time.h>
#include <stdio.h>
#include "../util/util.h"
#include "../util/attn.h"
#ifdef __linux__
	#include <wiringPi.h>
#else
	#include "../util/gpioDummy.h"
#endif
#include "mpsi.h"


//----------------------------------------------
// wiringPi pin numbers
// Note these are different than both the header pin numbers and the BCM GPIO numbers

#define MPSI__PIN_SDI		13						// header pin 21; MISO
#define MPSI__PIN_SDO		12						// header pin 19; MOSI
#define MPSI__PIN_SCP		14						// header pin 23; SCLK
#define MPSI__PIN_CTL		3						// header pin 15; GPIO3


//* END Private
//********************************************************************************
//* Public

MPSI*
MPSI_Open( clock_width )
	int			clock_width;
 {
	MPSI*			self = r_alloc( MPSI );

	wiringPiSetup();							// initialize GPIO library

	pinMode( MPSI__PIN_SDI, INPUT );					// initialize signals to/from MPSI
	pullUpDnControl( MPSI__PIN_SDI, PUD_DOWN);

	pinMode( MPSI__PIN_SDO, OUTPUT );
	pinMode( MPSI__PIN_SCP, OUTPUT );
	pinMode( MPSI__PIN_CTL, OUTPUT );

	digitalWrite( MPSI__PIN_SDO, 0 );
	digitalWrite( MPSI__PIN_SCP, 1 );
	digitalWrite( MPSI__PIN_CTL, 1 );

	self->clock_width = clock_width>=0 ? clock_width : 100;

	self->last_send  = 0;
	self->queued_req = 0;

	return( self );
 }


//----------------------------------------------

void
MPSI_Close( self )
	MPSI*			self;
 {
	r_free( self );
 }


//----------------------------------------------
// Low level wait

int
MPSI_Wait( self )
	MPSI*			self;
 {
	while( digitalRead(MPSI__PIN_SDI) == 0 )				// polling loop, wait for request bit to go low
		if( Attn )							// hack
			return( FALSE );

	return( TRUE );
 }


//----------------------------------------------
// Low-level data exchange, and execute by updating flag bits

int
MPSI_Transfer( self, send )
	MPSI*			self;
	int			send;
 {
	int			recv, i, j;

	digitalWrite( MPSI__PIN_CTL, 0 );					// take CTL low during transfer to ensure IR is not reloaded

	recv = 0;

	for( i=0; i<16; ++i )							// 16 bits in and out
	 {	digitalWrite( MPSI__PIN_SDO, send & 1 );			// send bit
		send = send >> 1;
		recv = (recv<<1) | (digitalRead(MPSI__PIN_SDI)&1);		// receive bit

		digitalWrite( MPSI__PIN_SCP, 0 );				// clock pulse
		for( j=self->clock_width; j; --j );
		digitalWrite( MPSI__PIN_SCP, 1 );
		for( j=self->clock_width; j; --j );
	 }

	digitalWrite( MPSI__PIN_CTL, 1 );					// flag bits are loaded on +edge

	return( recv );
 }


//----------------------------------------------
// Wait for a request from the 9830 using a simple polling-loop,
// then transfer data from the MPSI and return it.
// Active-low bits from MPSI are returned inverted to active-high.

int
MPSI_WaitForRequest( self )
	MPSI*			self;
 {
	int			req;

	if( self->queued_req & MPSI_IR_REQST )
	 {	req = self->queued_req;
		self->queued_req = 0;
		return( (req^0x0FFF) | 0x10000 );
	 }

	while( digitalRead(MPSI__PIN_SDI) == 0 )				// polling loop, wait for request bit to go low
		if( Attn )							// hack
			return( 0 );

	return( MPSI_Transfer( self, self->last_send&~(MPSI_OR_INT|MPSI_OR_CFI|MPSI_OR_ACK) ) ^ 0x0FFF );
 }


//----------------------------------------------
// Send data to the MPSI, queue request from MPSI to avoid loss.

void
MPSI_Exec( self, send )
	MPSI*			self;
	int			send;
 {
	self->queued_req = MPSI_Transfer( self, self->last_send=send );		// send data, hang on to this data so it can be restored on receive
 }


//* EOF
//********************************************************************************
