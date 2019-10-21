//********************************************************************************
//*
//*		Command Prompt Module
//*
//*	 2014 Feb: V1 adapted from .py version
//*
//********************************************************************************

#include <stdio.h>
#include "util.h"
#include "attn.h"
#include "cp.h"


//-----------------------------------------------
// Internal Commands

#define PCMD_HELP		1
#define PCMD_CMDS		2
#define PCMD_NUM		3


static void
Cp__Help( helpSrc, arg )
	char*		helpSrc;
	char*		arg;
 {
	char		ln[200];
	char		topic[50];
	char		*hs, *ls;
	int		all, display, cmd, i;

	if( !*arg )
		printf( "  Do 'help full' or 'help <topic>' for more info:\n" );

	all = s_kwMatch("all",arg) || s_kwMatch("full",arg);
	display = all;

	hs = helpSrc;
	while( *hs )
	 {	for( ls=ln; (*ls=*hs); ++hs,++ls )				// collect a line from doc src
			if( *hs == '\n' )
			 {	*ls = 0;
				++hs;
				break;
			 }

		ls = ln;
		if( *ls == '#' )						// check for topic line
		 {	if( (cmd = (*++ls=='C')) ) ++ls;			// check for command topic

			if( !*arg )						// if no arg, print all topics
			 {	printf( "  %s\n", ls );
				continue;
			 }

			if( display && !all )					// check for end of topic section
				break;

			i = sscanf( ls, " %s ", topic );			// look for topic match
			if( i==1 && s_kwMatch(topic,arg) )
				display = TRUE;					// switch to displaying

			if( cmd && (display || all) ) printf( "  Command: %s\n", ls );	// if command, print use
		 }
		else if( display )						// if display mode print section lines
			printf( "  %s\n", ln );
	 }

	if( *arg && !display )
		printf( "  -- no such topic. Do 'help' for topics.\n" );
 }


static CP_TBL			Cp__predefTbl[] =
 {
	{ "q*uit",	"",		"quit this program",				0		},
	{ "h*elp",	"",		"help if doc present, else ?",			PCMD_HELP	},
	{ "?",		"",		"display list of known commands",		PCMD_CMDS	},
	{ "n*umber",	"<num>",	"display number in decimal, hex and octal",	PCMD_NUM	},
	{ ".",		"<num>",	"same as number",				PCMD_NUM	},
	{ NULL,		NULL,		NULL,						0		},
 };


static void
Cp__predefCmds( self, cmd, cmdTbl )
	CP*			self;
	int			cmd;
	CP_TBL*			cmdTbl;
 {
	CP_TBL*			ct;
	char*			s;
	int			n;

	if( cmd == PCMD_HELP )						// CMD: help
	 {	if( self->helpSrc )
		 {	s = Cp_Str( self, "" );
			if( !Cp_Check(self) )
				return;
			Cp__Help( self->helpSrc, s );
			return;
		 }
		cmd = PCMD_CMDS;
	 }

	if( cmd == PCMD_CMDS )						// CMD: command list
	 {	for( ct=cmdTbl;        ct->cmd; ++ct ) printf( "  %-10s %-30s - %s\n",  ct->cmd, ct->helpArgs, ct->helpNote );
		for( ct=Cp__predefTbl; ct->cmd; ++ct ) printf( "  %-10s %-30s - %s\n",  ct->cmd, ct->helpArgs, ct->helpNote );
		return;
	 }

	if( cmd == PCMD_NUM )						// CMD: number conversion
	 {	n = Cp_Num( self, 0 );
		if( !Cp_Check(self) )
			return;
		printf( "  %d 0x%X 0%o\n", n, n, n );
		return;
	 }
 }


//** END Private
//********************************************************************************
//** Public


//-----------------------------------------------
// Initialisation

CP*
Cp_Open( helpSrc )
	char*			helpSrc;
 {
	CP*			self= r_alloc( CP );

	self->helpSrc = helpSrc;

	Attn_Init();

	return( self );
 }


//-----------------------------------------------
// Termination

void
Cp_Close( self )
	CP*			self;
 {
	r_free( self );
 }


//-----------------------------------------------
// Display <prompt> and wait for a command
// Return the command code for a match, 0 for quit
// Prepare for following argument parsing

int
Cp_Cmd( self, prompt, cmdTbl )
	CP*			self;
	char*			prompt;
	CP_TBL			cmdTbl[];
 {
	CP_TBL*			ct;
	char*			parse;
	char*			token;
	int			i;

	repeat
	 {	Attn = FALSE;

		printf( "%s", prompt );
		if( !fgets(self->cmdLn,100,stdin) )				// prompt for command
		 {	printf( "<EOF>\n" );						// EOF, force newline and quit
			break;
		 }

		parse = self->cmdLn;						// parse tokens
		for( i=0; (self->argv[i]=s_token(&parse)); ++i );

		self->argp = self->argv;					// arg pointer starts at first arg
		self->err = 0;							// command status starts as good

		token = *self->argp++;
		if( !token )							// no command
			continue;

		for( ct=cmdTbl; ct->cmd; ++ct )					// check for user command
		 {	if( s_kwMatch(ct->cmd,token) )
				return( ct->code );				// found, return to user with command code
		 }

		for( ct=Cp__predefTbl; ct->cmd; ++ct )				// check for internal command
			if( s_kwMatch(ct->cmd,token) )
				break;
		if( ct->cmd )
		 {	if( ct->code == 0 )
				break;						// quit command
			Cp__predefCmds( self, ct->code, cmdTbl );
		 }
		else
			printf( "  -- unknown command\n" );			// no match
	 }

	return( 0 );								// return to user to quit
 }


//-----------------------------------------------
// Check for failure during arg parsing
// if ok = TRUE, good
// if ok = 0

int
Cp_Check( self )
	CP*			self;
 {
	if( *self->argp ) self->err = -1;					// check for extraneous arguments

	if( self->err )
	 {	if( self->err < 0 ) printf( "  -- bad argument or syntax\n" );
		return( FALSE );
	 }

	return( TRUE );
 }


//-----------------------------------------------
// Parse off a free-form string argument

char*
Cp_Str( self, deflt )
	CP*			self;
	char*			deflt;
 {
	if( !*self->argp )
		return( deflt );

	return( *self->argp++ );
 }


//-----------------------------------------------
// Parse off an optional numeric argument

int
Cp_Num( self, deflt )
	CP*			self;
	int			deflt;
 {
	char*			token;
	int			n;

	token = *self->argp;
	if( !token )
		return( deflt );

	if( sscanf(token,"%i",&n) != 1 )
		return deflt;

	++self->argp;

	return( n );
 }


//-----------------------------------------------
// Parse off an optional keyword argument

int
Cp_Kw( self, kw )
	CP*			self;
	char*			kw;
 {
	char*			token;

	token = *self->argp;
	if( !token )
		return( FALSE );

	if( !s_kwMatch(kw,token) )
		return( FALSE );

	++self->argp;

	return( TRUE );
 }


//-----------------------------------------------
// Parse off a file argument, return file opened in specified mode.
// User is responsible for closing the file.

FILE*	
Cp_File( self, mode )
	CP*			self;
	char*			mode;
 {
	char*			fname;
	FILE*			fd;

	fname = Cp_Str( self, NULL );
	if( !fname )
	 {	printf( "  -- no file specified\n" );
		self->err = CP_FAILED;
		return( NULL );
	 }

	fd = fopen( fname, mode );
	if( !fd )
	 {	printf( "  -- error opening file\n" );
		self->err = CP_FAILED;
		return( NULL );
	 }

	return( fd );
 }


//* END Command Prompt
//********************************************************************************
