/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/firmware.h>
#include <linux/pm_qos_params.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/pm_runtime.h>
#include <mach/clk.h>
#include <mach/msm_memtypes.h>
#include <linux/interrupt.h>
#include <linux/memory_alloc.h>
#include <asm/sizes.h>
#include "vidc.h"
#include "vcd_res_tracker.h"
#include "vidc_init.h"

static unsigned int vidc_clk_table[3] = {
	48000000, 133330000, 200000000
};
static struct res_trk_context resource_context;

#define VIDC_FW	"vidc_1080p.fw"
#define VIDC_FW_SIZE SZ_1M

unsigned char *vidc_video_codec_fw;
u32 vidc_video_codec_fw_size;
static u32 res_trk_get_clk(void);
static void res_trk_put_clk(void);

static u32 res_trk_get_clk()
{
	if (resource_context.vcodec_clk ||
		resource_context.vcodec_pclk) {
		VCDRES_MSG_ERROR("%s() Clock reference exists\n",
						__func__);
		goto bail_out;
	}
	resource_context.vcodec_clk = clk_get(resource_context.device,
		"vcodec_clk");
	if (IS_ERR(resource_context.vcodec_clk)) {
		VCDRES_MSG_ERROR("%s(): vcodec_clk get failed\n",
						__func__);
		goto bail_out;
	}
	 resource_context.vcodec_pclk = clk_get(resource_context.device,
			"vcodec_pclk");
	if (IS_ERR(resource_context.vcodec_pclk)) {
		VCDRES_MSG_ERROR("%s(): vcodec_pclk get failed\n",
						__func__);
		goto release_vcodec_clk;
	}
	if (clk_set_rate(resource_context.vcodec_clk,
		vidc_clk_table[0])) {
		VCDRES_MSG_ERROR("%s(): set rate failed in power up\n",
						__func__);
		goto release_vcodec_pclk;
	}
	return true;
release_vcodec_pclk:
	clk_put(resource_context.vcodec_pclk);
	resource_context.vcodec_pclk = NULL;
release_vcodec_clk:
	clk_put(resource_context.vcodec_clk);
	resource_context.vcodec_clk = NULL;
bail_out:
	return false;
}

static void res_trk_put_clk()
{
	if (resource_context.vcodec_clk)
		clk_put(resource_context.vcodec_clk);
	if (resource_context.vcodec_pclk)
		clk_put(resource_context.vcodec_pclk);
	resource_context.vcodec_clk = NULL;
	resource_context.vcodec_pclk = NULL;
}

static u32 res_trk_shutdown_vidc(void)
{
	mutex_lock(&resource_context.lock);
	if (resource_context.clock_enabled) {
		mutex_unlock(&resource_context.lock);
		VCDRES_MSG_LOW("\n Calling CLK disable in Power Down\n");
		res_trk_disable_clocks();
		mutex_lock(&resource_context.lock);
	}
	res_trk_put_clk();
	if (resource_context.footswitch) {
		if (regulator_disable(resource_context.footswitch))
			VCDRES_MSG_ERROR("Regulator disable failed\n");
		regulator_put(resource_context.footswitch);
		resource_context.footswitch = NULL;
	}
	if (pm_runtime_put(resource_context.device) < 0)
		VCDRES_MSG_ERROR("Error : pm_runtime_put failed");
	mutex_unlock(&resource_context.lock);
	return true;
}

u32 res_trk_enable_clocks(void)
{
	VCDRES_MSG_LOW("\n in res_trk_enable_clocks()");
	mutex_lock(&resource_context.lock);
	if (!resource_context.clock_enabled) {
		VCDRES_MSG_LOW("Enabling IRQ in %s()\n", __func__);
		enable_irq(resource_context.irq_num);
		VCDRES_MSG_LOW("%s(): Enabling the clocks\n", __func__);
		if (resource_context.vcodec_clk &&
			resource_context.vcodec_pclk) {
			if (clk_enable(resource_context.vcodec_pclk)) {
				VCDRES_MSG_ERROR("vidc pclk Enable fail\n");
				goto bail_out;
			}
			if (clk_enable(resource_context.vcodec_clk)) {
				VCDRES_MSG_ERROR("vidc core clk Enable fail\n");
				goto vidc_disable_pclk;
			}

			VCDRES_MSG_LOW("%s(): Clocks enabled!\n", __func__);
		} else {
		   VCDRES_MSG_ERROR("%s(): Clocks enable failed!\n",
			__func__);
		   goto bail_out;
		}
	}
	resource_context.clock_enabled = 1;
	mutex_unlock(&resource_context.lock);
	return true;
vidc_disable_pclk:
	clk_disable(resource_context.vcodec_pclk);
bail_out:
	mutex_unlock(&resource_context.lock);
	return false;
}

static u32 res_trk_sel_clk_rate(unsigned long hclk_rate)
{
	u32 status = true;
	mutex_lock(&resource_context.lock);
	if (clk_set_rate(resource_context.vcodec_clk,
		hclk_rate)) {
		VCDRES_MSG_ERROR("vidc hclk set rate failed\n");
		status = false;
	} else
		resource_context.vcodec_clk_rate = hclk_rate;
	mutex_unlock(&resource_context.lock);
	return status;
}

static u32 res_trk_get_clk_rate(unsigned long *phclk_rate)
{
	u32 status = true;
	mutex_lock(&resource_context.lock);
	if (phclk_rate) {
		*phclk_rate = clk_get_rate(resource_context.vcodec_clk);
		if (!(*phclk_rate)) {
			VCDRES_MSG_ERROR("vidc hclk get rate failed\n");
			status = false;
		}
	} else
		status = false;
	mutex_unlock(&resource_context.lock);
	return status;
}

u32 res_trk_disable_clocks(void)
{
	u32 status = false;
	VCDRES_MSG_LOW("in res_trk_disable_clocks()\n");
	mutex_lock(&resource_context.lock);
	if (resource_context.clock_enabled) {
		VCDRES_MSG_LOW("Disabling IRQ in %s()\n", __func__);
		disable_irq_nosync(resource_context.irq_num);
		VCDRES_MSG_LOW("%s(): Disabling the clocks ...\n", __func__);
		resource_context.clock_enabled = 0;
		if (resource_context.vcodec_clk)
			clk_disable(resource_context.vcodec_clk);
		if (resource_context.vcodec_pclk)
			clk_disable(resource_context.vcodec_pclk);
		status = true;
	}
	mutex_unlock(&resource_context.lock);
	return status;
}

static u32 res_trk_vidc_pwr_up(void)
{
	mutex_lock(&resource_context.lock);

	if (pm_runtime_get(resource_context.device) < 0) {
		VCDRES_MSG_ERROR("Error : pm_runtime_get failed\n");
		goto bail_out;
	}
	resource_context.footswitch = regulator_get(NULL, "fs_ved");
	if (IS_ERR(resource_context.footswitch)) {
		VCDRES_MSG_ERROR("foot switch get failed\n");
		resource_context.footswitch = NULL;
	} else
		regulator_enable(resource_context.footswitch);
	if (!res_trk_get_clk())
		goto rel_vidc_pm_runtime;
	mutex_unlock(&resource_context.lock);
	return true;

rel_vidc_pm_runtime:
	if (pm_runtime_put(resource_context.device) < 0)
		VCDRES_MSG_ERROR("Error : pm_runtime_put failed");
bail_out:
	mutex_unlock(&resource_context.lock);
	return false;
}

u32 res_trk_power_up(void)
{
	VCDRES_MSG_LOW("clk_regime_rail_enable");
	VCDRES_MSG_LOW("clk_regime_sel_rail_control");
#ifdef CONFIG_MSM_BUS_SCALING
	resource_context.pcl = 0;
	if (resource_context.vidc_bus_client_pdata) {
		resource_context.pcl = msm_bus_scale_register_client(
			resource_context.vidc_bus_client_pdata);
		VCDRES_MSG_LOW("%s(), resource_context.pcl = %x", __func__,
			 resource_context.pcl);
	}
	if (resource_context.pcl == 0) {
		dev_err(resource_context.device,
			"register bus client returned NULL\n");
		return false;
	}
#endif
	return res_trk_vidc_pwr_up();
}

u32 res_trk_power_down(void)
{
	VCDRES_MSG_LOW("clk_regime_rail_disable");
#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_scale_client_update_request(resource_context.pcl, 0);
	msm_bus_scale_unregister_client(resource_context.pcl);
#endif
	VCDRES_MSG_MED("res_trk_power_down():: Calling "
		"res_trk_shutdown_vidc()\n");
	return res_trk_shutdown_vidc();
}

u32 res_trk_get_max_perf_level(u32 *pn_max_perf_lvl)
{
	if (!pn_max_perf_lvl) {
		VCDRES_MSG_ERROR("%s(): pn_max_perf_lvl is NULL\n",
			__func__);
		return false;
	}
	*pn_max_perf_lvl = RESTRK_1080P_MAX_PERF_LEVEL;
	return true;
}

#ifdef CONFIG_MSM_BUS_SCALING
int res_trk_update_bus_perf_level(struct vcd_dev_ctxt *dev_ctxt, u32 perf_level)
{
	struct vcd_clnt_ctxt *cctxt_itr = NULL;
	u32 enc_perf_level = 0, dec_perf_level = 0;
	u32 bus_clk_index, client_type = 0;
	int rc = 0;

	cctxt_itr = dev_ctxt->cctxt_list_head;
	while (cctxt_itr) {
		if (cctxt_itr->decoding)
			dec_perf_level += cctxt_itr->reqd_perf_lvl;
		else
			enc_perf_level += cctxt_itr->reqd_perf_lvl;
		cctxt_itr = cctxt_itr->next;
	}
	if (!enc_perf_level)
		client_type = 1;
	if (perf_level <= RESTRK_1080P_VGA_PERF_LEVEL)
		bus_clk_index = 0;
	else if (perf_level <= RESTRK_1080P_720P_PERF_LEVEL)
		bus_clk_index = 1;
	else
		bus_clk_index = 2;

	if (dev_ctxt->reqd_perf_lvl + dev_ctxt->curr_perf_lvl == 0)
		bus_clk_index = 2;

	bus_clk_index = (bus_clk_index << 1) + (client_type + 1);
	VCDRES_MSG_LOW("%s(), bus_clk_index = %d", __func__, bus_clk_index);
	VCDRES_MSG_LOW("%s(),context.pcl = %x", __func__, resource_context.pcl);
	VCDRES_MSG_LOW("%s(), bus_perf_level = %x", __func__, perf_level);
	rc = msm_bus_scale_client_update_request(resource_context.pcl,
		bus_clk_index);
	return rc;
}
#endif

u32 res_trk_set_perf_level(u32 req_perf_lvl, u32 *pn_set_perf_lvl,
	struct vcd_dev_ctxt *dev_ctxt)
{
	u32 vidc_freq = 0;
	if (!pn_set_perf_lvl || !dev_ctxt) {
		VCDRES_MSG_ERROR("%s(): NULL pointer! dev_ctxt(%p)\n",
			__func__, dev_ctxt);
		return false;
	}
	VCDRES_MSG_LOW("%s(), req_perf_lvl = %d", __func__, req_perf_lvl);
#ifdef CONFIG_MSM_BUS_SCALING
	if (!res_trk_update_bus_perf_level(dev_ctxt, req_perf_lvl) < 0) {
		VCDRES_MSG_ERROR("%s(): update buf perf level failed\n",
			__func__);
		return false;
	}

#endif
	if (dev_ctxt->reqd_perf_lvl + dev_ctxt->curr_perf_lvl == 0)
		req_perf_lvl = RESTRK_1080P_MAX_PERF_LEVEL;

	if (req_perf_lvl <= RESTRK_1080P_VGA_PERF_LEVEL) {
		vidc_freq = vidc_clk_table[0];
		*pn_set_perf_lvl = RESTRK_1080P_VGA_PERF_LEVEL;
	} else if (req_perf_lvl <= RESTRK_1080P_720P_PERF_LEVEL) {
		vidc_freq = vidc_clk_table[1];
		*pn_set_perf_lvl = RESTRK_1080P_720P_PERF_LEVEL;
	} else {
		vidc_freq = vidc_clk_table[2];
		*pn_set_perf_lvl = RESTRK_1080P_MAX_PERF_LEVEL;
	}
	resource_context.perf_level = *pn_set_perf_lvl;
	VCDRES_MSG_MED("VIDC: vidc_freq = %u, req_perf_lvl = %u\n",
		vidc_freq, req_perf_lvl);
#ifdef USE_RES_TRACKER
    if (req_perf_lvl != RESTRK_1080P_MIN_PERF_LEVEL) {
		VCDRES_MSG_MED("%s(): Setting vidc freq to %u\n",
			__func__, vidc_freq);
		if (!res_trk_sel_clk_rate(vidc_freq)) {
			VCDRES_MSG_ERROR("%s(): res_trk_sel_clk_rate FAILED\n",
				__func__);
			*pn_set_perf_lvl = 0;
			return false;
		}
	}
#endif
	VCDRES_MSG_MED("%s() set perl level : %d", __func__, *pn_set_perf_lvl);
	return true;
}

u32 res_trk_get_curr_perf_level(u32 *pn_perf_lvl)
{
	unsigned long freq;

	if (!pn_perf_lvl) {
		VCDRES_MSG_ERROR("%s(): pn_perf_lvl is NULL\n",
			__func__);
		return false;
	}
	VCDRES_MSG_LOW("clk_regime_msm_get_clk_freq_hz");
	if (!res_trk_get_clk_rate(&freq)) {
		VCDRES_MSG_ERROR("%s(): res_trk_get_clk_rate FAILED\n",
			__func__);
		*pn_perf_lvl = 0;
		return false;
	}
	*pn_perf_lvl = resource_context.perf_level;
	VCDRES_MSG_MED("%s(): freq = %lu, *pn_perf_lvl = %u", __func__,
		freq, *pn_perf_lvl);
	return true;
}

u32 res_trk_download_firmware(void)
{
	const struct firmware *fw_video = NULL;
	int rc = 0;
	u32 status = true;

	VCDRES_MSG_HIGH("%s(): Request firmware download\n",
		__func__);
	mutex_lock(&resource_context.lock);
	rc = request_firmware(&fw_video, VIDC_FW,
		resource_context.device);
	if (rc) {
		VCDRES_MSG_ERROR("request_firmware for %s error %d\n",
				VIDC_FW, rc);
		status = false;
		goto bail_out;
	}
	vidc_video_codec_fw = (unsigned char *)fw_video->data;
	vidc_video_codec_fw_size = (u32) fw_video->size;
bail_out:
	mutex_unlock(&resource_context.lock);
	return status;
}

void res_trk_init(struct device *device, u32 irq)
{
	if (resource_context.device || resource_context.irq_num ||
		!device) {
		VCDRES_MSG_ERROR("%s() Resource Tracker Init error\n",
			__func__);
	} else {
		memset(&resource_context, 0, sizeof(resource_context));
		mutex_init(&resource_context.lock);
		resource_context.device = device;
		resource_context.irq_num = irq;
		resource_context.vidc_platform_data =
			(struct msm_vidc_platform_data *) device->platform_data;
		if (resource_context.vidc_platform_data) {
			resource_context.memtype =
			resource_context.vidc_platform_data->memtype;
#ifdef CONFIG_MSM_BUS_SCALING
			resource_context.vidc_bus_client_pdata =
			resource_context.vidc_platform_data->
				vidc_bus_client_pdata;
#endif
		} else {
			resource_context.memtype = -1;
		}
		resource_context.core_type = VCD_CORE_1080P;
		if (!ddl_pmem_alloc(&resource_context.firmware_addr,
			VIDC_FW_SIZE, DDL_KILO_BYTE(128))) {
			pr_err("%s() Firmware buffer allocation failed",
				   __func__);
			memset(&resource_context.firmware_addr, 0,
				   sizeof(resource_context.firmware_addr));
		}
	}
}

u32 res_trk_get_core_type(void){
	return resource_context.core_type;
}

u32 res_trk_get_firmware_addr(struct ddl_buf_addr *firm_addr)
{
	int status = -1;
	if (resource_context.firmware_addr.mapped_buffer) {
		memcpy(firm_addr, &resource_context.firmware_addr,
			   sizeof(struct ddl_buf_addr));
		status = 0;
	}
	return status;
}

u32 res_trk_get_mem_type(void){
	return resource_context.memtype;
}
