#ifndef __TRUSTZONE_TA_VENC__
#define __TRUSTZONE_TA_VENC__

#define TZ_TA_VENC_UUID   "e7305aca-375e-4f69-ac6b-ba9d3c9a1f97"

//#define UT_ENABLE 0
//#define DONT_USE_BS_VA 1  // for VP path integration set to 1,  for mfv_ut set to 0
//#define USE_MTEE_M4U
//#define USE_MTEE_DAPC


/* Command for VENC TA */
#define TZCMD_VENC_AVC_INIT       0
#define TZCMD_VENC_AVC_ENCODE     1
#define TZCMD_VENC_AVC_ENCODE_NS     2
#define TZCMD_VENC_AVC_DEINIT      3
#define TZCMD_VENC_AVC_ALLOC_WORK_BUF   4
#define TZCMD_VENC_AVC_FREE_WORK_BUF    5
#define TZCMD_VENC_AVC_SHARE_WORK_BUF    6
#define TZCMD_VENC_AVC_COPY_WORK_BUF    7

#define TZCMD_VENC_TEST        100
#endif /* __TRUSTZONE_TA_VENC__ */
