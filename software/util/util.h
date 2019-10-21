//-------------------------------------
// Some c extensionas

#define TRUE			1
#define FALSE			0

#define repeat			for(;;)

extern int			min();
extern int			max();


//-------------------------------------
// Memory

extern void*			m_alloc();
extern void			m_free();
extern void			m_check();

#define r_alloc(type)		( (type*) m_alloc(sizeof(type)) )
#define r_free(r)		m_free( r )


//-------------------------------------
// Buffer

typedef struct
 {	int*		p;
	int		l;
	int		size;
 }
	BF;

extern BF*			bf_alloc();
extern void			bf_free();
extern void			bf_resize();
extern void			bf_append();
extern void			bf_put();
extern void			bf_fill();
extern void			bf_shift();


//-------------------------------------
// Time

extern void			t_delay_uS();


//-------------------------------------
// String Routines

extern int			s_kwMatch();
extern char*			s_token();


//-------------------------------------
// XNS Routines

extern void			XNS_Decode();
extern int			XNS_Encode();
