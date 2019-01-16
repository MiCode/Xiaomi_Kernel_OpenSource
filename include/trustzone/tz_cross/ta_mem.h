/** Commands for TA memory **/

#ifndef __TRUSTZONE_TA_MEM__
#define __TRUSTZONE_TA_MEM__

#define TZ_TA_MEM_UUID   "4477588a-8476-11e2-ad15-e41f1390d676"

/* Command for Secure Memory Management */
#define TZCMD_MEM_SHAREDMEM_REG       0
#define TZCMD_MEM_SHAREDMEM_UNREG     1
#define TZCMD_MEM_SECUREMEM_ALLOC     2
#define TZCMD_MEM_SECUREMEM_REF       3
#define TZCMD_MEM_SECUREMEM_UNREF     4
#define TZCMD_MEM_SECURECM_ALLOC      5
#define TZCMD_MEM_SECURECM_REF        6
#define TZCMD_MEM_SECURECM_UNREF      7
#define TZCMD_MEM_SECURECM_RELEASE    8
#define TZCMD_MEM_SECURECM_APPEND     9
#define TZCMD_MEM_SECURECM_READ      10
#define TZCMD_MEM_SECURECM_WRITE     11
#define TZCMD_MEM_SECURECM_RSIZE     12
#define TZCMD_MEM_TOTAL_SIZE         13

#endif /* __TRUSTZONE_TA_MEM__ */
