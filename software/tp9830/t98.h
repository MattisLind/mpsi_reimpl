//********************************************************************************
//*
//*		T98 Tape Drive Control
//*		- Public Interface -
//*
//*	Provides for access and control of a 9830-style cassette tape drive.
//*
//********************************************************************************

#define T98_CMD_RFN		0x0000				// read forward, normal speed
#define T98_CMD_RFH		0x0100				// read forward, high speed
#define T98_CMD_RRN		0x0200				// read reverse, normal speed
#define T98_CMD_RRH		0x0300				// read reverse, high speed
#define T98_CMD_WFN		0x0400				// write forward, normal speed
#define T98_CMD_STOP		0x0500				// stop 
#define T98_CMD_CONT6		0x0600				// continue previous command, pref 7
#define T98_CMD_CONT		0x0700				// continue previous command
#define T98_CMD_CTLBYTE		0x0800				// control byte modifier


typedef void			T98;


extern T98*			T98_Open();
extern void			T98_Close();
extern int			T98_Cmd();
extern int			T98_Read();
extern int			T98_Write();

extern void			T98_printStat();
extern int			T98_leader();
