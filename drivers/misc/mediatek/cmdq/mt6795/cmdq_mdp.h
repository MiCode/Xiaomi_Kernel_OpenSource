#ifndef __CMDQ_MDP_H__
#define __CMDQ_MDP_H__

#include <linux/types.h>

#ifdef __cplusplus
extern "C" {
#endif

	int32_t cmdqMdpClockOn(uint64_t engineFlag);

	int32_t cmdqMdpDumpInfo(uint64_t engineFlag, int level);

	int32_t cmdqVEncDumpInfo(uint64_t engineFlag, int level);

	int32_t cmdqMdpResetEng(uint64_t engineFlag);

	int32_t cmdqMdpClockOff(uint64_t engineFlag);

	const uint32_t cmdq_mdp_rdma_get_reg_offset_src_addr(void);
	const uint32_t cmdq_mdp_wrot_get_reg_offset_dst_addr(void);
	const uint32_t cmdq_mdp_wdma_get_reg_offset_dst_addr(void);

	void testcase_clkmgr_mdp(void);
#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_MDP_H__ */
