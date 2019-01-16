/** Commands for TA Debug **/

#ifndef __TRUSTZONE_TA_DBG__
#define __TRUSTZONE_TA_DBG__

#define TZ_TA_DBG_UUID   "42a10730-f349-11e2-a99a-d4856458b228"

// enable secure memory/chunk memory information debug
#define MTEE_TA_DBG_ENABLE_MEMINFO

/* Command for Debug */
#define TZCMD_DBG_SECUREMEM_INFO      0
#define TZCMD_DBG_SECURECM_INFO       1

#endif /* __TRUSTZONE_TA_DBG__ */
