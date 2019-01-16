/*******************************************************************************
 * Filename:
 * ---------
 *   eemcs_fs_ut.c
 *
 * Project:
 * --------
 *   MOLY
 *
 * Description:
 * ------------
 *   Implements the CCCI FS unit test functions
 *
 * Author:
 * -------
 *
 * ==========================================================================
 * $Log$
 *
 * 07 03 2013 ian.cheng
 * [ALPS00837674] [LTE_MD]  EEMCS ALPS.JB5.82LTE.DEV migration
 * [eemcs migration]
 *
 * 05 27 2013 ian.cheng
 * [ALPS00741900] [EEMCS] Modify device major number to 183
 * 1. update eemcs major number to 183
 * 2. fix typo of CCCI_CHNNEL_T
 *
 * 04 28 2013 ian.cheng
 * [ALPS00612780] [EEMCS] Submit EEMCS to ALPS.JB2.MT6582.MT6290.BSP.DEV
 * 1. merge mediatek/kernel/drivers/eemcs to dev branch
 * 2. merge mdediatek/kernel/drivers/net/lte_hdrv_em to dev branch
 *
 ****************************************************************************/

#include <linux/module.h>
#include <linux/limits.h>
#include <linux/nls.h>
#include <linux/wait.h>

#include "eemcs_kal.h"
#include "eemcs_ccci.h"
#include "eemcs_debug.h"
#include "eemcs_char.h"
#include "eemcs_file_ops.h"
#include "eemcs_fs_ut.h"


#ifdef _EEMCS_FS_UT
//=============================================================================
// CCCI FS Definition
//=============================================================================
#define FS_CCCI_EXCEPT_MAX_RETRY        7
#define FS_CCCI_MAX_RETRY               0xFFFF
#define FS_CCCI_REQ_BUFFER_NUM          5       // support concurrently request
#define FS_CCCI_MAX_ARG_NUM             6       // max parameter number
#define FS_CCCI_MAX_DATA_REQUEST        0x4000  // 16 KB
// FS_MAX_BUF_SIZE : argc + arg(0~5).len + arg(1~5).data + 16KB data
#define FS_CCCI_MAX_BUF_SIZE            (FS_CCCI_MAX_DATA_REQUEST + 12 * sizeof(unsigned int))
#define FS_CCCI_API_RESP_ID	            0xFFFF0000

#define MD_ROOT_DIR                     "Z:"
#define DSP_ROOT_DIR                    "Y:"
#define FS_ROOT_DIR                     "/data/nvram/md1"
#define FS_DSP_ROOT_DIR                 "/system/etc/firmware"

#define MAX_FS_PKT_BYTE                 (0x1000 - 128)
#define CCCI_FS_REQ_SEND_AGAIN          0x80000000
#define CCCI_FS_PEER_REQ_SEND_AGAIN(_p) (((((CCCI_BUFF_T*)(_p))->data[0] & CCCI_FS_REQ_SEND_AGAIN) != 0)? 1: 0)

//=============================================================================
// Parameters for APIs, sync with eemcs_fsd
//=============================================================================

/* FS_GetDrive Parameter */
#define FS_NO_ALT_DRIVE                     0x00000001
#define FS_ONLY_ALT_SERIAL                  0x00000002
#define FS_DRIVE_I_SYSTEM                   0x00000004
#define FS_DRIVE_V_NORMAL                   0x00000008
#define FS_DRIVE_V_REMOVABLE                0x00000010
#define FS_DRIVE_V_EXTERNAL                 0x00000020
#define FS_DRIVE_V_SIMPLUS                  0x00000040
#define FS_DRIVE_I_SYSTEM_DSP               0x00000080

/* FS_Move, FS_Count, FS_GetFolderSize, FS_XDelete, FS_XFindReset (Sorting) Parameter and Flag Passing */
#define FS_MOVE_COPY                        0x00000001
#define FS_MOVE_KILL                        0x00000002

#define FS_FILE_TYPE                        0x00000004     // Recursive Type API Common, Public
#define FS_DIR_TYPE                         0x00000008     // Recursive Type API Common, Public
#define FS_RECURSIVE_TYPE                   0x00000010     // Recursive Type API Common, Public

/* FS_Open Parameter */
#define FS_READ_WRITE                       0x00000000L
#define FS_READ_ONLY                        0x00000100L
#define FS_OPEN_SHARED                      0x00000200L
#define FS_OPEN_NO_DIR                      0x00000400L
#define FS_OPEN_DIR                         0x00000800L
#define FS_CREATE                           0x00010000L
#define FS_CREATE_ALWAYS                    0x00020000L
#define FS_COMMITTED                        0x01000000L
#define FS_CACHE_DATA                       0x02000000L
#define FS_LAZY_DATA                        0x04000000L
#define FS_NONBLOCK_MODE                    0x10000000L
#define FS_PROTECTION_MODE                  0x20000000L

/* Quota Management */
#define FS_QMAX_NO_LIMIT         0xf1f2f3f4 //~3.8GB
#define FS_COUNT_IN_BYTE         0x00000001
#define FS_COUNT_IN_CLUSTER      0x00000002

/* --------------------- FS ERROR CODE ---------------------- */
#define FS_NO_ERROR                             0
#define FS_ERROR_RESERVED                       -1
#define FS_PARAM_ERROR                          -2
#define FS_INVALID_FILENAME                     -3
#define FS_DRIVE_NOT_FOUND                      -4
#define FS_TOO_MANY_FILES                       -5
#define FS_NO_MORE_FILES                        -6
#define FS_WRONG_MEDIA                          -7
#define FS_INVALID_FILE_SYSTEM                  -8
#define FS_FILE_NOT_FOUND                       -9
#define FS_INVALID_FILE_HANDLE                  -10
#define FS_UNSUPPORTED_DEVICE                   -11
#define FS_UNSUPPORTED_DRIVER_FUNCTION          -12
#define FS_CORRUPTED_PARTITION_TABLE            -13
#define FS_TOO_MANY_DRIVES                      -14
#define FS_INVALID_FILE_POS                     -15
#define FS_ACCESS_DENIED                        -16
#define FS_STRING_BUFFER_TOO_SAMLL              -17
#define FS_GENERAL_FAILURE                      -18
#define FS_PATH_NOT_FOUND                       -19
#define FS_FAT_ALLOC_ERROR                      -20
#define FS_ROOT_DIR_FULL                        -21
#define FS_DISK_FULL                            -22
#define FS_TIMEOUT                              -23
#define FS_BAD_SECTOR                           -24
#define FS_DATA_ERROR                           -25
#define FS_MEDIA_CHANGED                        -26
#define FS_SECTOR_NOT_FOUND                     -27
#define FS_ADDRESS_MARK_NOT_FOUND               -28
#define FS_DRIVE_NOT_READY                      -29
#define FS_WRITE_PROTECTION                     -30
#define FS_DMA_OVERRUN                          -31                         
#define FS_CRC_ERROR                            -32
#define FS_DEVICE_RESOURCE_ERROR                -33
#define FS_INVALID_SECTOR_SIZE                  -34
#define FS_OUT_OF_BUFFERS                       -35
#define FS_FILE_EXISTS                          -36
#define FS_LONG_FILE_POS                        -37
#define FS_FILE_TOO_LARGE                       -38
#define FS_BAD_DIR_ENTRY                        -39
#define FS_ATTR_CONFLICT                        -40
#define FS_CHECKDISK_RETRY                      -41
#define FS_LACK_OF_PROTECTION_SPACE             -42
#define FS_SYSTEM_CRASH                         -43
#define FS_FAIL_GET_MEM                         -44
#define FS_READ_ONLY_ERROR                      -45
#define FS_DEVICE_BUSY                          -46
#define FS_ABORTED_ERROR                        -47
#define FS_QUOTA_OVER_DISK_SPACE                -48
#define FS_PATH_OVER_LEN_ERROR                  -49
#define FS_APP_QUOTA_FULL                       -50
#define FS_VF_MAP_ERROR                         -51
#define FS_DEVICE_EXPORTED_ERROR                -52 
#define FS_DISK_FRAGMENT                        -53 
#define FS_DIRCACHE_EXPIRED                     -54
#define FS_QUOTA_USAGE_WARNING                  -55

#define FS_MSDC_MOUNT_ERROR                     -100
#define FS_MSDC_READ_SECTOR_ERROR               -101
#define FS_MSDC_WRITE_SECTOR_ERROR              -102
#define FS_MSDC_DISCARD_SECTOR_ERROR            -103
#define FS_MSDC_PRESENT_NOT_READY               -104
#define FS_MSDC_NOT_PRESENT                     -105

#define FS_EXTERNAL_DEVICE_NOT_PRESENT          -106
#define FS_HIGH_LEVEL_FORMAT_ERROR              -107

#define FS_FLASH_MOUNT_ERROR                    -120
#define FS_FLASH_ERASE_BUSY                     -121
#define FS_NAND_DEVICE_NOT_SUPPORTED            -122
#define FS_FLASH_OTP_UNKNOWERR                  -123
#define FS_FLASH_OTP_OVERSCOPE                  -124
#define FS_FLASH_OTP_WRITEFAIL                  -125
#define FS_FDM_VERSION_MISMATCH                 -126
#define FS_FLASH_OTP_LOCK_ALREADY               -127
#define FS_FDM_FORMAT_ERROR                     -128

#define FS_LOCK_MUTEX_FAIL                      -141
#define FS_NO_NONBLOCKMODE                      -142
#define FS_NO_PROTECTIONMODE                    -143

#define FS_INTERRUPT_BY_SIGNAL                  -512


typedef struct {
        __packed char               FileName[8];
        __packed char               Extension[3];
        __packed char               Attributes;
        __packed char               NTReserved;
        __packed char               CreateTimeTenthSecond;
        __packed int                CreateDateTime;
        __packed unsigned short     LastAccessDate;
        __packed unsigned short     FirstClusterHi;
        __packed int                DateTime;
        __packed unsigned short     FirstCluster;
        __packed unsigned int       FileSize;
        // FS_FileOpenHint members (!Note that RTFDOSDirEntry structure is not changed!)
        unsigned int                Cluster;
        unsigned int                Index;
        unsigned int                Stamp;
        unsigned int                Drive;
        unsigned int                SerialNumber;       
} FS_DOSDirEntry;

//=============================================================================
// FS UT Definition
//=============================================================================

#define FS_UT_TEST_FILE_DIR             "/data/app"
#define FS_UT_1K_FILE                   "fs_ut_1k.dat"
#define FS_UT_2K_FILE                   "fs_ut_2k.dat"
#define FS_UT_4K_FILE                   "fs_ut_4k.dat"
#define FS_UT_6K_FILE                   "fs_ut_6k.dat"
#define FS_UT_8K_FILE                   "fs_ut_8k.dat"
#define FS_UT_10K_FILE                  "fs_ut_10k.dat"
#define FS_UT_12K_FILE                  "fs_ut_12k.dat"
#define FS_UT_14K_FILE                  "fs_ut_14k.dat"
#define FS_UT_16K_FILE                  "fs_ut_16k.dat"
#define FS_UT_PORT_INDEX                (1)

//#define _EEMCS_FS_UT_DBG_PKT            // Debug Incomming and Outgoing Command Buffer

#ifdef _EEMCS_FS_UT_DBG_PKT
#define dump_data_to_file(file,data,size) save_data_to_file
#else
#define dump_data_to_file(file,data,size)
#endif


//=============================================================================
// Type Definition
//=============================================================================

/* Drive enumeration for testing */
typedef enum EEMCS_FS_TEST_DRV_e {
    DRV_MD = 0,
    DRV_MD_DSP,
    DRV_CNT,
} EEMCS_FS_TEST_DRV;

/* FS Operation ID */
typedef enum
{   
    FS_CCCI_OP_PEEK = 0,  // peek the content in FS_REQ_BUF
    FS_CCCI_OP_OPEN = 0x1001,
    FS_CCCI_OP_SEEK,
    FS_CCCI_OP_READ,
    FS_CCCI_OP_WRITE,
    FS_CCCI_OP_CLOSE,
    FS_CCCI_OP_CLOSEALL,
    FS_CCCI_OP_CREATEDIR,
    FS_CCCI_OP_REMOVEDIR,
    FS_CCCI_OP_GETFILESIZE,
    FS_CCCI_OP_GETFOLDERSIZE,
    FS_CCCI_OP_RENAME,
    FS_CCCI_OP_MOVE,
    FS_CCCI_OP_COUNT,
    FS_CCCI_OP_GETDISKINFO,
    FS_CCCI_OP_DELETE,
    FS_CCCI_OP_GETATTRIBUTES,
    FS_CCCI_OP_OPENHINT,
    FS_CCCI_OP_FINDFIRST,
    FS_CCCI_OP_FINDNEXT,
    FS_CCCI_OP_FINDCLOSE,
    FS_CCCI_OP_LOCKFAT,
    FS_CCCI_OP_UNLOCKALL,
    FS_CCCI_OP_SHUTDOWN,
    FS_CCCI_OP_XDELETE,
    FS_CCCI_OP_CLEARDISKFLAG,
    FS_CCCI_OP_GETDRIVE,
    FS_CCCI_OP_GETCLUSTERSIZE,
    FS_CCCI_OP_SETDISKFLAG,
    FS_CCCI_OP_OTPWRITE,
    FS_CCCI_OP_OTPREAD,
    FS_CCCI_OP_OTPQUERYLENGTH,
    FS_CCCI_OP_OTPLOCK,
    FS_CCCI_OP_RESTORE = 0x1021,
    /* Following are for FS UT only */
    FS_CCCI_OP_REPEAT_START,
    FS_CCCI_OP_REPEAT_END,
} FS_CCCI_OP_ID_T;

/* Enumeration of receiving buffer status */
typedef enum EEMCS_FS_BUFF_STATUS_e {
    CCCI_FS_BUFF_IDLE = 0,          // current port is not waiting for more data
    CCCI_FS_BUFF_WAIT               // current port is waiting for more data to come in
} EEMCS_FS_BUFF_STATUS;

typedef struct FS_CCCI_LV_STRUC {
   KAL_UINT32 len;
   void *val;
} FS_CCCI_LV_T;

typedef struct CCCI_FS_PARA {
    KAL_INT32     index;            // port index
    KAL_UINT32    op_id;            // FS_CCCI_OP_ID_T
    FS_CCCI_LV_T  *pLV_in;          // [IN]  CCCI_FS local variable
    KAL_UINT32    LV_in_para_cnt;   // [IN]  parameter count in LV
    FS_CCCI_LV_T  *pLV_out;         // [OUT] CCCI_FS local variable
    KAL_UINT32    LV_out_para_cnt;  // [OUT] parameter count in LV
} CCCI_FS_PARA_T;

typedef struct FS_STREAMBUFFER_st {
    CCCI_BUFF_T ccci_header;    
    KAL_UINT32  fs_operationID;
    KAL_UINT8   buffer[FS_CCCI_MAX_BUF_SIZE];
} FS_STREAMBUFFER;

typedef struct EEMCS_FS_UT_SET_st {
    /* FS UT Variables */
    volatile KAL_INT32 ut_port_index;   // the port indicator currently in operation
    KAL_UINT32 test_case_idx;           // test case indicator currently in progress
    KAL_UINT32 ftest_idx;               // test file indicator currently in operation
    KAL_UINT32 drive_idx;               // test drive indicator currently in operation
    KAL_INT32 fhandle;                 // file handle currently in use
    KAL_INT32 find_handle;             // find handle currently in use
    KAL_UINT32 fs_write_total;          // data in bytes currently write to AP
    KAL_UINT32 loop_start;              // start indicator of loop to do file test
    KAL_UINT32 loop_end;                // end indicator of loop to do file test

    /* FS request buffer FS_REQ_BUF[FS_CCCI_REQ_BUFFER_NUM][FS_CCCI_MAX_BUF_SIZE] */
    FS_STREAMBUFFER FS_REQ_BUF[FS_CCCI_REQ_BUFFER_NUM];
    /* use to identify the current hwo rgpd count, ccci_fs_buff_status */
    KAL_UINT32  ccci_fs_buff_state[FS_CCCI_REQ_BUFFER_NUM];
    /* recording the current buffer offset in FS_REQ_BUF */
    KAL_UINT32 ccci_fs_buff_offset[FS_CCCI_REQ_BUFFER_NUM];
} EEMCS_FS_UT_SET;

/* Drive information for testing */
typedef struct EEMCS_FS_TEST_DRIVE_st {
    KAL_UINT32 id;                      // Drvie enumeration defined in EEMCS_FS_TEST_DRV
    char drive[NAME_MAX];               // Drvie name in modem side
    char fs_root[NAME_MAX];             // File system path mapped in AP side
    KAL_UINT32 type;                    // Drive type defined in modem side
} EEMCS_FS_TEST_DRIVE;

/* File information for testing */
typedef struct EEMCS_FS_TEST_FILE_st {
    char name[NAME_MAX];                // File name
    KAL_UINT32 size;                    // File size
} EEMCS_FS_TEST_FILE;

/* Test case information for testing */
typedef struct EEMCS_FS_TEST_CASE_st {
    KAL_UINT32 index;                   // Test case index
    KAL_UINT32 op_id;                   // Operation ID
    char reserved[NAME_MAX];            // Reserved part for different purpose
} EEMCS_FS_TEST_CASE;


//=============================================================================
// Global Variables
//=============================================================================

EEMCS_FS_UT_SET g_eemcs_fs_ut;

/* Drive list for FS UT */
const static EEMCS_FS_TEST_DRIVE g_test_drive[] = {
    { DRV_MD, MD_ROOT_DIR, FS_ROOT_DIR, FS_DRIVE_I_SYSTEM },
    { DRV_MD_DSP, DSP_ROOT_DIR, FS_DSP_ROOT_DIR, FS_DRIVE_I_SYSTEM_DSP }
};
#define EEMCS_FS_TEST_DRV_CNT sizeof(g_test_drive)/sizeof(g_test_drive[0])

/* File list for FS UT */
const static EEMCS_FS_TEST_FILE g_test_file[] = {
    { FS_UT_1K_FILE, 1 * 1024 },
    { FS_UT_2K_FILE, 2 * 1024 },
    { FS_UT_4K_FILE, 4 * 1024 },
    { FS_UT_6K_FILE, 6 * 1024 },
    { FS_UT_8K_FILE, 8 * 1024 },
    { FS_UT_10K_FILE, 10 * 1024, },
    { FS_UT_12K_FILE, 12 * 1024, },
    { FS_UT_14K_FILE, 14 * 1024, },
    { FS_UT_16K_FILE, 16 * 1024, }
};
#define EEMCS_FS_TEST_FILE_CNT sizeof(g_test_file)/sizeof(g_test_file[0])

/* Command list for FS UT */
EEMCS_FS_TEST_CASE g_test_case[] = {
    { 0, FS_CCCI_OP_GETDRIVE, "" },
    { 0, FS_CCCI_OP_GETCLUSTERSIZE, "" },
    { 0, FS_CCCI_OP_CREATEDIR, "fsut_dir" },                // directory to be created
    { 0, FS_CCCI_OP_CREATEDIR, "fsut_dir/lv2_dir" },        // directory to be created
    { 0, FS_CCCI_OP_REMOVEDIR, "fsut_dir/lv2_dir" },        // directory to be removed
    { 0, FS_CCCI_OP_CREATEDIR, "fsut_dir/lv2_dir" },        // directory to be created

    { 0, FS_CCCI_OP_REPEAT_START, "" },
    { 0, FS_CCCI_OP_OPEN, "" },
    { 0, FS_CCCI_OP_WRITE, "" },
    { 0, FS_CCCI_OP_GETFILESIZE, "" },
    { 0, FS_CCCI_OP_CLOSE, "" },
    { 0, FS_CCCI_OP_OPEN, "" },
    { 0, FS_CCCI_OP_READ, "" },
    { 0, FS_CCCI_OP_CLOSE, "" },
    { 0, FS_CCCI_OP_MOVE, "fsut_dir/lv2_dir" },             // destination directory for the files to move to
    { 0, FS_CCCI_OP_RENAME, "renamed_" },                   // prefix of the renamed file name
    { 0, FS_CCCI_OP_DELETE, "renamed_" },                   // prefix of the file name to delete
    { 0, FS_CCCI_OP_REPEAT_END, "" },

    { 0, FS_CCCI_OP_GETFOLDERSIZE, "fsut_dir/lv2_dir" },    // directory to get size
    { 0, FS_CCCI_OP_COUNT, "fsut_dir/lv2_dir" },            // directory to get file number
    { 0, FS_CCCI_OP_FINDFIRST, "fsut_dir/lv2_dir/fs_ut_*" },// pattern to find
    { 0, FS_CCCI_OP_FINDNEXT, "" },
    { 0, FS_CCCI_OP_FINDCLOSE, "" },
    { 0, FS_CCCI_OP_XDELETE, "fsut_dir" },                  // directory to be removed
};
#define EEMCS_FS_TEST_CASE_CNT sizeof(g_test_case)/sizeof(g_test_case[0])

/* Global variables to keep incomming and outgoing arguments */
FS_CCCI_LV_T g_LV_in[FS_CCCI_MAX_ARG_NUM];
FS_CCCI_LV_T g_LV_out[FS_CCCI_MAX_ARG_NUM];
KAL_UINT32 g_LV_in_num = 0;
KAL_UINT32 g_LV_out_num = 0;
CCCI_FS_PARA_T g_ccci_fs_paras;

/* Command debug indicator */
static volatile KAL_UINT32 op_read_cnt = 0;     // Indicator of FS_CCCI_OP_READ
static volatile KAL_UINT32 md2ap_cmd_cnt = 0;   // Indicator of MD to AP commands
static volatile KAL_UINT32 ap2md_cmd_cnt = 0;   // Indicator of AP to MD commands


//=============================================================================
// Forward Declaration
//=============================================================================

KAL_INT32 eemcs_fs_ut_send_cmd(void);


//=============================================================================
// Implemenataion
//=============================================================================

/*
 * @brief Generate a file with test data
 * @param
 *     file [in] The file name with full path
 *     size [in] The file size you want
 * @return
 *     KAL_SUCCESS is returned indicates success;
 *     KAL_FAIL otherwise.
 */
KAL_INT32 gen_ap_random_file(char *file, KAL_UINT32 size)
{
    struct file *fp = NULL;
    KAL_UINT32 i = 0;
    char data = 0;
    int ret = 0;
    DEBUG_LOG_FUNCTION_ENTRY;

    fp = file_open(file, O_RDWR | O_CREAT, 0777);
    if (fp == NULL) {
        DBGLOG(FSUT, ERR, "[FSUT] Failed to generate test file %s", file);
        DEBUG_LOG_FUNCTION_LEAVE;
        return KAL_FAIL;
    }

    do {
        for (i = 0; i < size; i++) {
            ret = file_write(fp, &data, sizeof(char));
            if (ret <= 0) {
                DBGLOG(FSUT, ERR, "[FSUT] Failed to write to file (%d) !!", i);
            }
            data++;
        }
    } while(0);
    file_close(fp);
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Get data from a file
 * @param
 *     file [in] The file name with full path
 *     size [in] The data size you want to get from the file
 * @return
 *     A non-null data pointer is returned indicates success;
 *     NULL otherwise.
 */
void *get_file_data(char *file, KAL_UINT32 size)
{
    void *data = NULL;
    struct file *fp = NULL;
    KAL_INT32 result = KAL_FAIL;
    DEBUG_LOG_FUNCTION_ENTRY;

    KAL_ASSERT(file != NULL);
    /* Open test file */
    fp = file_open(file, O_RDONLY, 0777);
    if (fp == NULL) {
        DBGLOG(FSUT, ERR, "[FSUT] Failed to open test file %s", file);
        goto _open_fail;
    }
    /* Allocate memory to store data */
    data = kmalloc(size, GFP_KERNEL);
    if (data == NULL) {
        DBGLOG(FSUT, ERR, "[FSUT] Failed to allocate memory");
        goto _alloc_fail;
    }
    result = file_read(fp, data, size);
    if (result != size) {
        DBGLOG(FSUT, ERR, "[FSUT] Failed to read test file (%d)", result);
        goto _read_fail;
    }
    goto _ok;

_read_fail:
    kfree(data);
_alloc_fail:
_ok:
    file_close(fp);
_open_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return data;
}

/*
 * @brief Get data from the test file
 * @param
 *     index [in] The file index in global variable g_test_file
 * @return
 *     A non-NULL data pointer is returned indicates success;
 *     NULL otherwise.
 */
void *get_test_file_data(KAL_UINT32 index)
{
    char src[NAME_MAX] = {0};

    sprintf(src, "%s/%s", FS_UT_TEST_FILE_DIR, g_test_file[index].name);
    return get_file_data(src, g_test_file[index].size);
}

/*
 * @brief Destroy file data
 * @param
 *     data [in] A data pointer returned from get_file_data()
 * @return
 *     None
 */
void destroy_file_data(void *data)
{
    DEBUG_LOG_FUNCTION_ENTRY;
    if (data != NULL) {
        kfree(data);
    }
    DEBUG_LOG_FUNCTION_LEAVE;
}

static int FS_ConvWcsToCs(const wchar_t* strScr, char* strDst, unsigned int src_length)
{
        char *ptr;
        int   length;

        length = 0;
        ptr    = (char *) strScr;

        while (length < src_length) {
                if (ptr[length * 2] == '\\') {
                        strDst[length] = '/';
                } else {
                        strDst[length] = ptr[length * 2];
                }

                length++;

                if (ptr[length * 2] == 0) {
                        strDst[length] = '\0';
                        break;
                }
        }

        return length;
}

/*
 * @brief Convert char string to wide char string
 * @param
 *     strSrc [in] Source char string to be converted
 *     strDst [out] The converted wide char string
 *     length [in] Length of source char string
 * @return
 *     The length of source char string have been converted
 */
static int FS_ConvCsToWcs(const char* strSrc, wchar_t* strDst, unsigned int src_length)
{
    char *ptr;
    unsigned int length;

    length = 0;
    ptr = (char *)strDst;

    while (length < src_length) {
        if (strSrc[length] == '/') {
            ptr[length * 2] = '\\';
            ptr[length * 2 + 1] = 0;
        } else {
            ptr[length * 2] = strSrc[length];
            ptr[length * 2 + 1] = 0;
        }
        length++;

        if (strSrc[length] == '\0') {
            ptr[length * 2] = 0;
            ptr[length * 2 + 1] = 0;
            break;
        }
    }

    if(length == src_length) {
        ptr[(length-1) * 2] = 0;
        ptr[(length-1) * 2 + 1] = 0;
        length -= 1;
    }

    return length;   
}

/*
 * @brief Calculate the length of wide char string
 * @param
 *     str [in] Source wide char string
 * @return
 *     The length of source wide char string
 */
static int FS_WcsLen(const wchar_t *str)
{
    KAL_UINT16 *ptr = NULL;
    int len = 0;

    ptr = (KAL_UINT16*)str;
    while (*ptr++ != 0)
        len++;
    return len;
}

/*
 * @brief Allocate a sk buffer and initial it's FS header fields
 * @param
 *     size [in] Size in bytes to allocate
 *     stream [in] FS header structure for reference
 *     again [in] Send again indicator
 * @return
 *     A pointer to the allocated sk buffer indicates success.
 *     Otherwise NULL is returned.
 */
struct sk_buff *eemcs_ut_alloc_skb(KAL_UINT32 size, FS_STREAMBUFFER *stream, KAL_UINT8 again)
{
    struct sk_buff *new_skb = NULL;
    FS_STREAMBUFFER *header = NULL;
    DEBUG_LOG_FUNCTION_ENTRY;

    KAL_ASSERT(size > 0);
    KAL_ASSERT(stream != NULL);

    new_skb = dev_alloc_skb(size);
    if (new_skb == NULL) {
        DBGLOG(FSUT, ERR, "[FSUT] Failed to alloc skb !!");
        goto _fail;
    }
    header = (FS_STREAMBUFFER *)skb_put(new_skb, size);
    memset(new_skb->data, 0, size);
    DBGLOG(FSUT, DBG, "[FSUT] eemcs_ut_alloc_skb() new_skb(0x%p, 0x%p) size = %d",
        new_skb, new_skb->data, new_skb->len);
    // Assign send again indicator
    if (again) {
        header->ccci_header.data[0] = stream->ccci_header.data[0] | CCCI_FS_REQ_SEND_AGAIN;
    } else {
        header->ccci_header.data[0] = stream->ccci_header.data[0] & ~CCCI_FS_REQ_SEND_AGAIN;
    }
    KAL_ASSERT(size <= stream->ccci_header.data[1]);
    //header->ccci_header.data[1] = stream->ccci_header.data[1];
    header->ccci_header.data[1] = size;
    header->ccci_header.reserved = stream->ccci_header.reserved;
    header->ccci_header.channel = stream->ccci_header.channel;
    // Assign OP ID
    header->fs_operationID = stream->fs_operationID;

    DEBUG_LOG_FUNCTION_LEAVE;
    return new_skb;
_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return NULL;
}

/*
 * @brief Dump FS header information to standard output
 * @param
 *     data [in] A pointer to the data buffer containing FS header
 * @return
 *     None
 */
void dump_fs_stream_header(void *data)
{
    FS_STREAMBUFFER *stream = NULL;

    KAL_ASSERT(data != NULL);
    stream = (FS_STREAMBUFFER *)data;
    DBGLOG(FSUT, DBG, "[DUMP] data[0] = 0x%X, data[1] = 0x%X, channel = 0x%X, reserved = 0x%X, op = 0x%X",
        stream->ccci_header.data[0], stream->ccci_header.data[1],
        stream->ccci_header.channel, stream->ccci_header.reserved,
        stream->fs_operationID);
}

/*
 * @brief
 *     Simulating MD sending a FS command to AP.
 *     This function will allocate a sk buffer, format its data buffer to
 *     FS commands format, then callback to CCCI CHAR layer directly.
 * @param
 *     None
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_send_skb(void)
{
    FS_STREAMBUFFER *stream = NULL;
    KAL_UINT32 pkt_cnt = 0;
    KAL_UINT8 *pBuff = NULL;        // pointer to data payload of FS buffer
    KAL_UINT32 data_to_send = 0;    // data size of each packet to send (excluding CCCI header and OP ID)
    KAL_UINT32 data_sent = 0;       // size of sent payload of FS buffer
    KAL_UINT32 op_id = 0;           // operation id
    KAL_UINT32 total_len = 0;       // total FS buffer length
    KAL_UINT32 data_len = 0;        // total FS buffer length excluding CCCI header and OP ID
    KAL_UINT8 *skb_ptr = NULL;
    struct sk_buff *new_skb = NULL;
    char cmd_file[NAME_MAX] = {0};
    char pkt_file[NAME_MAX] = {0};
    DEBUG_LOG_FUNCTION_ENTRY;


    stream = &g_eemcs_fs_ut.FS_REQ_BUF[g_ccci_fs_paras.index];
    KAL_ASSERT(stream != NULL);
    pBuff = stream->buffer;
    op_id = stream->fs_operationID;
    total_len = stream->ccci_header.data[1];
    // data length excluding CCCI header and OP ID
    data_len = total_len - sizeof(CCCI_BUFF_T) - sizeof(KAL_UINT32);

    sprintf(cmd_file, "%s/fs_ut_md2ap_%03d.dat", FS_UT_TEST_FILE_DIR, md2ap_cmd_cnt++);
    dump_data_to_file(cmd_file, (char *)stream, total_len);

    /* No fragment is needed */
    if (total_len <= MAX_FS_PKT_BYTE) {
        sprintf(pkt_file, "%s/fs_ut_md2ap_%03d_pkt_%d.dat", FS_UT_TEST_FILE_DIR, md2ap_cmd_cnt, pkt_cnt);
        DBGLOG(FSUT, DBG, "[FSUT] Small packet, no fragment.");

        // Allocate a new skb to do transmission
        new_skb = eemcs_ut_alloc_skb(total_len, (FS_STREAMBUFFER *)stream, 0);
        KAL_ASSERT(new_skb != NULL);
        skb_ptr = (KAL_UINT8 *)new_skb->data;
        dump_fs_stream_header((void *)skb_ptr);
        // Skip CCCI header and OP ID
        skb_ptr += sizeof(CCCI_BUFF_T) + sizeof(KAL_UINT32);
        // Copy from local buffer to skb
        memcpy(skb_ptr, pBuff, data_len);
        dump_data_to_file(pkt_file, (char *)(skb_ptr - sizeof(CCCI_BUFF_T) - sizeof(KAL_UINT32)), total_len);
        eemcs_fs_ut_callback(new_skb, 0);
    } else {
    /* Data fragment is needed */
        DBGLOG(FSUT, DBG, "[FSUT] Big packet, need fragment.");

        while ((data_sent + sizeof(CCCI_BUFF_T) + sizeof(KAL_UINT32)) < total_len) {
            memset(pkt_file, 0, NAME_MAX);
            sprintf(pkt_file, "%s/fs_ut_md2ap_%03d_pkt_%d.dat", FS_UT_TEST_FILE_DIR, md2ap_cmd_cnt, pkt_cnt);
            /* Each packet includes CCCI header, OP id, and data */
            /* Moret than 2 packets to send */
            if (data_len - data_sent > (MAX_FS_PKT_BYTE - sizeof(CCCI_BUFF_T) - sizeof(KAL_UINT32))) {
                data_to_send = MAX_FS_PKT_BYTE - sizeof(CCCI_BUFF_T) - sizeof(KAL_UINT32);
                // Prepare skb buffer
                new_skb = eemcs_ut_alloc_skb(MAX_FS_PKT_BYTE, (FS_STREAMBUFFER *)stream, 1);
                KAL_ASSERT(new_skb != NULL);
                skb_ptr = (KAL_UINT8 *)new_skb->data;
                dump_fs_stream_header((void *)skb_ptr);
                // Skip CCCI header and OP ID
                skb_ptr += sizeof(CCCI_BUFF_T) + sizeof(KAL_UINT32);
                KAL_ASSERT(((CCCI_BUFF_T*)new_skb->data)->data[1] == MAX_FS_PKT_BYTE);
            } else {
            /* The last packet */
                data_to_send = data_len - data_sent;
                // Prepare skb buffer, size = CCCI header + OP ID + remaining data
                new_skb = eemcs_ut_alloc_skb(sizeof(CCCI_BUFF_T) + sizeof(KAL_UINT32) + data_to_send, (FS_STREAMBUFFER *)stream, 0);
                KAL_ASSERT(new_skb != NULL);
                skb_ptr = (KAL_UINT8 *)new_skb->data;
                dump_fs_stream_header((void *)skb_ptr);
                // Skip CCCI header and OP ID
                skb_ptr += sizeof(CCCI_BUFF_T) + sizeof(KAL_UINT32);
                KAL_ASSERT(((CCCI_BUFF_T*)new_skb->data)->data[1] == (sizeof(CCCI_BUFF_T) + sizeof(KAL_UINT32) + data_to_send));
            }
            // Copy from local buffer to skb, data only
            memcpy(skb_ptr, pBuff, data_to_send);
            dump_data_to_file(pkt_file, (char *)(skb_ptr - sizeof(CCCI_BUFF_T) - sizeof(KAL_UINT32)), data_to_send + sizeof(CCCI_BUFF_T) - sizeof(KAL_UINT32));
            DBGLOG(FSUT, DBG, "[FSUT] MD send packet %d with %d bytes", pkt_cnt, ((CCCI_BUFF_T*)new_skb->data)->data[1]);
            data_sent += data_to_send;
            pBuff += data_to_send;
            pkt_cnt++;
            eemcs_fs_ut_callback(new_skb, 0);
        };
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief
 *     Store arguments information to the big buffer(g_eemcs_fs_ut.FS_REQ_BUF) for transmission
 * @param
 *     index [in] Port index currently in use
 *     op [in] Operation ID currently in progress
 *     pLV [in] A structure containing arguments information
 *     num [in] Number to arguments
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 ccci_fs_put_buff(KAL_INT32 index, KAL_UINT32 op, FS_CCCI_LV_T *pLV, KAL_UINT32 num)
{
    FS_STREAMBUFFER *fs_buf = NULL;
    KAL_UINT8 *ptr;
    KAL_UINT32 *argc;
    KAL_UINT32 i;
    FS_CCCI_LV_T *p_curr_lv_t = NULL;   // pointer of current local variable struct
    KAL_UINT32 curr_v_len;              // current variable length
    KAL_UINT32 total_len = 0;           // total variable length
    DEBUG_LOG_FUNCTION_ENTRY;

    // fs_buf = CCCI_FS internal buffer
    fs_buf = &g_eemcs_fs_ut.FS_REQ_BUF[index];
    memset(fs_buf, 0, sizeof(FS_STREAMBUFFER));

    // Assign operation id
    fs_buf->fs_operationID = op;
    total_len += sizeof(fs_buf->fs_operationID);

    // ptr = buffer pointer;
    ptr = fs_buf->buffer;

    // Set the number of parameters
    argc = (KAL_UINT32 *)ptr;
    *argc = num;
    ptr += sizeof(KAL_UINT32);
    total_len += sizeof(KAL_UINT32);

    // Set each parameter of pLV[i]
    for (i = 0; i < num; i++) {
        p_curr_lv_t = (FS_CCCI_LV_T *)ptr;

        // Set the data length
        p_curr_lv_t->len = pLV[i].len;
        // adjusted to be 4-byte aligned
        curr_v_len = ((pLV[i].len + 3) >> 2) << 2;
        DBGLOG(FSUT, DBG, "[FSUT] Copy LV[%d]. real length = %d, aligned length = %d", i, pLV[i].len, curr_v_len);

        // memcpy the data into CCCI_FS buffer
        memcpy(&(p_curr_lv_t->val), pLV[i].val, pLV[i].len);

        // Update the total_len
        total_len += curr_v_len;
        total_len += sizeof(p_curr_lv_t->len); // additional KAL_UINT32 is for p_curr_lv_t->len

        // Move ptr to next LV
        ptr += sizeof(p_curr_lv_t->len) + curr_v_len;
    }

    total_len += sizeof(CCCI_BUFF_T);
    fs_buf->ccci_header.data[0] = g_eemcs_fs_ut.FS_REQ_BUF[index].ccci_header.data[0];
    fs_buf->ccci_header.data[1] = total_len; 
    fs_buf->ccci_header.channel = CH_FS_RX;
    fs_buf->ccci_header.reserved = index;
    DBGLOG(FSUT, DBG, "[FSUT] ccci_fs_put_buff() %d args, total_len = %d, op = 0x%X, port_id = %d",
        *argc, fs_buf->ccci_header.data[1], fs_buf->fs_operationID, fs_buf->ccci_header.reserved);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief
 *     Parsing big buffer(g_eemcs_fs_ut.FS_REQ_BUF) and store arguments information
 *     to a arguments structure
 * @param
 *     index [in] Port index currently in use
 *     op [in] Operation ID currently in progress
 *     pLV [out] A structure to store argument information
 *     num [in] Number to arguments
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 ccci_fs_get_buff(KAL_INT32 index, KAL_UINT32 op, FS_CCCI_LV_T *pLV, KAL_UINT32 *num)
{
    KAL_UINT32 i, no_copy = 0;
    FS_STREAMBUFFER *fs_buf;
    KAL_UINT8 *ptr;
    FS_CCCI_LV_T *pTmp;
    DEBUG_LOG_FUNCTION_ENTRY;

    fs_buf = &g_eemcs_fs_ut.FS_REQ_BUF[index];

    if (op && (fs_buf->fs_operationID != (FS_CCCI_API_RESP_ID | op))) {
        DBGLOG(FSUT, ERR, "[FSUT] fs_buf->fs_operationID = 0x%X, op = 0x%X", fs_buf->fs_operationID, op);
        KAL_ASSERT(0);
    }

    ptr = fs_buf->buffer;

    // entry count
    pTmp = (FS_CCCI_LV_T *)ptr;
    if (op) {
        if (*num != pTmp->len){
            KAL_ASSERT(0);
        }
    } else {
        *num = pTmp->len;
        no_copy = 1;
    }

    // bypass the arguments number
    ptr += sizeof(KAL_UINT32);
    for (i = 0; i < *num; i++) {
        pTmp = (FS_CCCI_LV_T *)ptr;
        if (op && (pLV[i].len < pTmp->len)) {
            KAL_ASSERT(0);
        }

        pLV[i].len = pTmp->len;
        if (no_copy) {
            pLV[i].val = (void *)(ptr + sizeof(KAL_UINT32));
        } else {
            memcpy(pLV[i].val, ptr + sizeof(KAL_UINT32), pLV[i].len);
        }

        // adjusted to be 4-byte aligned
        ptr += sizeof(KAL_UINT32) + (((pTmp->len + 3) >> 2) << 2);
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Wrapper function for all operations
 * @param
 *     fs_para [in] A structure containing arguments information
 * @return
 *     This function will return KAL_SUCCESS always.
 */
static KAL_UINT32 CCCI_FS_OP_Wrapper(CCCI_FS_PARA_T* fs_para)
{
    KAL_INT32 ret;
    DEBUG_LOG_FUNCTION_ENTRY;

    ret = ccci_fs_put_buff(fs_para->index, fs_para->op_id, fs_para->pLV_in, fs_para->LV_in_para_cnt);
    if (KAL_SUCCESS != ret){
        DBGLOG(FSUT, ERR, "[FSUT] Failed to prepare MD FS command");
        DEBUG_LOG_FUNCTION_LEAVE;
        return KAL_FAIL;
    }
    eemcs_fs_ut_send_skb();
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Reset gobal arguments structures
 * @param
 *     None
 * @return
 *     None
 */
void eemcs_fs_ut_reset_args(void)
{
    memset(g_LV_in, 0, sizeof(FS_CCCI_LV_T) * FS_CCCI_MAX_ARG_NUM);
    memset(g_LV_out, 0, sizeof(FS_CCCI_LV_T) * FS_CCCI_MAX_ARG_NUM);
    g_LV_in_num = 0;
    g_LV_out_num = 0;
    memset(&g_ccci_fs_paras, 0, sizeof(CCCI_FS_PARA_T));
}

/*
 * @brief Get drive name
 * @param
 *     type [in] Drive type
 *     serial [in] Drive type serial
 *     alt_mask [in] Drvie mask
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_getdrive(KAL_UINT32 type, KAL_UINT32 serial, KAL_UINT32 alt_mask)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;
    DEBUG_LOG_FUNCTION_ENTRY;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // type
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&type;
    // serial
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&serial;
    // alt_mask
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&alt_mask;

    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_GETDRIVE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

/*
 * @brief Get cluster size of a specified drive
 * @param
 *     drive_index [in] Drive name
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_getclustersize(KAL_UINT32 drive_index)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;
    DEBUG_LOG_FUNCTION_ENTRY;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // drive index
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&drive_index;
    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_GETCLUSTERSIZE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

/*
 * @brief Create a directory
 * @param
 *     dir_path [in] Pull path of a directory
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_createdir(const wchar_t *dir_path)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;
    DEBUG_LOG_FUNCTION_ENTRY;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // directory name of full path
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(dir_path) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)dir_path;
    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_CREATEDIR;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

/*
 * @brief Delete a directory
 * @param
 *     dir_path [in] Pull path of a directory
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_removedir(const wchar_t *dir_name)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;
    DEBUG_LOG_FUNCTION_ENTRY;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // directory name with full path
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(dir_name) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)dir_name;
    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_REMOVEDIR;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

/*
 * @brief Open a file
 * @param
 *     file_path [in] A file full path to be opened
 *     flags [in] Flags to open the file
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_open(wchar_t *file_path, KAL_UINT32 flags)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;
    DEBUG_LOG_FUNCTION_ENTRY;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // full path of file name
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(file_path) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)file_path;
    // flags
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&flags;
    /* AP CCCI FS output value */
    /*
     * 1st KAL_UINT32 : number of arguments
     * 2nd KAL_UINT32 : g_LV_out[0].len
     * 3rd KAL_UINT32 : g_LV_out[0].val <-- Store file handle here
     */
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);

    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_OPEN;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;
    DBGLOG(FSUT, DBG, "[FSUT] eemcs_fs_ut_open() Port %d try to open a file", index);

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return ret;
}

/*
 * @brief Close a file
 * @param
 *     fhandle [in] The file handle returned from eemcs_fs_ut_open()
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_close(KAL_UINT32 fhandle)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;
    DEBUG_LOG_FUNCTION_ENTRY;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // file handle
    g_LV_in[g_LV_in_num].len = sizeof(fhandle);
    g_LV_in[g_LV_in_num++].val = (void *)&fhandle;
    /* AP CCCI FS outout parameters */
    /*
     * 1st KAL_UINT32 : number of arguments
     * 2nd KAL_UINT32 : g_LV_out[0].len
     * 3rd KAL_UINT32 : g_LV_out[0].val <-- Store return code here
     */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_CLOSE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;
    DBGLOG(FSUT, DBG, "[FSUT] eemcs_fs_ut_close() Port %d close file %d", index, fhandle);

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Write data to a file
 * @param
 *     fhandle  [in] The file handle returned from eemcs_fs_ut_open()
 *     data     [in] Data to write
 *     size     [in] Size of data
 *     written  [out] The size in bytes have been written to file
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_write(KAL_UINT32 fhandle, void *data, KAL_UINT32 size, KAL_UINT32 *written)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;
    DEBUG_LOG_FUNCTION_ENTRY;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // file handle
    g_LV_in[g_LV_in_num].len = sizeof(fhandle);
    g_LV_in[g_LV_in_num++].val = (void *)&fhandle;
    // data pointer
    g_LV_in[g_LV_in_num].len = size;
    g_LV_in[g_LV_in_num++].val = data;
    // data size
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&size;
    /* AP CCCI FS outout parameters */
    /*
     * 1st KAL_UINT32 : number of arguments
     * 2nd KAL_UINT32 : g_LV_out[0].len
     * 3rd KAL_UINT32 : g_LV_out[0].val <-- Store return code here
     * 4th KAL_UINT32 : g_LV_out[1].len
     * 5th KAL_UINT32 : g_LV_out[1].val <-- Store written bytes here, we ignore "written" parameter
     */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));
    // written bytes
    g_LV_out[g_LV_out_num].len = sizeof(KAL_UINT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (4 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_WRITE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;
    DBGLOG(FSUT, DBG, "[FSUT] eemcs_fs_ut_write() Port %d write %d bytes of data to file %d", index, size, fhandle);

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Get the size of a file
 * @param
 *     fhandle  [in] The file handle returned from eemcs_fs_ut_open()
 *     size     [out] Size of the file
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_get_file_size(KAL_UINT32 fhandle, KAL_UINT32 *size)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // file handle
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&fhandle;
    /*
     * AP CCCI FS output value
     * 1st KAL_UINT32 : number of arguments
     * 2nd KAL_UINT32 : g_LV_out[0].len
     * 3rd KAL_UINT32 : g_LV_out[0].val <-- Store return code here
     * 4th KAL_UINT32 : g_LV_out[1].len
     * 5th KAL_UINT32 : g_LV_out[1].val <-- Store file size here, we ignore "size" parameter
     */
    /* AP CCCI FS output parameters */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));
    // returned file size
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (4 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_GETFILESIZE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;
    DBGLOG(FSUT, DBG, "[FSUT] eemcs_fs_ut_file_size() Port %d get file size of file %d", index, fhandle);

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Read data from a file
 * @param
 *     fhandle  [in] The file handle returned from eemcs_fs_ut_open()
 *     data     [out] A data buffer to store read data
 *     size     [in] Size to read from the file
 *     read     [out] The actual size read from the file
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_read(KAL_UINT32 fhandle, void *data, KAL_UINT32 size, KAL_UINT32 *read)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // file handle
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&fhandle;
    // data size
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&size;
    /*
     * AP CCCI FS output value
     * 1st KAL_UINT32 : number of arguments
     * 2nd KAL_UINT32 : g_LV_out[0].len
     * 3rd KAL_UINT32 : g_LV_out[0].val <-- Store return code here
     * 4th KAL_UINT32 : g_LV_out[1].len
     * 5th KAL_UINT32 : g_LV_out[1].val <-- Store read size here, we ignore "read" parameter
     * 6th KAL_UINT32 : g_LV_out[2].len
     * 7th KAL_UINT32 : g_LV_out[2].val <-- Store read data here, we ignore "data" parameter
     */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));
    // read bytes
    g_LV_out[g_LV_out_num].len = sizeof(KAL_UINT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (4 * sizeof(KAL_UINT32)));
    // data pointer
    g_LV_out[g_LV_out_num].len = size;
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (6 * sizeof(KAL_UINT32)));;    

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_READ;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;
    DBGLOG(FSUT, DBG, "[FSUT] eemcs_fs_ut_read() Port %d read %d bytes of data from file %d",
        index, size, fhandle);

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Move a file to another path
 * @param
 *     src [in] Source path of file
 *     dst [in] Destination path of ile
 *     flags [in] Options of move command
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_move(const wchar_t *src, const wchar_t *dst, KAL_UINT32 flags)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // source path
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(src) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)src;
    // destination path
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(dst) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)dst;
    // flags
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&flags;
    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_MOVE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Rename a file
 * @param
 *     old_name [in] Original name of file
 *     new_name [in] New name of ile
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_rename(const wchar_t *old_name, const wchar_t* new_name)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // old file name
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(old_name) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)old_name;
    // new file name
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(new_name) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)new_name;
    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_RENAME;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Delete a file
 * @param
 *     file_name [in] Full path of a file
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_delete(const wchar_t *file_name)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // file name
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(file_name) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)file_name;
    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_DELETE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Get size of a filder
 * @param
 *     src [in] Full path of folder
 *     flags [in] Options of GETFOLDERSIZE command.
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_getfoldersize(const wchar_t *dir, KAL_UINT32 flags)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // directory name
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(dir) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)dir;
    // flags
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&flags;
    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_GETFOLDERSIZE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Count file/folder number of a specified folder
 * @param
 *     dir_path [in] Full path of a folder
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_count(const wchar_t *dir_path, KAL_UINT32 flags)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // full path of directory
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(dir_path) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)dir_path;
    // Flag
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&flags;
    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_COUNT;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief Delete a folder
 * @param
 *     dir_path [in] Full path of a folder
 *     flags [in] Options of XDELETE command
 * @return
 *     This function will return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_xdelete(const wchar_t *dir_path, KAL_UINT32 flags)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // full path of folder name
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(dir_path) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)dir_path;
    // Flag
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&flags;

    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_XDELETE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 eemcs_fs_ut_findfirst(const wchar_t* pattern, KAL_UINT8 attr, KAL_UINT8 attr_mask, FS_DOSDirEntry * file_info, wchar_t* file_name, KAL_UINT32 max_length)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;
    KAL_INT32 MaxLength_nonWCHAR = max_length/2 -1;
    KAL_UINT32 aligned_entry_size = 0;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // pattern to find
    g_LV_in[g_LV_in_num].len = (FS_WcsLen(pattern) * sizeof(wchar_t)) + sizeof(wchar_t);
    g_LV_in[g_LV_in_num++].val = (void *)pattern;
    // attribute
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT8);
    g_LV_in[g_LV_in_num++].val = (void *)&attr;
    // attribute mask
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT8);
    g_LV_in[g_LV_in_num++].val = (void *)&attr_mask;
    // max. length //AP return length = (MaxLength+1)*2; !!! for WCHAR !!!!
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    //LV_in[LV_in_num++].val = (void *)&MaxLength;
    g_LV_in[g_LV_in_num++].val = (void *)&MaxLength_nonWCHAR;

    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));
    // file information
    g_LV_out[g_LV_out_num].len = sizeof(FS_DOSDirEntry);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (4 * sizeof(KAL_UINT32)));
    // file name
    aligned_entry_size = ((sizeof(FS_DOSDirEntry) + 3) >> 2) << 2;
    g_LV_out[g_LV_out_num].len = max_length;
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (5 * sizeof(KAL_UINT32)) + aligned_entry_size);

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_FINDFIRST;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 eemcs_fs_ut_findnext(KAL_UINT32 handle, FS_DOSDirEntry *file_info, wchar_t *file_name, KAL_UINT32 max_length)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;
    KAL_INT32 MaxLength_nonWCHAR = max_length/2 -1;
    KAL_UINT32 aligned_entry_size = 0;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // file handle returned from eemcs_fs_ut_findfirst
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&handle;
    // MaxLength //AP return length = (MaxLength+1)*2; !!! for WCHAR !!!!
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    //LV_in[LV_in_num++].val = (void *)&MaxLength;
    g_LV_in[g_LV_in_num++].val = (void *)&MaxLength_nonWCHAR;
    
    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));
    // file information
    g_LV_out[g_LV_out_num].len = sizeof(FS_DOSDirEntry);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (4 * sizeof(KAL_UINT32)));
    // file name
    aligned_entry_size = ((sizeof(FS_DOSDirEntry) + 3) >> 2) << 2;
    g_LV_out[g_LV_out_num].len = max_length;
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (5 * sizeof(KAL_UINT32)) + aligned_entry_size);

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_FINDNEXT;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

KAL_INT32 eemcs_fs_ut_findclose(KAL_UINT32 handle)
{
    KAL_INT32 index = g_eemcs_fs_ut.ut_port_index;
    KAL_INT32 ret;

    eemcs_fs_ut_reset_args();
    /* AP CCCI FS input parameters */
    // File Handle
    g_LV_in[g_LV_in_num].len = sizeof(KAL_UINT32);
    g_LV_in[g_LV_in_num++].val = (void *)&handle;

    /* AP CCCI FS output value */
    // return code
    g_LV_out[g_LV_out_num].len = sizeof(KAL_INT32);
    g_LV_out[g_LV_out_num++].val = (void *)(g_eemcs_fs_ut.FS_REQ_BUF[index].buffer + (2 * sizeof(KAL_UINT32)));

    g_ccci_fs_paras.index = index;
    g_ccci_fs_paras.op_id = FS_CCCI_OP_CLOSE;
    g_ccci_fs_paras.pLV_in = g_LV_in;
    g_ccci_fs_paras.LV_in_para_cnt = g_LV_in_num;
    g_ccci_fs_paras.pLV_out = g_LV_out;
    g_ccci_fs_paras.LV_out_para_cnt = g_LV_out_num;

    ret = CCCI_FS_OP_Wrapper(&g_ccci_fs_paras);

    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief The handler after receiving ACK commands from AP
 * @param
 *     None
 * @return
 *     KAL_SUCCESS indicates the ACK command is handled correctly.
 *     KAL_FAIL indicates the ACK command is not supported.
 */
KAL_INT32 eemcs_fs_ut_ul_handler(void)
{
    KAL_UINT32 op_id = g_ccci_fs_paras.op_id;
    DEBUG_LOG_FUNCTION_ENTRY;

    switch (op_id) {
        case FS_CCCI_OP_GETDRIVE:
        {
            char drv_name = *((KAL_UINT32*)g_LV_out[0].val);
            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_GETDRIVE. Drive = %c", drv_name);
            if (g_test_drive[g_eemcs_fs_ut.drive_idx].drive[0] == drv_name)
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_GETDRIVE [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_GETDRIVE [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_GETCLUSTERSIZE:
        {
            KAL_UINT32 size = *((KAL_UINT32*)g_LV_out[0].val);

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_GETCLUSTERSIZE. Cluster Size = %d", size);
            if (size > 0)
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_GETCLUSTERSIZE [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_GETCLUSTERSIZE [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_CREATEDIR:
        {
            struct file *fp = NULL;
            KAL_INT32 ret = *((KAL_UINT32*)g_LV_out[0].val);
            char dir[NAME_MAX] = {0};

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_CREATEDIR. Return Code = %d", ret);
            if (ret == FS_NO_ERROR) {
                sprintf(dir, "%s/%s",
                    g_test_drive[g_eemcs_fs_ut.drive_idx].fs_root,
                    g_test_case[g_eemcs_fs_ut.test_case_idx].reserved);
                if ((fp = file_open(dir, O_RDONLY, 0777)) != NULL) {
                    DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_CREATEDIR [PASS] ^_^");
                    file_close(fp);
                } else {
                    DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_CREATEDIR [FAIL] @_@");
                    DBGLOG(FSUT, TRA, "[FSUT] ==> Folder doesn't exist !!");
                }
            }
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_CREATEDIR [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_REMOVEDIR:
        {
            struct file *fp = NULL;
            KAL_INT32 ret = *((KAL_UINT32*)g_LV_out[0].val);
            char dir[NAME_MAX] = {0};

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_REMOVEDIR. Return Code = %d", ret);
            if (ret == FS_NO_ERROR) {
                sprintf(dir, "%s/%s",
                    g_test_drive[g_eemcs_fs_ut.drive_idx].fs_root,
                    g_test_case[g_eemcs_fs_ut.test_case_idx].reserved);
                if ((fp = file_open(dir, O_RDONLY, 0777)) == NULL) {
                    DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_REMOVEDIR [PASS] ^_^");
                } else {
                    DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_REMOVEDIR [FAIL] @_@");
                    DBGLOG(FSUT, ERR, "[FSUT] ==> Folder still exist !!");
                }
            }
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_CREATEDIR [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_OPEN:
        {
            g_eemcs_fs_ut.fhandle = *((KAL_UINT32*)g_LV_out[0].val);
            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_OPEN. Handle Index = %d", g_eemcs_fs_ut.fhandle);
            /*
             * Check file handle value
             */
            if (g_eemcs_fs_ut.fhandle >= 1)
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_OPEN [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_OPEN [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_WRITE:
        {
            KAL_INT32 ret = *((KAL_UINT32*)(g_LV_out[0].val));
            KAL_UINT32 written = *((KAL_UINT32*)(g_LV_out[1].val));
            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_WRITE. Return Code = %d, Written = %d",
                ret, written);
            if (ret == FS_NO_ERROR)
                g_eemcs_fs_ut.fs_write_total += written;
            /*
             * Compare file content with source file
             */
            if (g_eemcs_fs_ut.fs_write_total == g_test_file[g_eemcs_fs_ut.ftest_idx].size) {
                char dst[NAME_MAX] = {0};
                void *src_data = NULL;
                void *dst_data = NULL;

                /* Get source file data */
                src_data = get_test_file_data(g_eemcs_fs_ut.ftest_idx);
                KAL_ASSERT(src_data != NULL);
                /* Get OP_WRITE file data */
                sprintf(dst, "%s/%s",
                    g_test_drive[g_eemcs_fs_ut.drive_idx].fs_root,
                    g_test_file[g_eemcs_fs_ut.ftest_idx].name);
                dst_data = get_file_data(dst, g_eemcs_fs_ut.fs_write_total);
                KAL_ASSERT(dst_data != NULL);
                /* Compare */
                if (memcmp(src_data, dst_data, g_eemcs_fs_ut.fs_write_total) == 0)
                    DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_WRITE [PASS] ^_^");
                else
                    DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_WRITE [FAIL] @_@");
                destroy_file_data(src_data);
                destroy_file_data(dst_data);
                g_eemcs_fs_ut.fs_write_total = 0;
                break;
            } else
                goto _wait;
        }
        case FS_CCCI_OP_GETFILESIZE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_GETFILESIZE. Return Code = %d, File Size = %d",
                *((KAL_UINT32*)(g_LV_out[0].val)),
                *((KAL_UINT32*)(g_LV_out[1].val)));
            /*
             * Compare file size with source file
             */
            if (g_test_file[g_eemcs_fs_ut.ftest_idx].size == *((KAL_UINT32*)(g_LV_out[1].val)))
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_GETFILESIZE [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_GETFILESIZE [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_READ:
        {
            void *src_data = NULL;
            char op_read[NAME_MAX] = {0};

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_READ. Return Code = %d, Read = %d, Data = 0x%p",
                *((KAL_UINT32*)(g_LV_out[0].val)),
                *((KAL_UINT32*)(g_LV_out[1].val)),
                g_LV_out[2].val);
            /*
             * Compare read data with source file
             */
            if (*((KAL_UINT32*)(g_LV_out[1].val)) == g_test_file[g_eemcs_fs_ut.ftest_idx].size) {
                src_data = get_test_file_data(g_eemcs_fs_ut.ftest_idx);
                KAL_ASSERT(src_data);
                if (memcmp(src_data, g_LV_out[2].val, *((KAL_UINT32*)(g_LV_out[1].val))) == 0)
                    DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_READ [PASS] ^_^");
                else {
                    DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_READ [FAIL] @_@");
                    sprintf(op_read, "%s/fs_ut_op_read_%03d.dat", FS_UT_TEST_FILE_DIR, op_read_cnt++);
                    dump_data_to_file(op_read, g_LV_out[2].val, *((KAL_UINT32*)(g_LV_out[1].val)));
                }
                destroy_file_data(src_data);
            } else {
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_READ FAIL @_@");
            }
            break;
        }
        case FS_CCCI_OP_CLOSE:
        {
            KAL_INT32 ret = *((KAL_UINT32*)(g_LV_out[0].val));

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_CLOSE. Return Code = %d", ret);
            if (ret == FS_NO_ERROR)
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_CLOSE [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_CLOSE [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_MOVE:
        {
            KAL_INT32 ret = *((KAL_UINT32*)(g_LV_out[0].val));

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_MOVE. Return Code = %d", ret);
            if (ret == FS_NO_ERROR) {
                char csSrc[NAME_MAX] = {0};
                char csDst[NAME_MAX] = {0};
                char *srcData = NULL;
                char *dstData = NULL;

                // Prepare source file path
                sprintf(csSrc, "%s/%s", g_test_drive[g_eemcs_fs_ut.drive_idx].fs_root, g_test_file[g_eemcs_fs_ut.ftest_idx].name);
                // Prepare destination file path
                sprintf(csDst, "%s/%s/%s",
                    g_test_drive[g_eemcs_fs_ut.drive_idx].fs_root,
                    g_test_case[g_eemcs_fs_ut.test_case_idx].reserved,
                    g_test_file[g_eemcs_fs_ut.ftest_idx].name);
                srcData = get_file_data(csSrc, g_test_file[g_eemcs_fs_ut.ftest_idx].size);
                KAL_ASSERT(srcData);
                dstData = get_file_data(csDst, g_test_file[g_eemcs_fs_ut.ftest_idx].size);
                KAL_ASSERT(dstData);
                if (memcmp(srcData, dstData, g_test_file[g_eemcs_fs_ut.ftest_idx].size) == 0)
                    DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_MOVE [PASS] ^_^");
                else
                    DBGLOG(FSUT, TRA, "[FSUT] ==> Data of destination file is incorrect !!");
            } else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_MOVE [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_RENAME:
        {
            struct file *fp = NULL;
            KAL_INT32 ret = *((KAL_UINT32*)(g_LV_out[0].val));

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_RENAME. Return Code = %d", ret);
            if (ret == FS_NO_ERROR) {
                char csRenamed[NAME_MAX] = {0};

                // Prepare renamed file name
                sprintf(csRenamed, "%s/%s%s",
                    g_test_drive[g_eemcs_fs_ut.drive_idx].fs_root,
                    g_test_case[g_eemcs_fs_ut.test_case_idx].reserved,
                    g_test_file[g_eemcs_fs_ut.ftest_idx].name);
                if ((fp = file_open(csRenamed, O_RDONLY, 0777)) != NULL) {
                    DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_RENAME [PASS] ^_^");
                    file_close(fp);
                } else
                    DBGLOG(FSUT, TRA, "[FSUT] ==> File doesn't exist !!");
            } else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_RENAME [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_DELETE:
        {
            struct file *fp = NULL;
            KAL_INT32 ret = *((KAL_UINT32*)(g_LV_out[0].val));

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_DELETE. Return Code = %d", ret);
            if (ret == FS_NO_ERROR) {
                char csDeleted[NAME_MAX] = {0};

                // Prepare renamed file name
                sprintf(csDeleted, "%s/%s%s",
                    g_test_drive[g_eemcs_fs_ut.drive_idx].fs_root,
                    g_test_case[g_eemcs_fs_ut.test_case_idx].reserved,
                    g_test_file[g_eemcs_fs_ut.ftest_idx].name);
                if ((fp = file_open(csDeleted, O_RDONLY, 0777)) == NULL) {
                    DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_DELETE [PASS] ^_^");
                } else
                    DBGLOG(FSUT, TRA, "[FSUT] ==> File still exist !!");
            } else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_DELETE [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_GETFOLDERSIZE:
        {
            KAL_INT32 ret = *((KAL_UINT32*)(g_LV_out[0].val));

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_GETFOLDERSIZE. Cluster Size = %d", ret);
            if (ret >= 0)
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_GETFOLDERSIZE [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_GETFOLDERSIZE [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_COUNT:
        {
            KAL_INT32 ret = *((KAL_UINT32*)(g_LV_out[0].val));

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_COUNT. Fild Count = %d", ret);
            if (ret == EEMCS_FS_TEST_FILE_CNT)
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_COUNT [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_COUNT [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_XDELETE:
        {
            struct file *fp = NULL;
            KAL_INT32 ret = *((KAL_UINT32*)g_LV_out[0].val);
            char dir[NAME_MAX] = {0};

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_XDELETE. Deleted = %d", ret);
            if (ret >= 0) {
                sprintf(dir, "%s/%s",
                    g_test_drive[g_eemcs_fs_ut.drive_idx].fs_root,
                    g_test_case[g_eemcs_fs_ut.test_case_idx].reserved);
                if ((fp = file_open(dir, O_RDONLY, 0777)) == NULL) {
                    DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_XDELETE [PASS] ^_^");
                } else {
                    DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_XDELETE [FAIL] @_@");
                    DBGLOG(FSUT, ERR, "[FSUT] ==> Folder still exist !!");
                }
            }
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_XDELETE [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_FINDFIRST:
        {
            char found[NAME_MAX] = {0};

            g_eemcs_fs_ut.find_handle = *((KAL_UINT32*)g_LV_out[0].val);
            FS_ConvWcsToCs(g_LV_out[2].val, found, g_LV_out[2].len);
            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_FINDFIRST. Handle = %d, Found = %s",
                g_eemcs_fs_ut.find_handle, found);
            if (g_eemcs_fs_ut.find_handle >= 1)
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_FINDFIRST [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_FINDFIRST [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_FINDNEXT:
        {
            KAL_INT32 ret = *((KAL_UINT32*)g_LV_out[0].val);
            char found[NAME_MAX] = {0};

            FS_ConvWcsToCs(g_LV_out[2].val, found, g_LV_out[2].len);
            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_FINDNEXT. Return Code = %d, Found = %s",
                ret, found);
            if (ret == FS_NO_ERROR)
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_FINDNEXT [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_FINDNEXT [FAIL] @_@");
            break;
        }
        case FS_CCCI_OP_FINDCLOSE:
        {
            KAL_INT32 ret = *((KAL_UINT32*)g_LV_out[0].val);

            DBGLOG(FSUT, TRA, "[FSUT] ---------- ACK FS_CCCI_OP_FINDCLOSE. Return Code = %d", ret);
            if (ret == FS_NO_ERROR)
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS_CCCI_OP_FINDCLOSE [PASS] ^_^");
            else
                DBGLOG(FSUT, TRA, "[FSUT] !!!!!!!!!! FS_CCCI_OP_FINDCLOSE [FAIL] @_@");
            break;
        }

        default:
            DBGLOG(FSUT, ERR, "[FSUT] Error ACK OP ID %d", op_id);
            goto _fail;
    }
    g_eemcs_fs_ut.test_case_idx++;
    eemcs_fs_ut_send_cmd();
_wait:
    DEBUG_LOG_FUNCTION_LEAVE;    
    return KAL_SUCCESS;
_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_FAIL;
}

void dump_fsd_skb_data(void *data)
{
    FS_STREAMBUFFER *stream = NULL;
    CCCI_BUFF_T *ccci_h = NULL;

    stream = (FS_STREAMBUFFER *)data;
    ccci_h = (CCCI_BUFF_T*)&stream->ccci_header;

    DBGLOG(FSUT, DBG, "[FSUT][SKB] Stream Header = 0x%p", stream);
    DBGLOG(FSUT, DBG, "[FSUT][SKB] CCCI Header = 0x%p", ccci_h);
    DBGLOG(FSUT, DBG, "[FSUT][SKB] OP ID = 0x%p", &stream->fs_operationID);
    DBGLOG(FSUT, DBG, "[FSUT][SKB] Argc = 0x%p", &stream->buffer[0]);

    DBGLOG(FSUT, DBG, "[FSUT][SKB] CCCI_H(0x%X)(0x%X)(0x%X)(0x%X)",
        ccci_h->data[0], ccci_h->data[1], ccci_h->channel, ccci_h->reserved);
    DBGLOG(FSUT, DBG, "[FSUT][SKB] OP ID = 0x%X", stream->fs_operationID);
    DBGLOG(FSUT, DBG, "[FSUT][SKB] %d Arguments", *((KAL_UINT32*)stream->buffer));
}

/*
 * @brief
 *     A function for FS UT.
 *     This function receives sk buffer containing FS commands from AP.
 * @param
 *     chn [in] Incomming channel of FS commands.
 *     skb [in] A sk buffer containing FS commands.
 * @return
 *     This function return KAL_SUCCESS always.
 */
KAL_INT32 eemcs_fs_ut_UL_write_skb_to_swq(CCCI_CHANNEL_T chn, struct sk_buff *skb)
{
    FS_STREAMBUFFER *stream = NULL;
    CCCI_BUFF_T *p_buff = NULL;
    KAL_INT32 port_index = 0;
    void *p_ccci_fs_buff;
    void *copy_src = NULL;
    void *copy_dst = NULL;
    KAL_UINT32 copy_size = 0;
    char src_file[NAME_MAX] = {0};
    char dst_file[NAME_MAX] = {0};
    DEBUG_LOG_FUNCTION_ENTRY;


    KAL_ASSERT(chn == CH_FS_TX);
    KAL_ASSERT(skb != NULL);
    dump_fs_stream_header((void *)skb->data);

    stream = (FS_STREAMBUFFER *)skb->data;
    p_buff = &stream->ccci_header;
    port_index = p_buff->reserved;
    dump_fsd_skb_data((void*)stream);

    p_ccci_fs_buff = (void*)(&g_eemcs_fs_ut.FS_REQ_BUF[port_index]);
    /******************************************
     *
     *  FSM description for re-sent mechanism
     *   (ccci_fs_buff_state == CCCI_FS_BUFF_IDLE) ==> initial status & end status
     *   (ccci_fs_buff_state == CCCI_FS_BUFF_WAIT) ==> need to receive again
     *
     ******************************************/
    sprintf(src_file, "%s/fs_ut_ap2md_src_%03d.dat", FS_UT_TEST_FILE_DIR, ap2md_cmd_cnt);
    sprintf(dst_file, "%s/fs_ut_ap2md_dst_%03d.dat", FS_UT_TEST_FILE_DIR, ap2md_cmd_cnt++);
    if (!CCCI_FS_PEER_REQ_SEND_AGAIN(p_buff)) {
        if (g_eemcs_fs_ut.ccci_fs_buff_state[port_index] == CCCI_FS_BUFF_IDLE) {
            /* copy data memory and CCCI header */
            copy_src = p_buff;
            copy_dst = p_ccci_fs_buff;
            copy_size = p_buff->data[1];
            DBGLOG(FSUT, DBG, "[FSUT][1] Port %d copy %d bytes from 0x%p to 0x%p",
                port_index, copy_size, copy_src, copy_dst);
            memcpy(copy_dst, copy_src, copy_size);
            dump_data_to_file(src_file, copy_src, copy_size);
            dump_data_to_file(dst_file, copy_dst, copy_size);
        } else if (g_eemcs_fs_ut.ccci_fs_buff_state[port_index] == CCCI_FS_BUFF_WAIT) {
            /* copy data memory and NULL, excluding CCCI header, OP id */
            copy_src = stream->buffer;
            copy_dst = p_ccci_fs_buff + g_eemcs_fs_ut.ccci_fs_buff_offset[port_index];
            copy_size = p_buff->data[1] - sizeof(CCCI_BUFF_T) - sizeof(KAL_UINT32);
            DBGLOG(FSUT, DBG, "[FSUT][2] Port %d copy %d bytes from 0x%p to 0x%p (offset %d from 0x%p)",
                port_index, copy_size, copy_src, copy_dst,
                g_eemcs_fs_ut.ccci_fs_buff_offset[port_index], p_ccci_fs_buff);
            memcpy(copy_dst, copy_src, copy_size);
            dump_data_to_file(src_file, copy_src, copy_size);
            dump_data_to_file(dst_file, copy_dst, copy_size);
            /* update CCCI header info */
            copy_src = p_buff;
            copy_dst = p_ccci_fs_buff;
            copy_size = sizeof(CCCI_BUFF_T);
            DBGLOG(FSUT, DBG, "[FSUT][3] Port %d copy %d bytes from 0x%p to 0x%p",
                port_index, copy_size, copy_src, copy_dst);
            memcpy(copy_dst, copy_src, copy_size);
            sprintf(src_file, "%s/fs_ut_ap2md_src_%03d.dat", FS_UT_TEST_FILE_DIR, ap2md_cmd_cnt);
            sprintf(dst_file, "%s/fs_ut_ap2md_dst_%03d.dat", FS_UT_TEST_FILE_DIR, ap2md_cmd_cnt++);
            dump_data_to_file(src_file, copy_src, copy_size);
            dump_data_to_file(dst_file, copy_dst, copy_size);
        } else {
            /* No such ccci_fs_buff_state state */
            KAL_ASSERT(0);
        }
        g_eemcs_fs_ut.ccci_fs_buff_state[port_index] = CCCI_FS_BUFF_IDLE;
        g_eemcs_fs_ut.ccci_fs_buff_offset[port_index] = 0;
    } else {
        if (g_eemcs_fs_ut.ccci_fs_buff_state[port_index] == CCCI_FS_BUFF_IDLE) {
            /* only "OP id" and "data" size and "CCCI header" */    
            copy_src = p_buff;
            copy_dst = p_ccci_fs_buff;
            copy_size = p_buff->data[1];
            DBGLOG(FSUT, DBG, "[FSUT][4] Port %d copy %d bytes from 0x%p to 0x%p",
                port_index, copy_size, copy_src, copy_dst);
            memcpy(copy_dst, copy_src, copy_size);
            g_eemcs_fs_ut.ccci_fs_buff_offset[port_index] += copy_size;
            dump_data_to_file(src_file, copy_src, copy_size);
            dump_data_to_file(dst_file, copy_dst, copy_size);
        } else if (g_eemcs_fs_ut.ccci_fs_buff_state[port_index] == CCCI_FS_BUFF_WAIT) {
            /* only "data" size, excluding CCCI header and OP id */
            copy_src = (void*)&stream->buffer[0];
            copy_dst = p_ccci_fs_buff + g_eemcs_fs_ut.ccci_fs_buff_offset[port_index];
            copy_size = p_buff->data[1] - sizeof(CCCI_BUFF_T) - sizeof(KAL_UINT32);
            DBGLOG(FSUT, DBG, "[FSUT][5] Port %d copy %d bytes from 0x%p to 0x%p (offset %d from 0x%p)",
                port_index, copy_size, copy_src, copy_dst,
                g_eemcs_fs_ut.ccci_fs_buff_offset[port_index], p_ccci_fs_buff);
            memcpy(copy_dst, copy_src, copy_size);
            g_eemcs_fs_ut.ccci_fs_buff_offset[port_index] += copy_size;
            dump_data_to_file(src_file, copy_src, copy_size);
            dump_data_to_file(dst_file, copy_dst, copy_size);
        } else {
            /* No such ccci_fs_buff_state state */
            KAL_ASSERT(0);
        }
        g_eemcs_fs_ut.ccci_fs_buff_state[port_index] = CCCI_FS_BUFF_WAIT;
    }

    if (g_eemcs_fs_ut.ccci_fs_buff_state[port_index] == CCCI_FS_BUFF_IDLE) {
        DBGLOG(FSUT, DBG, "[FSUT] Port %d packet is receiving done ...", port_index);
        dump_fs_stream_header(p_ccci_fs_buff);
        // Use ccci_fs_get_buffer to decode data
        ccci_fs_get_buff(port_index, g_ccci_fs_paras.op_id, g_ccci_fs_paras.pLV_out, &g_ccci_fs_paras.LV_out_para_cnt);
        // Get what your want from argument list
        eemcs_fs_ut_ul_handler();
    } else {
        DBGLOG(FSUT, DBG, "[FSUT] Port %d is still waiting ...", port_index);
    }
    
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
}

/*
 * @brief A function to simulate MD sending a FS operation to AP
 * @param
 *     None
 * @return
 *     KAL_SUCCESS indicates UT is in progress correctly.
 *     KAL_FAIL indicates UT is something wrong.
 */
KAL_INT32 eemcs_fs_ut_send_cmd(void)
{
    EEMCS_FS_TEST_CASE *test_case = NULL;
    char csSrc[NAME_MAX] = {0};
    char csDst[NAME_MAX] = {0};
    wchar_t wcsSrc[NAME_MAX] = {0};
    wchar_t wcsDst[NAME_MAX] = {0};
    DEBUG_LOG_FUNCTION_ENTRY;

_repeat:
    /* All commands are tested */
    if (g_eemcs_fs_ut.test_case_idx >= EEMCS_FS_TEST_CASE_CNT) {
        /* All file are tested */
        if (g_eemcs_fs_ut.ftest_idx >= EEMCS_FS_TEST_FILE_CNT - 1) {
            /* All drive are tested */            
            if (g_eemcs_fs_ut.drive_idx >= EEMCS_FS_TEST_DRV_CNT - 1) {
                DBGLOG(FSUT, TRA, "[FSUT] ++++++++++ FS UT DONE !!! ++++++++++");
                goto _ok;
            } else {
            /* Test next drive */
                g_eemcs_fs_ut.test_case_idx = 0;
                g_eemcs_fs_ut.ftest_idx = 0;
                g_eemcs_fs_ut.drive_idx++;
                DBGLOG(FSUT, TRA, "[FSUT] Test next drive (%s)",
                    g_test_drive[g_eemcs_fs_ut.drive_idx].drive);
            }
        } else {
        /* Test next file */
            g_eemcs_fs_ut.test_case_idx = 0;
            g_eemcs_fs_ut.ftest_idx++;
            DBGLOG(FSUT, TRA, "[FSUT] Test next file (%s\\%s)",
                g_test_drive[g_eemcs_fs_ut.drive_idx].drive,
                g_test_file[g_eemcs_fs_ut.ftest_idx].name);
        }
    }

    test_case = &g_test_case[g_eemcs_fs_ut.test_case_idx];
    switch (test_case->op_id) {
        case FS_CCCI_OP_REPEAT_START:
        {
            g_eemcs_fs_ut.test_case_idx++;
            goto _repeat;
            break;
        }
        case FS_CCCI_OP_REPEAT_END:
        {
            if (g_eemcs_fs_ut.ftest_idx < EEMCS_FS_TEST_FILE_CNT - 1) {
                g_eemcs_fs_ut.test_case_idx = g_eemcs_fs_ut.loop_start;
                g_eemcs_fs_ut.ftest_idx++;
                DBGLOG(FSUT, DBG, "[FSUT] Repeat to test next file ...");
                goto _repeat;
            } else {
                g_eemcs_fs_ut.test_case_idx++;
                goto _repeat;
            }
        }
        case FS_CCCI_OP_GETDRIVE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_GETDRIVE");
            eemcs_fs_ut_getdrive(g_test_drive[g_eemcs_fs_ut.drive_idx].type,
                2, FS_DRIVE_V_REMOVABLE | g_test_drive[g_eemcs_fs_ut.drive_idx].type);
            break;
        }
        case FS_CCCI_OP_GETCLUSTERSIZE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_GETCLUSTERSIZE");
            eemcs_fs_ut_getclustersize(g_test_drive[g_eemcs_fs_ut.drive_idx].drive[0]);
            break;
        }
        case FS_CCCI_OP_CREATEDIR:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_CREATEDIR");
            sprintf(csSrc, "%s/%s", g_test_drive[g_eemcs_fs_ut.drive_idx].drive, g_test_case[g_eemcs_fs_ut.test_case_idx].reserved);
            DBGLOG(FSUT, TRA, "[FSUT] Create Directory %s", csSrc);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            eemcs_fs_ut_createdir(wcsSrc);
            break;
        }
        case FS_CCCI_OP_REMOVEDIR:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_REMOVEDIR");
            sprintf(csSrc, "%s/%s", g_test_drive[g_eemcs_fs_ut.drive_idx].drive, g_test_case[g_eemcs_fs_ut.test_case_idx].reserved);
            DBGLOG(FSUT, TRA, "[FSUT] Remove Directory %s", csSrc);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            eemcs_fs_ut_removedir(wcsSrc);
            break;
        }
        case FS_CCCI_OP_OPEN:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_OPEN");
            sprintf(csSrc, "%s/%s", g_test_drive[g_eemcs_fs_ut.drive_idx].drive, g_test_file[g_eemcs_fs_ut.ftest_idx].name);
            DBGLOG(FSUT, TRA, "[FSUT] Test File %s", csSrc);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            eemcs_fs_ut_open(wcsSrc, FS_CREATE);
            break;
        }
        case FS_CCCI_OP_WRITE:
        {
            void *data = NULL;

            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_WRITE");
            sprintf(csSrc, "%s/%s", FS_UT_TEST_FILE_DIR, g_test_file[g_eemcs_fs_ut.ftest_idx].name);
            data = get_file_data(csSrc, g_test_file[g_eemcs_fs_ut.ftest_idx].size);
            KAL_ASSERT(data != NULL);
            eemcs_fs_ut_write(g_eemcs_fs_ut.fhandle, data, g_test_file[g_eemcs_fs_ut.ftest_idx].size, NULL);
            destroy_file_data(data);
            break;
        }
        case FS_CCCI_OP_GETFILESIZE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_GETFILESIZE");
            eemcs_fs_ut_get_file_size(g_eemcs_fs_ut.fhandle, NULL);
            break;
        }
        case FS_CCCI_OP_READ:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_READ");
            eemcs_fs_ut_read(g_eemcs_fs_ut.fhandle, NULL, g_test_file[g_eemcs_fs_ut.ftest_idx].size, NULL);
            break;
        }
        case FS_CCCI_OP_CLOSE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_CLOSE");
            eemcs_fs_ut_close(g_eemcs_fs_ut.fhandle);
            break;
        }
        case FS_CCCI_OP_MOVE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_MOVE");
            sprintf(csSrc, "%s/%s", g_test_drive[g_eemcs_fs_ut.drive_idx].drive, g_test_file[g_eemcs_fs_ut.ftest_idx].name);
            sprintf(csDst, "%s/%s/%s",
                g_test_drive[g_eemcs_fs_ut.drive_idx].drive,
                g_test_case[g_eemcs_fs_ut.test_case_idx].reserved,
                g_test_file[g_eemcs_fs_ut.ftest_idx].name);
            DBGLOG(FSUT, TRA, "[FSUT] Move file from %s to %s", csSrc, csDst);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            FS_ConvCsToWcs(csDst, wcsDst, NAME_MAX);
            eemcs_fs_ut_move(wcsSrc, wcsDst, FS_MOVE_COPY);
            break;
        }
        case FS_CCCI_OP_RENAME:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_RENAME");
            // Prepare file path
            sprintf(csSrc, "%s/%s", g_test_drive[g_eemcs_fs_ut.drive_idx].drive, g_test_file[g_eemcs_fs_ut.ftest_idx].name);
            sprintf(csDst, "%s/%s%s",
                g_test_drive[g_eemcs_fs_ut.drive_idx].drive,
                g_test_case[g_eemcs_fs_ut.test_case_idx].reserved,
                g_test_file[g_eemcs_fs_ut.ftest_idx].name);
            DBGLOG(FSUT, TRA, "[FSUT] Rename from %s to %s", csSrc, csDst);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            FS_ConvCsToWcs(csDst, wcsDst, NAME_MAX);
            eemcs_fs_ut_rename(wcsSrc, wcsDst);
            break;
        }
        case FS_CCCI_OP_DELETE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_DELETE");
            // Prepare file path
            sprintf(csSrc, "%s/%s%s",
                g_test_drive[g_eemcs_fs_ut.drive_idx].drive,
                g_test_case[g_eemcs_fs_ut.test_case_idx].reserved,
                g_test_file[g_eemcs_fs_ut.ftest_idx].name);
            DBGLOG(FSUT, TRA, "[FSUT] Delete file %s", csSrc);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            eemcs_fs_ut_delete(wcsSrc);
            break;
        }
        case FS_CCCI_OP_GETFOLDERSIZE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_GETFOLDERSIZE");
            // Prepare folder path
            sprintf(csSrc, "%s/%s",
                g_test_drive[g_eemcs_fs_ut.drive_idx].drive,
                g_test_case[g_eemcs_fs_ut.test_case_idx].reserved);
            DBGLOG(FSUT, TRA, "[FSUT] Get size of folder %s", csSrc);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            eemcs_fs_ut_getfoldersize(wcsSrc, FS_COUNT_IN_CLUSTER);
            break;
        }
        case FS_CCCI_OP_COUNT:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_COUNT");
            sprintf(csSrc, "%s/%s",
                g_test_drive[g_eemcs_fs_ut.drive_idx].drive,
                g_test_case[g_eemcs_fs_ut.test_case_idx].reserved);
            DBGLOG(FSUT, TRA, "[FSUT] Get count of folder %s", csSrc);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            eemcs_fs_ut_count(wcsSrc, FS_FILE_TYPE);
            break;
        }
        case FS_CCCI_OP_XDELETE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_XDELETE");
            sprintf(csSrc, "%s/%s",
                g_test_drive[g_eemcs_fs_ut.drive_idx].drive,
                g_test_case[g_eemcs_fs_ut.test_case_idx].reserved);
            DBGLOG(FSUT, TRA, "[FSUT] Delete folder %s recursively", csSrc);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            eemcs_fs_ut_xdelete(wcsSrc, FS_FILE_TYPE|FS_DIR_TYPE|FS_RECURSIVE_TYPE);
            break;
        }
        case FS_CCCI_OP_FINDFIRST:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_FINDFIRST");
            sprintf(csSrc, "%s/%s",
                g_test_drive[g_eemcs_fs_ut.drive_idx].drive,
                g_test_case[g_eemcs_fs_ut.test_case_idx].reserved);
            DBGLOG(FSUT, TRA, "[FSUT] Find pattern %s", csSrc);
            FS_ConvCsToWcs(csSrc, wcsSrc, NAME_MAX);
            eemcs_fs_ut_findfirst(wcsSrc, 0, 0, NULL, NULL, NAME_MAX);
            break;
        }
        case FS_CCCI_OP_FINDNEXT:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_FINDNEXT");
            eemcs_fs_ut_findnext(g_eemcs_fs_ut.find_handle, NULL, NULL, NAME_MAX);
            break;
        }
        case FS_CCCI_OP_FINDCLOSE:
        {
            DBGLOG(FSUT, TRA, "[FSUT] ====> IN FS_CCCI_OP_FINDCLOSE");
            eemcs_fs_ut_findclose(g_eemcs_fs_ut.find_handle);
            break;
        }
        default:
            DBGLOG(FSUT, ERR, "[FSUT] !!!!> Error FS UT Test Case Index %d", g_eemcs_fs_ut.test_case_idx);
            goto _fail;
    }
_ok:
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_FAIL;
}

KAL_INT32 eemcs_fs_ut_init(void)
{
#ifdef _EEMCS_FS_UT
    KAL_UINT32 i = 0;
    char file_name[NAME_MAX] = {0};
    DEBUG_LOG_FUNCTION_ENTRY;

    memset(&g_eemcs_fs_ut, 0, sizeof(EEMCS_FS_UT_SET));
    g_eemcs_fs_ut.ut_port_index = FS_UT_PORT_INDEX;
    /* Initialize buffer status */
    for (i = 0; i < FS_CCCI_REQ_BUFFER_NUM; i++) {
        g_eemcs_fs_ut.ccci_fs_buff_offset[i] = 0;
        g_eemcs_fs_ut.ccci_fs_buff_state[i] = CCCI_FS_BUFF_IDLE;
    }
    /* Find start and end indicator of repeat commands */
    for (i = 0; i < EEMCS_FS_TEST_CASE_CNT; i++) {
        g_test_case[i].index =i;
        if (g_test_case[i].op_id == FS_CCCI_OP_REPEAT_START)
            g_eemcs_fs_ut.loop_start = i;
        if (g_test_case[i].op_id == FS_CCCI_OP_REPEAT_END)
            g_eemcs_fs_ut.loop_end = i;
    }
    /* Generate test binary file */
    for (i = 0; i < EEMCS_FS_TEST_FILE_CNT; i++) {
        sprintf(file_name, "%s/%s", FS_UT_TEST_FILE_DIR, g_test_file[i].name);
        if (gen_ap_random_file(file_name, g_test_file[i].size) == KAL_FAIL) {
            eemcs_fs_ut_exit();
            goto _fail;
        }
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
_fail:
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_FAIL;
#else
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif
}

KAL_INT32 eemcs_fs_ut_exit(void)
{
#ifdef _EEMCS_FS_UT
    int ret = 0;
    KAL_UINT32 i = 0;
    char file_name[NAME_MAX] = {0};
    DEBUG_LOG_FUNCTION_ENTRY;

    for (i = 0; i < EEMCS_FS_TEST_FILE_CNT; i++) {
        sprintf(file_name, "%s/%s", FS_UT_TEST_FILE_DIR, g_test_file[i].name);
        ret = remove_file(file_name);
        if (ret != 0) {
            DBGLOG(FSUT, ERR, "[FSUT] Failed to remove file %s", file_name);
        }
    }
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#else
    DEBUG_LOG_FUNCTION_ENTRY;
    DEBUG_LOG_FUNCTION_LEAVE;
    return KAL_SUCCESS;
#endif
}


/*
 * @brief Trigger FS UT procedure
 * @param
 *     None
 * @return
 *     None
 */
void eemcs_fs_ut_trigger(void)
{
    KAL_INT32 ut_port_index = 0;
    KAL_UINT32 loop_start = 0;
    KAL_UINT32 loop_end = 0;

    /* Reset some variables */
    // Keep port index
    ut_port_index = g_eemcs_fs_ut.ut_port_index;
    loop_start = g_eemcs_fs_ut.loop_start;
    loop_end = g_eemcs_fs_ut.loop_end;
    memset(&g_eemcs_fs_ut, 0, sizeof(EEMCS_FS_UT_SET));
    g_eemcs_fs_ut.ut_port_index = ut_port_index;
    g_eemcs_fs_ut.loop_start = loop_start;
    g_eemcs_fs_ut.loop_end = loop_end;
    eemcs_fs_ut_reset_args();
    /* Trigger FS UT to run */
    eemcs_fs_ut_send_cmd();
}

/*
 * @brief Set the port index to be tested
 * @param
 *     index [in] Port index
 * @return
 *     KAL_SUCCESS if port index is set correctly;
 *     KAL_FAIL otherwise.
 */
KAL_INT32 eemcs_fs_ut_set_index(KAL_UINT32 index)
{
    if (index >=0 && index < 5) {
        g_eemcs_fs_ut.ut_port_index = index;
        return KAL_SUCCESS;
    } else {
        DBGLOG(FSUT, ERR, "[FSUT] %d is an invalid index !!", index);
        return KAL_FAIL;
    }
}

/*
 * @brief Return the port index currently in use.
 * @param
 *     None
 * @return
 *     The port index currently in use.
 */
KAL_UINT32 eemcs_fs_ut_get_index(void)
{
    return g_eemcs_fs_ut.ut_port_index;
}

/*
 * @brief Display information about FS UT
 * @param
 *     None
 * @return
 *     None
 */
void eemcs_fs_ut_dump(void)
{
    printk("[FSUT] g_eemcs_fs_ut.test_case_idx = %d\r\n", g_eemcs_fs_ut.test_case_idx);
    printk("[FSUT] g_eemcs_fs_ut.ftest_idx = %d\r\n", g_eemcs_fs_ut.ftest_idx);
    printk("[FSUT] g_eemcs_fs_ut.fs_write_total = %d\r\n", g_eemcs_fs_ut.fs_write_total);
    printk("[FSUT] g_eemcs_fs_ut.ut_port_index = %d\r\n", g_eemcs_fs_ut.ut_port_index);
    printk("[FSUT] g_eemcs_fs_ut.loop_start = %d\r\n", g_eemcs_fs_ut.loop_start);
    printk("[FSUT] g_eemcs_fs_ut.loop_end = %d\r\n", g_eemcs_fs_ut.loop_end);
}
#endif //_EEMCS_FS_UT
