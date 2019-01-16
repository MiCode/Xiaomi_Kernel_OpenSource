#ifndef __TRUSTZONE_TA_NDBG__
#define __TRUSTZONE_TA_NDBG__

#define TZ_TA_NDBG_UUID   "820b5780-dd5b-11e2-a28f-0800200c9a66"


/* Data Structure for NDBG TA */
/* You should define data structure used both in REE/TEE here
   N/A for Test TA */
#define NDBG_BAT_ST_SIZE    16
#define URAN_SIZE           16   
#define NDBG_REE_ENTROPY_SZ (NDBG_BAT_ST_SIZE + URAN_SIZE)

/* Command for DAPC TA */

#define TZCMD_NDBG_INIT           0
#define TZCMD_NDBG_WAIT_RESEED    1
#define TZCMD_NDBG_RANDOM         2

#endif /* __TRUSTZONE_TA_NDBG__ */
