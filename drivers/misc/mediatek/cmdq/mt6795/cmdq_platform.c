#include "cmdq_platform.h"
#include "cmdq_core.h"
#include "cmdq_reg.h"
#include "mach/mt_clkmgr.h"

#include <linux/vmalloc.h>
#include <mach/mt_boot.h>
#include <linux/seq_file.h>

#include "smi_debug.h"

#define MMSYS_CONFIG_BASE  cmdq_dev_get_module_base_VA_MMSYS_CONFIG()

typedef struct RegDef {
	int offset;
	const char *name;
} RegDef;

const bool cmdq_core_support_sync_non_suspendable(void)
{
	return false;
}

const bool cmdq_core_support_wait_and_receive_event_in_same_tick(void)
{
	const unsigned int code = mt_get_chip_hw_code();
	CHIP_SW_VER ver = mt_get_chip_sw_ver();
	bool support = false;

	if (0x6795 == code) {
		support = true;		
	}
	else if (CHIP_SW_VER_02 <= ver) {
		/* SW V2 */
		support = true;
	} else if (CHIP_SW_VER_01 <= ver) {
		support = false;
	}

	return support;
}

const uint32_t cmdq_core_get_subsys_LSB_in_argA(void)
{
	return 16;
}

void cmdq_core_fix_command_desc_scenario_for_user_space_request(cmdqCommandStruct *pCommand)
{
	pCommand->scenario = CMDQ_SCENARIO_USER_SPACE;
}

bool cmdq_core_is_request_from_user_space(const CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_USER_SPACE:
		return true;
	default:
		return false;
	}
	return false;
}

bool cmdq_core_is_disp_scenario(const CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_TRIGGER_LOOP:
	case CMDQ_SCENARIO_PRIMARY_TRIGGER_LOOP:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
		return true;
	default:
		return false;
	}
	/* freely dispatch */
	return false;
}

bool cmdq_core_should_enable_prefetch(CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_PRIMARY_TRIGGER_LOOP:
	case CMDQ_SCENARIO_DEBUG_PREFETCH:	/* HACK: force debug into 0/1 thread */
		/* any path that connects to Primary DISP HW */
		/* should enable prefetch. */
		/* MEMOUT scenarios does not. */
		/* Also, since thread 0/1 shares one prefetch buffer, */
		/* we allow only PRIMARY path to use prefetch. */
		return true;

	default:
		return false;
	}

	return false;
}

bool cmdq_core_should_profile(CMDQ_SCENARIO_ENUM scenario)
{
#ifdef CMDQ_GPR_SUPPORT
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
	case CMDQ_SCENARIO_DEBUG:
		return true;
	default:
		return false;
	}
	return false;
#else
	/* note command profile method depends on GPR */
	CMDQ_ERR("func:%s failed since CMDQ dosen't support GPR\n", __func__);
	return false;
#endif
}

const bool cmdq_core_is_a_secure_thread(const int32_t thread)
{
	return false;
}

const bool cmdq_core_is_valid_notify_thread_for_secure_path(const int32_t thread)
{
	return false;
}

int cmdq_core_get_thread_index_from_scenario_and_secure_data(CMDQ_SCENARIO_ENUM scenario, const bool secure)
{
	if (!secure) {
		return cmdq_core_disp_thread_index_from_scenario(scenario);
	} else {
		CMDQ_ERR("no dedicated secure thread for senario:%d\n", scenario);
		return CMDQ_INVALID_THREAD;
	}
}

int cmdq_core_disp_thread_index_from_scenario(CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_DEBUG_PREFETCH:	/* HACK: force debug into 0/1 thread */
		/* primary config: thread 0 */
		return 0;

	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_SUB_MEMOUT:
		/* when HW thread 0 enables pre-fetch, any thread 1 operation will let HW thread 0's behavior abnormally */
		/* forbid thread 1 */
		return 5;
			
	case CMDQ_SCENARIO_PRIMARY_TRIGGER_LOOP:
		return 2;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
		return 6;

	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
		return 3;

	/*case CMDQ_SCENARIO_DISP_COLOR:*/
	/*case CMDQ_SCENARIO_USER_DISP_COLOR:*/
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
		return 4;
	default:
		/* freely dispatch */
		return CMDQ_INVALID_THREAD;
	}

	/* freely dispatch */
	return CMDQ_INVALID_THREAD;
}

CMDQ_HW_THREAD_PRIORITY_ENUM cmdq_core_priority_from_scenario(CMDQ_SCENARIO_ENUM scenario)
{
	switch (scenario) {
	case CMDQ_SCENARIO_PRIMARY_DISP:
	case CMDQ_SCENARIO_PRIMARY_ALL:
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
	case CMDQ_SCENARIO_SUB_DISP:
	case CMDQ_SCENARIO_SUB_ALL:
	case CMDQ_SCENARIO_SUB_MEMOUT:
	case CMDQ_SCENARIO_MHL_DISP:
	case CMDQ_SCENARIO_RDMA0_DISP:
	case CMDQ_SCENARIO_RDMA2_DISP:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL ... CMDQ_SCENARIO_DISP_CONFIG_OD:
		/* currently, a prefetch thread is always in high priority. */
		return CMDQ_THR_PRIO_DISPLAY_CONFIG;

	case CMDQ_SCENARIO_DEBUG_PREFETCH:	/* HACK: force debug into 0/1 thread */
		return CMDQ_THR_PRIO_DISPLAY_CONFIG;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
		return CMDQ_THR_PRIO_DISPLAY_ESD;

	default:
		/* other cases need exta logic, see below. */
		break;
	}

	if (cmdq_platform_is_loop_scenario(scenario, true)) {
		return CMDQ_THR_PRIO_DISPLAY_TRIGGER;
	} else {
		return CMDQ_THR_PRIO_NORMAL;
	}
}

bool cmdq_platform_force_loop_irq_from_scenario(CMDQ_SCENARIO_ENUM scenario)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (CMDQ_SCENARIO_SECURE_NOTIFY_LOOP == scenario) {
		/* For secure notify loop, we need IRQ to update secure task */
		return true;
	}
#endif
	return false;
}

bool cmdq_platform_is_loop_scenario(CMDQ_SCENARIO_ENUM scenario, bool displayOnly)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (!displayOnly && CMDQ_SCENARIO_SECURE_NOTIFY_LOOP == scenario) {
		return true;
	}
#endif
	if (CMDQ_SCENARIO_TRIGGER_LOOP == scenario || CMDQ_SCENARIO_PRIMARY_TRIGGER_LOOP == scenario) {
		return true;
	}
	return false;
}

void cmdq_core_get_reg_id_from_hwflag(uint64_t hwflag,
				      CMDQ_DATA_REGISTER_ENUM *valueRegId,
				      CMDQ_DATA_REGISTER_ENUM *destRegId,
				      CMDQ_EVENT_ENUM *regAccessToken)
{
	*regAccessToken = CMDQ_SYNC_TOKEN_INVALID;

	if (hwflag & (1LL << CMDQ_ENG_JPEG_ENC)) {
		*valueRegId = CMDQ_DATA_REG_JPEG;
		*destRegId = CMDQ_DATA_REG_JPEG_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_0;
	} else if (hwflag & (1LL << CMDQ_ENG_MDP_TDSHP0)) {
		*valueRegId = CMDQ_DATA_REG_2D_SHARPNESS_0;
		*destRegId = CMDQ_DATA_REG_2D_SHARPNESS_0_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_1;
	} else if (hwflag & (1LL << CMDQ_ENG_MDP_TDSHP1)) {
		*valueRegId = CMDQ_DATA_REG_2D_SHARPNESS_1;
		*destRegId = CMDQ_DATA_REG_2D_SHARPNESS_1_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_2;
	} else if (hwflag & ((1LL << CMDQ_ENG_DISP_COLOR0 | (1LL << CMDQ_ENG_DISP_COLOR1)))) {
		*valueRegId = CMDQ_DATA_REG_PQ_COLOR;
		*destRegId = CMDQ_DATA_REG_PQ_COLOR_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_3;
	} else {
		/* assume others are debug cases */
		*valueRegId = CMDQ_DATA_REG_DEBUG;
		*destRegId = CMDQ_DATA_REG_DEBUG_DST;
		*regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	}

	return;
}

const char *cmdq_core_module_from_event_id(CMDQ_EVENT_ENUM event, uint32_t instA, uint32_t instB)
{
	const char *module = "CMDQ";

	switch (event) {
	case CMDQ_EVENT_DISP_RDMA0_SOF:
	case CMDQ_EVENT_DISP_RDMA1_SOF:
	case CMDQ_EVENT_DISP_RDMA2_SOF:
	case CMDQ_EVENT_DISP_RDMA0_EOF:
	case CMDQ_EVENT_DISP_RDMA1_EOF:
	case CMDQ_EVENT_DISP_RDMA2_EOF:
	case CMDQ_EVENT_DISP_RDMA0_UNDERRUN:
	case CMDQ_EVENT_DISP_RDMA1_UNDERRUN:
	case CMDQ_EVENT_DISP_RDMA2_UNDERRUN:
		module = "DISP_RDMA";
		break;

	case CMDQ_EVENT_DISP_WDMA0_SOF:
	case CMDQ_EVENT_DISP_WDMA1_SOF:
	case CMDQ_EVENT_DISP_WDMA0_EOF:
	case CMDQ_EVENT_DISP_WDMA1_EOF:
		module = "DISP_WDMA";
		break;

	case CMDQ_EVENT_DISP_OVL0_SOF:
	case CMDQ_EVENT_DISP_OVL1_SOF:
	case CMDQ_EVENT_DISP_OVL0_EOF:
	case CMDQ_EVENT_DISP_OVL1_EOF:
		module = "DISP_OVL";
		break;

	case CMDQ_EVENT_MDP_DSI0_TE_SOF:
	case CMDQ_EVENT_MDP_DSI1_TE_SOF:
	case CMDQ_EVENT_DISP_COLOR0_SOF...CMDQ_EVENT_DISP_OD_SOF:
	case CMDQ_EVENT_DISP_COLOR0_EOF...CMDQ_EVENT_DISP_DPI0_EOF:
	case CMDQ_EVENT_MUTEX0_STREAM_EOF...CMDQ_EVENT_MUTEX4_STREAM_EOF:
	case CMDQ_SYNC_TOKEN_CONFIG_DIRTY:
	case CMDQ_SYNC_TOKEN_STREAM_EOF:
		module = "DISP";
		break;

	case CMDQ_EVENT_MDP_RDMA0_SOF:
	case CMDQ_EVENT_MDP_RDMA1_SOF:
	case CMDQ_EVENT_MDP_MVW_SOF...CMDQ_EVENT_MDP_CROP_SOF:
	case CMDQ_EVENT_MDP_RDMA0_EOF...CMDQ_EVENT_MDP_CROP_EOF:
	case CMDQ_EVENT_MUTEX5_STREAM_EOF...CMDQ_EVENT_MUTEX9_STREAM_EOF:
		module = "MDP";
		break;

	case CMDQ_EVENT_ISP_PASS2_2_EOF...CMDQ_EVENT_ISP_PASS1_0_EOF:
	case CMDQ_EVENT_ISP_CAMSV_2_PASS1_DONE...CMDQ_EVENT_ISP_SENINF_CAM0_FULL:
		module = "ISP";
		break;

	case CMDQ_EVENT_JPEG_ENC_PASS2_EOF...CMDQ_EVENT_JPEG_DEC_EOF:
		module = "JPGENC";
		break;

	default:
		module = "CMDQ";
		break;
	}

	return module;
}

const char *cmdq_core_parse_module_from_reg_addr(uint32_t reg_addr)
{
	const uint32_t addr_base_and_page = (reg_addr & 0xFFFFF000);
	const uint32_t addr_base_shifted = (reg_addr & 0xFFFF0000) >> 16;
	const char *module = "CMDQ";

	/* for well-known base, we check them with 12-bit mask */
	/* defined in mt_reg_base.h */
#define DECLARE_REG_RANGE(base, name) case base: return #name;
	switch (addr_base_and_page) {
		DECLARE_REG_RANGE(0x14000000, MMSYS);
		DECLARE_REG_RANGE(0x14001000, MDP_RDMA0);
		DECLARE_REG_RANGE(0x14002000, MDP_RDMA1);
		DECLARE_REG_RANGE(0x14003000, MDP_RSZ0);
		DECLARE_REG_RANGE(0x14004000, MDP_RSZ1);
		DECLARE_REG_RANGE(0x14005000, MDP_RSZ2);
		DECLARE_REG_RANGE(0x14006000, MDP_WDMA);
		DECLARE_REG_RANGE(0x14007000, MDP_WROT0);
		DECLARE_REG_RANGE(0x14008000, MDP_WROT1);
		DECLARE_REG_RANGE(0x14009000, MDP_TDSHP0);
		DECLARE_REG_RANGE(0x1400A000, MDP_TDSHP1);
		DECLARE_REG_RANGE(0x1400B000, MDP_CROP);
		DECLARE_REG_RANGE(0x1400C000, DISP_OVL0);
		DECLARE_REG_RANGE(0x1400D000, DISP_OVL1);
		DECLARE_REG_RANGE(0x14013000, COLOR0);
		DECLARE_REG_RANGE(0x14014000, COLOR1);
		DECLARE_REG_RANGE(0x14015000, AAL);
		DECLARE_REG_RANGE(0x14016000, GAMA);
		DECLARE_REG_RANGE(0x14020000, MMSYS_MUTEX);
		DECLARE_REG_RANGE(0x18000000, VENC_GCON);
		DECLARE_REG_RANGE(0x18002000, VENC);
		DECLARE_REG_RANGE(0x18003000, JPGENC);
		DECLARE_REG_RANGE(0x18004000, JPGDEC);
	}
#undef DECLARE_REG_RANGE

	/* for other register address we rely on GCE subsys to group them with */
	/* 16-bit mask. */
#undef DECLARE_CMDQ_SUBSYS
#define DECLARE_CMDQ_SUBSYS(msb, id, grp, base) case msb: return #grp;
	switch (addr_base_shifted) {
#include "cmdq_subsys.h"
	}
#undef DECLARE_CMDQ_SUBSYS
	return module;
}

const int32_t cmdq_core_can_module_entry_suspend(EngineStruct *engineList)
{
	int32_t status = 0;
	int i;
	CMDQ_ENG_ENUM e = 0;

	CMDQ_ENG_ENUM mdpEngines[] =
	    { CMDQ_ENG_ISP_IMGI, CMDQ_ENG_MDP_RDMA0, CMDQ_ENG_MDP_RDMA1, CMDQ_ENG_MDP_RSZ0,
		CMDQ_ENG_MDP_RSZ1, CMDQ_ENG_MDP_RSZ2, CMDQ_ENG_MDP_TDSHP0, CMDQ_ENG_MDP_TDSHP1,
		CMDQ_ENG_MDP_WROT0,
		CMDQ_ENG_MDP_WROT1, CMDQ_ENG_MDP_WDMA
	};

	for (i = 0; i < (sizeof(mdpEngines) / sizeof(CMDQ_ENG_ENUM)); ++i) {
		e = mdpEngines[i];
		if (0 != engineList[e].userCount) {
			CMDQ_ERR("suspend but engine %d has userCount %d, owner=%d\n",
				 e, engineList[e].userCount, engineList[e].currOwner);
			status = -EBUSY;
		}
	}

	return status;
}

ssize_t cmdq_core_print_status_clock(char *buf)
{
	int32_t length = 0;
	char *pBuffer = buf;

#ifdef CMDQ_PWR_AWARE
	pBuffer += sprintf(pBuffer, "MT_CG_INFRA_GCE: %d, MT_CG_DISP0_MUTEX_32K: %d\n",
				   clock_is_on(MT_CG_INFRA_GCE), clock_is_on(MT_CG_DISP0_MUTEX_32K));
#endif
	length = pBuffer - buf;
	return length;
}

void cmdq_core_print_status_seq_clock(struct seq_file *m)
{
#ifdef CMDQ_PWR_AWARE
	seq_printf(m, "MT_CG_INFRA_GCE: %d, MT_CG_DISP0_MUTEX_32K: %d\n",
				clock_is_on(MT_CG_INFRA_GCE), clock_is_on(MT_CG_DISP0_MUTEX_32K));
#endif
}

void cmdq_core_enable_common_clock_locked_impl(bool enable)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		CMDQ_VERBOSE("[CLOCK] Enable SMI & LARB0 Clock\n");
		enable_clock(MT_CG_DISP0_SMI_COMMON, "CMDQ_MDP");
		enable_clock(MT_CG_DISP0_SMI_LARB0, "CMDQ_MDP");

		CMDQ_VERBOSE("[CLOCK] enable MT_CG_DISP0_MUTEX_32K\n");
		enable_clock(MT_CG_DISP0_MUTEX_32K, "CMDQ_MDP");
	} else {
		CMDQ_VERBOSE("[CLOCK] Disable SMI & LARB0 Clock\n");
		/* disable, reverse the sequence */
		disable_clock(MT_CG_DISP0_SMI_LARB0, "CMDQ_MDP");
		disable_clock(MT_CG_DISP0_SMI_COMMON, "CMDQ_MDP");

		CMDQ_VERBOSE("[CLOCK] disable MT_CG_DISP0_MUTEX_32K\n");
		disable_clock(MT_CG_DISP0_MUTEX_32K, "CMDQ_MDP");
	}
#endif /* CMDQ_PWR_AWARE */
}

void cmdq_core_enable_gce_clock_locked_impl(bool enable)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		CMDQ_VERBOSE("[CLOCK] Enable CMDQ(GCE) Clock\n");
		cmdq_core_enable_cmdq_clock_locked_impl(enable, CMDQ_DRIVER_DEVICE_NAME);
	} else {
		CMDQ_VERBOSE("[CLOCK] Disable CMDQ(GCE) Clock\n");
		cmdq_core_enable_cmdq_clock_locked_impl(enable, CMDQ_DRIVER_DEVICE_NAME);
	}
#endif /* CMDQ_PWR_AWARE */
}

void cmdq_core_enable_cmdq_clock_locked_impl(bool enable, char *deviceName)
{
#ifdef CMDQ_PWR_AWARE
	if (enable) {
		enable_clock(MT_CG_INFRA_GCE, deviceName);

	} else {
		disable_clock(MT_CG_INFRA_GCE, deviceName);
	}
#endif /* CMDQ_PWR_AWARE */
}

const char* cmdq_core_parse_error_module_by_hwflag_impl(struct TaskStruct *pTask)
{
	const char *module = NULL;
	const uint32_t ISP_ONLY[2] = {
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O)),
		((1LL << CMDQ_ENG_ISP_IMGI) | (1LL << CMDQ_ENG_ISP_IMG2O) | (1LL << CMDQ_ENG_ISP_IMGO))};

	/* common part for both normal and secure path */
	/* for JPEG scenario, use HW flag is sufficient */
	if (pTask->engineFlag & (1LL << CMDQ_ENG_JPEG_ENC)) {
		module = "JPGENC";
	} else if (pTask->engineFlag & (1LL << CMDQ_ENG_JPEG_DEC)) {
		module = "JPGDEC";
	} else if ((ISP_ONLY[0] == pTask->engineFlag) || (ISP_ONLY[1] == pTask->engineFlag)) {
		module = "ISP_ONLY";
	} else if (cmdq_core_is_disp_scenario(pTask->scenario)) {
		module = "DISP";
	}

	/* for secure path, use HW flag is sufficient */
	do {
		if (NULL != module) {
			break;
		}

		if (false == pTask->secData.isSecure) {
			/* normal path, need parse current running instruciton for more detail */
			break;
		}

		else if (CMDQ_ENG_MDP_GROUP_FLAG(pTask->engineFlag)) {
			module = "MDP";
			break;
		}

		module = "CMDQ";
	} while(0);

	/* other case, we need to analysis instruction for more detail */
	return module;
}

void cmdq_core_dump_mmsys_config(void)
{
	int i = 0;
	uint32_t value = 0;

	const static struct RegDef configRegisters[] = {
		{0x01c, "ISP_MOUT_EN"},
		{0x020, "MDP_RDMA_MOUT_EN"},
		{0x024, "MDP_PRZ0_MOUT_EN"},
		{0x028, "MDP_PRZ1_MOUT_EN"},
		{0x02C, "MDP_PRZ2_MOUT_EN"},
		{0x030, "MDP_TDSHP0_MOUT_EN"},
		{0x034, "MDP_TDSHP1_MOUT_EN"},
		{0x038, "MDP0_MOUT_EN"},
		{0x03C, "MDP1_MOUT_EN"},
		{0x040, "DISP_OVL0_MOUT_EN"},
		{0x044, "DISP_OVL1_MOUT_EN"},
		{0x048, "DISP_OD_MOUT_EN"},
		{0x04C, "DISP_GAMMA_MOUT_EN"},
		{0x050, "DISP_UFOE_MOUT_EN"},
		/* {0x054, "MMSYS_MOUT_RST"}, */
		{0x058, "MDP_PRZ0_SEL_IN"},
		{0x05C, "MDP_PRZ1_SEL_IN"},
		{0x060, "MDP_PRZ2_SEL_IN"},
		{0x064, "MDP_TDSHP0_SEL_IN"},
		{0x068, "MDP_TDSHP1_SEL_IN"},
		{0x06C, "MDP0_SEL_IN"},
		{0x070, "MDP1_SEL_IN"},
		{0x074, "MDP_CROP_SEL_IN"},
		{0x078, "MDP_WDMA_SEL_IN"},
		{0x07C, "MDP_WROT0_SEL_IN"},
		{0x080, "MDP_WROT1_SEL_IN"},
		{0x084, "DISP_COLOR0_SEL_IN"},
		{0x088, "DISP_COLOR1_SEL_IN"},
		{0x08C, "DISP_AAL_SEL_IN"},
		{0x090, "DISP_PATH0_SEL_IN"},
		{0x094, "DISP_PATH1_SEL_IN"},
		{0x098, "DISP_WDMA0_SEL_IN"},
		{0x09C, "DISP_WDMA1_SEL_IN"},
		{0x0A0, "DISP_UFOE_SEL_IN"},
		{0x0A4, "DSI0_SEL_IN"},
		{0x0A8, "DSI1_SEL_IN"},
		{0x0AC, "DPI_SEL_IN"},
		{0x0B0, "DISP_RDMA0_SOUT_SEL_IN"},
		{0x0B4, "DISP_RDMA1_SOUT_SEL_IN"},
		{0x0B8, "DISP_RDMA2_SOUT_SEL_IN"},
		{0x0BC, "DISP_COLOR0_SOUT_SEL_IN"},
		{0x0C0, "DISP_COLOR1_SOUT_SEL_IN"},
		{0x0C4, "DISP_PATH0_SOUT_SEL_IN"},
		{0x0C8, "DISP_PATH1_SOUT_SEL_IN"},
		{0x0F0, "MMSYS_MISC"},
		/* ACK and REQ related */
		{0x8b0, "DISP_DL_VALID_0"},
		{0x8b4, "DISP_DL_VALID_1"},
		{0x8b8, "DISP_DL_READY_0"},
		{0x8bc, "DISP_DL_READY_1"},
		{0x8c0, "MDP_DL_VALID_0"},
		{0x8c4, "MDP_DL_VALID_1"},
		{0x8c8, "MDP_DL_READY_0"},
		{0x8cc, "MDP_DL_READY_1"}
	};

	for (i = 0; i < sizeof(configRegisters) / sizeof(configRegisters[0]); ++i) {
		value = CMDQ_REG_GET16(MMSYS_CONFIG_BASE + configRegisters[i].offset);
		CMDQ_ERR("%s: 0x%08x\n", configRegisters[i].name, value);
	}

	return;
}

void cmdq_core_dump_clock_gating(void)
{
	uint32_t value[3] = { 0 };

	value[0] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE + 0x100);
	value[1] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE + 0x110);
	value[2] = CMDQ_REG_GET32(MMSYS_CONFIG_BASE + 0x890);
	CMDQ_ERR("MMSYS_CG_CON0(deprecated): 0x%08x, MMSYS_CG_CON1: 0x%08x\n", value[0], value[1]);
	CMDQ_ERR("MMSYS_DUMMY_REG: 0x%08x\n", value[2]);
#ifndef CONFIG_MTK_FPGA
	CMDQ_ERR("ISPSys clock state %d\n", subsys_is_on(SYS_ISP));
	CMDQ_ERR("DisSys clock state %d\n", subsys_is_on(SYS_DIS));
	CMDQ_ERR("VDESys clock state %d\n", subsys_is_on(SYS_VDE));
#endif
}

int cmdq_core_dump_smi(const int showSmiDump)
{
	int isSMIHang = 0;

#ifndef CONFIG_MTK_FPGA
	isSMIHang = smi_debug_bus_hanging_detect(
						SMI_DBG_DISPSYS | SMI_DBG_VDEC | SMI_DBG_IMGSYS | SMI_DBG_VENC | SMI_DBG_MJC,
						showSmiDump);
	CMDQ_ERR("SMI Hang? = %d\n", isSMIHang);
#endif

	return isSMIHang;
}

void cmdq_core_dump_secure_metadata(cmdqSecDataStruct *pSecData)
{
	/* do nothing */
}
uint64_t cmdq_rec_flag_from_scenario(CMDQ_SCENARIO_ENUM scn)
{
	uint64_t flag = 0;

	switch (scn) {
	case CMDQ_SCENARIO_JPEG_DEC:
		flag = (1LL << CMDQ_ENG_JPEG_DEC);
		break;
	case CMDQ_SCENARIO_PRIMARY_DISP:
		flag = (1LL << CMDQ_ENG_DISP_OVL0) |
		    (1LL << CMDQ_ENG_DISP_COLOR0) |
		    (1LL << CMDQ_ENG_DISP_AAL) |
		    (1LL << CMDQ_ENG_DISP_RDMA0) |
		    (1LL << CMDQ_ENG_DISP_UFOE) | (1LL << CMDQ_ENG_DISP_DSI0_CMD);
		break;
	case CMDQ_SCENARIO_PRIMARY_MEMOUT:
		flag = ((1LL << CMDQ_ENG_DISP_OVL0) | (1LL << CMDQ_ENG_DISP_WDMA0));
		break;
	case CMDQ_SCENARIO_PRIMARY_ALL:
		flag = ((1LL << CMDQ_ENG_DISP_OVL0) |
			(1LL << CMDQ_ENG_DISP_WDMA0) |
			(1LL << CMDQ_ENG_DISP_COLOR0) |
			(1LL << CMDQ_ENG_DISP_AAL) |
			(1LL << CMDQ_ENG_DISP_RDMA0) |
			(1LL << CMDQ_ENG_DISP_UFOE) | (1LL << CMDQ_ENG_DISP_DSI0_CMD));
		break;
	case CMDQ_SCENARIO_SUB_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_COLOR1) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA1) | (1LL << CMDQ_ENG_DISP_DSI1_CMD));
		break;
	case CMDQ_SCENARIO_SUB_MEMOUT:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) | (1LL << CMDQ_ENG_DISP_WDMA1));
		break;
	case CMDQ_SCENARIO_SUB_ALL:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_WDMA1) |
			(1LL << CMDQ_ENG_DISP_COLOR1) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA1) | (1LL << CMDQ_ENG_DISP_DSI1_CMD));
		break;
	case CMDQ_SCENARIO_MHL_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_OVL1) |
			(1LL << CMDQ_ENG_DISP_COLOR1) |
			(1LL << CMDQ_ENG_DISP_GAMMA) |
			(1LL << CMDQ_ENG_DISP_RDMA1) | (1LL << CMDQ_ENG_DISP_DPI));
		break;
	case CMDQ_SCENARIO_RDMA0_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_RDMA0) |
			(1LL << CMDQ_ENG_DISP_UFOE) | (1LL << CMDQ_ENG_DISP_DSI0_CMD));
		break;
	case CMDQ_SCENARIO_RDMA2_DISP:
		flag = ((1LL << CMDQ_ENG_DISP_RDMA2) | (1LL << CMDQ_ENG_DISP_DPI));
		break;
	case CMDQ_SCENARIO_TRIGGER_LOOP:
	case CMDQ_SCENARIO_PRIMARY_TRIGGER_LOOP:
	case CMDQ_SCENARIO_DISP_CONFIG_AAL:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_PRIMARY_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_GAMMA:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_DITHER:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PWM:
	case CMDQ_SCENARIO_DISP_CONFIG_SUB_PQ:
	case CMDQ_SCENARIO_DISP_CONFIG_OD:
		/* Trigger loop does not related to any HW by itself. */
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_USER_SPACE:
		/* user space case, engine flag is passed seprately */
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_DEBUG:
	case CMDQ_SCENARIO_DEBUG_PREFETCH:
		flag = 0LL;
		break;

	case CMDQ_SCENARIO_DISP_ESD_CHECK:
	case CMDQ_SCENARIO_DISP_SCREEN_CAPTURE:
		/* ESD check uses separate thread (not config, not trigger) */
		flag = 0LL;
		break;

	default:
		CMDQ_ERR("Unknown scenario type %d\n", scn);
		flag = 0LL;
		break;
	}

	return flag;
}

void cmdq_core_gpr_dump(void)
{
	int i = 0;
	long offset = 0;
	uint32_t value = 0;

	CMDQ_LOG("========= GPR dump ========= \n");
	for (i = 0; i < 16; i++) {
		offset = CMDQ_GPR_R32(i);
		value = CMDQ_REG_GET32(offset);
		CMDQ_LOG("[GPR %2d]+0x%lx = 0x%08x\n", i, offset, value);
	}
	CMDQ_LOG("========= GPR dump ========= \n");

	return;

}

int32_t cmdq_platform_get_thread_index_trigger_loop(void)
{
	return cmdq_core_disp_thread_index_from_scenario(CMDQ_SCENARIO_PRIMARY_TRIGGER_LOOP);
}

void cmdq_test_setup(void)
{
	/* unconditionally set CMDQ_SYNC_TOKEN_CONFIG_ALLOW and mutex STREAM_DONE */
	/* so that DISPSYS scenarios may pass check. */
	cmdqCoreSetEvent(CMDQ_SYNC_TOKEN_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX0_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX1_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX2_STREAM_EOF);
	cmdqCoreSetEvent(CMDQ_EVENT_MUTEX3_STREAM_EOF);
}

void cmdq_test_cleanup(void)
{
	return; /* do nothing */
}
