//********************************************************************************
//*
//*			9830 Tape Format Manipulation & Analysis Routines
//*
//*	The tape image is represented in a buffer of integers, even though the tape data is bytes.
//*	This is to allow marking of BOF control-bytes in the buffer.
//*
//* 2014 Feb:	 Split from main program
//*
//********************************************************************************

#include <stdio.h>
#include "../util/util.h"
#include "t98.h"
#include "tpf.h"

#define BOT_PADDING		300			// number of bytes of padding at beginning of tape


static int
byteToASCII( byte )
	int			byte;
 {
	int			c = byte & 0xFF;

	return( c>=' ' && c<='~' ? c : '.' );
 }


//-----------------------------------------------
// Decode XNS from file, insert in buffer as double, little-endian bytes

static void
XNS_DecToDLE( bf, bi, fd )
	BF*			bf;
	int			bi;
	FILE*			fd;
 {
	int			num, c;

	num = 0;

	while( (c=fgetc(fd)) != EOF )
	 {	if( c >= '0' && c <= '9' )
			num = (num<<4) | (c-'0');
		else if( c >= 'A' && c <= 'F' )
			num = (num<<4) | (c-'A'+10);
		else if( c == '~' )
		 {	Tpf_putWord( bf, bi, num );
			bi += 2;
			num = 0;
		 }
		else if( c == '!' )
			break;
		else
			num = 0;
	 }
 }


//* END Private
//********************************************************************************
//* Public


//--------------------------------------------------------------------------------
// Form 9830 word from two bytes in buffer

int
Tpf_getWord( bf, bi )
	BF*			bf;
	int			bi;
 {
	return( (bf->p[bi+1]<<8) | bf->p[bi] );
 }


//--------------------------------------------------------------------------------
// Put 9830 word as two bytes in buffer

void
Tpf_putWord( bf, bi, word )
	BF*			bf;
	int			bi;
 {
	bf_put( bf, bi,    word     & 0xFF );
	bf_put( bf, bi+1, (word>>8) & 0xFF );
 }


//--------------------------------------------------------------------------------
// Calc checksum over an area

int
Tpf_checksum( bf, bi, nWords )
	BF*			bf;
	int			bi;
	int			nWords;
 {
	int			sum;

	sum = 0;

	while( nWords-- > 0 )
	 {	sum += Tpf_getWord( bf, bi );
		bi += 2;
	 }

	return( sum & 0xFFFF );
 }


//--------------------------------------------------------------------------------
// Update the header checksum for the file at BOF <bi>

void
Tpf_putHeaderChecksum( bf, bi )
	BF*			bf;
	int			bi;
 {
	Tpf_putWord( bf, bi+TPF_OF_HDRCHECKSUM, Tpf_checksum(bf,bi+TPF_OF_NUM,TPF_HDR_LEN) );
 }


//--------------------------------------------------------------------------------
// Update the content checksum for the file at BOF <bi>

void
Tpf_putContentChecksum( bf, bi )
	BF*			bf;
	int			bi;
 {
	int			fLen = Tpf_getWord( bf, bi+TPF_OF_LEN );

	Tpf_putWord( bf, bi+TPF_OF_CONTENT+fLen*2, Tpf_checksum(bf,bi+TPF_OF_CONTENT,fLen) );
 }


//--------------------------------------------------------------------------------
// Put the full file header for the file at BOF <bi>

void
Tpf_putHeader( bf, bi, fNum, fLen, fType, fSpace, fFLine, fLLine, fCom )
	BF*			bf;
	int			bi;
	int			fNum, fLen, fType, fSpace, fFLine, fLLine, fCom;
 {
	bf_put( bf, bi, 0x3C|T98_CMD_CTLBYTE );

	Tpf_putWord( bf, bi+TPF_OF_NUM,    fNum   );
	Tpf_putWord( bf, bi+TPF_OF_LEN,    fLen   );
	Tpf_putWord( bf, bi+TPF_OF_TYPE,   fType  );
	Tpf_putWord( bf, bi+TPF_OF_SPACE,  fSpace );

	Tpf_putWord( bf, bi+TPF_OF_FLINE,  fFLine );
	Tpf_putWord( bf, bi+TPF_OF_LLINE,  fLLine );
	Tpf_putWord( bf, bi+TPF_OF_COM,    fCom   );

	Tpf_putWord( bf, bi+TPF_OF_COM+2,  0      );
	Tpf_putWord( bf, bi+TPF_OF_COM+4,  0      );
	Tpf_putWord( bf, bi+TPF_OF_COM+6,  0      );
	Tpf_putWord( bf, bi+TPF_OF_COM+8,  0      );
	Tpf_putWord( bf, bi+TPF_OF_COM+10, 0      );

	Tpf_putHeaderChecksum( bf, bi );
 }


//--------------------------------------------------------------------------------
// Display an analysis of the tape image

void
Tpf_AnaImage( bf )
	BF*			bf;
 {
	int			bi, pad, skip, fileSeen;

	Tpf_AnaHeader( NULL, 0 );

	fileSeen = FALSE;
	pad = 0;

	for( bi=0; bi<bf->l; ++bi )
	 {	if( bf->p[bi] == 0 )
		 {	++pad;
			continue;
		 }

		if( (bf->p[bi]&0xFF) != 0x3C )
		 {	printf( "  -- unexpected non-zero byte at 0x%04X: 0x%02X\n", bi, bf->p[bi] );
			++pad;
			continue;
		 }

		if( pad > 0 )
		 {	if( !fileSeen ) printf( "%68s", "" );
			printf( "  / pad: %d bytes\n", pad );
			pad = 0;
		 }
		else
			printf( "\n" );

		fileSeen = TRUE;

		skip = Tpf_AnaHeader( bf, bi );
		if( skip < 0 )
			break;

		bi += skip;
	 }

	if( pad > 0 )
		printf( "  / pad: %d bytes\n", pad );
	else
		printf( "\n" );
 }


//--------------------------------------------------------------------------------
// Display an analysis of the header of tape file at <fbi>
// Return -1 if the buffer ended before the indicated file size
// Return the total numbers of bytes occupied by the file

int
Tpf_AnaHeader( bf, fbi )
	BF*			bf;
	int			fbi;
 {
	int			fNum, fLen, fType, fSpace, fFLine, fLLine, fCom, fHcs, fCcs;
	int			dcs;

	if( !bf )
	 {	printf( "  location   num  type space   len   fLn   lLn   com     hcs     ccs\n" );
		printf( "  --------  ----  ----  ----  ----  ----  ----  ----  ------  ------\n" );
		return( 0 );
	 }

	printf( "   0x%05X:", fbi );

	if( fbi+(TPF_HDR_LEN+1)*2 >= bf->l )
	 {	printf( "  -- truncated header" );
		return( -1 );
	 }

	fNum   = Tpf_getWord( bf, fbi+TPF_OF_NUM );
	fLen   = Tpf_getWord( bf, fbi+TPF_OF_LEN );
	fType  = Tpf_getWord( bf, fbi+TPF_OF_TYPE );
	fSpace = Tpf_getWord( bf, fbi+TPF_OF_SPACE );
	fFLine = Tpf_getWord( bf, fbi+TPF_OF_FLINE );
	fLLine = Tpf_getWord( bf, fbi+TPF_OF_LLINE );
	fCom   = Tpf_getWord( bf, fbi+TPF_OF_COM );
	fHcs   = Tpf_getWord( bf, fbi+TPF_OF_HDRCHECKSUM );

	printf( "%5d %5d %5d %5d %5d %5d %5d  0x%04X", fNum, fType, fSpace, fLen, fFLine, fLLine, fCom, fHcs );

	if( fbi+(TPF_HDR_LEN+1+fSpace+1)*2 >= bf->l )
	 {	printf( "  ????  -- truncated content" );
		return( -1 );
	 }

	fCcs = Tpf_getWord( bf, fbi+TPF_OF_CONTENT+fLen*2 );
	printf( "  0x%04X", fCcs );

	dcs = Tpf_checksum( bf, fbi+TPF_OF_NUM, TPF_HDR_LEN );
	if( fHcs != dcs ) printf( "  -- header checksum mismatch: 0x%04X", dcs );

	dcs = Tpf_checksum( bf, fbi+TPF_OF_CONTENT, fLen );
	if( fCcs != dcs ) printf( "  -- content checksum mismatch: 0x%04X", dcs );

	return( (TPF_HDR_LEN+1+fSpace+1)*2 );
 }


//--------------------------------------------------------------------------------
// Display the tape file at <fbi>

void
Tpf_AnaFile( bf, fbi )
	BF*			bf;
	int			fbi;
 {
	int*			b = bf->p;
	int			curIdx, endIdx, bi, i;
	char			hexStr[3*16+1], *hexS;
	char			octStr[8* 7+1], *octS;
	char			ascStr[8* 2+2], *ascS;

	Tpf_AnaHeader( NULL, 0 );
	i = Tpf_AnaHeader( bf, fbi );
	if( i < 0 )
	 {	printf( "\n" );
		return;
	 }
	printf( "\n\n" );

	curIdx = fbi + TPF_OF_CONTENT;
	endIdx = min( curIdx+Tpf_getWord(bf,fbi+TPF_OF_LEN)*2, bf->l );

	while( curIdx < endIdx )
	 {	hexS = hexStr;
		octS = octStr;
		ascS = ascStr;

		for( i=16,bi=curIdx; i && bi<endIdx; --i,++bi )				// format hex bytes
			if( b[bi] > 0xFF )
				hexS += sprintf( hexS, "M%02X", b[bi]&0xFF );
			else
				hexS += sprintf( hexS, " %02X", b[bi] );

		for( i=8,bi=curIdx; i && bi<endIdx; --i,bi+=2 )				// format octal words and reversed-bytes ASCII
		 {	octS += sprintf( octS, " %06o", Tpf_getWord(bf,bi) );

			*ascS++ = byteToASCII( b[bi+1] );				// !!!! BUG: bad if buffer ended in half-word
			*ascS++ = byteToASCII( b[bi]   );
		 }
		*ascS = '\0';

		printf( "  0x%04X: %-48s  %-56s  %-17s\n", curIdx, hexStr, octStr, ascStr );

		curIdx += 16;
	}
 }


//--------------------------------------------------------------------------------
// Find the file <fNum> in the image buffer, return the index of the file BOF.
// If <fNum> < 0, the <bi> of the last file is returned, if there are no files in the image buffer, -1 is returned.
// If <fNum> >= 0 and no match is found, -1 is returned.

int
Tpf_FindFile( bf, fNum )
	BF*			bf;
	int			fNum;
 {
	int			bi, fbi, fSpace;

	fbi = -1;

	for( bi=0; bi<bf->l; ++bi )
	 {	if( bf->p[bi] == 0 )						// skip padding
			continue;

		if( (bf->p[bi]&0xFF) != 0x3C )					// check for BOF
		 {	printf( "  -- unexpected non-zero byte at 0x%4X: 0x%02X\n", bi, bf->p[bi] );
			continue;
		 }

		fbi = bi;							// found a file
		if( fNum == Tpf_getWord(bf,bi+TPF_OF_NUM) )			// if match return
			return( fbi );

		fSpace = Tpf_getWord( bf, bi+TPF_OF_SPACE );			// jump over file
		bi += (TPF_HDR_LEN+1+fSpace+1)*2;
	 }

	if( fNum >= 0 ) fbi = -1;

	return( fbi );
 }


//--------------------------------------------------------------------------------
// Mark the buffer with files, starting with file <fNum>.
// If <fNum> < 0 writing starts with the last file on the tape.
// If there are no files already in the buffer, start with BOT_PADDING bytes of zeroes.
// An EOT file of size 13 is then written at the end.
// If <numFiles> < 0, an EOT file is appended to the buffer.

void
Tpf_Mark( bf, numFiles, fSpace, fNum )
	BF*			bf;
	int			numFiles, fSpace, fNum;
 {
	int			fbi, zeroes;

	fbi = Tpf_FindFile( bf, fNum );						// find start of file
	if( fbi < 0 )
	 {	if( fNum >= 0 )
		 {	printf( "  -- file fNum not found in image\n" );
			return;
		 }
		fbi = BOT_PADDING;						// image is empty, start with padding
		bf_fill( bf, 0, fbi, 0 );
		fNum = 0;
	 }
	else
	 {	fNum = Tpf_getWord( bf, fbi+TPF_OF_NUM );
		if( numFiles < 0 ) ++fNum;
	 }

	if( numFiles < 0 )
	 {	numFiles = 0;
		fbi = bf->l;
	 }

	for( ++numFiles; numFiles; --numFiles )					// note extra file for EOT file
	 {	if( numFiles == 1 ) fSpace = 13;				// last file is 'EOT' placeholder

		zeroes = (fSpace+1)*2 + fSpace/2;				// content + checksum + 25% padding

		Tpf_putHeader( bf, fbi, fNum, 0, TPF_TYPE_EMPTY, fSpace, 0, 0, 0 );
		bf_fill( bf, fbi+TPF_OF_CONTENT, zeroes, 0 );
		fbi += 1+(TPF_HDR_LEN+1)*2 + zeroes;

		++fNum;
	 }
 }


//--------------------------------------------------------------------------------
// Load file <fd> into the buffer as tape file <fNum>
// If <fNum> < 0, the file is loaded into the last tape file and a new EOT file is marked.
// The buffer index of the start of the tape file is returned.

int
Tpf_LoadFile( bf, fNum, fd )
	BF*			bf;
	int			fNum;
	FILE*			fd;
 {
	int			fSpace, fLen, fbi, doLast;

	doLast = fNum < 0;

	fbi = Tpf_FindFile( bf, fNum );
	if( fbi < 0 )
	 {	if( fNum >= 0 )
		 {	printf( "  -- file fNum not found in image\n" );
			return( -1 );
		 }
		Tpf_Mark( bf, 0, 0, -1 );					// empty image, mark an EOT file
		fbi = Tpf_FindFile( bf, 0 );
	 }

	fNum   = Tpf_getWord( bf, fbi+TPF_OF_NUM );				// preserve these for rewrite
	fSpace = Tpf_getWord( bf, fbi+TPF_OF_SPACE );

	XNS_DecToDLE( bf, fbi+1, fd );						// read file into buffer, !!! may corrupt following file !!!

	fLen = Tpf_getWord( bf, fbi+TPF_OF_LEN );				// fix up header and checksums
	Tpf_putWord( bf, fbi+TPF_OF_NUM, fNum );
	Tpf_putWord( bf, fbi+TPF_OF_SPACE, max(fSpace,fLen) );
	Tpf_putHeaderChecksum(  bf, fbi );
	Tpf_putContentChecksum( bf, fbi );

	if( doLast ) Tpf_Mark( bf, -1, 0, -1 );					// mark a new EOT file 

	return( fbi );
 }


//--------------------------------------------------------------------------------
// Cleanup the image

void
Tpf_Cleanup( bf, doZeroes, doBOFs, doSeq, doSums, doPad, doTrunc )
	BF*			bf;
	int			doZeroes, doBOFs, doSeq, doSums, doPad, doTrunc;
 {
	int			bi, fSpace, fNum, last, pad, needed;

	fNum = 0;
	last = 0;
	bi   = 0;

	fSpace = 2*BOT_PADDING;						// fake an initial file size for BOT padding
	pad = 0;

	while( bi < bf->l )
	 {	if( (bf->p[bi]&0xFF) != 0x3C )
		 {	if( doZeroes ) bf->p[bi] = 0;
			++pad;
			++bi;
			continue;
		 }

		if( doPad )
		 {	needed = fSpace/2 - pad;			// ensure at least 25% padding
			if( needed > 0 )
			 {	bf_shift( bf, bi, needed );
				bf_fill( bf, bi, needed, 0 );
				bi += needed;
			 }
		 }

		if( doBOFs )
			bf->p[bi] |= T98_CMD_CTLBYTE;

		if( doSeq )
			Tpf_putWord( bf, bi+TPF_OF_NUM, fNum++ );

		if( doSums )
		 {	Tpf_putHeaderChecksum( bf, bi );
			Tpf_putContentChecksum( bf, bi );
		 }

		fSpace = Tpf_getWord( bf, bi+TPF_OF_SPACE );
		bi += (TPF_HDR_LEN+1+fSpace+1)*2 + 1;

		last = bi;
		pad = 0;
	 }

	if( doTrunc && last<bf->l )
		bf->l = last;

	if( doPad )
		bf_fill( bf, bf->l, 300, 0 );
 }


//* EOF
//********************************************************************************
