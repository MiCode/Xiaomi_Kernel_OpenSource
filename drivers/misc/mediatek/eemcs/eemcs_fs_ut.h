#ifndef __EEMCS_FS_UT__
#define __EEMCS_FS_UT__

#include "eemcs_ccci.h"
#include "eemcs_cfg.h"

#ifdef _EEMCS_FS_UT
KAL_INT32 eemcs_fs_ut_init(void);
KAL_INT32 eemcs_fs_ut_exit(void);

void eemcs_fs_ut_trigger(void);
void eemcs_fs_ut_dump(void);
KAL_INT32 eemcs_fs_ut_set_index(KAL_UINT32 index);
KAL_UINT32 eemcs_fs_ut_get_index(void);

KAL_INT32 eemcs_fs_ut_UL_write_skb_to_swq(CCCI_CHANNEL_T, struct sk_buff *);

#endif //_EEMCS_FS_UT
#endif // __EEMCS_FS_UT__
