#ifndef __EEMCS_RPC_UT__
#define __EEMCS_RPC_UT__

#include "eemcs_ccci.h"


#define md_gpd_idx_ut_val 1
#define num_para_from_md_ut_val 2
#define num_para_from_ap_ut_val 2



void eemcs_rpc_ut_trigger(void);
KAL_INT32 eemcs_rpc_ut_UL_write_skb_to_swq(CCCI_CHANNEL_T, struct sk_buff *);

#if 0 
KAL_INT32 eemcs_rpc_ut_init(void);
KAL_INT32 eemcs_rpc_ut_exit(void);
void eemcs_rpc_ut_dump(void);
KAL_INT32 eemcs_rpc_ut_set_index(KAL_UINT32 index);
KAL_UINT32 eemcs_rpc_ut_get_index(void);
#endif

#endif // __EEMCS_RPC_UT__
