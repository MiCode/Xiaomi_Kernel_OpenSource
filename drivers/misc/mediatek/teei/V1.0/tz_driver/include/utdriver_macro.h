#include "utdriver_irq.h"

#define START_STATUS    (0)
#define END_STATUS      (1)
#define VFS_SIZE        0x80000

#define FAST_CALL_TYPE			(0x100)
#define STANDARD_CALL_TYPE		(0x200)
#define TYPE_NONE			(0x300)

#define SHMEM_ENABLE   0
#define SHMEM_DISABLE  1

#define VDRV_MAX_SIZE			(0x80000)

#define FAST_CREAT_NQ                   (0x40)
#define FAST_ACK_CREAT_NQ               (0x41)
#define FAST_CREAT_VDRV                 (0x42)
#define FAST_ACK_CREAT_VDRV             (0x43)
#define FAST_CREAT_SYS_CTL              (0x44)
#define FAST_ACK_CREAT_SYS_CTL  	(0x45)
#define FAST_CREAT_FDRV                 (0x46)
#define FAST_ACK_CREAT_FDRV             (0x47)

#define NQ_CALL_TYPE                    (0x60)
#define VDRV_CALL_TYPE                  (0x61)
#define SCHD_CALL_TYPE                  (0x62)
#define FDRV_ACK_TYPE                   (0x63)

#define MAX_BUFF_SIZE           (4096)

#define VALID_TYPE      (1)
#define INVALID_TYPE    (0)

#define MESSAGE_SIZE                    (4096)

#define NQ_SIZE                 (4096)
#define NQ_BUFF_SIZE            (4096)
#define NQ_BLOCK_SIZE           (32)
#define BLOCK_MAX_COUNT         (NQ_BUFF_SIZE / NQ_BLOCK_SIZE - 1)

#define FP_SYS_NO               (100)
#define FP_BUFF_SIZE            (512 * 1024)

#define KEYMASTER_SYS_NO               (101)
#define KEYMASTER_BUFF_SIZE            (512 * 1024)

#define CTL_BUFF_SIZE                   (4096)
#define VDRV_MAX_SIZE                   (0x80000)
#define NQ_VALID                                1

#define TEEI_VFS_NUM                    0x8

#define MESSAGE_LENGTH                  (4096)
#define MESSAGE_SIZE                    (4096)

#define CAPI_CALL       0x01
#define FDRV_CALL       0x02
#define BDRV_CALL       0x03
#define SCHED_CALL	0x04

#define VFS_SYS_NO 	0x08
#define REETIME_SYS_NO	0x07

#define UT_DMA_ZONE
