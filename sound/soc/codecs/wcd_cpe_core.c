/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/firmware.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/elf.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <sound/soc.h>
#include <sound/lsm_params.h>
#include <sound/cpe_core.h>
#include <sound/cpe_cmi.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/core-resource.h>
#include <linux/mfd/wcd9xxx/wcd9330_registers.h>
#include "wcd_cpe_core.h"
#include "wcd_cpe_services.h"
#include "wcd_cmi_api.h"
#include "../msm/qdsp6v2/audio_acdb.h"

#define CMI_CMD_TIMEOUT (10 * HZ)
#define WCD_CPE_LSM_MAX_SESSIONS 1
#define WCD_CPE_AFE_MAX_PORTS 1

#define ELF_FLAG_EXECUTE (1 << 0)
#define ELF_FLAG_WRITE (1 << 1)
#define ELF_FLAG_READ (1 << 2)

#define ELF_FLAG_RW (ELF_FLAG_READ | ELF_FLAG_WRITE)

#define WCD_CPE_GRAB_LOCK(lock, name)		\
{						\
	pr_debug("%s: %s lock acquire\n",	\
		 __func__, name);		\
	mutex_lock(lock);			\
}

#define WCD_CPE_REL_LOCK(lock, name)		\
{						\
	pr_debug("%s: %s lock release\n",	\
		 __func__, name);		\
	mutex_unlock(lock);			\
}

enum afe_port_state {
	AFE_PORT_STATE_DEINIT = 0,
	AFE_PORT_STATE_INIT,
	AFE_PORT_STATE_CONFIG,
	AFE_PORT_STATE_STARTED,
	AFE_PORT_STATE_SUSPENDED,
};

struct wcd_cmi_afe_port_data {
	u8 port_id;
	struct mutex afe_lock;
	struct completion afe_cmd_complete;
	enum afe_port_state port_state;
	u8 cmd_result;
	u32 mem_handle;
};

static struct cpe_lsm_session
		*lsm_sessions[WCD_CPE_LSM_MAX_SESSIONS + 1];
struct wcd_cpe_core * (*wcd_get_cpe_core) (struct snd_soc_codec *);
static struct wcd_cmi_afe_port_data afe_ports[WCD_CPE_AFE_MAX_PORTS + 1];

/* wcd_cpe_is_valid_elf_hdr: check if the ELF header is valid
 * @core: handle to wcd_cpe_core
 * @fw_size: size of firmware from request_firmware
 * @ehdr: the elf header to be checked for
 * return true if all checks pass, true if any elf check fails
 */
static bool wcd_cpe_is_valid_elf_hdr(struct wcd_cpe_core *core, size_t fw_size,
				     const struct elf32_hdr *ehdr)
{
	if (fw_size < sizeof(*ehdr)) {
		dev_err(core->dev, "%s:Firmware too small\n", __func__);
		goto elf_check_fail;
	}

	if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
		dev_err(core->dev, "%s: Not an ELF file\n", __func__);
		goto elf_check_fail;
	}

	if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
		dev_err(core->dev, "%s: Not a executable image\n", __func__);
		goto elf_check_fail;
	}

	if (ehdr->e_phnum == 0) {
		dev_err(core->dev, "%s: no segments to load\n", __func__);
		goto elf_check_fail;
	}

	if (sizeof(struct elf32_phdr) * ehdr->e_phnum +
	    sizeof(struct elf32_hdr) > fw_size) {
		dev_err(core->dev, "%s: Too small MDT file\n", __func__);
		goto elf_check_fail;
	}

	return true;

elf_check_fail:
	return false;
}

/*
 * wcd_cpe_load_each_segment: download segment to CPE
 * @core: handle to struct wcd_cpe_core
 * @file_idx: index of split firmware image file name
 * @phdr: program header from metadata
 */
static int wcd_cpe_load_each_segment(struct wcd_cpe_core *core,
			  int file_idx, const struct elf32_phdr *phdr)
{
	const struct firmware *split_fw;
	char split_fname[32];
	int ret = 0;
	struct cpe_svc_mem_segment *segment;

	if (!core || !phdr) {
		pr_err("%s: Invalid params\n", __func__);
		return -EINVAL;
	}

	/* file size can be 0 for bss segments */
	if (phdr->p_filesz == 0 || phdr->p_memsz == 0)
		return 0;

	segment = kzalloc(sizeof(struct cpe_svc_mem_segment), GFP_KERNEL);
	if (!segment) {
		dev_err(core->dev,
			"%s: no memory for segment info, file_idx = %d\n"
			, __func__, file_idx);
		return -ENOMEM;
	}

	snprintf(split_fname, sizeof(split_fname), "%s.b%02d",
		 core->fname, file_idx);

	ret = request_firmware(&split_fw, split_fname, core->dev);
	if (ret) {
		dev_err(core->dev, "firmware %s not found\n",
			split_fname);
		ret = -EIO;
		goto fw_req_fail;
	}

	if (phdr->p_flags & ELF_FLAG_EXECUTE)
		segment->type = CPE_SVC_INSTRUCTION_MEM;
	else if (phdr->p_flags & ELF_FLAG_RW)
		segment->type = CPE_SVC_DATA_MEM;
	else {
		dev_err(core->dev, "%s invalid flags 0x%x\n",
			__func__, phdr->p_flags);
		goto done;
	}

	segment->cpe_addr = phdr->p_paddr;
	segment->size = phdr->p_filesz;
	segment->data = (u8 *) split_fw->data;

	dev_dbg(core->dev,
		"%s: cpe segment type %s read from firmware\n", __func__,
		(segment->type == CPE_SVC_INSTRUCTION_MEM) ?
			"INSTRUCTION" : "DATA");

	ret = cpe_svc_download_segment(core->cpe_handle, segment);
	if (ret) {
		dev_err(core->dev,
			"%s: Failed to download %s, error = %d\n",
			__func__, split_fname, ret);
		goto done;
	}

done:
	release_firmware(split_fw);

fw_req_fail:
	kfree(segment);
	return ret;
}

/*
 * wcd_cpe_enable_cpe_clks: enable the clocks for CPE
 * @core: handle to wcd_cpe_core
 * @enable: flag indicating whether to enable/disable cpe clocks
 */
static int wcd_cpe_enable_cpe_clks(struct wcd_cpe_core *core, bool enable)
{
	int ret = 0;

	if (!core || !core->cpe_cdc_cb.cdc_clk_en ||
	    !core->cpe_cdc_cb.cpe_clk_en) {
		dev_err(core->dev,
			"%s: invalid handle\n",
			__func__);
		return -EINVAL;
	}

	ret = core->cpe_cdc_cb.cdc_clk_en(core->codec, enable);
	if (ret) {
		dev_err(core->dev, "%s: Failed to enable RCO\n",
			__func__);
		return ret;
	}

	ret = core->cpe_cdc_cb.cpe_clk_en(core->codec, enable);
	if (ret) {
		dev_err(core->dev,
			"%s: cpe_clk_en() failed, err = %d\n",
			__func__, ret);
		return ret;
	}

	return 0;

}

/*
 * wcd_cpe_load_fw_image: work function to load the fw image
 * @work: work that is scheduled to perform the image loading
 *
 * Parse the mdt file to look for program headers, load each
 * split file corresponding to the program headers.
 */
static void wcd_cpe_load_fw_image(struct work_struct *work)
{
	struct wcd_cpe_core *core;
	struct snd_soc_codec *codec;
	struct wcd9xxx *wcd9xxx;
	int ret, phdr_idx;
	const struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	const struct firmware *fw;
	const u8 *elf_ptr;
	char mdt_name[64];
	bool img_dload_fail = false;

	core = container_of(work, struct wcd_cpe_core, load_fw_work);
	codec = core->codec;
	wcd9xxx = codec->control_data;


	if (!core || !core->cpe_handle) {
		pr_err("%s: Invalid handle\n", __func__);
		return;
	}

	snprintf(mdt_name, sizeof(mdt_name), "%s.mdt", core->fname);
	ret = request_firmware(&fw, mdt_name, core->dev);
	if (IS_ERR_VALUE(ret)) {
		dev_err(core->dev, "firmware %s not found\n", mdt_name);
		return;
	}

	ehdr = (struct elf32_hdr *) fw->data;
	if (!wcd_cpe_is_valid_elf_hdr(core, fw->size, ehdr)) {
		dev_err(core->dev, "%s: fw mdt %s is invalid\n",
			__func__, mdt_name);
		goto done;
	}

	elf_ptr = fw->data + sizeof(*ehdr);

	/* Reset CPE first */
	ret = cpe_svc_reset(core->cpe_handle);
	if (IS_ERR_VALUE(ret)) {
		dev_err(core->dev,
			"%s: Failed to reset CPE with error %d\n",
			__func__, ret);
		goto done;
	}

	dev_dbg(core->dev, "%s: starting image download, image = %s\n",
		__func__, core->fname);

	/* parse every program header and request corresponding firmware */
	for (phdr_idx = 0; phdr_idx < ehdr->e_phnum; phdr_idx++) {
		phdr = (struct elf32_phdr *)elf_ptr;

		dev_dbg(core->dev,
			"index = %d, vaddr = 0x%x, paddr = 0x%x, filesz = 0x%x, memsz = 0x%x, flags = 0x%x\n"
			, phdr_idx, phdr->p_vaddr, phdr->p_paddr,
			phdr->p_filesz, phdr->p_memsz, phdr->p_flags);

		ret = wcd_cpe_load_each_segment(core, phdr_idx, phdr);
		if (IS_ERR_VALUE(ret)) {
			dev_err(core->dev,
				"Failed to load segment %d .. aborting img dload\n",
				phdr_idx);
			img_dload_fail = true;
			goto done;
		}

		elf_ptr = elf_ptr + sizeof(*phdr);

	}

	if (!img_dload_fail) {
		wcd_cpe_enable_cpe_clks(core, true);
		ret = cpe_svc_boot(core->cpe_handle, core->cpe_debug_mode);
		if (IS_ERR_VALUE(ret))
			dev_err(core->dev,
				"%s: Failed to boot CPE\n",
				__func__);
	}

done:
	release_firmware(fw);
	return;
}

/*
 * wcd_cpe_get_core_handle: get the handle to wcd_cpe_core
 * @codec: codec from which this handle is to be obtained
 * Codec driver should provide a callback function to obtain
 * handle to wcd_cpe_core during initialization of wcd_cpe_core
 */
void *wcd_cpe_get_core_handle(
	struct snd_soc_codec *codec)
{
	struct wcd_cpe_core *core = NULL;

	if (!codec) {
		pr_err("%s: Invalid codec handle\n",
			__func__);
		goto done;
	}

	if (!wcd_get_cpe_core) {
		dev_err(codec->dev,
			"%s: codec callback not available\n",
			__func__);
		goto done;
	}

	core = wcd_get_cpe_core(codec);

	if (!core)
		dev_err(codec->dev,
			"%s: handle to core not available\n",
			__func__);
done:
	return core;
}

/*
 * svass_engine_irq: threaded interrupt handler for svass engine irq
 * @irq: interrupt number
 * @data: data pointer passed during irq registration
 */
static irqreturn_t svass_engine_irq(int irq, void *data)
{
	struct wcd_cpe_core *core = data;
	int ret = 0;

	if (!core) {
		pr_err("%s: Invalid data for interrupt handler\n",
			__func__);
		goto done;
	}

	ret = cpe_svc_process_irq(core->cpe_handle, CPE_IRQ_OUTBOX_IRQ);
	if (IS_ERR_VALUE(ret))
		dev_err(core->dev,
			"%s: Error processing irq from cpe_Services\n",
			__func__);
done:
	return IRQ_HANDLED;
}

/*
 * svass_exception_irq: threaded irq handler for sva error interrupts
 * @irq: interrupt number
 * @data: data pointer passed during irq registration
 *
 * Once a error interrupt is received, it is not cleared, since
 * clearing this interrupt will raise spurious interrupts unless
 * CPE is reset.
 */
static irqreturn_t svass_exception_irq(int irq, void *data)
{
	struct wcd_cpe_core *core = data;
	u8 status = 0;

	status = snd_soc_read(core->codec, 0x54);

	dev_err(core->dev,
		"%s: err_interrupt status = 0x2%x\n",
		__func__, status);

	return IRQ_HANDLED;
}

/*
 * wcd_cpe_cmi_afe_cb: callback called on response to afe commands
 * @param: parameter containing the response code, etc
 *
 * Process the request to the command sent to CPE and wakeup the
 * command send wait.
 */
void wcd_cpe_cmi_afe_cb(const struct cmi_api_notification *param)
{
	struct cmi_hdr *hdr;
	struct wcd_cmi_afe_port_data *afe_port_d;
	u8 port_id;

	if (!param) {
		pr_err("%s: param is null\n", __func__);
		return;
	}

	if (param->event != CMI_API_MSG) {
		pr_err("%s: unhandled event 0x%x\n",
			__func__, param->event);
		return;
	}

	pr_debug("%s: param->result = %d\n",
		 __func__, param->result);

	hdr = (struct cmi_hdr *) param->message;

	/*
	 * for AFE cmd response, port id is
	 * stored at session id field of header
	 */
	port_id = CMI_HDR_GET_SESSION_ID(hdr);
	if (port_id > WCD_CPE_AFE_MAX_PORTS) {
		pr_err("%s: invalid port_id %d\n",
			__func__, port_id);
		return;
	}

	afe_port_d = &(afe_ports[port_id]);

	if (hdr->opcode == CPE_CMI_BASIC_RSP_OPCODE) {

		u8 *payload = ((u8 *)param->message) + (sizeof(struct cmi_hdr));
		u8 result = payload[0];
		afe_port_d->cmd_result = result;
		complete(&afe_port_d->afe_cmd_complete);

	} else if (hdr->opcode == CPE_AFE_PORT_CMDRSP_SHARED_MEM_ALLOC) {

		struct cpe_cmdrsp_shmem_alloc *cmdrsp_shmem_alloc =
			(struct cpe_cmdrsp_shmem_alloc *) param->message;

		if (cmdrsp_shmem_alloc->addr == 0) {
			pr_err("%s: Failed AFE shared mem alloc\n", __func__);
			afe_port_d->cmd_result = CMI_SHMEM_ALLOC_FAILED;
		} else {
			pr_debug("%s AFE shared mem addr = 0x%x\n",
				 __func__, cmdrsp_shmem_alloc->addr);
			afe_port_d->mem_handle = cmdrsp_shmem_alloc->addr;
			afe_port_d->cmd_result = 0;
		}
		complete(&afe_port_d->afe_cmd_complete);
	}

	return;
}

/*
 * wcd_cpe_initialize_afe_port_data: Initialize all AFE ports
 *
 * Initialize the data for all the afe ports. Assign the
 * afe port state to INIT state.
 */
static void wcd_cpe_initialize_afe_port_data(void)
{
	struct wcd_cmi_afe_port_data *afe_port_d;
	int i = 0;

	for (i = 1; i <= WCD_CPE_AFE_MAX_PORTS; i++) {
		afe_port_d = &afe_ports[i];
		afe_port_d->port_id = i;
		init_completion(&afe_port_d->afe_cmd_complete);
		afe_port_d->port_state = AFE_PORT_STATE_INIT;
		mutex_init(&afe_port_d->afe_lock);
	}
}

/*
 * wcd_cpe_deinitialize_afe_port_data: De-initialize all AFE ports
 *
 * De-Initialize the data for all the afe ports. Assign the
 * afe port state to DEINIT state.
 */
static void wcd_cpe_deinitialize_afe_port_data(void)
{
	struct wcd_cmi_afe_port_data *afe_port_d;
	int i = 0;

	for (i = 1; i <= WCD_CPE_AFE_MAX_PORTS; i++) {
		afe_port_d = &afe_ports[i];
		afe_port_d->port_state = AFE_PORT_STATE_DEINIT;
		mutex_destroy(&afe_port_d->afe_lock);
	}
}

/*
 * wcd_cpe_svc_event_cb: callback from cpe services, indicating
 * CPE is online or offline.
 * @param: parameter / payload for event to be notified
 */
static void wcd_cpe_svc_event_cb(const struct cpe_svc_notification *param)
{
	struct snd_soc_codec *codec;
	struct wcd_cpe_core *core;

	if (!param) {
		pr_err("%s: Invalid event\n", __func__);
		return;
	}

	codec = param->payload;

	if (!codec || !wcd_cpe_get_core_handle(codec)) {
		pr_err("%s: Invalid handle to codec/core\n",
			__func__);
		return;
	}

	core = wcd_cpe_get_core_handle(codec);

	switch (param->event) {
	case CPE_SVC_ONLINE:
		dev_info(core->dev, "%s CPE is now online\n",
			 __func__);

		/* Register for AFE Service */
		core->cmi_afe_handle = cmi_register(wcd_cpe_cmi_afe_cb,
						CMI_CPE_AFE_SERVICE_ID);
		wcd_cpe_initialize_afe_port_data();
		if (!core->cmi_afe_handle) {
			dev_err(core->dev,
				"%s: Failed to register AFE service with CMI\n",
				__func__);
			return;
		}

		break;
	case CPE_SVC_OFFLINE:
		cmi_deregister(core->cmi_afe_handle);
		core->cmi_afe_handle = NULL;
		wcd_cpe_deinitialize_afe_port_data();

		dev_info(core->dev, "%s: CPE is now offline\n",
			 __func__);
		break;
	default:
		dev_err(core->dev,
			"%s: unhandled notification\n",
			__func__);
		break;
	}

	return;
}

/*
 * wcd_cpe_setup_sva_err_intr: setup the irqs for CPE
 * @core: handle to wcd_cpe_core
 * All interrupts needed for CPE are acquired. If any
 * request_irq fails, then all irqs are free'd
 */
static int wcd_cpe_setup_irqs(struct wcd_cpe_core *core)
{
	int ret;
	struct snd_soc_codec *codec = core->codec;
	struct wcd9xxx *wcd9xxx = codec->control_data;
	struct wcd9xxx_core_resource *core_res = &wcd9xxx->core_res;

	ret = wcd9xxx_request_irq(core_res, WCD9330_IRQ_SVASS_ENGINE,
				  svass_engine_irq, "SVASS_Engine", core);
	if (ret) {
		dev_err(core->dev,
			"%s: Failed to request svass engine irq\n",
			__func__);
		goto fail_engine_irq;
	}

	/* Make sure all error interrupts are cleared */
	snd_soc_update_bits(codec, TOMTOM_A_SVASS_INT_CLR,
			    0x3F, 0x3F);

	/* Enable all error interrupts */
	snd_soc_update_bits(codec, TOMTOM_A_SVASS_INT_MASK,
			    0x3F, 0x3F);

	ret = wcd9xxx_request_irq(core_res, WCD9330_IRQ_SVASS_ERR_EXCEPTION,
				  svass_exception_irq, "SVASS_Exception", core);
	if (ret) {
		dev_err(core->dev,
			"%s: Failed to request svass err irq\n",
			__func__);
		goto fail_exception_irq;
	}

	return 0;

fail_exception_irq:
	wcd9xxx_free_irq(core_res, WCD9330_IRQ_SVASS_ENGINE, core);

fail_engine_irq:
	return ret;
}

/*
 * wcd_cpe_init_and_boot: Initialize and bootup CPE hardware block
 * @img_fname: filename for firmware image
 * @codec: handle to codec requesting for image download
 * @params: parameter structure passed from caller
 *
 * This API will initialize the cpe core and schedule work
 * to perform firmware image download to CPE and bootup
 * CPE. Will also request for CPE related interrupts.
 */
struct wcd_cpe_core *wcd_cpe_init_and_boot(const char *img_fname,
	struct snd_soc_codec *codec,
	struct wcd_cpe_params *params)
{
	struct wcd_cpe_core *core;
	int ret = 0;

	if (!codec) {
		pr_err("%s: Invalid codec\n", __func__);
		return NULL;
	}

	if (!params) {
		dev_err(codec->dev,
			"%s: No params supplied for codec %s\n",
			__func__, codec->name);
		return NULL;
	}

	if (!params->codec || !params->get_cpe_core ||
	    !params->cdc_cb) {
		dev_err(codec->dev,
			"%s: Invalid params for codec %s\n",
			__func__, codec->name);
		return NULL;
	}

	core = kzalloc(sizeof(struct wcd_cpe_core), GFP_KERNEL);
	if (!core) {
		dev_err(codec->dev,
			"%s: Failed to allocate cpe core data\n",
			__func__);
		return NULL;
	}

	snprintf(core->fname, sizeof(core->fname), "%s", img_fname);

	wcd_get_cpe_core = params->get_cpe_core;

	core->codec = params->codec;
	core->dev = params->codec->dev;
	core->cpe_debug_mode = params->dbg_mode;

	core->cdc_info.major_version = params->cdc_major_ver;
	core->cdc_info.minor_version = params->cdc_minor_ver;
	core->cdc_info.id = params->cdc_id;

	core->cpe_cdc_cb.cdc_clk_en = params->cdc_cb->cdc_clk_en;
	core->cpe_cdc_cb.cpe_clk_en = params->cdc_cb->cpe_clk_en;

	INIT_WORK(&core->load_fw_work, wcd_cpe_load_fw_image);

	core->cpe_handle = cpe_svc_initialize(NULL, &core->cdc_info, codec);
	if (!core->cpe_handle) {
		dev_err(core->dev,
			"%s: failed to initialize cpe services\n",
			__func__);
		goto fail_cpe_initialize;
	}

	core->cpe_reg_handle = cpe_svc_register(core->cpe_handle,
					wcd_cpe_svc_event_cb,
					CPE_SVC_ONLINE | CPE_SVC_OFFLINE,
					"codec cpe handler");
	if (!core->cpe_reg_handle) {
		dev_err(core->dev,
			"%s: failed to register cpe service\n",
			__func__);
		goto fail_cpe_register;
	}

	ret = wcd_cpe_setup_irqs(core);
	if (ret)
		goto fail_setup_irq;

	schedule_work(&core->load_fw_work);
	return core;

fail_setup_irq:
	cpe_svc_deregister(core->cpe_handle, core->cpe_reg_handle);

fail_cpe_register:
	cpe_svc_deinitialize(core->cpe_handle);

fail_cpe_initialize:
	kfree(core);
	return NULL;
}
EXPORT_SYMBOL(wcd_cpe_init_and_boot);

/*
 * wcd_cpe_cmi_lsm_callback: callback called from cpe services
 *			     to notify command response for lsm
 *			     service
 * @param: param containing the response code and status
 *
 * This callback is registered with cpe services while registering
 * the LSM service
 */
void wcd_cpe_cmi_lsm_callback(const struct cmi_api_notification *param)
{
	struct cmi_hdr *hdr;
	struct cpe_lsm_session *lsm_session;
	u8 session_id;

	if (!param) {
		pr_err("%s: param is null\n", __func__);
		return;
	}

	if (param->event != CMI_API_MSG) {
		pr_err("%s: unhandled event 0x%x\n", __func__, param->event);
		return;
	}

	hdr = (struct cmi_hdr *) param->message;
	session_id = CMI_HDR_GET_SESSION_ID(hdr);

	if (session_id > WCD_CPE_LSM_MAX_SESSIONS) {
		pr_err("%s: invalid lsm session id = %d\n",
			__func__, session_id);
		return;
	}

	lsm_session = lsm_sessions[session_id];

	if (hdr->opcode == CPE_CMI_BASIC_RSP_OPCODE) {

		u8 *payload = ((u8 *)param->message) + (sizeof(struct cmi_hdr));
		u8 result = payload[0];
		lsm_session->cmd_err_code |= result;
		complete(&lsm_session->cmd_comp);

	} else if (hdr->opcode == CPE_LSM_SESSION_CMDRSP_SHARED_MEM_ALLOC) {

		struct cpe_cmdrsp_shmem_alloc *cmdrsp_shmem_alloc =
			(struct cpe_cmdrsp_shmem_alloc *) param->message;

		if (cmdrsp_shmem_alloc->addr == 0) {
			pr_err("%s: Failed LSM shared mem alloc\n", __func__);
			lsm_session->cmd_err_code = CMI_SHMEM_ALLOC_FAILED;

		} else {

			pr_debug("%s LSM shared mem addr = 0x%x\n",
				__func__, cmdrsp_shmem_alloc->addr);
			lsm_session->lsm_mem_handle = cmdrsp_shmem_alloc->addr;
			lsm_session->cmd_err_code = 0;
		}

		complete(&lsm_session->cmd_comp);

	} else if (hdr->opcode == CPE_LSM_SESSION_EVENT_DETECTION_STATUS_V2) {

		struct cpe_lsm_event_detect_v2 *event_detect_v2 =
			(struct cpe_lsm_event_detect_v2 *) param->message;

		if (!lsm_session->priv_d) {
			pr_err("%s: private data is not present\n",
				__func__);
			return;
		}

		pr_debug("%s: event payload, status = %u, size = %u\n",
			__func__, event_detect_v2->detection_status,
			event_detect_v2->size);

		if (lsm_session->event_cb)
			lsm_session->event_cb(
				lsm_session->priv_d,
				event_detect_v2->detection_status,
				event_detect_v2->size,
				event_detect_v2->payload);
	}

	return;
}

/*
 * wcd_cpe_cmi_send_lsm_msg: send a message to lsm service
 * @session: session on which to send the message
 * @message: actual message containing header and payload
 *
 * Sends message to lsm service for specified session and wait
 * for response back on the message.
 * should be called after acquiring session specific mutex
 */
static int wcd_cpe_cmi_send_lsm_msg(struct cpe_lsm_session *session,
				    void *message)
{
	int ret = 0;
	struct cmi_hdr *hdr = message;

	pr_debug("%s: sending message with opcode 0x%x\n",
		 __func__, hdr->opcode);

	ret = cmi_send_msg(message);
	if (ret) {
		pr_err("%s: msg opcode (0x%x) send failed (%d)\n",
			__func__, hdr->opcode, ret);
		return ret;
	}

	ret = wait_for_completion_timeout(&session->cmd_comp,
					  CMI_CMD_TIMEOUT);
	if (ret > 0) {
		pr_debug("%s: command 0x%x, received response 0x%x\n",
			__func__, hdr->opcode, session->cmd_err_code);
		if (session->cmd_err_code)
			return session->cmd_err_code;
	} else {
		pr_err("%s: command (0x%x) send timed out\n",
			__func__, hdr->opcode);
		return -ETIMEDOUT;
	}

	INIT_COMPLETION(session->cmd_comp);

	return 0;
}


/*
 * fill_cmi_header: fill the cmi header with specified values
 *
 * @hdr: header to be updated with values
 * @session_id: session id of the header,
 *		in case of AFE service it is port_id
 * @service_id: afe/lsm, etc
 * @version: update the version field in header
 * @payload_size: size of the payload following after header
 * @opcode: opcode of the message
 * @obm_flag: indicates if this header is for obm message
 *
 */
static int fill_cmi_header(struct cmi_hdr *hdr,
			   u8 session_id, u8 service_id,
			   bool version, u8 payload_size,
			   u16 opcode, bool obm_flag)
{
	/* sanitize the data */
	if (!IS_VALID_SESSION_ID(session_id) ||
	    !IS_VALID_SERVICE_ID(service_id) ||
	    !IS_VALID_PLD_SIZE(payload_size)) {
		pr_err("Invalid header creation request\n");
		return -EINVAL;
	}

	CMI_HDR_SET_SESSION(hdr, session_id);
	CMI_HDR_SET_SERVICE(hdr, service_id);
	if (version)
		CMI_HDR_SET_VERSION(hdr, 1);
	else
		CMI_HDR_SET_VERSION(hdr, 0);

	CMI_HDR_SET_PAYLOAD_SIZE(hdr, payload_size);

	hdr->opcode = opcode;

	if (obm_flag)
		CMI_HDR_SET_OBM(hdr, CMI_OBM_FLAG_OUT_BAND);
	else
		CMI_HDR_SET_OBM(hdr, CMI_OBM_FLAG_IN_BAND);

	return 0;
}

static int fill_lsm_cmd_header_v0_inband(struct cmi_hdr *hdr,
		u8 session_id, u8 payload_size, u16 opcode)
{
	return fill_cmi_header(hdr, session_id,
			       CMI_CPE_LSM_SERVICE_ID, false,
			       payload_size, opcode, false);
}


static int wcd_cpe_is_valid_lsm_session(struct wcd_cpe_core *core,
		struct cpe_lsm_session *session,
		const char *func)
{
	if (unlikely(IS_ERR_OR_NULL(core))) {
		pr_err("%s: invalid handle to core\n",
			func);
		return -EINVAL;
	}

	if (unlikely(IS_ERR_OR_NULL(session))) {
		dev_err(core->dev, "%s: invalid session\n",
			func);
		return -EINVAL;
	}

	if (session->id > WCD_CPE_LSM_MAX_SESSIONS) {
		dev_err(core->dev, "%s: invalid session id (%u)\n",
			func, session->id);
		return -EINVAL;
	}

	dev_dbg(core->dev, "%s: session_id = %u\n",
		func, session->id);
	return 0;
}

/*
 * wcd_cpe_cmd_lsm_open_tx: compose and send lsm open command
 * @core_handle: handle to cpe core
 * @session: session for which the command needs to be sent
 * @app_id: application id part of the command
 * @sample_rate: sample rate for this session
 */
static int wcd_cpe_cmd_lsm_open_tx(void *core_handle,
		struct cpe_lsm_session *session,
		u16 app_id, u16 sample_rate)
{
	struct cpe_lsm_cmd_open_tx cmd_open_tx;
	struct wcd_cpe_core *core = core_handle;
	int ret = 0;

	ret = wcd_cpe_is_valid_lsm_session(core, session,
					   __func__);
	if (ret)
		return ret;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

	memset(&cmd_open_tx, 0, sizeof(struct cpe_lsm_cmd_open_tx));
	if (fill_lsm_cmd_header_v0_inband(&cmd_open_tx.hdr,
				session->id, OPEN_CMD_PAYLOAD_SIZE,
				CPE_LSM_SESSION_CMD_OPEN_TX)) {
		ret = -EINVAL;
		goto end_ret;
	}

	cmd_open_tx.app_id = app_id;
	cmd_open_tx.sampling_rate = sample_rate;

	ret = wcd_cpe_cmi_send_lsm_msg(session, &cmd_open_tx);
	if (ret)
		dev_err(core->dev,
			"%s: failed to send open_tx cmd, err = %d\n",
			__func__, ret);
end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}

/*
 * wcd_cpe_cmd_close_tx: compose and send lsm close command
 * @core_handle: handle to cpe core
 * @session: session for which the command needs to be sent
 */
static int wcd_cpe_cmd_lsm_close_tx(void *core_handle,
			struct cpe_lsm_session *session)
{
	struct cmi_hdr cmd_close_tx;
	struct wcd_cpe_core *core = core_handle;
	int ret = 0;

	ret = wcd_cpe_is_valid_lsm_session(core, session,
					   __func__);
	if (ret)
		return ret;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

	memset(&cmd_close_tx, 0, sizeof(cmd_close_tx));
	if (fill_lsm_cmd_header_v0_inband(&cmd_close_tx, session->id,
			    0, CPE_LSM_SESSION_CMD_CLOSE_TX)) {
		ret = -EINVAL;
		goto end_ret;
	}

	ret = wcd_cpe_cmi_send_lsm_msg(session, &cmd_close_tx);
	if (ret)
		dev_err(core->dev,
			"%s: lsm close_tx cmd failed, err = %d\n",
			__func__, ret);
end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}


/*
 * wcd_cpe_cmd_shmem_alloc: compose and send lsm shared
 *			    memory allocation command
 * @core_handle: handle to cpe core
 * @session: session for which the command needs to be sent
 * @size: size of memory to be allocated
 */
static int wcd_cpe_cmd_lsm_shmem_alloc(void *core_handle,
			struct cpe_lsm_session *session,
			u32 size)
{
	struct cpe_cmd_shmem_alloc cmd_shmem_alloc;
	struct wcd_cpe_core *core = core_handle;
	int ret = 0;

	ret = wcd_cpe_is_valid_lsm_session(core, session,
					   __func__);
	if (ret)
		return ret;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

	memset(&cmd_shmem_alloc, 0, sizeof(cmd_shmem_alloc));
	if (fill_lsm_cmd_header_v0_inband(&cmd_shmem_alloc.hdr, session->id,
			    SHMEM_ALLOC_CMD_PLD_SIZE,
			    CPE_LSM_SESSION_CMD_SHARED_MEM_ALLOC)) {
		ret = -EINVAL;
		goto end_ret;
	}

	cmd_shmem_alloc.size = size;
	ret = wcd_cpe_cmi_send_lsm_msg(session, &cmd_shmem_alloc);
	if (ret)
		dev_err(core->dev,
			"%s: lsm_shmem_alloc cmd send fail, %d\n",
			__func__, ret);
end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}

/*
 * wcd_cpe_cmd_lsm_shmem_dealloc: deallocate the shared memory
 *				  for the specified session
 * @core_handle: handle to cpe core
 * @session: session for which memory needs to be deallocated.
 */
static int wcd_cpe_cmd_lsm_shmem_dealloc(void *core_handle,
		struct cpe_lsm_session *session)
{
	struct cpe_cmd_shmem_dealloc cmd_dealloc;
	struct wcd_cpe_core *core = core_handle;
	int ret = 0;

	ret = wcd_cpe_is_valid_lsm_session(core, session,
					   __func__);
	if (ret)
		return ret;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

	memset(&cmd_dealloc, 0, sizeof(cmd_dealloc));
	if (fill_lsm_cmd_header_v0_inband(&cmd_dealloc.hdr, session->id,
			    SHMEM_DEALLOC_CMD_PLD_SIZE,
			    CPE_LSM_SESSION_CMD_SHARED_MEM_DEALLOC)) {
		ret = -EINVAL;
		goto end_ret;
	}

	cmd_dealloc.addr = session->lsm_mem_handle;
	ret = wcd_cpe_cmi_send_lsm_msg(session, &cmd_dealloc);
	if (ret) {
		dev_err(core->dev,
			"%s: lsm_shmem_dealloc cmd failed, rc %d\n",
			__func__, ret);
		goto end_ret;
	}

	memset(&session->lsm_mem_handle, 0,
	       sizeof(session->lsm_mem_handle));

end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}

/*
 * wcd_cpe_lsm_send_acdb_cal: send the calibration for lsm service
 *			      from acdb to the cpe
 * @session: session for which the calibration needs to be set.
 */
static int wcd_cpe_lsm_send_acdb_cal(struct cpe_lsm_session *session)
{

	u8 *msg_pld;
	struct cmi_hdr *hdr;
	struct acdb_cal_block lsm_cal;
	void *inb_msg;
	int rc = 0;

	rc = get_ulp_lsm_cal(&lsm_cal);
	if (rc) {
		pr_err("%s: Fail to obtain acdb cal, err = %d\n",
			__func__, rc);
		return rc;
	}

	inb_msg = kzalloc(sizeof(struct cmi_hdr) + lsm_cal.cal_size,
			  GFP_KERNEL);
	if (!inb_msg) {
		pr_err("%s: no memory for lsm acdb cal\n",
			__func__);
		rc = -ENOMEM;
		return rc;
	}

	hdr = (struct cmi_hdr *) inb_msg;

	rc = fill_lsm_cmd_header_v0_inband(hdr, session->id,
			lsm_cal.cal_size,
			CPE_LSM_SESSION_CMD_SET_PARAMS);
	if (rc) {
		pr_err("%s: invalid params for header, err = %d\n",
			__func__, rc);
		kfree(inb_msg);
		return rc;
	}

	msg_pld = ((u8 *) inb_msg) + sizeof(struct cmi_hdr);
	memcpy(msg_pld, lsm_cal.cal_kvaddr,
	       lsm_cal.cal_size);

	rc = wcd_cpe_cmi_send_lsm_msg(session, inb_msg);
	if (rc)
		pr_err("%s: acdb lsm_params send failed, err = %d\n",
			__func__, rc);

	kfree(inb_msg);
	return rc;

}

/*
 * wcd_cpe_lsm_set_params: set the parameters for lsm service
 * @session: session for which the parameters are to be set
 * @detect_mode: mode for detection
 * @detect_failure: flag indicating failure detection enabled/disabled
 *
 */
static int wcd_cpe_lsm_set_params(struct cpe_lsm_session *session,
				 enum lsm_detection_mode detect_mode,
				 bool detect_failure)
{
	struct cpe_lsm_params lsm_params;
	struct cpe_lsm_operation_mode *op_mode = &lsm_params.op_mode;
	struct cpe_lsm_connect_to_port *connect_port =
					&lsm_params.connect_port;
	int ret = 0;
	u8 pld_size = CPE_PARAM_PAYLOAD_SIZE;

	ret = wcd_cpe_lsm_send_acdb_cal(session);
	if (ret) {
		pr_err("%s: fail to sent acdb cal, err = %d",
			__func__, ret);
		return ret;
	}

	memset(&lsm_params, 0, sizeof(lsm_params));

	if (fill_lsm_cmd_header_v0_inband(&lsm_params.hdr,
				session->id,
				pld_size,
				CPE_LSM_SESSION_CMD_SET_PARAMS)) {
		ret = -EINVAL;
		goto err_ret;
	}

	op_mode->param.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	op_mode->param.param_id = LSM_PARAM_ID_OPERATION_MODE;
	op_mode->param.param_size = PARAM_SIZE_LSM_OP_MODE;
	op_mode->param.reserved = 0;
	op_mode->minor_version = 1;
	if (detect_mode == LSM_MODE_KEYWORD_ONLY_DETECTION)
		op_mode->mode = 1;
	else
		op_mode->mode = 3;

	if (detect_failure)
		op_mode->mode |= 0x04;

	op_mode->reserved = 0;

	connect_port->param.module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	connect_port->param.param_id = LSM_PARAM_ID_CONNECT_TO_PORT;
	connect_port->param.param_size = PARAM_SIZE_LSM_CONNECT_PORT;
	connect_port->param.reserved = 0;
	connect_port->minor_version = 1;
	connect_port->afe_port_id = CPE_AFE_PORT_1_TX;
	connect_port->reserved = 0;

	ret = wcd_cpe_cmi_send_lsm_msg(session, &lsm_params);
	if (ret)
		pr_err("%s: lsm_set_params failed, rc %dn",
			__func__, ret);
err_ret:
	return ret;
}

/*
 * wcd_cpe_lsm_set_conf_levels: send the confidence levels for listen
 * @session: session for which the confidence levels are to be set
 *
 * The actual confidence levels are part of the session.
 */
static int wcd_cpe_lsm_set_conf_levels(struct cpe_lsm_session *session)
{
	struct cpe_lsm_conf_level conf_level_data;
	struct cmi_hdr *hdr = &(conf_level_data.hdr);
	struct cpe_param_data *param_d = &(conf_level_data.param);
	u8 pld_size = 0;
	u8 pad_bytes = 0;
	void *message;
	int ret = 0;

	memset(&conf_level_data, 0, sizeof(conf_level_data));

	pld_size = (sizeof(struct cpe_lsm_conf_level) - sizeof(struct cmi_hdr));
	pld_size += session->num_confidence_levels;
	pad_bytes = ((4 - (pld_size % 4)) % 4);
	pld_size += pad_bytes;

	fill_cmi_header(hdr, session->id, CMI_CPE_LSM_SERVICE_ID,
			false, pld_size,
			CPE_LSM_SESSION_CMD_SET_PARAMS, false);

	param_d->module_id = LSM_MODULE_ID_VOICE_WAKEUP;
	param_d->param_id = LSM_PARAM_ID_MIN_CONFIDENCE_LEVELS;
	param_d->param_size = pld_size -
				sizeof(struct cpe_param_data);
	param_d->reserved = 0;

	conf_level_data.num_active_models = session->num_confidence_levels;

	message = kzalloc(sizeof(struct cpe_lsm_conf_level) +
			   conf_level_data.num_active_models + pad_bytes,
			   GFP_KERNEL);
	if (!message) {
		pr_err("%s: no memory for conf_level\n", __func__);
		return -ENOMEM;
	}

	memcpy(message, &conf_level_data,
	       sizeof(struct cpe_lsm_conf_level));
	memcpy(((u8 *) message) + sizeof(struct cpe_lsm_conf_level),
		session->conf_levels, conf_level_data.num_active_models);

	ret = wcd_cpe_cmi_send_lsm_msg(session, message);
	if (ret)
		pr_err("%s: lsm_set_conf_levels failed, err = %d\n",
			__func__, ret);
	kfree(message);
	return ret;
}

/*
 * wcd_cpe_lsm_reg_snd_model: register the sound model for listen
 * @session: session for which to register the sound model
 * @detect_mode: detection mode, user dependent/independent
 * @detect_failure: flag to indicate if failure detection is enabled
 *
 * The memory required for sound model should be pre-allocated on CPE
 * before this function is invoked.
 */
static int wcd_cpe_lsm_reg_snd_model(void *core_handle,
				 struct cpe_lsm_session *session,
				 enum lsm_detection_mode detect_mode,
				 bool detect_failure)
{
	int ret = 0;
	struct cmi_obm_msg obm_msg;
	struct wcd_cpe_core *core = core_handle;

	ret = wcd_cpe_is_valid_lsm_session(core, session,
					   __func__);
	if (ret)
		return ret;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	ret = wcd_cpe_lsm_set_params(session, detect_mode,
			       detect_failure);
	if (ret) {
		dev_err(core->dev,
			"%s: lsm set params failed, rc = %d\n",
			__func__, ret);
		goto err_ret;
	}

	ret = wcd_cpe_lsm_set_conf_levels(session);
	if (ret) {
		dev_err(core->dev,
			"%s: lsm confidence levels failed, rc = %d\n",
			__func__, ret);
		goto err_ret;
	}

	ret = fill_cmi_header(&obm_msg.hdr, session->id,
			CMI_CPE_LSM_SERVICE_ID, 0, 20,
			CPE_LSM_SESSION_CMD_REGISTER_SOUND_MODEL, true);
	if (ret) {
		dev_err(core->dev,
			"%s: Invalid parameters, rc = %d\n",
			__func__, ret);
		goto err_ret;
	}

	obm_msg.pld.version = 0;
	obm_msg.pld.size = session->snd_model_size;
	obm_msg.pld.data_ptr.kvaddr = session->snd_model_data;
	obm_msg.pld.mem_handle = session->lsm_mem_handle;

	ret = wcd_cpe_cmi_send_lsm_msg(session, &obm_msg);
	if (ret)
		dev_err(core->dev,
			"%s: snd_model_register failed, %d\n",
			__func__, ret);
err_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}

/*
 * wcd_cpe_lsm_dereg_snd_model: deregister the sound model for listen
 * @core_handle: handle to cpe core
 * @session: session for which to deregister the sound model
 *
 */
static int wcd_cpe_lsm_dereg_snd_model(void *core_handle,
				struct cpe_lsm_session *session)
{
	struct cmi_hdr cmd_dereg_snd_model;
	struct wcd_cpe_core *core = core_handle;
	int ret = 0;

	ret = wcd_cpe_is_valid_lsm_session(core, session,
					   __func__);
	if (ret)
		return ret;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

	memset(&cmd_dereg_snd_model, 0, sizeof(cmd_dereg_snd_model));
	if (fill_lsm_cmd_header_v0_inband(&cmd_dereg_snd_model, session->id,
			    0, CPE_LSM_SESSION_CMD_DEREGISTER_SOUND_MODEL)) {
		ret = -EINVAL;
		goto end_ret;
	}

	ret = wcd_cpe_cmi_send_lsm_msg(session, &cmd_dereg_snd_model);
	if (ret)
		dev_err(core->dev,
			"%s: failed to send dereg_snd_model cmd\n",
			__func__);
end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}

/*
 * wcd_cpe_cmd_lsm_start: send the start command to lsm
 * @core_handle: handle to the CPE core
 * @session: session for which start command to be sent
 *
 */
static int wcd_cpe_cmd_lsm_start(void *core_handle,
			struct cpe_lsm_session *session)
{
	struct cmi_hdr cmd_lsm_start;
	struct wcd_cpe_core *core = core_handle;
	int ret = 0;

	ret = wcd_cpe_is_valid_lsm_session(core, session,
					   __func__);
	if (ret)
		return ret;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

	memset(&cmd_lsm_start, 0, sizeof(struct cmi_hdr));
	if (fill_lsm_cmd_header_v0_inband(&cmd_lsm_start, session->id, 0,
					  CPE_LSM_SESSION_CMD_START)) {
		ret = -EINVAL;
		goto end_ret;
	}

	ret = wcd_cpe_cmi_send_lsm_msg(session, &cmd_lsm_start);
	if (ret)
		dev_err(core->dev, "failed to send lsm_start cmd\n");
end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}

/*
 * wcd_cpe_cmd_lsm_stop: send the stop command for LSM service
 * @core_handle: handle to the cpe core
 * @session: session for which stop command to be sent
 *
 */
static int wcd_cpe_cmd_lsm_stop(void *core_handle,
		struct cpe_lsm_session *session)
{
	struct cmi_hdr cmd_lsm_stop;
	struct wcd_cpe_core *core = core_handle;
	int ret = 0;

	ret = wcd_cpe_is_valid_lsm_session(core, session,
					   __func__);
	if (ret)
		return ret;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

	memset(&cmd_lsm_stop, 0, sizeof(struct cmi_hdr));
	if (fill_lsm_cmd_header_v0_inband(&cmd_lsm_stop, session->id, 0,
					  CPE_LSM_SESSION_CMD_STOP)) {
		ret = -EINVAL;
		goto end_ret;
	}

	ret = wcd_cpe_cmi_send_lsm_msg(session, &cmd_lsm_stop);
	if (ret)
		dev_err(core->dev,
			"%s: failed to send lsm_stop cmd\n",
			__func__);
end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;

}

/*
 * wcd_cpe_alloc_lsm_session: allocate a lsm session
 * @core: handle to wcd_cpe_core
 * @lsm_priv_d: lsm private data
 */
static struct cpe_lsm_session *wcd_cpe_alloc_lsm_session(
	void *core_handle, void *lsm_priv_d,
	void (*event_cb) (void *, u8, u8, u8 *))
{
	struct cpe_lsm_session *session;
	int i, session_id = -1;
	struct wcd_cpe_core *core = core_handle;

	for (i = 1; i <= WCD_CPE_LSM_MAX_SESSIONS; i++)
		if (!lsm_sessions[i])
			session_id = i;

	if (session_id < 0) {
		dev_err(core->dev,
			"%s: max allowed sessions already allocated\n",
			__func__);
		return NULL;
	}

	session = kzalloc(sizeof(struct cpe_lsm_session), GFP_KERNEL);
	if (!session) {
		dev_err(core->dev,
			"%s: failed to allocate session, no memory\n",
			__func__);
		return NULL;
	}

	session->id = session_id;
	session->event_cb = event_cb;
	session->cmi_reg_handle = cmi_register(wcd_cpe_cmi_lsm_callback,
						CMI_CPE_LSM_SERVICE_ID);
	session->priv_d = lsm_priv_d;
	mutex_init(&session->lsm_lock);

	if (!session->cmi_reg_handle) {
		dev_err(core->dev,
			"%s: Failed to register LSM service with CMI\n",
			__func__);
		goto err_ret;
	}

	session->lsm_mem_handle = 0;
	init_completion(&session->cmd_comp);

	lsm_sessions[session_id] = session;
	return session;

err_ret:
	kfree(session);
	return NULL;
}

/*
 * wcd_cpe_dealloc_lsm_session: deallocate lsm session
 * @core: handle to wcd_cpe_core
 * @session: lsm session to be deallocated
 */
static int wcd_cpe_dealloc_lsm_session(void *core_handle,
			struct cpe_lsm_session *session)
{
	struct wcd_cpe_core *core = core_handle;

	if (!session) {
		dev_err(core->dev,
			"%s: Invalid lsm session\n", __func__);
		return 0;
	}

	dev_dbg(core->dev, "%s: session %d being deallocated\n",
		__func__, session->id);
	if (session->id > WCD_CPE_LSM_MAX_SESSIONS) {
		dev_err(core->dev,
			"%s: Wrong session id %d max allowed = %d\n",
			__func__, session->id,
			WCD_CPE_LSM_MAX_SESSIONS);
		return -EINVAL;
	}
	mutex_destroy(&session->lsm_lock);
	lsm_sessions[session->id] = NULL;
	cmi_deregister(session->cmi_reg_handle);
	kfree(session);
	return 0;
}

/*
 * wcd_cpe_get_lsm_ops: register lsm driver to codec
 * @lsm_ops: structure with lsm callbacks
 * @codec: codec to which this lsm driver is registered to
 */
int wcd_cpe_get_lsm_ops(struct wcd_cpe_lsm_ops *lsm_ops)
{
	lsm_ops->lsm_alloc_session = wcd_cpe_alloc_lsm_session;
	lsm_ops->lsm_dealloc_session = wcd_cpe_dealloc_lsm_session;
	lsm_ops->lsm_open_tx = wcd_cpe_cmd_lsm_open_tx;
	lsm_ops->lsm_close_tx = wcd_cpe_cmd_lsm_close_tx;
	lsm_ops->lsm_shmem_alloc = wcd_cpe_cmd_lsm_shmem_alloc;
	lsm_ops->lsm_shmem_dealloc = wcd_cpe_cmd_lsm_shmem_dealloc;
	lsm_ops->lsm_register_snd_model = wcd_cpe_lsm_reg_snd_model;
	lsm_ops->lsm_deregister_snd_model = wcd_cpe_lsm_dereg_snd_model;
	lsm_ops->lsm_start = wcd_cpe_cmd_lsm_start;
	lsm_ops->lsm_stop = wcd_cpe_cmd_lsm_stop;
	return 0;
}
EXPORT_SYMBOL(wcd_cpe_get_lsm_ops);


static int fill_afe_cmd_header(struct cmi_hdr *hdr, u8 port_id,
				u16 opcode, u8 pld_size,
				bool obm_flag)
{
	CMI_HDR_SET_SESSION(hdr, port_id);
	CMI_HDR_SET_SERVICE(hdr, CMI_CPE_AFE_SERVICE_ID);

	CMI_HDR_SET_PAYLOAD_SIZE(hdr, pld_size);

	hdr->opcode = opcode;

	if (obm_flag)
		CMI_HDR_SET_OBM(hdr, CMI_OBM_FLAG_OUT_BAND);
	else
		CMI_HDR_SET_OBM(hdr, CMI_OBM_FLAG_IN_BAND);

	return 0;
}

/*
 * wcd_cpe_cmi_send_afe_msg: send message to AFE service
 * @port_cfg: configuration data for the afe port
 *	      for which this message is to be sent
 * @message: actual message with header and payload
 *
 * Port specific lock needs to be acquired before this
 * function can be invoked
 */
static int wcd_cpe_cmi_send_afe_msg(
	struct wcd_cmi_afe_port_data *port_d,
	void *message)
{
	int ret = 0;
	struct cmi_hdr *hdr = message;

	pr_debug("%s: sending message with opcode 0x%x\n",
		__func__, hdr->opcode);

	ret = cmi_send_msg(message);
	if (ret) {
		pr_err("%s: cmd 0x%x send failed, err = %d\n",
			__func__, hdr->opcode, ret);
		return ret;
	}

	ret = wait_for_completion_timeout(&port_d->afe_cmd_complete,
					  CMI_CMD_TIMEOUT);
	if (ret > 0) {
		pr_debug("%s: command 0x%x, received response 0x%x\n",
			 __func__, hdr->opcode, port_d->cmd_result);
		return port_d->cmd_result;
	} else {
		pr_err("%s: command 0x%x send timed out\n",
			__func__, hdr->opcode);
		return -ETIMEDOUT;
	}

	INIT_COMPLETION(port_d->afe_cmd_complete);
	return ret;
}



/*
 * wcd_cpe_afe_shmem_alloc: allocate the cpe memory for afe service
 * @port_cfg: configuration data for the port which needs
 *	      memory to be allocated on CPE
 * @size: size of the memory to be allocated
 */
static int wcd_cpe_afe_shmem_alloc(
	struct wcd_cmi_afe_port_data *port_d,
	u32 size)
{
	struct cpe_cmd_shmem_alloc cmd_shmem_alloc;
	int ret = 0;

	pr_debug("%s: enter: size = %d\n", __func__, size);

	memset(&cmd_shmem_alloc, 0, sizeof(cmd_shmem_alloc));
	if (fill_afe_cmd_header(&cmd_shmem_alloc.hdr, port_d->port_id,
			    CPE_AFE_PORT_CMD_SHARED_MEM_ALLOC,
			    SHMEM_ALLOC_CMD_PLD_SIZE, false)) {
		ret = -EINVAL;
		goto end_ret;
	}

	cmd_shmem_alloc.size = size;

	ret = wcd_cpe_cmi_send_afe_msg(port_d, &cmd_shmem_alloc);
	if (ret) {
		pr_err("%s: afe_shmem_alloc fail,ret = %d\n",
			__func__, ret);
		goto end_ret;
	}

	pr_debug("%s: completed %s, mem_handle = 0x%x\n",
		__func__, "CPE_AFE_CMD_SHARED_MEM_ALLOC",
		port_d->mem_handle);

end_ret:
	return ret;
}

/*
 * wcd_cpe_afe_shmem_dealloc: deallocate the cpe memory for
 *			      afe service
 * @port_d: configuration data for the port which needs
 *	      memory to be deallocated on CPE
 * The memory handle to be de-allocated is saved in the
 * port configuration data
 */
static int wcd_cpe_afe_shmem_dealloc(
	struct wcd_cmi_afe_port_data *port_d)
{
	struct cpe_cmd_shmem_dealloc cmd_dealloc;
	int ret = 0;

	pr_debug("%s: enter, port_id = %d\n",
		 __func__, port_d->port_id);

	memset(&cmd_dealloc, 0, sizeof(cmd_dealloc));
	if (fill_afe_cmd_header(&cmd_dealloc.hdr, port_d->port_id,
				CPE_AFE_PORT_CMD_SHARED_MEM_DEALLOC,
				SHMEM_DEALLOC_CMD_PLD_SIZE, false)) {
		ret = -EINVAL;
		goto end_ret;
	}

	cmd_dealloc.addr = port_d->mem_handle;
	ret = wcd_cpe_cmi_send_afe_msg(port_d, &cmd_dealloc);
	if (ret) {
		pr_err("failed to send shmem_dealloc cmd\n");
		goto end_ret;
	}
	memset(&port_d->mem_handle, 0,
	       sizeof(port_d->mem_handle));

end_ret:
	return ret;
}

/*
 * wcd_cpe_afe_send_acdb_cal: send the acdb calibration to AFE port
 * @port_d: configuration data for the port for which the
 *	      calibration needs to be appplied
 */
static int wcd_cpe_afe_send_acdb_cal(void *core_handle,
		struct wcd_cmi_afe_port_data *port_d)
{

	struct acdb_cal_block afe_listen_cal;
	struct wcd_cpe_core *core = core_handle;
	struct cmi_obm_msg obm_msg;
	void *inb_msg = NULL;
	void *msg;
	int rc = 0;
	bool is_obm_msg;

	rc = get_ulp_afe_cal(&afe_listen_cal);
	if (IS_ERR_VALUE(rc)) {
		dev_err(core->dev,
			"%s: Invalid afe cal for listen, error = %d\n",
			__func__, rc);
		return rc;
	}

	is_obm_msg = (afe_listen_cal.cal_size >
		      CMI_INBAND_MESSAGE_SIZE) ? true : false;

	if (is_obm_msg) {
		struct cmi_hdr *hdr = &(obm_msg.hdr);
		struct cmi_obm *pld = &(obm_msg.pld);

		rc = wcd_cpe_afe_shmem_alloc(port_d,
					afe_listen_cal.cal_size);
		if (rc) {
			dev_err(core->dev,
				"%s: AFE shmem alloc fail %d\n",
				__func__, rc);
			return rc;
		}

		rc = fill_cmi_header(hdr, port_d->port_id,
				     CMI_CPE_AFE_SERVICE_ID,
				     0, 20, CPE_AFE_CMD_SET_PARAM,
				     true);
		if (rc) {
			dev_err(core->dev,
				"%s: invalid params for header, err = %d\n",
				__func__, rc);
			return rc;
		}

		pld->version = 0;
		pld->size = afe_listen_cal.cal_size;
		pld->data_ptr.kvaddr = afe_listen_cal.cal_kvaddr;
		pld->mem_handle = port_d->mem_handle;
		msg = &obm_msg;

	} else {
		u8 *msg_pld;
		struct cmi_hdr *hdr;
		inb_msg = kzalloc(sizeof(struct cmi_hdr) +
					afe_listen_cal.cal_size,
				  GFP_KERNEL);
		if (!inb_msg) {
			dev_err(core->dev,
				"%s: no memory for afe cal inband\n",
				__func__);
			rc = -ENOMEM;
			return rc;
		}

		hdr = (struct cmi_hdr *) inb_msg;

		rc = fill_cmi_header(hdr, port_d->port_id,
				     CMI_CPE_AFE_SERVICE_ID,
				     0, afe_listen_cal.cal_size,
				     CPE_AFE_CMD_SET_PARAM, false);
		if (rc) {
			dev_err(core->dev,
				"%s: invalid params for header, err = %d\n",
				__func__, rc);
			kfree(inb_msg);
			inb_msg = NULL;
			return rc;
		}

		msg_pld = ((u8 *) inb_msg) + sizeof(struct cmi_hdr);
		memcpy(msg_pld, afe_listen_cal.cal_kvaddr,
		       afe_listen_cal.cal_size);

		msg = inb_msg;
	}

	rc = wcd_cpe_cmi_send_afe_msg(port_d, msg);
	if (rc)
		pr_err("%s: afe cal for listen failed, rc = %d\n",
			__func__, rc);

	if (is_obm_msg) {
		wcd_cpe_afe_shmem_dealloc(port_d);
		port_d->mem_handle = 0;
	} else {
		kfree(inb_msg);
		inb_msg = NULL;
	}

	return rc;
}

/*
 * wcd_cpe_is_valid_port: check validity of afe port id
 * @core: handle to core to check for validity
 * @afe_cfg: client provided afe configuration
 * @func: function name invoking this validity check,
 *	  used for logging purpose only.
 */
static int wcd_cpe_is_valid_port(struct wcd_cpe_core *core,
		struct wcd_cpe_afe_port_cfg *afe_cfg,
		const char *func)
{
	if (unlikely(IS_ERR_OR_NULL(core))) {
		pr_err("%s: Invalid core handle\n", func);
		return -EINVAL;
	}

	if (afe_cfg->port_id > WCD_CPE_AFE_MAX_PORTS) {
		dev_err(core->dev,
			"%s: invalid afe port (%u)\n",
			func, afe_cfg->port_id);
		return -EINVAL;
	}

	dev_dbg(core->dev,
		"%s: port_id = %u\n",
		func, afe_cfg->port_id);

	return 0;
}

/*
 * wcd_cpe_afe_set_params: set the parameters for afe port
 * @afe_cfg: configuration data for the port for which the
 *	      parameters are to be set
 */
static int wcd_cpe_afe_set_params(void *core_handle,
		struct wcd_cpe_afe_port_cfg *afe_cfg)
{
	struct cpe_afe_params afe_params;
	struct cpe_afe_hw_mad_ctrl *hw_mad_ctrl = &afe_params.hw_mad_ctrl;
	struct cpe_afe_port_cfg *port_cfg = &afe_params.port_cfg;
	struct wcd_cpe_core *core = core_handle;
	struct wcd_cmi_afe_port_data *afe_port_d;
	int ret = 0, pld_size = 0;

	ret = wcd_cpe_is_valid_port(core, afe_cfg, __func__);
	if (ret)
		return ret;

	afe_port_d = &afe_ports[afe_cfg->port_id];
	afe_port_d->port_id = afe_cfg->port_id;

	WCD_CPE_GRAB_LOCK(&afe_port_d->afe_lock, "afe");

	ret = wcd_cpe_afe_send_acdb_cal(core, afe_port_d);
	if (ret) {
		dev_err(core->dev,
			"%s: afe acdb cal send failed, err = %d\n",
			__func__, ret);
		goto err_ret;
	}

	pld_size = CPE_AFE_PARAM_PAYLOAD_SIZE;
	memset(&afe_params, 0, sizeof(afe_params));

	if (fill_afe_cmd_header(&afe_params.hdr,
				afe_cfg->port_id,
				CPE_AFE_CMD_SET_PARAM,
				(u8) pld_size, false)) {
		ret = -EINVAL;
		goto err_ret;
	}

	hw_mad_ctrl->param.module_id = CPE_AFE_MODULE_HW_MAD;
	hw_mad_ctrl->param.param_id = CPE_AFE_PARAM_ID_HW_MAD_CTL;
	hw_mad_ctrl->param.param_size = PARAM_SIZE_AFE_HW_MAD_CTRL;
	hw_mad_ctrl->param.reserved = 0;
	hw_mad_ctrl->minor_version = 1;
	hw_mad_ctrl->mad_type = MAD_TYPE_AUDIO;
	hw_mad_ctrl->mad_enable = 1;

	port_cfg->param.module_id = CPE_AFE_MODULE_AUDIO_DEV_INTERFACE;
	port_cfg->param.param_id = CPE_AFE_PARAM_ID_GENERIC_PORT_CONFIG;
	port_cfg->param.param_size = PARAM_SIZE_AFE_PORT_CFG;
	port_cfg->param.reserved = 0;
	port_cfg->minor_version = 1;
	port_cfg->bit_width = afe_cfg->bit_width;
	port_cfg->num_channels = afe_cfg->num_channels;
	port_cfg->sample_rate = afe_cfg->sample_rate;

	ret = wcd_cpe_cmi_send_afe_msg(afe_port_d, &afe_params);
	if (ret)
		dev_err(core->dev,
			"%s: afe_port_config failed, err = %d\n",
			__func__, ret);
err_ret:
	WCD_CPE_REL_LOCK(&afe_port_d->afe_lock, "afe");
	return ret;
}

/*
 * wcd_cpe_afe_port_start: send the start command to afe service
 * @core_handle: handle to the cpe core
 * @port_cfg: configuration data for the afe port which needs
 *	      to be started.
 */
static int wcd_cpe_afe_port_start(void *core_handle,
			struct wcd_cpe_afe_port_cfg *port_cfg)
{

	struct cmi_hdr hdr;
	struct wcd_cpe_core *core = core_handle;
	struct wcd_cmi_afe_port_data *afe_port_d;
	int ret = 0;

	ret = wcd_cpe_is_valid_port(core, port_cfg, __func__);
	if (ret)
		return ret;

	afe_port_d = &afe_ports[port_cfg->port_id];
	afe_port_d->port_id = port_cfg->port_id;

	WCD_CPE_GRAB_LOCK(&afe_port_d->afe_lock, "afe");

	memset(&hdr, 0, sizeof(struct cmi_hdr));
	fill_afe_cmd_header(&hdr, port_cfg->port_id,
			    CPE_AFE_PORT_CMD_START,
			    0, false);
	ret = wcd_cpe_cmi_send_afe_msg(afe_port_d, &hdr);
	if (ret)
		dev_err(core->dev,
			"%s: afe_port_start cmd failed, err = %d\n",
			__func__, ret);
	WCD_CPE_REL_LOCK(&afe_port_d->afe_lock, "afe");
	return ret;
}

/*
 * wcd_cpe_afe_port_stop: send stop command to afe service
 * @core_handle: handle to the cpe core
 * @port_cfg: configuration data for the afe port which needs
 *	      to be stopped.
 */
static int wcd_cpe_afe_port_stop(void *core_handle,
	struct wcd_cpe_afe_port_cfg *port_cfg)
{
	struct cmi_hdr hdr;
	struct wcd_cpe_core *core = core_handle;
	struct wcd_cmi_afe_port_data *afe_port_d;
	int ret = 0;

	ret = wcd_cpe_is_valid_port(core, port_cfg, __func__);
	if (ret)
		return ret;

	afe_port_d = &afe_ports[port_cfg->port_id];
	afe_port_d->port_id = port_cfg->port_id;

	WCD_CPE_GRAB_LOCK(&afe_port_d->afe_lock, "afe");

	memset(&hdr, 0, sizeof(hdr));
	fill_afe_cmd_header(&hdr, port_cfg->port_id,
			    CPE_AFE_PORT_CMD_STOP,
			    0, false);
	ret = wcd_cpe_cmi_send_afe_msg(afe_port_d, &hdr);
	if (ret)
		dev_err(core->dev,
			"%s: afe_stop cmd failed, err = %d\n",
			__func__, ret);

	WCD_CPE_REL_LOCK(&afe_port_d->afe_lock, "afe");
	return ret;
}

/*
 * wcd_cpe_afe_port_suspend: send suspend command to afe service
 * @core_handle: handle to the cpe core
 * @port_cfg: configuration data for the afe port which needs
 *	      to be suspended.
 */
static int wcd_cpe_afe_port_suspend(void *core_handle,
		struct wcd_cpe_afe_port_cfg *port_cfg)
{
	struct cmi_hdr hdr;
	struct wcd_cpe_core *core = core_handle;
	struct wcd_cmi_afe_port_data *afe_port_d;
	int ret = 0;

	ret = wcd_cpe_is_valid_port(core, port_cfg, __func__);
	if (ret)
		return ret;

	afe_port_d = &afe_ports[port_cfg->port_id];
	afe_port_d->port_id = port_cfg->port_id;

	WCD_CPE_GRAB_LOCK(&afe_port_d->afe_lock, "afe");

	memset(&hdr, 0, sizeof(struct cmi_hdr));
	fill_afe_cmd_header(&hdr, port_cfg->port_id,
			    CPE_AFE_PORT_CMD_SUSPEND,
			    0, false);
	ret = wcd_cpe_cmi_send_afe_msg(afe_port_d, &hdr);
	if (ret)
		dev_err(core->dev,
			"%s: afe_suspend cmd failed, err = %d\n",
			__func__, ret);
	WCD_CPE_REL_LOCK(&afe_port_d->afe_lock, "afe");
	return ret;
}

/*
 * wcd_cpe_afe_port_resume: send the resume command to afe service
 * @core_handle: handle to the cpe core
 * @port_cfg: configuration data for the afe port which needs
 *	      to be resumed.
 */
static int wcd_cpe_afe_port_resume(void *core_handle,
		struct wcd_cpe_afe_port_cfg *port_cfg)
{
	struct cmi_hdr hdr;
	struct wcd_cpe_core *core = core_handle;
	struct wcd_cmi_afe_port_data *afe_port_d;
	int ret = 0;

	ret = wcd_cpe_is_valid_port(core, port_cfg, __func__);
	if (ret)
		return ret;

	afe_port_d = &afe_ports[port_cfg->port_id];
	afe_port_d->port_id = port_cfg->port_id;

	WCD_CPE_GRAB_LOCK(&afe_port_d->afe_lock, "afe");

	memset(&hdr, 0, sizeof(hdr));
	fill_afe_cmd_header(&hdr, port_cfg->port_id,
			    CPE_AFE_PORT_CMD_RESUME,
			    0, false);
	ret = wcd_cpe_cmi_send_afe_msg(afe_port_d, &hdr);
	if (ret)
		dev_err(core->dev,
			"%s: afe_resume cmd failed, err = %d\n",
			__func__, ret);
	WCD_CPE_REL_LOCK(&afe_port_d->afe_lock, "afe");
	return ret;

}

/*
 * wcd_cpe_register_afe_driver: register lsm driver to codec
 * @cpe_ops: structure with lsm callbacks
 * @codec: codec to which this lsm driver is registered to
 */
int wcd_cpe_get_afe_ops(struct wcd_cpe_afe_ops *afe_ops)
{
	afe_ops->afe_set_params = wcd_cpe_afe_set_params;
	afe_ops->afe_port_start = wcd_cpe_afe_port_start;
	afe_ops->afe_port_stop = wcd_cpe_afe_port_stop;
	afe_ops->afe_port_suspend = wcd_cpe_afe_port_suspend;
	afe_ops->afe_port_resume = wcd_cpe_afe_port_resume;

	return 0;
}
EXPORT_SYMBOL(wcd_cpe_get_afe_ops);

MODULE_DESCRIPTION("WCD CPE Core");
MODULE_LICENSE("GPL v2");
