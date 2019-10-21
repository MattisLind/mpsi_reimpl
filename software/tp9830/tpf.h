						// Field byte offsets from BOF byte
#define TPF_OF_NUM		(1+0)
#define TPF_OF_LEN		(1+2)			// valid words in content
#define TPF_OF_TYPE		(1+4)
#define TPF_OF_SPACE		(1+6)			// maximum file size in words (space on tape)
#define TPF_OF_FLINE		(1+8)			// first BASIC line
#define TPF_OF_LLINE		(1+10)			// last BASIC line
#define TPF_OF_COM		(1+12)
#define TPF_OF_HDRCHECKSUM	(1+24)
#define TPF_OF_CONTENT		(1+26)

#define TPF_HDR_LEN		12			// number of words in header, not including checksum

						// File Types
#define TPF_TYPE_EMPTY		0
#define TPF_TYPE_BIN		1


extern int			Tpf_getWord();
extern void			Tpf_putWord();
extern int			Tpf_checksum();
extern void			Tpf_putHeaderChecksum();
extern void			Tpf_putContentChecksum();
extern void			Tpf_putHeader();

extern void			Tpf_AnaImage();
extern int			Tpf_AnaHeader();
extern void			Tpf_AnaFile();

extern int			Tpf_FindFile();
extern void			Tpf_Mark();
extern int			Tpf_LoadFile();
extern void			Tpf_Cleanup();
