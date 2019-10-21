//********************************************************************************
//*
//*			Special File Support Routines
//*
//********************************************************************************
#include <stdio.h>
#include "../util/util.h"
#include "tpf.h"

/*
static int
DB_get( fd )
	FILE*			fd;
 {
	int			bl, bh;

	bl = fgetc( fd );
	if( bl == EOF )
		return( -1 );

	bh = fgetc( fd );
	if( bh == EOF )
		return ( -1 );

	return( (bh<<8) | bl );
 }
*/

//* END Private
//********************************************************************************
//* Public


//-----------------------------------------------
// Load the buffer with a double-byte binary tape image file.

int
LoadDBImage( bf, fd )
	BF*			bf;
	FILE*			fd;
 {
	int			bl, bh;

	bf->l = 0;

	repeat
	 {	bl = fgetc( fd );
		if( bl == EOF )
			break;

		bh = fgetc( fd );
		if( bh == EOF )
		 {	printf( "  -- error: dbl-byte image file with odd number of bytes" );
			return( FALSE );
		 }

		bf_append( bf, (bh<<8) | bl );
	 }

	return( TRUE );
 }


//-----------------------------------------------
// Read a double-byte binary file as output from the assembler into the buffer
// as a tape file.
/*
void
LoadDBFile( bf, fbi, fd )
	BF*			bf;
	int			fbi;
	FILE*			fd;
 {
	int			cnt, w;

	for( cnt=0; ; ++cnt )
	 {	w = DB_get( fd );
		if( w < 0 )
			break;
		Tpf_putWord( bf, fbi+cnt*2, w );
	 }
 }
*/

//********************************************************************************
// Hex-ASCII byte display

void
habDisplay( bf, startIdx, cnt )
	BF*			bf;
	int			startIdx;
	int			cnt;
 {
	int*			b = bf->p;
	int			curIdx, endIdx, hci, i, c;

	curIdx = startIdx;
	endIdx = min( startIdx+cnt, bf->l );

	while( curIdx < endIdx )
	 {	printf( "%04X: ", curIdx );				// print index

		hci = curIdx;
		for( i=0; i<16; ++i )					// print hex bytes
		 {	if( b[hci] > 0xFF )
				printf( "*%02X", (b[hci]&0xFF) );
			else
				printf( " %02X", b[hci] );

			if( ++hci == endIdx )
			 {	++i;
				break;
			 }
		 }
		while( i++ < 17 ) printf( "   " );			// pad line out if fewer than 16 bytes, also separator

		for( i=0; i<16; ++i )					// now print ASCII rep
		 {	c = b[curIdx] & 0xFF;
			printf( "%c", (c>=' ' && c<='~') ? c : '.' );

			if( ++curIdx == endIdx )
				break;
		 }

		printf( "\n" );
	 }
 }


//* EOF
//********************************************************************************
