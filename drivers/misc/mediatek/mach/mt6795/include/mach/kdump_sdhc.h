#ifdef __cplusplus
extern "C" {
#endif


#define MaxFindFileClusNum 100

#define DEBUG_KDUMP
#ifdef DEBUG_KDUMP
#define DBGKDUMP_PRINTK printk
#endif

#ifndef UINT32
typedef unsigned int UINT32 ; 
#endif

#ifndef INT32
typedef signed int INT32 ; 
#endif


#ifndef BOOL
typedef unsigned int BOOL ; 
#endif

#ifndef WORD
typedef unsigned short WORD ; 
#endif

#ifndef DWORD
typedef unsigned int DWORD ; 
#endif

#ifndef BYTE
typedef unsigned char BYTE ; 
#endif

#ifndef PBYTE
typedef unsigned char* PBYTE ; 
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

typedef enum  
{
    WRITE_FILE_DIRECT = 0,
    FORMAT_BEF_WRITE = 1
}FileWriteType;



typedef enum FileSysType 
{
    FAT_16 = 0,
    FAT_32 = 1
}FATType;

typedef struct 
{
    DWORD   BPB_BytsPerSec;
    DWORD   BPB_SecPerClus;
    DWORD   BPB_RsvdSecCnt;
    DWORD   BPB_NumFATs;
    DWORD   BPB_FATSz;
    DWORD   BPB_RootEntCnt;
    DWORD   BPB_RootClus;
	DWORD   BPB_TotSec;
    FATType	FileSysType;
    DWORD   BootStartSec;
    DWORD   FATStartSec;
    DWORD   RootDirStartSec;
    DWORD   ClusStartSec;
} FAT_Para;



typedef struct {
    BYTE    name[11];            // file name
    BYTE    attr;                // file attribute bits (system, hidden, etc.)
    BYTE    NTflags;             // ???
    BYTE    createdTimeMsec;     // ??? (milliseconds needs 11 bits for 0-2000)
    WORD    createdTime;         // time of file creation
    WORD    createdDate;         // date of file creation
    WORD    lastAccessDate;      // date of last file access
    WORD    clusFirstHigh;       // high word of first cluster
    WORD    time;                // time of last file change
    WORD    date;                // date of last file change
    WORD    clusFirst;           // low word of first cluster
    DWORD   size;                // file size in bytes
} DirEntry;

typedef struct {
    BYTE    seqNum;              // sequence number
    BYTE    name1[10];           // name characters (five UTF-16 characters)
    BYTE    attr;                // attributes (always 0x0F)
    BYTE    NTflags;             // reserved (alwyas 0x00)
    BYTE    checksum;            // checksum of DOS file name
    BYTE    name2[12];           // name characters (six UTF-16 characters)
    WORD    clusFirst;           // word of first cluster (always 0x0000)
    BYTE    name3[4];            // name characters (2 UTF-16 characters)
} LfnEntry;

#define buf_size 64*1024      //must larger than cluster size

typedef struct {
	BYTE   FileBuffer[buf_size];	// File cluster cache, assume maximum cluster size is 64KB
	BYTE   FATBuffer[512];		// FAT cache
	DWORD  BufferLen;			// data cached length in FileBuffer
	DWORD  TotalLen;			// File total length
	DWORD  PrevClusterNum;		// Prev cluster number
	DWORD  CurrClusterNum;		// Current cluster number
	DWORD  FATSector;			// Current FAT sector number
	DWORD  CheckSum;			// File write content checksum
	BOOL   DiskFull;
} FileHandler;

extern BOOL MSDC_Init(void);
extern BOOL MSDC_DeInit(void);
extern BOOL OpenDumpFile_sd(FileHandler *pFileHandler);
extern BOOL WriteDumpFile_sd(FileHandler *pFileHandler, BYTE *Ptr, DWORD Length, DWORD Total);
extern BOOL CloseDumpFile_sd(FileHandler *pFileHandler);

BOOL WriteSectorToSD(UINT32 sector_addr, PBYTE pdBuf, INT32 blockLen) ;
BOOL ReadSectorFromSD(UINT32 sector_addr,PBYTE pdBuf,INT32 blockLen) ;
#ifdef __cplusplus
}
#endif
