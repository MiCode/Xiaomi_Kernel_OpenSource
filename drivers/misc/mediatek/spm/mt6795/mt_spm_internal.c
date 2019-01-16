#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/atomic.h>
#include <linux/string.h>

#include "mt_spm_internal.h"

/**************************************
 * Config and Parameter
 **************************************/
#define LOG_BUF_SIZE		256


/**************************************
 * Define and Declare
 **************************************/
DEFINE_SPINLOCK(__spm_lock);
atomic_t __spm_mainpll_req = ATOMIC_INIT(0);

/* define [x] = " XXX" */
#undef SPM_WAKE_SRC
#define SPM_WAKE_SRC(id, name)	\
	[id] = " " #name
static const char *wakesrc_str[32] = SPM_WAKE_SRC_LIST;


/**************************************
 * Function and API
 **************************************/
void __spm_reset_and_init_pcm(const struct pcm_desc *pcmdesc)
{
	u32 con1;

	/* reset PCM */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_PCM_SW_RESET);
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY);
	BUG_ON(spm_read(SPM_PCM_FSM_STA) != PCM_FSM_STA_DEF);	/* PCM reset failed */

	/* init PCM_CON0 (disable event vector) */
	spm_write(SPM_PCM_CON0, CON0_CFG_KEY | CON0_IM_SLEEP_DVS);

	/* init PCM_CON1 (disable PCM timer but keep PCM WDT setting) */
	con1 = spm_read(SPM_PCM_CON1) & (CON1_PCM_WDT_WAKE_MODE | CON1_PCM_WDT_EN);
	spm_write(SPM_PCM_CON1, con1 | CON1_CFG_KEY | CON1_EVENT_LOCK_EN |
				CON1_SPM_SRAM_ISO_B | CON1_SPM_SRAM_SLP_B |
				(pcmdesc->replace ? 0 : CON1_IM_NONRP_EN) |
				CON1_MIF_APBEN);
}

void __spm_kick_im_to_fetch(const struct pcm_desc *pcmdesc)
{
	u32 ptr, len, con0;

	/* tell IM where is PCM code (use slave mode if code existed) */
	ptr = base_va_to_pa(pcmdesc->base);
	len = pcmdesc->size - 1;
	if (spm_read(SPM_PCM_IM_PTR) != ptr || spm_read(SPM_PCM_IM_LEN) != len ||
	    pcmdesc->sess > 2) {
		spm_write(SPM_PCM_IM_PTR, ptr);
		spm_write(SPM_PCM_IM_LEN, len);
	} else {
		spm_write(SPM_PCM_CON1, spm_read(SPM_PCM_CON1) | CON1_CFG_KEY | CON1_IM_SLAVE);
	}

	/* kick IM to fetch (only toggle IM_KICK) */
	con0 = spm_read(SPM_PCM_CON0) & ~(CON0_IM_KICK | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY | CON0_IM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY);
}

void __spm_init_pcm_register(void)
{
	/* init r0 with POWER_ON_VAL0 */
	spm_write(SPM_PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL0));
	spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R0);
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* init r7 with POWER_ON_VAL1 */
	spm_write(SPM_PCM_REG_DATA_INI, spm_read(SPM_POWER_ON_VAL1));
	spm_write(SPM_PCM_PWR_IO_EN, PCM_RF_SYNC_R7);
	spm_write(SPM_PCM_PWR_IO_EN, 0);
}

void __spm_init_event_vector(const struct pcm_desc *pcmdesc)
{
	/* init event vector register */
	spm_write(SPM_PCM_EVENT_VECTOR0, pcmdesc->vec0);
	spm_write(SPM_PCM_EVENT_VECTOR1, pcmdesc->vec1);
	spm_write(SPM_PCM_EVENT_VECTOR2, pcmdesc->vec2);
	spm_write(SPM_PCM_EVENT_VECTOR3, pcmdesc->vec3);
	spm_write(SPM_PCM_EVENT_VECTOR4, pcmdesc->vec4);
	spm_write(SPM_PCM_EVENT_VECTOR5, pcmdesc->vec5);
	spm_write(SPM_PCM_EVENT_VECTOR6, pcmdesc->vec6);
	spm_write(SPM_PCM_EVENT_VECTOR7, pcmdesc->vec7);

	/* event vector will be enabled by PCM itself */
}

void __spm_set_power_control(const struct pwr_ctrl *pwrctrl)
{
	/* set other SYS request mask */
	spm_write(SPM_AP_STANBY_CON, (!!pwrctrl->md_apsrc_sel << 22) |
				     (!pwrctrl->md32_req_mask << 21) |
				     (!pwrctrl->md2_req_mask << 20) |
				     (!pwrctrl->md1_req_mask << 19) |
				     (!!pwrctrl->md_ddr_dbc_en << 18) |
				     (!pwrctrl->mfg_req_mask << 17) |
				     (!pwrctrl->disp_req_mask << 16) |
				     (!!pwrctrl->mcusys_idle_mask << 7) |
				     (!!pwrctrl->ca15top_idle_mask << 6) |
				     (!!pwrctrl->ca7top_idle_mask << 5) |
				     (!!pwrctrl->wfi_op << 4));
	spm_write(SPM_PCM_SRC_REQ, (!pwrctrl->ccif_to_ap_mask << 3) |
				   (!pwrctrl->ccif_to_md_mask << 2) |
				   (!!pwrctrl->pcm_f26m_req << 1) |
				   (!!pwrctrl->pcm_apsrc_req << 0));
	spm_write(SPM_PCM_PASR_DPD_2, (!pwrctrl->isp1_ddr_en_mask << 4) |
				      (!pwrctrl->isp0_ddr_en_mask << 3) |
				      (!pwrctrl->dpi_ddr_en_mask << 2) |
				      (!pwrctrl->dsi1_ddr_en_mask << 1) |
				      (!pwrctrl->dsi0_ddr_en_mask << 0));
	spm_write(SPM_CLK_CON, (spm_read(SPM_CLK_CON) & ~CC_SRCLKENA_MASK_0) |
			       (pwrctrl->srclkenai_mask ? CC_SRCLKENA_MASK_0 : 0));

	/* set CPU WFI mask */
	spm_write(SPM_SLEEP_CA15_WFI0_EN, !!pwrctrl->ca15_wfi0_en);
	spm_write(SPM_SLEEP_CA15_WFI1_EN, !!pwrctrl->ca15_wfi1_en);
	spm_write(SPM_SLEEP_CA15_WFI2_EN, !!pwrctrl->ca15_wfi2_en);
	spm_write(SPM_SLEEP_CA15_WFI3_EN, !!pwrctrl->ca15_wfi3_en);
	spm_write(SPM_SLEEP_CA7_WFI0_EN, !!pwrctrl->ca7_wfi0_en);
	spm_write(SPM_SLEEP_CA7_WFI1_EN, !!pwrctrl->ca7_wfi1_en);
	spm_write(SPM_SLEEP_CA7_WFI2_EN, !!pwrctrl->ca7_wfi2_en);
	spm_write(SPM_SLEEP_CA7_WFI3_EN, !!pwrctrl->ca7_wfi3_en);
}

void __spm_set_wakeup_event(const struct pwr_ctrl *pwrctrl)
{
	u32 val, mask, isr;

	/* set PCM timer (set to max when disable) */
	if (pwrctrl->timer_val_cust == 0)
		val = pwrctrl->timer_val ? : PCM_TIMER_MAX;
	else
		val = pwrctrl->timer_val_cust;

	spm_write(SPM_PCM_TIMER_VAL, val);
	spm_write(SPM_PCM_CON1, spm_read(SPM_PCM_CON1) | CON1_CFG_KEY | CON1_PCM_TIMER_EN);

	/* unmask AP wakeup source */
	if (pwrctrl->wake_src_cust == 0)
		mask = pwrctrl->wake_src;
	else
		mask = pwrctrl->wake_src_cust;

	if (pwrctrl->syspwreq_mask)
		mask &= ~WAKE_SRC_SYSPWREQ;
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~mask);

	/* unmask MD32 wakeup source */
	spm_write(SPM_SLEEP_MD32_WAKEUP_EVENT_MASK, ~pwrctrl->wake_src_md32);

	/* unmask SPM ISR (keep TWAM setting) */
	isr = spm_read(SPM_SLEEP_ISR_MASK) & ISRM_TWAM;
	spm_write(SPM_SLEEP_ISR_MASK, isr | ISRM_RET_IRQ_AUX);
}

void __spm_kick_pcm_to_run(const struct pwr_ctrl *pwrctrl)
{
	u32 con0;

	/* init register to match PCM expectation */
	spm_write(SPM_PCM_MAS_PAUSE_MASK, 0xffffffff);
	spm_write(SPM_PCM_REG_DATA_INI, 0);
	spm_write(SPM_CLK_CON, spm_read(SPM_CLK_CON) & ~CC_DISABLE_DORM_PWR);

	/* set PCM flags and data */
	spm_write(SPM_PCM_FLAGS, pwrctrl->pcm_flags);
	spm_write(SPM_PCM_RESERVE, pwrctrl->pcm_reserve);

	/* lock Infra DCM when PCM runs */
	spm_write(SPM_CLK_CON, (spm_read(SPM_CLK_CON) & ~CC_LOCK_INFRA_DCM) |
			       (pwrctrl->infra_dcm_lock ? CC_LOCK_INFRA_DCM : 0));

	/* enable r0 and r7 to control power */
	spm_write(SPM_PCM_PWR_IO_EN, (pwrctrl->r0_ctrl_en ? PCM_PWRIO_EN_R0 : 0) |
				     (pwrctrl->r7_ctrl_en ? PCM_PWRIO_EN_R7 : 0));

	/* kick PCM to run (only toggle PCM_KICK) */
	con0 = spm_read(SPM_PCM_CON0) & ~(CON0_IM_KICK | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY | CON0_PCM_KICK);
	spm_write(SPM_PCM_CON0, con0 | CON0_CFG_KEY);
}

void __spm_get_wakeup_status(struct wake_status *wakesta)
{
	/* get PC value if PCM assert (pause abort) */
	wakesta->assert_pc = spm_read(SPM_PCM_REG_DATA_INI);

	/* get wakeup event */
	wakesta->r12 = spm_read(SPM_PCM_REG12_DATA);
	wakesta->raw_sta = spm_read(SPM_SLEEP_ISR_RAW_STA);
	wakesta->wake_misc = spm_read(SPM_SLEEP_WAKEUP_MISC);

	/* get sleep time */
	wakesta->timer_out = spm_read(SPM_PCM_TIMER_OUT);

	/* get other SYS and co-clock status */
	wakesta->r13 = spm_read(SPM_PCM_REG13_DATA);
	wakesta->idle_sta = spm_read(SPM_SLEEP_SUBSYS_IDLE_STA);

	/* get debug flag for PCM execution check */
	wakesta->debug_flag = spm_read(SPM_PCM_PASR_DPD_3);

	/* get special pattern (0xf0000 or 0x10000) if sleep abort */
	wakesta->event_reg = spm_read(SPM_PCM_EVENT_REG_STA);

	/* get ISR status */
	wakesta->isr = spm_read(SPM_SLEEP_ISR_STATUS);
}

void __spm_clean_after_wakeup(void)
{
	/* disable r0 and r7 to control power */
	spm_write(SPM_PCM_PWR_IO_EN, 0);

	/* clean CPU wakeup event */
	spm_write(SPM_SLEEP_CPU_WAKEUP_EVENT, 0);

	/* clean PCM timer event */
	spm_write(SPM_PCM_CON1, CON1_CFG_KEY | (spm_read(SPM_PCM_CON1) & ~CON1_PCM_TIMER_EN));

	/* clean wakeup event raw status (for edge trigger event) */
	spm_write(SPM_SLEEP_WAKEUP_EVENT_MASK, ~0);

	/* clean ISR status (except TWAM) */
	spm_write(SPM_SLEEP_ISR_MASK, spm_read(SPM_SLEEP_ISR_MASK) | ISRM_ALL_EXC_TWAM);
	spm_write(SPM_SLEEP_ISR_STATUS, ISRC_ALL_EXC_TWAM);
	spm_write(SPM_PCM_SW_INT_CLEAR, PCM_SW_INT_ALL);
}

#define spm_print(suspend, fmt, args...)	\
do {						\
	if (!suspend)				\
		spm_debug(fmt, ##args);		\
	else					\
		spm_crit2(fmt, ##args);		\
} while (0)

wake_reason_t __spm_output_wake_reason(const struct wake_status *wakesta,
				       const struct pcm_desc *pcmdesc,
				       bool suspend)
{
	int i;
	char buf[LOG_BUF_SIZE] = { 0 };
	wake_reason_t wr = WR_UNKNOWN;

	if (wakesta->assert_pc != 0) {
		spm_print(suspend, "PCM ASSERT AT %u (%s), r13 = 0x%x, debug_flag = 0x%x\n",
			  wakesta->assert_pc, pcmdesc->version, wakesta->r13, wakesta->debug_flag);
		return WR_PCM_ASSERT;
	}

	if (wakesta->r12 & WAKE_SRC_SPM_MERGE) {
		if (wakesta->wake_misc & WAKE_MISC_PCM_TIMER) {
			strcat(buf, " PCM_TIMER");
			wr = WR_PCM_TIMER;
		}
		if (wakesta->wake_misc & WAKE_MISC_TWAM) {
			strcat(buf, " TWAM");
			wr = WR_WAKE_SRC;
		}
		if (wakesta->wake_misc & WAKE_MISC_CPU_WAKE) {
			strcat(buf, " CPU");
			wr = WR_WAKE_SRC;
		}
	}
	for (i = 1; i < 32; i++) {
		if (wakesta->r12 & (1U << i)) {
			strcat(buf, wakesrc_str[i]);
			wr = WR_WAKE_SRC;
		}
	}
	BUG_ON(strlen(buf) >= LOG_BUF_SIZE);

	spm_print(suspend, "wake up by%s, timer_out = %u, r13 = 0x%x, debug_flag = 0x%x\n",
		  buf, wakesta->timer_out, wakesta->r13, wakesta->debug_flag);

	spm_print(suspend, "raw_sta = 0x%x, idle_sta = 0x%x, event_reg = 0x%x, isr = 0x%x\n",
		  wakesta->raw_sta, wakesta->idle_sta, wakesta->event_reg, wakesta->isr);

	return wr;
}

void __spm_dbgout_md_ddr_en(bool enable)
{
	/* set TEST_MODE_CFG */
	spm_write(0xf0000230, (spm_read(0xf0000230) & ~(0x7fff << 16)) |
			      (0x3 << 26) | (0x3 << 21) | (0x3 << 16));

	/* set md_ddr_en to GPIO150 */
	spm_write(0xf0001500, 0x70e);
	spm_write(0xf00057e4, 0x7);

	/* set emi_clk_off_req to GPIO140 */
	spm_write(0xf000150c, 0x3fe);
	spm_write(0xf00057c4, 0x7);

	/* enable debug output */
	spm_write(SPM_PCM_DEBUG_CON, !!enable);
}

MODULE_DESCRIPTION("SPM-Internal Driver v0.6");
