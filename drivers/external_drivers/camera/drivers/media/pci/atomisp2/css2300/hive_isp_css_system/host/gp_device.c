#include "gp_device.h"

#ifndef __INLINE_GP_DEVICE__
#include "gp_device_private.h"
#endif /* __INLINE_GP_DEVICE__ */

void gp_device_get_state(
	const gp_device_ID_t		ID,
	gp_device_state_t			*state)
{
assert(ID < N_GP_DEVICE_ID);
assert(state != NULL);
	if (state == NULL)
		return;

	state->switch_if = gp_device_reg_load(ID,
		_REG_GP_SWITCH_IF_ADDR);
	state->switch_dma = gp_device_reg_load(ID,
		_REG_GP_SWITCH_DMA_ADDR);
	state->switch_gdc = gp_device_reg_load(ID,
		_REG_GP_SWITCH_GDC_ADDR);
	state->syngen_enable = gp_device_reg_load(ID,
		_REG_GP_SYNGEN_ENABLE_ADDR);
	state->syngen_nr_pix = gp_device_reg_load(ID,
		_REG_GP_SYNGEN_NR_PIX_ADDR);
	state->syngen_nr_lines = gp_device_reg_load(ID,
		_REG_GP_SYNGEN_NR_LINES_ADDR);
	state->syngen_hblank_cycles = gp_device_reg_load(ID,
		_REG_GP_SYNGEN_HBLANK_CYCLES_ADDR);
	state->syngen_vblank_cycles = gp_device_reg_load(ID,
		_REG_GP_SYNGEN_VBLANK_CYCLES_ADDR);
	state->isel_sof = gp_device_reg_load(ID,
		_REG_GP_ISEL_SOF_ADDR);
	state->isel_eof = gp_device_reg_load(ID,
		_REG_GP_ISEL_EOF_ADDR);
	state->isel_sol = gp_device_reg_load(ID,
		_REG_GP_ISEL_SOL_ADDR);
	state->isel_eol = gp_device_reg_load(ID,
		_REG_GP_ISEL_EOL_ADDR);
	state->isel_lfsr_enable = gp_device_reg_load(ID,
		_REG_GP_ISEL_LFSR_ENABLE_ADDR);
	state->isel_lfsr_enable_b = gp_device_reg_load(ID,
		_REG_GP_ISEL_LFSR_ENABLE_B_ADDR);
	state->isel_lfsr_reset_value = gp_device_reg_load(ID,
		_REG_GP_ISEL_LFSR_RESET_VALUE_ADDR);
	state->isel_tpg_enable = gp_device_reg_load(ID,
		_REG_GP_ISEL_TPG_ENABLE_ADDR);
	state->isel_tpg_enable_b = gp_device_reg_load(ID,
		_REG_GP_ISEL_TPG_ENABLE_B_ADDR);
	state->isel_hor_cnt_mask = gp_device_reg_load(ID,
		_REG_GP_ISEL_HOR_CNT_MASK_ADDR);
	state->isel_ver_cnt_mask = gp_device_reg_load(ID,
		_REG_GP_ISEL_VER_CNT_MASK_ADDR);
	state->isel_xy_cnt_mask = gp_device_reg_load(ID,
		_REG_GP_ISEL_XY_CNT_MASK_ADDR);
	state->isel_hor_cnt_delta = gp_device_reg_load(ID,
		_REG_GP_ISEL_HOR_CNT_DELTA_ADDR);
	state->isel_ver_cnt_delta = gp_device_reg_load(ID,
		_REG_GP_ISEL_VER_CNT_DELTA_ADDR);
	state->isel_ch_id = gp_device_reg_load(ID,
		_REG_GP_ISEL_CH_ID_ADDR);
	state->isel_fmt_type = gp_device_reg_load(ID,
		_REG_GP_ISEL_FMT_TYPE_ADDR);
	state->isel_data_sel = gp_device_reg_load(ID,
		_REG_GP_ISEL_DATA_SEL_ADDR);
	state->isel_sband_sel = gp_device_reg_load(ID,
		_REG_GP_ISEL_SBAND_SEL_ADDR);
	state->isel_sync_sel = gp_device_reg_load(ID,
		_REG_GP_ISEL_SYNC_SEL_ADDR);
	state->inp_swi_lut_reg_0 = gp_device_reg_load(ID,
		_REG_GP_INP_SWI_LUT_REG_0_ADDR);
	state->inp_swi_lut_reg_1 = gp_device_reg_load(ID,
		_REG_GP_INP_SWI_LUT_REG_1_ADDR);
	state->inp_swi_lut_reg_2 = gp_device_reg_load(ID,
		_REG_GP_INP_SWI_LUT_REG_2_ADDR);
	state->inp_swi_lut_reg_3 = gp_device_reg_load(ID,
		_REG_GP_INP_SWI_LUT_REG_3_ADDR);
	state->inp_swi_lut_reg_4 = gp_device_reg_load(ID,
		_REG_GP_INP_SWI_LUT_REG_4_ADDR);
	state->inp_swi_lut_reg_5 = gp_device_reg_load(ID,
		_REG_GP_INP_SWI_LUT_REG_5_ADDR);
	state->inp_swi_lut_reg_6 = gp_device_reg_load(ID,
		_REG_GP_INP_SWI_LUT_REG_6_ADDR);
	state->inp_swi_lut_reg_7 = gp_device_reg_load(ID,
		_REG_GP_INP_SWI_LUT_REG_7_ADDR);
	state->inp_swi_fsync_lut_reg = gp_device_reg_load(ID,
		_REG_GP_INP_SWI_FSYNC_LUT_REG_ADDR);
	state->sdram_wakeup = gp_device_reg_load(ID,
		_REG_GP_SDRAM_WAKEUP_ADDR);
	state->idle = gp_device_reg_load(ID,
		_REG_GP_IDLE_ADDR);
	state->irq_request = gp_device_reg_load(ID,
		_REG_GP_IRQ_REQUEST_ADDR);
	state->mipi_dword_full = gp_device_reg_load(ID,
		_REG_GP_MIPI_DWORD_FULL_ADDR);
	state->mipi_used_dword = gp_device_reg_load(ID,
		_REG_GP_MIPI_USED_DWORD_ADDR);
return;
}

