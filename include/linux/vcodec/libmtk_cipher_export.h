#ifndef _LIBMTK_CIPHER_EXPORT_H
#define _LIBMTK_CIPHER_EXPORT_H

#define HEVC_BLK_LEN                            (20480)     // bytes
#define HEVC_MOD                                "HEVC_MOD"
#define HEVC_NANO                               1000000000ULL
#define HEVC_CIPHER_LEN                         (102400)    // bytes


typedef struct
{
    unsigned char buf[HEVC_BLK_LEN];
    unsigned int len;
} HEVC_BLK;

typedef enum
{
    VIDEO_ENCRYPT_CODEC_NONE      = 0x0,
    VIDEO_ENCRYPT_CODEC_HEVC_ENC  = 0x1,
    VIDEO_ENCRYPT_CODEC_HEVC_DEC  = 0x2,
    VIDEO_ENCRYPT_CODEC_MAX       = 0xffffffff
} VIDEO_ENCRYPT_CODEC_T;


typedef int (*hevc_api_funp)(HEVC_BLK *p_hevc_blk);
typedef int (*hevc_api_initk_funp)(unsigned char *key, unsigned int klen);


#define SEC_OK                                  0x0
#define SEC_FAIL                                0x1

/* HEVC shared lib*/
#define ERR_HEVC_NOT_CORRECT_MODE               0x10000
#define ERR_HEVC_DATA_NOT_ALIGNED               0x10001
#define ERR_HEVC_ENC_IOCTL_FAIL                 0x10002
#define ERR_HEVC_DEC_IOCTL_FAIL                 0x10002
#define ERR_HEVC_CIPHER_UT_FAIL                 0x10003
#define ERR_HEVC_DATA_IS_NULL                   0x10004
#define ERR_HEVC_DATA_LENGTH_NOT_VALID          0x10005
#define ERR_HEVC_SW_ENC_ERROR                   0x10006
#define ERR_HEVC_SW_DEC_ERROR                   0x10007
#define ERR_HEVC_INIT_SW_KEY_ERROR              0x10008


/* HEVC sample*/
#define ERR_HEVC_CIPHER_LIB_NOT_FOUND           0x20001
#define ERR_HEVC_SW_DEC_BLOCK_SYM_NOT_FOUND     0x20002
#define ERR_HEVC_HW_ENC_BLOCK_SYM_NOT_FOUND     0x20003
#define ERR_HEVC_INIT_SW_KEY_SYM_NOT_FOUND      0x20004
#define ERR_HEVC_INPUT_FILE_NOT_FOUND           0x20005
#define ERR_HEVC_OUTPUT_FILE_NOT_FOUND          0x20006
#define ERR_HEVC_SW_DEC_FILE_SYM_NOT_FOUND      0x20007
#define ERR_HEVC_SW_DEC_FILE_FAILED             0x20008
#define ERR_HEVC_UNKNOWN                        0x2FFFF


/* Define LOG LEVEL*/
#define SEC_LOG_TRACE 0 //For source code trace
#define SEC_LOG_DEBUG 0 //For debug purpose
#define SEC_LOG_ERROR 1 //For critical error dump
#define SEC_LOG_INFO  1 //For information to know when processing in normal 

/* DEBUG MACRO */
#define SMSG_TRACE(...) \
    do { if (SEC_LOG_TRACE)  printf(__VA_ARGS__); } while (0)

#define SMSG_DEBUG(...) \
    do { if (SEC_LOG_DEBUG) printf(__VA_ARGS__); } while (0)

#define SMSG_ERROR(...) \
    do { if (SEC_LOG_ERROR) printf(__VA_ARGS__); } while (0)

#define SMSG_INFO(...) \
    do { if (SEC_LOG_INFO) printf(__VA_ARGS__); } while (0)


#define HEVC_ENCRYTP_FILE_PATH            "/data/mediaserver"
#define HEVC_ENC_SW_ENCRYPT_FILE_PATH     "/system/lib/libhevce_sb.ca7.android.so"
#define HEVC_ENC_HW_ENCRYPT_FILE_PATH     "/data/mediaserver/sb.ca7.android_hwenc.so"
#define HEVC_ENC_HW_DECRYPT_FILE_PATH     "/data/mediaserver/sb.ca7.android_hwdec.so"
#define HEVC_DEC_SW_ENCRYTP_FILE_PATH     "/system/lib/libHEVCdec_sa.ca7.android.so"
#define HEVC_DEC_HW_ENCRYPT_FILE_PATH     "/data/mediaserver/dec_sa.ca7.android_hwenc.so"
#define HEVC_DEC_HW_DECRYPT_FILE_PATH     "/data/mediaserver/dec_sa.ca7.android_hwdec.so"

#endif   /*_LIBMTK_CIPHER_EXPORT_H*/

