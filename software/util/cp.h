#define CP_BADARG		(-1)			// generic command syntax error
#define CP_FAILED		1			// error in command, msg has already been displayed


typedef struct
 {	char*			cmd;
	char*			helpArgs;
	char*			helpNote;
	int			code;
 }
				CP_TBL;


typedef struct
 {	char*			helpSrc;
	char			cmdLn[100+1];
	char*			argv[50+1];
	char**			argp;
	int			err;
 }
				CP;


extern CP*			Cp_Open();
extern void			Cp_Close();
extern int			Cp_Cmd();
extern int			Cp_Check();
extern char*			Cp_Str();
extern int			Cp_Num();
extern int			Cp_Kw();
extern FILE*			Cp_File();
