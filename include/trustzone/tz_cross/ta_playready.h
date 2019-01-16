#ifndef __TRUSTZONE_TA_PLAYREADY__
#define __TRUSTZONE_TA_PLAYREADY__


#define TZ_TA_PLAYREADY_UUID         "b25bf100-d276-11e2-8b8b-0800200c9a66"
                               
#define PLAYREADY_PROVISIONED_CERT  1
#define PLAYREADY_PROVISIONED_KEY 2
#define PLAYREADY_PROVISIONED_CLEAR_KEY  3  // for debug

#define TZ_DRM_UI64EQL   1
#define TZ_DRM_UI64LES    2

#define TZ_TOKEN_TOKEN    1
#define TZ_TOKEN_VALUE     2
#define TZ_VALUE_TOKEN     3

#define SUPPORT_MULTIPLE_INSTANCE 1

#define C_SECONDS_IN_ROLLBACK_GRACE_PERIOD 30

/* Data Structure for Playready TA */
/* You should define data structure used both in REE/TEE here
     N/A for Playready TA */

/* Command for Playready TA */
#define TZCMD_PLAYREADY_BASE                             0x00000000
#define TZCMD_PLAYREADY_PROVISIONED_DATA_GET             0x00000001
#define TZCMD_PLAYREADY_ECCP256_KEYPAIR_GEN              0x00000002
#define TZCMD_PLAYREADY_ECCP256_KEY_SET                  0x00000003
#define TZCMD_PLAYREADY_ECDSAP256_SIGN                   0x00000004
#define TZCMD_PLAYREADY_ECCP256_DECRYPT                  0x00000005
#define TZCMD_PLAYREADY_OMAC1_KEY_SET                    0x00000006
#define TZCMD_PLAYREADY_OMAC1_VERIFY                     0x00000007
#define TZCMD_PLAYREADY_OMAC1_SIGN                       0x00000008
#define TZCMD_PLAYREADY_COPYBYTE                         0x00000009
#define TZCMD_PLAYREADY_CONTENTKEY_AESCTR_SET            0x0000000a
#define TZCMD_PLAYREADY_CONTENT_AESCTR_DECRYPT           0x0000000b
#define TZCMD_PLAYREADY_AESECB_KEY_SET                   0x0000000c
#define TZCMD_PLAYREADY_AESECB_ENCRYPT                   0x0000000d
#define TZCMD_PLAYREADY_AESECB_DECRYPT                   0x0000000e
#define TZCMD_PLAYREADY_GET_KFKEY                        0x0000000f
#define TZCMD_PLAYREADY_AESCBC_KEY_SET                   0x00000010
#define TZCMD_PLAYREADY_AESCBC_ENCRYPT                   0x00000011
#define TZCMD_PLAYREADY_AESCBC_DECRYPT                   0x00000012
#define TZCMD_PLAYREADY_HANDLE_CONTENT_AESCTR_DECRYPT    0x00000013
#define TZCMD_PLAYREADY_KEYFILE_DECRYPT                  0x00000014
#define TZCMD_PLAYREADY_KEYFILE_ENCRYPT                  0x00000015
#define TZCMD_PLAYREADY_TOKENTIME_COMPARE          0x00000016
#define TZCMD_PLAYREADY_TOKENTIME_UPDATE                 0x00000017
#define TZCMD_PLAYREADY_MACHINEDATETIME_CHECK            0x00000019


typedef struct PLAYREADY_IVDATA {
    unsigned long long qwInitializationVector;
    unsigned long long qwBlockOffset;
    unsigned long  bByteOffset;
} PLAYREADY_IVDATA;


typedef struct TZ_PLAYREADY_ENCINFO{
    char                                        role[100];	
    unsigned int                         dataSize;                  //total enc buffer size	
    unsigned int                         segNum;                  //trunk number	
    PLAYREADY_IVDATA          iv[10];                      //IV data for each trunk
    unsigned int                         offset[10];               //pointer to an integer array, each element describe clear data size
    unsigned int                         length[10];              //pointer to an integer array, each element describe enc data size
    unsigned int                      dstHandle;              //true : dstData is a handle; false : dstData is a buffer;        

}TZ_PLAYREADY_ENCINFO;

#ifdef SUPPORT_MULTIPLE_INSTANCE

#define DRM_AES_KEYSIZE_128 ( 16 ) /* Size ( in bytes ) of a 128 bit key */

/* Now at least two or more process will use ta_playready.c at same time , drm server and media server */
#define MAX_AESECB_KEYS_INSTANCE   4
#define MAX_OMAC1_KEYS_INSTANCE   4

typedef struct TZ_PLAYREADY_AESECB_KEYS{
    uint32_t handle;   // tee session handle     
    char aesecbKey[DRM_AES_KEYSIZE_128];
    uint32_t bProtect;
    uint32_t bInUse;
}TZ_PLAYREADY_AESECB_KEYS;

typedef struct TZ_PLAYREADY_OMAC1_KEYS{
    uint32_t handle;   // tee session handle 
    char omac1Key[DRM_AES_KEYSIZE_128];
    uint32_t bProtect;
    uint32_t bInUse;	
}TZ_PLAYREADY_OMAC1_KEYS;

#endif

#endif /* __TRUSTZONE_TA_PLAYREADY__ */

