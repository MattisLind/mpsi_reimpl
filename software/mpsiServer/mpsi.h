//**********************************************
//**	MPSI Interface Definitions
//**********************************************

//----------------------------------------------
// Dedicated bits of input and output registers

			// input register bits
#define	MPSI_IR_REQST		0x8000		// request from the 9830
#define	MPSI_IR_DEVMASK		0x7000		// mask for the three devices
#define	MPSI_IR_TAPE		0x4000
#define	MPSI_IR_GENPURP		0x2000
#define	MPSI_IR_PRINTER		0x1000
#define	MPSI_IR_INPUT		0x0800		// the direction bit (SO3/DO11) sent by the 9830 for SBS-class IO

			// output register bits
#define	MPSI_OR_INT		0x8000		// interrupt used with the tape device
#define	MPSI_OR_CFI		0x4000		// acknowledge used with the tape device
#define	MPSI_OR_ACK		0x2000		// acknowledge/ready bit used for SBS-class devices

//----------------------------------------------
// Interface Descriptor Type

typedef struct
 {
	int			clock_width;

	int			last_send;
	int			queued_req;
 }
	MPSI;


//----------------------------------------------
// Interface Routines

extern MPSI*			MPSI_Open();
extern void			MPSI_Close();

// raw interface
extern int			MPSI_Wait();
extern int			MPSI_Transfer();

// server interface
extern int			MPSI_WaitForRequest();
extern void			MPSI_Exec();
