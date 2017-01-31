/* Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/pm_qos.h>
#include <linux/dma-mapping.h>
#include <sound/soc.h>
#include <sound/info.h>
#include <sound/lsm_params.h>
#include <sound/cpe_core.h>
#include <sound/cpe_cmi.h>
#include <sound/cpe_err.h>
#include <soc/qcom/pm.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-irq.h>
#include <sound/audio_cal_utils.h>
#include "wcd_cpe_core.h"
#include "wcd_cpe_services.h"
#include "wcd_cmi_api.h"

#define CMI_CMD_TIMEOUT (10 * HZ)
#define WCD_CPE_LSM_MAX_SESSIONS 2
#define WCD_CPE_AFE_MAX_PORTS 4
#define AFE_SVC_EXPLICIT_PORT_START 1
#define WCD_CPE_EC_PP_BUF_SIZE	480 /* 5 msec buffer */

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

#define WCD_CPE_STATE_MAX_LEN 11
#define CPE_OFFLINE_WAIT_TIMEOUT (2 * HZ)
#define CPE_READY_WAIT_TIMEOUT (3 * HZ)
#define WCD_CPE_SYSFS_DIR_MAX_LENGTH 32

#define CPE_ERR_IRQ_CB(core) \
	(core->cpe_cdc_cb->cpe_err_irq_control)

/*
 * AFE output buffer size is always
 * (sample_rate * number of bytes per sample/2*1000)
 */
#define AFE_OUT_BUF_SIZE(bit_width, sample_rate) \
	(((sample_rate) * (bit_width / BITS_PER_BYTE))/(2*1000))

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

struct cpe_lsm_ids {
	u32 module_id;
	u32 param_id;
};

static struct wcd_cpe_core *core_d;
static struct cpe_lsm_session
		*lsm_sessions[WCD_CPE_LSM_MAX_SESSIONS + 1];
struct wcd_cpe_core * (*wcd_get_cpe_core) (struct snd_soc_codec *);
static struct wcd_cmi_afe_port_data afe_ports[WCD_CPE_AFE_MAX_PORTS + 1];
static void wcd_cpe_svc_event_cb(const struct cpe_svc_notification *param);
static int wcd_cpe_setup_irqs(struct wcd_cpe_core *core);
static void wcd_cpe_cleanup_irqs(struct wcd_cpe_core *core);
static ssize_t cpe_ftm_test_trigger(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos);
static u32 ramdump_enable;
static u32 cpe_ftm_test_status;
static const struct file_operations cpe_ftm_test_trigger_fops = {
	.open = simple_open,
	.write = cpe_ftm_test_trigger,
};

static int wcd_cpe_afe_svc_cmd_mode(void *core_handle,
				    u8 mode);
struct wcd_cpe_attribute {
	struct attribute attr;
	ssize_t (*show)(struct wcd_cpe_core *core, char *buf);
	ssize_t (*store)(struct wcd_cpe_core *core, const char *buf,
			 ssize_t count);
};

#define WCD_CPE_ATTR(_name, _mode, _show, _store) \
static struct wcd_cpe_attribute cpe_attr_##_name = { \
	.attr = {.name = __stringify(_name), .mode = _mode}, \
	.show = _show, \
	.store = _store, \
}

#define to_wcd_cpe_attr(a) \
	container_of((a), struct wcd_cpe_attribute, attr)

#define kobj_to_cpe_core(kobj) \
	container_of((kobj), struct wcd_cpe_core, cpe_kobj)

/* wcd_cpe_lsm_session_active: check if any session is active
 * return true if any session is active.
 */
static bool wcd_cpe_lsm_session_active(void)
{
	int index = 1;
	bool lsm_active = false;

	/* session starts from index 1 */
	for (; index <= WCD_CPE_LSM_MAX_SESSIONS; index++) {
		if (lsm_sessions[index] != NULL) {
			lsm_active = true;
			break;
		} else {
			lsm_active = false;
		}
	}
	return lsm_active;
}

static int wcd_cpe_get_sfr_dump(struct wcd_cpe_core *core)
{
	struct cpe_svc_mem_segment dump_seg;
	int rc;
	u8 *sfr_dump;

	sfr_dump = kzalloc(core->sfr_buf_size, GFP_KERNEL);
	if (!sfr_dump) {
		dev_err(core->dev,
			"%s: No memory for sfr dump\n",
			__func__);
		goto done;
	}

	dump_seg.type = CPE_SVC_DATA_MEM;
	dump_seg.cpe_addr = core->sfr_buf_addr;
	dump_seg.size = core->sfr_buf_size;
	dump_seg.data = sfr_dump;
	dev_dbg(core->dev,
		"%s: reading SFR from CPE, size = %zu\n",
		__func__, core->sfr_buf_size);

	rc = cpe_svc_ramdump(core->cpe_handle, &dump_seg);
	if (IS_ERR_VALUE(rc)) {
		dev_err(core->dev,
			"%s: Failed to read cpe sfr_dump, err = %d\n",
			__func__, rc);
		goto free_sfr_dump;
	}

	dev_info(core->dev,
		 "%s: cpe_sfr = %s\n", __func__, sfr_dump);

free_sfr_dump:
	kfree(sfr_dump);
done:
	/* Even if SFR dump failed, do not return error */
	return 0;
}

static int wcd_cpe_collect_ramdump(struct wcd_cpe_core *core)
{
	struct cpe_svc_mem_segment dump_seg;
	int rc;

	if (!core->cpe_ramdump_dev || !core->cpe_dump_v_addr ||
	    core->hw_info.dram_size == 0) {
		dev_err(core->dev,
			"%s: Ramdump devices not set up, size = %zu\n",
			__func__, core->hw_info.dram_size);
		return -EINVAL;
	}

	dump_seg.type = CPE_SVC_DATA_MEM;
	dump_seg.cpe_addr = core->hw_info.dram_offset;
	dump_seg.size = core->hw_info.dram_size;
	dump_seg.data = core->cpe_dump_v_addr;

	dev_dbg(core->dev,
		"%s: Reading ramdump from CPE\n",
		__func__);

	rc = cpe_svc_ramdump(core->cpe_handle, &dump_seg);
	if (IS_ERR_VALUE(rc)) {
		dev_err(core->dev,
			"%s: Failed to read CPE ramdump, err = %d\n",
			__func__, rc);
		return rc;
	}

	dev_dbg(core->dev,
		"%s: completed reading ramdump from CPE\n",
		__func__);

	core->cpe_ramdump_seg.address = (unsigned long) core->cpe_dump_addr;
	core->cpe_ramdump_seg.size = core->hw_info.dram_size;
	core->cpe_ramdump_seg.v_address = core->cpe_dump_v_addr;

	rc = do_ramdump(core->cpe_ramdump_dev,
			&core->cpe_ramdump_seg, 1);
	if (rc)
		dev_err(core->dev,
			"%s: fail to dump cpe ram to device, err = %d\n",
			__func__, rc);
	return rc;
}

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
	int ret, ret1;

	if (!core || !core->cpe_cdc_cb ||
	    !core->cpe_cdc_cb->cpe_clk_en) {
		pr_err("%s: invalid handle\n",
			__func__);
		return -EINVAL;
	}

	ret = core->cpe_cdc_cb->cdc_clk_en(core->codec, enable);
	if (ret) {
		dev_err(core->dev, "%s: Failed to enable RCO\n",
			__func__);
		return ret;
	}

	if (!enable && core->cpe_clk_ref > 0)
		core->cpe_clk_ref--;

	/*
	 * CPE clk will be enabled at the first time
	 * and be disabled at the last time.
	 */
	if (core->cpe_clk_ref == 0) {
		ret = core->cpe_cdc_cb->cpe_clk_en(core->codec, enable);
		if (ret) {
			dev_err(core->dev,
				"%s: cpe_clk_en() failed, err = %d\n",
				__func__, ret);
			goto cpe_clk_fail;
		}
	}

	if (enable)
		core->cpe_clk_ref++;

	return 0;

cpe_clk_fail:
	/* Release the codec clk if CPE clk enable failed */
	if (enable) {
		ret1 = core->cpe_cdc_cb->cdc_clk_en(core->codec, !enable);
		if (ret1)
			dev_err(core->dev,
				"%s: Fail to release codec clk, err = %d\n",
				__func__, ret1);
	}

	return ret;
}

/*
 * wcd_cpe_bus_vote_max_bw: Function to vote for max bandwidth on codec bus
 * @core: handle to core for cpe
 * @vote: flag to indicate enable/disable of vote
 *
 * This function will try to use the codec provided callback to
 * vote/unvote for the max bandwidth of the bus that is used by
 * the codec for register reads/writes.
 */
static int wcd_cpe_bus_vote_max_bw(struct wcd_cpe_core *core,
		bool vote)
{
	if (!core || !core->cpe_cdc_cb) {
		pr_err("%s: Invalid handle to %s\n",
			__func__,
			(!core) ? "core" : "codec callbacks");
		return -EINVAL;
	}

	if (core->cpe_cdc_cb->bus_vote_bw) {
		dev_dbg(core->dev, "%s: %s cdc bus max bandwidth\n",
			 __func__, vote ? "Vote" : "Unvote");
		core->cpe_cdc_cb->bus_vote_bw(core->codec, vote);
	}

	return 0;
}

/*
 * wcd_cpe_load_fw: Function to load the fw image
 * @core: cpe core pointer
 * @load_type: indicates whether to load to data section
 *	       or the instruction section
 *
 * Parse the mdt file to look for program headers, load each
 * split file corresponding to the program headers.
 */
static int wcd_cpe_load_fw(struct wcd_cpe_core *core,
	unsigned int load_type)
{

	int ret, phdr_idx;
	struct snd_soc_codec *codec = NULL;
	struct wcd9xxx *wcd9xxx = NULL;
	const struct elf32_hdr *ehdr;
	const struct elf32_phdr *phdr;
	const struct firmware *fw;
	const u8 *elf_ptr;
	char mdt_name[64];
	bool img_dload_fail = false;
	bool load_segment;

	if (!core || !core->cpe_handle) {
		pr_err("%s: Error CPE core %pK\n", __func__,
		       core);
		return -EINVAL;
	}
	codec = core->codec;
	wcd9xxx = dev_get_drvdata(codec->dev->parent);
	snprintf(mdt_name, sizeof(mdt_name), "%s.mdt", core->fname);
	ret = request_firmware(&fw, mdt_name, core->dev);
	if (IS_ERR_VALUE(ret)) {
		dev_err(core->dev, "firmware %s not found\n", mdt_name);
		return ret;
	}

	ehdr = (struct elf32_hdr *) fw->data;
	if (!wcd_cpe_is_valid_elf_hdr(core, fw->size, ehdr)) {
		dev_err(core->dev, "%s: fw mdt %s is invalid\n",
			__func__, mdt_name);
		ret = -EINVAL;
		goto done;
	}

	elf_ptr = fw->data + sizeof(*ehdr);

	if (load_type == ELF_FLAG_EXECUTE) {
		/* Reset CPE first */
		ret = cpe_svc_reset(core->cpe_handle);
		if (IS_ERR_VALUE(ret)) {
			dev_err(core->dev,
				"%s: Failed to reset CPE with error %d\n",
				__func__, ret);
			goto done;
		}
	}

	dev_dbg(core->dev, "%s: start image dload, name = %s, load_type = 0x%x\n",
		__func__, core->fname, load_type);

	wcd_cpe_bus_vote_max_bw(core, true);

	/* parse every program header and request corresponding firmware */
	for (phdr_idx = 0; phdr_idx < ehdr->e_phnum; phdr_idx++) {
		phdr = (struct elf32_phdr *)elf_ptr;
		load_segment = false;

		dev_dbg(core->dev,
			"index = %d, vaddr = 0x%x, paddr = 0x%x, "
			"filesz = 0x%x, memsz = 0x%x, flags = 0x%x\n"
			, phdr_idx, phdr->p_vaddr, phdr->p_paddr,
			phdr->p_filesz, phdr->p_memsz, phdr->p_flags);

		switch (load_type) {
		case ELF_FLAG_EXECUTE:
			if (phdr->p_flags & load_type)
				load_segment = true;
			break;
		case ELF_FLAG_RW:
			if (!(phdr->p_flags & ELF_FLAG_EXECUTE) &&
			    (phdr->p_flags & load_type))
				load_segment = true;
			break;
		default:
			pr_err("%s: Invalid load_type 0x%x\n",
				__func__, load_type);
			ret = -EINVAL;
			goto rel_bus_vote;
		}

		if (load_segment) {
			ret = wcd_cpe_load_each_segment(core,
						phdr_idx, phdr);
			if (IS_ERR_VALUE(ret)) {
				dev_err(core->dev,
					"Failed to load segment %d, aborting img dload\n",
					phdr_idx);
				img_dload_fail = true;
				goto rel_bus_vote;
			}
		} else {
			dev_dbg(core->dev,
				"%s: skipped segment with index %d\n",
				__func__, phdr_idx);
		}

		elf_ptr = elf_ptr + sizeof(*phdr);
	}
	if (load_type == ELF_FLAG_EXECUTE)
		core->ssr_type = WCD_CPE_IMEM_DOWNLOADED;

rel_bus_vote:
	wcd_cpe_bus_vote_max_bw(core, false);

done:
	release_firmware(fw);
	return ret;
}

/*
 * wcd_cpe_change_online_state - mark cpe online/offline state
 * @core: core session to mark
 * @online: whether online of offline
 *
 */
static void wcd_cpe_change_online_state(struct wcd_cpe_core *core,
			int online)
{
	struct wcd_cpe_ssr_entry *ssr_entry = NULL;
	unsigned long ret;

	if (!core) {
		pr_err("%s: Invalid core handle\n",
			__func__);
		return;
	}

	ssr_entry = &core->ssr_entry;
	WCD_CPE_GRAB_LOCK(&core->ssr_lock, "SSR");
	ssr_entry->offline = !online;
	wmb();
	ret = xchg(&ssr_entry->offline_change, 1);
	wake_up_interruptible(&ssr_entry->offline_poll_wait);
	WCD_CPE_REL_LOCK(&core->ssr_lock, "SSR");
	pr_debug("%s: change state 0x%x offline_change 0x%x\n"
		 " core->offline 0x%x, ret = %ld\n",
		 __func__, online,
		 ssr_entry->offline_change,
		 core->ssr_entry.offline, ret);
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
	int ret = 0;
	core = container_of(work, struct wcd_cpe_core, load_fw_work);
	ret = wcd_cpe_load_fw(core, ELF_FLAG_EXECUTE);
	if (!ret)
		wcd_cpe_change_online_state(core, 1);
	else
		pr_err("%s: failed to load instruction section, err = %d\n",
			__func__, ret);
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
 * wcd_cpe_state_read - update read status in procfs
 * @entry: snd_info_entry
 * @buf: buffer where the read status is updated.
 *
 */
static ssize_t wcd_cpe_state_read(struct snd_info_entry *entry,
			       void *file_private_data, struct file *file,
			       char __user *buf, size_t count, loff_t pos)
{
	int len = 0;
	char buffer[WCD_CPE_STATE_MAX_LEN];
	struct wcd_cpe_core *core = NULL;
	struct wcd_cpe_ssr_entry *ssr_entry = NULL;

	core = (struct wcd_cpe_core *) entry->private_data;
	if (!core) {
		pr_err("%s: CPE core NULL\n", __func__);
		return -EINVAL;
	}
	ssr_entry = &core->ssr_entry;
	rmb();
	dev_dbg(core->dev,
		"%s: Offline 0x%x\n", __func__,
		 ssr_entry->offline);

	WCD_CPE_GRAB_LOCK(&core->ssr_lock, "SSR");
	len = snprintf(buffer, sizeof(buffer), "%s\n",
		       ssr_entry->offline ? "OFFLINE" : "ONLINE");
	WCD_CPE_REL_LOCK(&core->ssr_lock, "SSR");

	return simple_read_from_buffer(buf, count, &pos, buffer, len);
}

/*
 * wcd_cpe_state_poll - polls for change state
 * @entry: snd_info_entry
 * @wait: wait for duration for poll wait
 *
 */
static unsigned int wcd_cpe_state_poll(struct snd_info_entry *entry,
					void *private_data, struct file *file,
					poll_table *wait)
{
	struct wcd_cpe_core *core = NULL;
	struct wcd_cpe_ssr_entry *ssr_entry = NULL;
	int ret = 0;

	core = (struct wcd_cpe_core *) entry->private_data;
	if (!core) {
		pr_err("%s: CPE core NULL\n", __func__);
		return -EINVAL;
	}

	ssr_entry = &core->ssr_entry;

	dev_dbg(core->dev, "%s: CPE Poll wait\n",
	       __func__);
	poll_wait(file, &ssr_entry->offline_poll_wait, wait);
	dev_dbg(core->dev, "%s: Wake-up Poll wait\n",
	       __func__);
	WCD_CPE_GRAB_LOCK(&core->ssr_lock, "SSR");

	if (xchg(&ssr_entry->offline_change, 0))
		ret = POLLIN | POLLPRI | POLLRDNORM;

	WCD_CPE_REL_LOCK(&core->ssr_lock, "SSR");

	dev_dbg(core->dev, "%s: ret (%d) from poll_wait\n",
		__func__, ret);
	return ret;
}

/*
 * wcd_cpe_is_online_state - return true if card is online state
 * @core: core offline to query
 */
static bool wcd_cpe_is_online_state(void *core_handle)
{
	struct wcd_cpe_core *core = core_handle;
	if (core_handle) {
		return !core->ssr_entry.offline;
	} else {
		pr_err("%s: Core handle NULL\n", __func__);
		/* still return 1- offline if core ptr null */
		return false;
	}
}

static struct snd_info_entry_ops wcd_cpe_state_proc_ops = {
	.read = wcd_cpe_state_read,
	.poll = wcd_cpe_state_poll,
};

static int wcd_cpe_check_new_image(struct wcd_cpe_core *core)
{
	int rc = 0;
	char temp_img_name[WCD_CPE_IMAGE_FNAME_MAX];

	if (!strcmp(core->fname, core->dyn_fname) &&
	    core->ssr_type != WCD_CPE_INITIALIZED) {
		dev_dbg(core->dev,
			"%s: Firmware unchanged, fname = %s, ssr_type 0x%x\n",
			__func__, core->fname, core->ssr_type);
		goto done;
	}

	/*
	 * Different firmware name requested,
	 * Re-load the instruction section
	 */
	strlcpy(temp_img_name, core->fname,
		WCD_CPE_IMAGE_FNAME_MAX);
	strlcpy(core->fname, core->dyn_fname,
		WCD_CPE_IMAGE_FNAME_MAX);

	rc = wcd_cpe_load_fw(core, ELF_FLAG_EXECUTE);
	if (rc) {
		dev_err(core->dev,
			"%s: Failed to dload new image %s, err = %d\n",
			__func__, core->fname, rc);
		/* If new image download failed, revert back to old image */
		strlcpy(core->fname, temp_img_name,
			WCD_CPE_IMAGE_FNAME_MAX);
		rc = wcd_cpe_load_fw(core, ELF_FLAG_EXECUTE);
		if (rc)
			dev_err(core->dev,
				"%s: Failed to re-dload image %s, err = %d\n",
				__func__, core->fname, rc);
	} else {
		dev_info(core->dev, "%s: fw changed to %s\n",
			 __func__, core->fname);
	}
done:
	return rc;
}

static int wcd_cpe_enable(struct wcd_cpe_core *core,
		bool enable)
{
	int ret = 0;

	if (enable) {
		/* Reset CPE first */
		ret = cpe_svc_reset(core->cpe_handle);
		if (IS_ERR_VALUE(ret)) {
			dev_err(core->dev,
				"%s: CPE Reset failed, error = %d\n",
				__func__, ret);
			goto done;
		}

		ret = wcd_cpe_setup_irqs(core);
		if (ret) {
			dev_err(core->dev,
				"%s: CPE IRQs setup failed, error = %d\n",
				__func__, ret);
			goto done;
		}
		ret = wcd_cpe_check_new_image(core);
		if (ret)
			goto fail_boot;

		/* Dload data section */
		ret = wcd_cpe_load_fw(core, ELF_FLAG_RW);
		if (ret) {
			dev_err(core->dev,
				"%s: Failed to dload data section, err = %d\n",
				__func__, ret);
			goto fail_boot;
		}

		ret = wcd_cpe_enable_cpe_clks(core, true);
		if (IS_ERR_VALUE(ret)) {
			dev_err(core->dev,
				"%s: CPE clk enable failed, err = %d\n",
				__func__, ret);
			goto fail_boot;
		}

		ret = cpe_svc_boot(core->cpe_handle,
				   core->cpe_debug_mode);
		if (IS_ERR_VALUE(ret)) {
			dev_err(core->dev,
				"%s: Failed to boot CPE\n",
				__func__);
			goto fail_boot;
		}

		/* wait for CPE to be online */
		dev_dbg(core->dev,
			"%s: waiting for CPE bootup\n",
			__func__);

		wait_for_completion(&core->online_compl);

		dev_dbg(core->dev,
			"%s: CPE bootup done\n",
			__func__);

		core->ssr_type = WCD_CPE_ENABLED;
	} else {
		if (core->ssr_type == WCD_CPE_BUS_DOWN_EVENT ||
		    core->ssr_type == WCD_CPE_SSR_EVENT) {
			/*
			 * If this disable vote is when
			 * SSR is in progress, do not disable CPE here,
			 * instead SSR handler will control CPE.
			 */
			wcd_cpe_enable_cpe_clks(core, false);
			wcd_cpe_cleanup_irqs(core);
			goto done;
		}

		ret = cpe_svc_shutdown(core->cpe_handle);
		if (IS_ERR_VALUE(ret)) {
			dev_err(core->dev,
				"%s: CPE shutdown failed, error %d\n",
				__func__, ret);
			goto done;
		}

		wcd_cpe_enable_cpe_clks(core, false);
		wcd_cpe_cleanup_irqs(core);
		core->ssr_type = WCD_CPE_IMEM_DOWNLOADED;
	}

	return ret;

fail_boot:
	wcd_cpe_cleanup_irqs(core);

done:
	return ret;
}

/*
 * wcd_cpe_boot_ssr: Load the images to CPE after ssr and bootup cpe
 * @core: handle to the core
 */
static int wcd_cpe_boot_ssr(struct wcd_cpe_core *core)
{
	int rc = 0;

	if (!core || !core->cpe_handle) {
		pr_err("%s: Invalid handle\n", __func__);
		rc = -EINVAL;
		goto fail;
	}
	/* Load the instruction section and mark CPE as online */
	rc = wcd_cpe_load_fw(core, ELF_FLAG_EXECUTE);
	if (rc) {
		dev_err(core->dev,
			"%s: Failed to load instruction, err = %d\n",
			__func__, rc);
		goto fail;
	} else {
		wcd_cpe_change_online_state(core, 1);
	}

fail:
	return rc;
}

/*
 * wcd_cpe_clr_ready_status:
 *	Clear the value from the ready status for CPE
 * @core: handle to the core
 * @value: flag/bitmask that is to be cleared
 *
 * This function should not be invoked with ssr_lock acquired
 */
static void wcd_cpe_clr_ready_status(struct wcd_cpe_core *core,
			       u8 value)
{
	WCD_CPE_GRAB_LOCK(&core->ssr_lock, "SSR");
	core->ready_status &= ~(value);
	dev_dbg(core->dev,
		"%s: ready_status = 0x%x\n",
		__func__, core->ready_status);
	WCD_CPE_REL_LOCK(&core->ssr_lock, "SSR");
}

/*
 * wcd_cpe_set_and_complete:
 *	Set the ready status with the provided value and
 *	flag the completion object if ready status moves
 *	to ready to download
 * @core: handle to the core
 * @value: flag/bitmask that is to be set
 */
static void wcd_cpe_set_and_complete(struct wcd_cpe_core *core,
				u8 value)
{
	WCD_CPE_GRAB_LOCK(&core->ssr_lock, "SSR");
	core->ready_status |= value;
	if ((core->ready_status & WCD_CPE_READY_TO_DLOAD) ==
	    WCD_CPE_READY_TO_DLOAD) {
		dev_dbg(core->dev,
			"%s: marking ready, status = 0x%x\n",
			__func__, core->ready_status);
		complete(&core->ready_compl);
	}
	WCD_CPE_REL_LOCK(&core->ssr_lock, "SSR");
}


/*
 * wcd_cpe_ssr_work: work function to handle CPE SSR
 * @work: work that is scheduled to perform CPE shutdown
 *	and restart
 */
static void wcd_cpe_ssr_work(struct work_struct *work)
{

	int rc = 0;
	u32 irq = 0;
	struct wcd_cpe_core *core = NULL;
	u8 status = 0;

	core = container_of(work, struct wcd_cpe_core, ssr_work);
	if (!core) {
		pr_err("%s: Core handle NULL\n", __func__);
		return;
	}

	/* Obtain pm request up in case of suspend mode */
	pm_qos_add_request(&core->pm_qos_req,
			   PM_QOS_CPU_DMA_LATENCY,
			   PM_QOS_DEFAULT_VALUE);
	pm_qos_update_request(&core->pm_qos_req,
			msm_cpuidle_get_deep_idle_latency());

	dev_dbg(core->dev,
		"%s: CPE SSR with event %d\n",
		__func__, core->ssr_type);

	if (core->ssr_type == WCD_CPE_SSR_EVENT) {
		if (CPE_ERR_IRQ_CB(core))
			core->cpe_cdc_cb->cpe_err_irq_control(
					core->codec,
					CPE_ERR_IRQ_STATUS,
					&status);
		if (status & core->irq_info.cpe_fatal_irqs)
			irq = CPE_IRQ_WDOG_BITE;
	} else {
		/* If bus is down, cdc reg cannot be read */
		irq = CPE_IRQ_WDOG_BITE;
	}

	if (core->cpe_users > 0) {
		rc = cpe_svc_process_irq(core->cpe_handle, irq);
		if (IS_ERR_VALUE(rc))
			/*
			 * Even if process_irq fails,
			 * wait for cpe to move to offline state
			 */
			dev_err(core->dev,
				"%s: irq processing failed, error = %d\n",
				__func__, rc);

		rc = wait_for_completion_timeout(&core->offline_compl,
						 CPE_OFFLINE_WAIT_TIMEOUT);
		if (!rc) {
			dev_err(core->dev,
				"%s: wait for cpe offline timed out\n",
				__func__);
			goto err_ret;
		}
		if (core->ssr_type != WCD_CPE_BUS_DOWN_EVENT) {
			wcd_cpe_get_sfr_dump(core);

			/*
			 * Ramdump has to be explicitly enabled
			 * through debugfs and cannot be collected
			 * when bus is down.
			 */
			if (ramdump_enable)
				wcd_cpe_collect_ramdump(core);
		}
	} else {
		pr_err("%s: no cpe users, mark as offline\n", __func__);
		wcd_cpe_change_online_state(core, 0);
		wcd_cpe_set_and_complete(core,
					 WCD_CPE_BLK_READY);
	}

	rc = wait_for_completion_timeout(&core->ready_compl,
					 CPE_READY_WAIT_TIMEOUT);
	if (!rc) {
		dev_err(core->dev,
			"%s: ready to online timed out, status = %u\n",
			__func__, core->ready_status);
		goto err_ret;
	}

	rc = wcd_cpe_boot_ssr(core);

	/* Once image are downloaded make sure all
	 * error interrupts are cleared
	 */
	if (CPE_ERR_IRQ_CB(core))
		core->cpe_cdc_cb->cpe_err_irq_control(core->codec,
					CPE_ERR_IRQ_CLEAR, NULL);

err_ret:
	/* remove after default pm qos */
	pm_qos_update_request(&core->pm_qos_req,
			      PM_QOS_DEFAULT_VALUE);
	pm_qos_remove_request(&core->pm_qos_req);
}

/*
 * wcd_cpe_ssr_handle: handle SSR events here.
 * @core_handle: handle to the cpe core
 * @event: indicates ADSP or CDSP SSR.
 */
int wcd_cpe_ssr_event(void *core_handle,
		      enum wcd_cpe_ssr_state_event event)
{
	struct wcd_cpe_core *core = core_handle;

	if (!core) {
		pr_err("%s: Invalid handle to core\n",
			__func__);
		return -EINVAL;
	}

	/*
	 * If CPE is not even enabled, the SSR event for
	 * CPE needs to be ignored
	 */
	if (core->ssr_type == WCD_CPE_INITIALIZED) {
		dev_info(core->dev,
			"%s: CPE initialized but not enabled, skip CPE ssr\n",
			 __func__);
		return 0;
	}

	dev_dbg(core->dev,
		"%s: Schedule ssr work, event = %d\n",
		__func__, core->ssr_type);

	switch (event) {
	case WCD_CPE_BUS_DOWN_EVENT:
		/*
		 * If bus down, then CPE block is also
		 * treated to be down
		 */
		wcd_cpe_clr_ready_status(core, WCD_CPE_READY_TO_DLOAD);
		core->ssr_type = event;
		schedule_work(&core->ssr_work);
		break;

	case WCD_CPE_SSR_EVENT:
		wcd_cpe_clr_ready_status(core, WCD_CPE_BLK_READY);
		core->ssr_type = event;
		schedule_work(&core->ssr_work);
		break;

	case WCD_CPE_BUS_UP_EVENT:
		wcd_cpe_set_and_complete(core, WCD_CPE_BUS_READY);
		/*
		 * In case of bus up event ssr_type will be changed
		 * to WCD_CPE_ACTIVE once CPE is online
		 */
		break;

	default:
		dev_err(core->dev,
			"%s: unhandled SSR event %d\n",
			__func__, event);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(wcd_cpe_ssr_event);

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

	if (!core || !CPE_ERR_IRQ_CB(core)) {
		pr_err("%s: Invalid %s\n",
		       __func__,
		       (!core) ? "core" : "cdc control");
		return IRQ_HANDLED;
	}

	core->cpe_cdc_cb->cpe_err_irq_control(core->codec,
			CPE_ERR_IRQ_STATUS, &status);

	while (status != 0) {
		if (status & core->irq_info.cpe_fatal_irqs) {
			dev_err(core->dev,
				"%s: CPE SSR event,err_status = 0x%02x\n",
				__func__, status);
			wcd_cpe_ssr_event(core, WCD_CPE_SSR_EVENT);
			/*
			 * If fatal interrupt is received,
			 * trigger SSR and stop processing
			 * further interrupts
			 */
			break;
		}
		/*
		 * Mask the interrupt that was raised to
		 * avoid spurious interrupts
		 */
		core->cpe_cdc_cb->cpe_err_irq_control(core->codec,
					CPE_ERR_IRQ_MASK, &status);

		/* Clear only the interrupt that was raised */
		core->cpe_cdc_cb->cpe_err_irq_control(core->codec,
					CPE_ERR_IRQ_CLEAR, &status);
		dev_err(core->dev,
			"%s: err_interrupt status = 0x%x\n",
			__func__, status);

		/* Read status for pending interrupts */
		core->cpe_cdc_cb->cpe_err_irq_control(core->codec,
					CPE_ERR_IRQ_STATUS, &status);
	}

	return IRQ_HANDLED;
}

/*
 * wcd_cpe_cmi_afe_cb: callback called on response to afe commands
 * @param: parameter containing the response code, etc
 *
 * Process the request to the command sent to CPE and wakeup the
 * command send wait.
 */
static void wcd_cpe_cmi_afe_cb(const struct cmi_api_notification *param)
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
	int i;

	for (i = 0; i <= WCD_CPE_AFE_MAX_PORTS; i++) {
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
	int i;

	for (i = 0; i <= WCD_CPE_AFE_MAX_PORTS; i++) {
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
	struct cpe_svc_boot_event *boot_data;
	bool active_sessions;

	if (!param) {
		pr_err("%s: Invalid event\n", __func__);
		return;
	}

	codec = param->private_data;
	if (!codec) {
		pr_err("%s: Invalid handle to codec\n",
			__func__);
		return;
	}

	core = wcd_cpe_get_core_handle(codec);
	if (!core) {
		pr_err("%s: Invalid handle to core\n",
			__func__);
		return;
	}

	dev_dbg(core->dev,
		"%s: event = 0x%x, ssr_type = 0x%x\n",
		__func__, param->event, core->ssr_type);

	switch (param->event) {
	case CPE_SVC_BOOT:
		boot_data = (struct cpe_svc_boot_event *)
				param->payload;
		core->sfr_buf_addr = boot_data->debug_address;
		core->sfr_buf_size = boot_data->debug_buffer_size;
		dev_dbg(core->dev,
			"%s: CPE booted, sfr_addr = %d, sfr_size = %zu\n",
			__func__, core->sfr_buf_addr,
			core->sfr_buf_size);
		break;
	case CPE_SVC_ONLINE:
		core->ssr_type = WCD_CPE_ACTIVE;
		dev_dbg(core->dev, "%s CPE is now online\n",
			 __func__);
		complete(&core->online_compl);
		break;
	case CPE_SVC_OFFLINE:
		/*
		 * offline can happen during normal shutdown,
		 * but we are interested in offline only during
		 * SSR.
		 */
		if (core->ssr_type != WCD_CPE_SSR_EVENT &&
		    core->ssr_type != WCD_CPE_BUS_DOWN_EVENT)
			break;

		active_sessions = wcd_cpe_lsm_session_active();
		wcd_cpe_change_online_state(core, 0);
		complete(&core->offline_compl);
		dev_err(core->dev, "%s: CPE is now offline\n",
			 __func__);
		break;
	case CPE_SVC_CMI_CLIENTS_DEREG:

		/*
		 * Only when either CPE SSR is in progress,
		 * or the bus is down, we need to mark the CPE
		 * as ready. In all other cases, this event is
		 * ignored
		 */
		if (core->ssr_type == WCD_CPE_SSR_EVENT ||
		    core->ssr_type == WCD_CPE_BUS_DOWN_EVENT)
			wcd_cpe_set_and_complete(core,
						 WCD_CPE_BLK_READY);
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
 * wcd_cpe_cleanup_irqs: free the irq resources required by cpe
 * @core: handle the cpe core
 *
 * This API will free the IRQs for CPE but does not mask the
 * CPE interrupts. If masking is needed, it has to be done
 * explicity by caller.
 */
static void wcd_cpe_cleanup_irqs(struct wcd_cpe_core *core)
{

	struct snd_soc_codec *codec = core->codec;
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &wcd9xxx->core_res;

	wcd9xxx_free_irq(core_res,
			 core->irq_info.cpe_engine_irq,
			 core);
	wcd9xxx_free_irq(core_res,
			 core->irq_info.cpe_err_irq,
			 core);

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
	struct wcd9xxx *wcd9xxx = dev_get_drvdata(codec->dev->parent);
	struct wcd9xxx_core_resource *core_res = &wcd9xxx->core_res;

	ret = wcd9xxx_request_irq(core_res,
				  core->irq_info.cpe_engine_irq,
				  svass_engine_irq, "SVASS_Engine", core);
	if (ret) {
		dev_err(core->dev,
			"%s: Failed to request svass engine irq\n",
			__func__);
		goto fail_engine_irq;
	}

	/* Make sure all error interrupts are cleared */
	if (CPE_ERR_IRQ_CB(core))
		core->cpe_cdc_cb->cpe_err_irq_control(
					core->codec,
					CPE_ERR_IRQ_CLEAR,
					NULL);

	/* Enable required error interrupts */
	if (CPE_ERR_IRQ_CB(core))
		core->cpe_cdc_cb->cpe_err_irq_control(
					core->codec,
					CPE_ERR_IRQ_UNMASK,
					NULL);

	ret = wcd9xxx_request_irq(core_res,
				  core->irq_info.cpe_err_irq,
				  svass_exception_irq, "SVASS_Exception", core);
	if (ret) {
		dev_err(core->dev,
			"%s: Failed to request svass err irq\n",
			__func__);
		goto fail_exception_irq;
	}

	return 0;

fail_exception_irq:
	wcd9xxx_free_irq(core_res,
			 core->irq_info.cpe_engine_irq, core);

fail_engine_irq:
	return ret;
}

static int wcd_cpe_get_cal_index(int32_t cal_type)
{
	int cal_index = -EINVAL;

	if (cal_type == ULP_AFE_CAL_TYPE)
		cal_index = WCD_CPE_LSM_CAL_AFE;
	else if (cal_type == ULP_LSM_CAL_TYPE)
		cal_index = WCD_CPE_LSM_CAL_LSM;
	else if (cal_type == ULP_LSM_TOPOLOGY_ID_CAL_TYPE)
		cal_index = WCD_CPE_LSM_CAL_TOPOLOGY_ID;
	else
		pr_err("%s: invalid cal_type %d\n",
			__func__, cal_type);

	return cal_index;
}

static int wcd_cpe_alloc_cal(int32_t cal_type, size_t data_size, void *data)
{
	int ret = 0;
	int cal_index;

	cal_index = wcd_cpe_get_cal_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: invalid caltype %d\n",
			__func__, cal_type);
		return -EINVAL;
	}

	ret = cal_utils_alloc_cal(data_size, data,
				  core_d->cal_data[cal_index],
				  0, NULL);
	if (ret < 0)
		pr_err("%s: cal_utils_alloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
	return ret;
}

static int wcd_cpe_dealloc_cal(int32_t cal_type, size_t data_size,
			   void *data)
{
	int ret = 0;
	int cal_index;

	cal_index = wcd_cpe_get_cal_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: invalid caltype %d\n",
			__func__, cal_type);
		return -EINVAL;
	}

	ret = cal_utils_dealloc_cal(data_size, data,
				    core_d->cal_data[cal_index]);
	if (ret < 0)
		pr_err("%s: cal_utils_dealloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
	return ret;
}

static int wcd_cpe_set_cal(int32_t cal_type, size_t data_size, void *data)
{
	int ret = 0;
	int cal_index;

	cal_index = wcd_cpe_get_cal_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: invalid caltype %d\n",
			__func__, cal_type);
		return -EINVAL;
	}

	ret = cal_utils_set_cal(data_size, data,
				core_d->cal_data[cal_index],
				0, NULL);
	if (ret < 0)
		pr_err("%s: cal_utils_set_cal failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
	return ret;
}

static int wcd_cpe_cal_init(struct wcd_cpe_core *core)
{
	int ret = 0;

	struct cal_type_info cal_type_info[] = {
		{{ULP_AFE_CAL_TYPE,
		 {wcd_cpe_alloc_cal, wcd_cpe_dealloc_cal, NULL,
		  wcd_cpe_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{ULP_LSM_CAL_TYPE,
		 {wcd_cpe_alloc_cal, wcd_cpe_dealloc_cal, NULL,
		  wcd_cpe_set_cal, NULL, NULL} },
		 {NULL, NULL, cal_utils_match_buf_num} },

		{{ULP_LSM_TOPOLOGY_ID_CAL_TYPE,
		 {wcd_cpe_alloc_cal, wcd_cpe_dealloc_cal, NULL,
		  wcd_cpe_set_cal, NULL, NULL} },
		 {NULL, NULL, cal_utils_match_buf_num} },
	};

	ret = cal_utils_create_cal_types(WCD_CPE_LSM_CAL_MAX,
					 core->cal_data,
					 cal_type_info);
	if (ret < 0)
		pr_err("%s: could not create cal type!\n",
		       __func__);
	return ret;
}

/*
 * wcd_cpe_enable: setup the cpe interrupts and schedule
 *	the work to download image and bootup the CPE.
 * core: handle to cpe core structure
 */
static int wcd_cpe_vote(struct wcd_cpe_core *core,
		bool enable)
{
	int ret = 0;

	if (!core) {
		pr_err("%s: Invalid handle to core\n",
			__func__);
		ret = -EINVAL;
		goto done;
	}

	dev_dbg(core->dev,
		"%s: enter, enable = %s, cpe_users = %u\n",
		__func__, (enable ? "true" : "false"),
		core->cpe_users);

	if (enable) {
		core->cpe_users++;
		if (core->cpe_users == 1) {
			ret = wcd_cpe_enable(core, enable);
			if (ret) {
				dev_err(core->dev,
					"%s: CPE enable failed, err = %d\n",
					__func__, ret);
				goto done;
			}
		} else {
			dev_dbg(core->dev,
				"%s: cpe already enabled, users = %u\n",
				__func__, core->cpe_users);
			goto done;
		}
	} else {
		core->cpe_users--;
		if (core->cpe_users == 0) {
			ret = wcd_cpe_enable(core, enable);
			if (ret) {
				dev_err(core->dev,
					"%s: CPE disable failed, err = %d\n",
					__func__, ret);
				goto done;
			}
		} else {
			dev_dbg(core->dev,
				"%s: %u valid users on cpe\n",
				__func__, core->cpe_users);
			goto done;
		}
	}

	dev_dbg(core->dev,
		"%s: leave, enable = %s, cpe_users = %u\n",
		__func__, (enable ? "true" : "false"),
		core->cpe_users);

done:
	return ret;
}

static int wcd_cpe_debugfs_init(struct wcd_cpe_core *core)
{
	int rc = 0;

	struct dentry *dir = debugfs_create_dir("wcd_cpe", NULL);
	if (IS_ERR_OR_NULL(dir)) {
		dir = NULL;
		rc = -ENODEV;
		goto err_create_dir;
	}

	if (!debugfs_create_u32("ramdump_enable", S_IRUGO | S_IWUSR,
				dir, &ramdump_enable)) {
		dev_err(core->dev, "%s: Failed to create debugfs node %s\n",
			__func__, "ramdump_enable");
		rc = -ENODEV;
		goto err_create_entry;
	}

	if (!debugfs_create_file("cpe_ftm_test_trigger", S_IWUSR,
				dir, core, &cpe_ftm_test_trigger_fops)) {
		dev_err(core->dev, "%s: Failed to create debugfs node %s\n",
			__func__, "cpe_ftm_test_trigger");
		rc = -ENODEV;
		goto err_create_entry;
	}

	if (!debugfs_create_u32("cpe_ftm_test_status", S_IRUGO,
				dir, &cpe_ftm_test_status)) {
		dev_err(core->dev, "%s: Failed to create debugfs node %s\n",
			__func__, "cpe_ftm_test_status");
		rc = -ENODEV;
		goto err_create_entry;
	}

err_create_entry:
	debugfs_remove(dir);

err_create_dir:
	return rc;
}

static ssize_t fw_name_show(struct wcd_cpe_core *core, char *buf)
{
	return snprintf(buf, WCD_CPE_IMAGE_FNAME_MAX, "%s",
			core->dyn_fname);
}

static ssize_t fw_name_store(struct wcd_cpe_core *core,
		const char *buf, ssize_t count)
{
	int copy_count = count;
	const char *pos;

	pos = memchr(buf, '\n', count);
	if (pos)
		copy_count = pos - buf;

	if (copy_count > WCD_CPE_IMAGE_FNAME_MAX) {
		dev_err(core->dev,
			"%s: Invalid length %d, max allowed %d\n",
			__func__, copy_count, WCD_CPE_IMAGE_FNAME_MAX);
		return -EINVAL;
	}

	strlcpy(core->dyn_fname, buf, copy_count + 1);

	return count;
}

WCD_CPE_ATTR(fw_name, 0660, fw_name_show, fw_name_store);

static ssize_t wcd_cpe_sysfs_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct wcd_cpe_attribute *cpe_attr = to_wcd_cpe_attr(attr);
	struct wcd_cpe_core *core = kobj_to_cpe_core(kobj);
	ssize_t ret = -EINVAL;

	if (core && cpe_attr->show)
		ret = cpe_attr->show(core, buf);

	return ret;
}

static ssize_t wcd_cpe_sysfs_store(struct kobject *kobj,
		struct attribute *attr, const char *buf,
		size_t count)
{
	struct wcd_cpe_attribute *cpe_attr = to_wcd_cpe_attr(attr);
	struct wcd_cpe_core *core = kobj_to_cpe_core(kobj);
	ssize_t ret = -EINVAL;

	if (core && cpe_attr->store)
		ret = cpe_attr->store(core, buf, count);

	return ret;
}

static const struct sysfs_ops wcd_cpe_sysfs_ops = {
	.show = wcd_cpe_sysfs_show,
	.store = wcd_cpe_sysfs_store,
};

static struct kobj_type wcd_cpe_ktype = {
	.sysfs_ops = &wcd_cpe_sysfs_ops,
};

static int wcd_cpe_sysfs_init(struct wcd_cpe_core *core, int id)
{
	char sysfs_dir_name[WCD_CPE_SYSFS_DIR_MAX_LENGTH];
	int rc = 0;

	snprintf(sysfs_dir_name, WCD_CPE_SYSFS_DIR_MAX_LENGTH,
		 "%s%d", "wcd_cpe", id);

	rc = kobject_init_and_add(&core->cpe_kobj, &wcd_cpe_ktype,
				  kernel_kobj,
				  sysfs_dir_name);
	if (unlikely(rc)) {
		dev_err(core->dev,
			"%s: Failed to add kobject %s, err = %d\n",
			__func__, sysfs_dir_name, rc);
		goto done;
	}

	rc = sysfs_create_file(&core->cpe_kobj, &cpe_attr_fw_name.attr);
	if (rc) {
		dev_err(core->dev,
			"%s: Failed to fw_name sysfs entry to %s\n",
			__func__, sysfs_dir_name);
		goto fail_create_file;
	}

	return 0;

fail_create_file:
	kobject_put(&core->cpe_kobj);
done:
	return rc;
}

static ssize_t cpe_ftm_test_trigger(struct file *file,
				     const char __user *user_buf,
				     size_t count, loff_t *ppos)
{
	struct wcd_cpe_core *core = file->private_data;
	int ret = 0;

	/* Enable the clks for cpe */
	ret = wcd_cpe_enable_cpe_clks(core, true);
	if (IS_ERR_VALUE(ret)) {
		dev_err(core->dev,
			"%s: CPE clk enable failed, err = %d\n",
			__func__, ret);
		goto done;
	}

	/* Get the CPE_STATUS */
	ret = cpe_svc_ftm_test(core->cpe_handle, &cpe_ftm_test_status);
	if (IS_ERR_VALUE(ret)) {
		dev_err(core->dev,
			"%s: CPE FTM test failed, err = %d\n",
			__func__, ret);
		if (ret == CPE_SVC_BUSY) {
			cpe_ftm_test_status = 1;
			ret = 0;
		}
	}

	/* Disable the clks for cpe */
	ret = wcd_cpe_enable_cpe_clks(core, false);
	if (IS_ERR_VALUE(ret)) {
		dev_err(core->dev,
			"%s: CPE clk disable failed, err = %d\n",
			__func__, ret);
	}

done:
	if (ret < 0)
		return ret;
	else
		return count;
}

static int wcd_cpe_validate_params(
	struct snd_soc_codec *codec,
	struct wcd_cpe_params *params)
{

	if (!codec) {
		pr_err("%s: Invalid codec\n", __func__);
		return -EINVAL;
	}

	if (!params) {
		dev_err(codec->dev,
			"%s: No params supplied for codec %s\n",
			__func__, codec->component.name);
		return -EINVAL;
	}

	if (!params->codec || !params->get_cpe_core ||
	    !params->cdc_cb) {
		dev_err(codec->dev,
			"%s: Invalid params for codec %s\n",
			__func__, codec->component.name);
		return -EINVAL;
	}

	return 0;
}

/*
 * wcd_cpe_init: Initialize CPE related structures
 * @img_fname: filename for firmware image
 * @codec: handle to codec requesting for image download
 * @params: parameter structure passed from caller
 *
 * This API will initialize the cpe core but will not
 * download the image or boot the cpe core.
 */
struct wcd_cpe_core *wcd_cpe_init(const char *img_fname,
	struct snd_soc_codec *codec,
	struct wcd_cpe_params *params)
{
	struct wcd_cpe_core *core;
	int ret = 0;
	struct snd_card *card = NULL;
	struct snd_info_entry *entry = NULL;
	char proc_name[WCD_CPE_STATE_MAX_LEN];
	const char *cpe_name = "cpe";
	const char *state_name = "_state";
	const struct cpe_svc_hw_cfg *hw_info;
	int id = 0;

	if (wcd_cpe_validate_params(codec, params))
		return NULL;

	core = kzalloc(sizeof(struct wcd_cpe_core), GFP_KERNEL);
	if (!core) {
		dev_err(codec->dev,
			"%s: Failed to allocate cpe core data\n",
			__func__);
		return NULL;
	}

	snprintf(core->fname, sizeof(core->fname), "%s", img_fname);
	strlcpy(core->dyn_fname, core->fname, WCD_CPE_IMAGE_FNAME_MAX);

	wcd_get_cpe_core = params->get_cpe_core;

	core->codec = params->codec;
	core->dev = params->codec->dev;
	core->cpe_debug_mode = params->dbg_mode;

	core->cdc_info.major_version = params->cdc_major_ver;
	core->cdc_info.minor_version = params->cdc_minor_ver;
	core->cdc_info.id = params->cdc_id;

	core->cpe_cdc_cb = params->cdc_cb;

	memcpy(&core->irq_info, &params->cdc_irq_info,
	       sizeof(core->irq_info));

	INIT_WORK(&core->load_fw_work, wcd_cpe_load_fw_image);
	INIT_WORK(&core->ssr_work, wcd_cpe_ssr_work);
	init_completion(&core->offline_compl);
	init_completion(&core->ready_compl);
	init_completion(&core->online_compl);
	init_waitqueue_head(&core->ssr_entry.offline_poll_wait);
	mutex_init(&core->ssr_lock);
	core->cpe_users = 0;
	core->cpe_clk_ref = 0;

	/*
	 * By default, during probe, it is assumed that
	 * both CPE hardware block and underlying bus to codec
	 * are ready
	 */
	core->ready_status = WCD_CPE_READY_TO_DLOAD;

	core->cpe_handle = cpe_svc_initialize(NULL, &core->cdc_info,
					      params->cpe_svc_params);
	if (!core->cpe_handle) {
		dev_err(core->dev,
			"%s: failed to initialize cpe services\n",
			__func__);
		goto fail_cpe_initialize;
	}

	core->cpe_reg_handle = cpe_svc_register(core->cpe_handle,
					wcd_cpe_svc_event_cb,
					CPE_SVC_ONLINE | CPE_SVC_OFFLINE |
					CPE_SVC_BOOT |
					CPE_SVC_CMI_CLIENTS_DEREG,
					"codec cpe handler");
	if (!core->cpe_reg_handle) {
		dev_err(core->dev,
			"%s: failed to register cpe service\n",
			__func__);
		goto fail_cpe_register;
	}

	card = codec->component.card->snd_card;
	snprintf(proc_name, (sizeof("cpe") + sizeof("_state") +
		 sizeof(id) - 2), "%s%d%s", cpe_name, id, state_name);
	entry = snd_info_create_card_entry(card, proc_name,
					   card->proc_root);
	if (entry) {
		core->ssr_entry.entry = entry;
		core->ssr_entry.offline = 1;
		entry->size = WCD_CPE_STATE_MAX_LEN;
		entry->content = SNDRV_INFO_CONTENT_DATA;
		entry->c.ops = &wcd_cpe_state_proc_ops;
		entry->private_data = core;
		ret = snd_info_register(entry);
		if (ret < 0) {
			dev_err(core->dev,
				"%s: snd_info_register failed (%d)\n",
				 __func__, ret);
			snd_info_free_entry(entry);
			entry = NULL;
		}
	} else {
		dev_err(core->dev,
			"%s: Failed to create CPE SSR status entry\n",
			__func__);
		/*
		 * Even if SSR entry creation fails, continue
		 * with image download
		 */
	}

	core_d = core;
	ret = wcd_cpe_cal_init(core);
	if (IS_ERR_VALUE(ret)) {
		dev_err(core->dev,
			"%s: CPE calibration init failed, err = %d\n",
			__func__, ret);
		goto fail_cpe_reset;
	}

	wcd_cpe_debugfs_init(core);

	wcd_cpe_sysfs_init(core, id);

	hw_info = cpe_svc_get_hw_cfg(core->cpe_handle);
	if (!hw_info) {
		dev_err(core->dev,
			"%s: hw info not available\n",
			__func__);
		goto schedule_dload_work;
	} else {
		core->hw_info.dram_offset = hw_info->DRAM_offset;
		core->hw_info.dram_size = hw_info->DRAM_size;
		core->hw_info.iram_offset = hw_info->IRAM_offset;
		core->hw_info.iram_size = hw_info->IRAM_size;
	}

	/* Setup the ramdump device and buffer */
	core->cpe_ramdump_dev = create_ramdump_device("cpe",
						      core->dev);
	if (!core->cpe_ramdump_dev) {
		dev_err(core->dev,
			"%s: Failed to create ramdump device\n",
			__func__);
		goto schedule_dload_work;
	}

	arch_setup_dma_ops(core->dev, 0, 0, NULL, 0);
	core->cpe_dump_v_addr = dma_alloc_coherent(core->dev,
						   core->hw_info.dram_size,
						   &core->cpe_dump_addr,
						   GFP_KERNEL);
	if (!core->cpe_dump_v_addr) {
		dev_err(core->dev,
			"%s: Failed to alloc memory for cpe dump, size = %zd\n",
			__func__, core->hw_info.dram_size);
		goto schedule_dload_work;
	} else {
		memset(core->cpe_dump_v_addr, 0, core->hw_info.dram_size);
	}

schedule_dload_work:
	core->ssr_type = WCD_CPE_INITIALIZED;
	schedule_work(&core->load_fw_work);
	return core;

fail_cpe_reset:
	cpe_svc_deregister(core->cpe_handle, core->cpe_reg_handle);

fail_cpe_register:
	cpe_svc_deinitialize(core->cpe_handle);

fail_cpe_initialize:
	kfree(core);
	return NULL;
}
EXPORT_SYMBOL(wcd_cpe_init);

/*
 * wcd_cpe_cmi_lsm_callback: callback called from cpe services
 *			     to notify command response for lsm
 *			     service
 * @param: param containing the response code and status
 *
 * This callback is registered with cpe services while registering
 * the LSM service
 */
static void wcd_cpe_cmi_lsm_callback(const struct cmi_api_notification *param)
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
		lsm_session->cmd_err_code = result;
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
 * @core: handle to cpe core
 * @session: session on which to send the message
 * @message: actual message containing header and payload
 *
 * Sends message to lsm service for specified session and wait
 * for response back on the message.
 * should be called after acquiring session specific mutex
 */
static int wcd_cpe_cmi_send_lsm_msg(
			struct wcd_cpe_core *core,
			struct cpe_lsm_session *session,
			void *message)
{
	int ret = 0;
	struct cmi_hdr *hdr = message;

	pr_debug("%s: sending message with opcode 0x%x\n",
		 __func__, hdr->opcode);

	if (unlikely(!wcd_cpe_is_online_state(core))) {
		dev_err(core->dev,
			"%s: MSG not sent, CPE offline\n",
			 __func__);
		goto done;
	}

	if (CMI_HDR_GET_OBM_FLAG(hdr))
		wcd_cpe_bus_vote_max_bw(core, true);

	reinit_completion(&session->cmd_comp);
	ret = cmi_send_msg(message);
	if (ret) {
		pr_err("%s: msg opcode (0x%x) send failed (%d)\n",
			__func__, hdr->opcode, ret);
		goto rel_bus_vote;
	}

	ret = wait_for_completion_timeout(&session->cmd_comp,
					  CMI_CMD_TIMEOUT);
	if (ret > 0) {
		pr_debug("%s: command 0x%x, received response 0x%x\n",
			__func__, hdr->opcode, session->cmd_err_code);
		if (session->cmd_err_code == CMI_SHMEM_ALLOC_FAILED)
			session->cmd_err_code = CPE_ENOMEMORY;
		if (session->cmd_err_code > 0)
			pr_err("%s: CPE returned error[%s]\n",
				__func__, cpe_err_get_err_str(
				session->cmd_err_code));
		ret = cpe_err_get_lnx_err_code(session->cmd_err_code);
		goto rel_bus_vote;
	} else {
		pr_err("%s: command (0x%x) send timed out\n",
			__func__, hdr->opcode);
		ret = -ETIMEDOUT;
		goto rel_bus_vote;
	}


rel_bus_vote:

	if (CMI_HDR_GET_OBM_FLAG(hdr))
		wcd_cpe_bus_vote_max_bw(core, false);

done:
	return ret;
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

/*
 * fill_lsm_cmd_header_v0_inband:
 *	Given the header, fill the header with information
 *	for lsm service, version 0 and inband message
 * @hdr: the cmi header to be filled.
 * @session_id: ID for the lsm session
 * @payload_size: size for cmi message payload
 * @opcode: opcode for cmi message
 */
static int fill_lsm_cmd_header_v0_inband(struct cmi_hdr *hdr,
		u8 session_id, u8 payload_size, u16 opcode)
{
	return fill_cmi_header(hdr, session_id,
			       CMI_CPE_LSM_SERVICE_ID, false,
			       payload_size, opcode, false);
}

/*
 * wcd_cpe_is_valid_lsm_session:
 *	Check session paramters to identify validity for the sesion
 * @core: handle to cpe core
 * @session: handle to the lsm session
 * @func: invoking function to be printed in error logs
 */
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

static int wcd_cpe_cmd_lsm_open_tx_v2(
	struct wcd_cpe_core *core,
	struct cpe_lsm_session *session)
{
	struct cpe_lsm_cmd_open_tx_v2 cmd_open_tx_v2;
	struct cal_block_data *top_cal = NULL;
	struct audio_cal_info_lsm_top *lsm_top;
	int ret = 0;

	ret = wcd_cpe_is_valid_lsm_session(core, session,
					   __func__);
	if (ret)
		return ret;

	if (core->cal_data[WCD_CPE_LSM_CAL_TOPOLOGY_ID] == NULL) {
		dev_err(core->dev,
			"%s: LSM_TOPOLOGY cal not allocated!\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&core->cal_data[WCD_CPE_LSM_CAL_TOPOLOGY_ID]->lock);
	top_cal = cal_utils_get_only_cal_block(
			core->cal_data[WCD_CPE_LSM_CAL_TOPOLOGY_ID]);
	if (!top_cal) {
		dev_err(core->dev,
			"%s: Failed to get LSM TOPOLOGY cal block\n",
			__func__);
		ret = -EINVAL;
		goto unlock_cal_mutex;
	}

	lsm_top = (struct audio_cal_info_lsm_top *)
			top_cal->cal_info;

	if (!lsm_top) {
		dev_err(core->dev,
			"%s: cal_info for LSM_TOPOLOGY not found\n",
			__func__);
		ret = -EINVAL;
		goto unlock_cal_mutex;
	}

	dev_dbg(core->dev,
		"%s: topology_id = 0x%x, acdb_id = 0x%x, app_type = 0x%x\n",
		__func__, lsm_top->topology, lsm_top->acdb_id,
		lsm_top->app_type);

	if (lsm_top->topology == 0) {
		dev_err(core->dev,
			"%s: topology id not sent for app_type 0x%x\n",
			__func__, lsm_top->app_type);
		ret = -EINVAL;
		goto unlock_cal_mutex;
	}

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

	memset(&cmd_open_tx_v2, 0, sizeof(struct cpe_lsm_cmd_open_tx_v2));
	if (fill_lsm_cmd_header_v0_inband(&cmd_open_tx_v2.hdr,
				session->id, OPEN_V2_CMD_PAYLOAD_SIZE,
				CPE_LSM_SESSION_CMD_OPEN_TX_V2)) {
		ret = -EINVAL;
		goto end_ret;
	}

	cmd_open_tx_v2.topology_id = lsm_top->topology;
	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cmd_open_tx_v2);
	if (ret)
		dev_err(core->dev,
			"%s: failed to send open_tx_v2 cmd, err = %d\n",
			__func__, ret);
	else
		session->is_topology_used = true;
end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");

unlock_cal_mutex:
	mutex_unlock(&core->cal_data[WCD_CPE_LSM_CAL_TOPOLOGY_ID]->lock);
	return ret;
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

	/* Try to open with topology first */
	ret = wcd_cpe_cmd_lsm_open_tx_v2(core, session);
	if (!ret)
		goto done;

	dev_dbg(core->dev, "%s: Try open_tx without topology\n",
		__func__);

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

	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cmd_open_tx);
	if (ret)
		dev_err(core->dev,
			"%s: failed to send open_tx cmd, err = %d\n",
			__func__, ret);
end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
done:
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

	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cmd_close_tx);
	if (ret)
		dev_err(core->dev,
			"%s: lsm close_tx cmd failed, err = %d\n",
			__func__, ret);
	else
		session->is_topology_used = false;
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
	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cmd_shmem_alloc);
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
	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cmd_dealloc);
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
 * wcd_cpe_send_lsm_cal: send the calibration for lsm service
 *			      from acdb to the cpe
 * @core: handle to cpe core
 * @session: session for which the calibration needs to be set.
 */
static int wcd_cpe_send_lsm_cal(
			struct wcd_cpe_core *core,
			struct cpe_lsm_session *session)
{

	u8 *msg_pld;
	struct cmi_hdr *hdr;
	struct cal_block_data *lsm_cal = NULL;
	void *inb_msg;
	int rc = 0;

	if (core->cal_data[WCD_CPE_LSM_CAL_LSM] == NULL) {
		pr_err("%s: LSM cal not allocated!\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&core->cal_data[WCD_CPE_LSM_CAL_LSM]->lock);
	lsm_cal = cal_utils_get_only_cal_block(
			core->cal_data[WCD_CPE_LSM_CAL_LSM]);
	if (!lsm_cal) {
		pr_err("%s: failed to get lsm cal block\n", __func__);
		rc = -EINVAL;
		goto unlock_cal_mutex;
	}

	if (lsm_cal->cal_data.size == 0) {
		dev_dbg(core->dev, "%s: No LSM cal to send\n",
			__func__);
		rc = 0;
		goto unlock_cal_mutex;
	}

	inb_msg = kzalloc(sizeof(struct cmi_hdr) + lsm_cal->cal_data.size,
			  GFP_KERNEL);
	if (!inb_msg) {
		pr_err("%s: no memory for lsm acdb cal\n",
			__func__);
		rc = -ENOMEM;
		goto unlock_cal_mutex;
	}

	hdr = (struct cmi_hdr *) inb_msg;

	rc = fill_lsm_cmd_header_v0_inband(hdr, session->id,
			lsm_cal->cal_data.size,
			CPE_LSM_SESSION_CMD_SET_PARAMS);
	if (rc) {
		pr_err("%s: invalid params for header, err = %d\n",
			__func__, rc);
		goto free_msg;
	}

	msg_pld = ((u8 *) inb_msg) + sizeof(struct cmi_hdr);
	memcpy(msg_pld, lsm_cal->cal_data.kvaddr,
	       lsm_cal->cal_data.size);

	rc = wcd_cpe_cmi_send_lsm_msg(core, session, inb_msg);
	if (rc)
		pr_err("%s: acdb lsm_params send failed, err = %d\n",
			__func__, rc);

free_msg:
	kfree(inb_msg);

unlock_cal_mutex:
	mutex_unlock(&core->cal_data[WCD_CPE_LSM_CAL_LSM]->lock);
	return rc;

}

static void wcd_cpe_set_param_data(struct cpe_param_data *param_d,
		struct cpe_lsm_ids *ids, u32 p_size,
		u32 set_param_cmd)
{
	param_d->module_id = ids->module_id;
	param_d->param_id = ids->param_id;

	switch (set_param_cmd) {
	case CPE_LSM_SESSION_CMD_SET_PARAMS_V2:
		param_d->p_size.param_size = p_size;
		break;
	case CPE_LSM_SESSION_CMD_SET_PARAMS:
	default:
		param_d->p_size.sr.param_size =
			(u16) p_size;
		param_d->p_size.sr.reserved = 0;
		break;
	}
}

static int wcd_cpe_send_param_epd_thres(struct wcd_cpe_core *core,
		struct cpe_lsm_session *session,
		void *data, struct cpe_lsm_ids *ids)
{
	struct snd_lsm_ep_det_thres *ep_det_data;
	struct cpe_lsm_param_epd_thres epd_cmd;
	struct cmi_hdr *msg_hdr = &epd_cmd.hdr;
	struct cpe_param_data *param_d =
				&epd_cmd.param;
	int rc;

	memset(&epd_cmd, 0, sizeof(epd_cmd));
	ep_det_data = (struct snd_lsm_ep_det_thres *) data;
	if (fill_lsm_cmd_header_v0_inband(msg_hdr,
				session->id,
				CPE_CMD_EPD_THRES_PLD_SIZE,
				CPE_LSM_SESSION_CMD_SET_PARAMS_V2)) {
		rc = -EINVAL;
		goto err_ret;
	}

	wcd_cpe_set_param_data(param_d, ids,
			       CPE_EPD_THRES_PARAM_SIZE,
			       CPE_LSM_SESSION_CMD_SET_PARAMS_V2);

	epd_cmd.minor_version = 1;
	epd_cmd.epd_begin = ep_det_data->epd_begin;
	epd_cmd.epd_end = ep_det_data->epd_end;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	rc = wcd_cpe_cmi_send_lsm_msg(core, session, &epd_cmd);
	if (unlikely(rc))
		dev_err(core->dev,
			"%s: set_param(EPD Threshold) failed, rc %dn",
			__func__, rc);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
err_ret:
	return rc;
}

static int wcd_cpe_send_param_opmode(struct wcd_cpe_core *core,
		struct cpe_lsm_session *session,
		void *data, struct cpe_lsm_ids *ids)
{
	struct snd_lsm_detect_mode *opmode_d;
	struct cpe_lsm_param_opmode opmode_cmd;
	struct cmi_hdr *msg_hdr = &opmode_cmd.hdr;
	struct cpe_param_data *param_d =
				&opmode_cmd.param;
	int rc;

	memset(&opmode_cmd, 0, sizeof(opmode_cmd));
	opmode_d = (struct snd_lsm_detect_mode *) data;
	if (fill_lsm_cmd_header_v0_inband(msg_hdr,
				session->id,
				CPE_CMD_OPMODE_PLD_SIZE,
				CPE_LSM_SESSION_CMD_SET_PARAMS_V2)) {
		rc = -EINVAL;
		goto err_ret;
	}

	wcd_cpe_set_param_data(param_d, ids,
			       CPE_OPMODE_PARAM_SIZE,
			       CPE_LSM_SESSION_CMD_SET_PARAMS_V2);

	opmode_cmd.minor_version = 1;
	if (opmode_d->mode == LSM_MODE_KEYWORD_ONLY_DETECTION)
		opmode_cmd.mode = 1;
	else
		opmode_cmd.mode = 3;

	if (opmode_d->detect_failure)
		opmode_cmd.mode |= 0x04;

	opmode_cmd.reserved = 0;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	rc = wcd_cpe_cmi_send_lsm_msg(core, session, &opmode_cmd);
	if (unlikely(rc))
		dev_err(core->dev,
			"%s: set_param(operation_mode) failed, rc %dn",
			__func__, rc);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
err_ret:
	return rc;
}

static int wcd_cpe_send_param_gain(struct wcd_cpe_core *core,
		struct cpe_lsm_session *session,
		void *data, struct cpe_lsm_ids *ids)
{
	struct snd_lsm_gain *gain_d;
	struct cpe_lsm_param_gain gain_cmd;
	struct cmi_hdr *msg_hdr = &gain_cmd.hdr;
	struct cpe_param_data *param_d =
				&gain_cmd.param;
	int rc;

	memset(&gain_cmd, 0, sizeof(gain_cmd));
	gain_d = (struct snd_lsm_gain *) data;
	if (fill_lsm_cmd_header_v0_inband(msg_hdr,
				session->id,
				CPE_CMD_GAIN_PLD_SIZE,
				CPE_LSM_SESSION_CMD_SET_PARAMS_V2)) {
		rc = -EINVAL;
		goto err_ret;
	}

	wcd_cpe_set_param_data(param_d, ids,
			       CPE_GAIN_PARAM_SIZE,
			       CPE_LSM_SESSION_CMD_SET_PARAMS_V2);

	gain_cmd.minor_version = 1;
	gain_cmd.gain = gain_d->gain;
	gain_cmd.reserved = 0;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	rc = wcd_cpe_cmi_send_lsm_msg(core, session, &gain_cmd);
	if (unlikely(rc))
		dev_err(core->dev,
			"%s: set_param(lsm_gain) failed, rc %dn",
			__func__, rc);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
err_ret:
	return rc;
}

static int wcd_cpe_send_param_connectport(struct wcd_cpe_core *core,
		struct cpe_lsm_session *session,
		void *data, struct cpe_lsm_ids *ids, u16 port_id)
{
	struct cpe_lsm_param_connectport con_port_cmd;
	struct cmi_hdr *msg_hdr = &con_port_cmd.hdr;
	struct cpe_param_data *param_d =
				&con_port_cmd.param;
	int rc;

	memset(&con_port_cmd, 0, sizeof(con_port_cmd));
	if (fill_lsm_cmd_header_v0_inband(msg_hdr,
				session->id,
				CPE_CMD_CONNECTPORT_PLD_SIZE,
				CPE_LSM_SESSION_CMD_SET_PARAMS_V2)) {
		rc = -EINVAL;
		goto err_ret;
	}

	wcd_cpe_set_param_data(param_d, ids,
			       CPE_CONNECTPORT_PARAM_SIZE,
			       CPE_LSM_SESSION_CMD_SET_PARAMS_V2);

	con_port_cmd.minor_version = 1;
	con_port_cmd.afe_port_id = port_id;
	con_port_cmd.reserved = 0;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	rc = wcd_cpe_cmi_send_lsm_msg(core, session, &con_port_cmd);
	if (unlikely(rc))
		dev_err(core->dev,
			"%s: set_param(connect_port) failed, rc %dn",
			__func__, rc);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
err_ret:
	return rc;
}

static int wcd_cpe_send_param_conf_levels(
		struct wcd_cpe_core *core,
		struct cpe_lsm_session *session,
		struct cpe_lsm_ids *ids)
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
			CPE_LSM_SESSION_CMD_SET_PARAMS_V2, false);

	wcd_cpe_set_param_data(param_d, ids,
			       pld_size - sizeof(struct cpe_param_data),
			       CPE_LSM_SESSION_CMD_SET_PARAMS_V2);

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

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	ret = wcd_cpe_cmi_send_lsm_msg(core, session, message);
	if (ret)
		pr_err("%s: lsm_set_conf_levels failed, err = %d\n",
			__func__, ret);
	kfree(message);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}

static int wcd_cpe_send_param_snd_model(struct wcd_cpe_core *core,
	struct cpe_lsm_session *session, struct cpe_lsm_ids *ids)
{
	int ret = 0;
	struct cmi_obm_msg obm_msg;
	struct cpe_param_data *param_d;


	ret = fill_cmi_header(&obm_msg.hdr, session->id,
			CMI_CPE_LSM_SERVICE_ID, 0, 20,
			CPE_LSM_SESSION_CMD_SET_PARAMS_V2, true);
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

	param_d = (struct cpe_param_data *) session->snd_model_data;
	wcd_cpe_set_param_data(param_d, ids,
			(session->snd_model_size - sizeof(*param_d)),
			CPE_LSM_SESSION_CMD_SET_PARAMS_V2);

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &obm_msg);
	if (ret)
		dev_err(core->dev,
			"%s: snd_model_register failed, %d\n",
			__func__, ret);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");

err_ret:
	return ret;
}

static int wcd_cpe_send_param_dereg_model(
	struct wcd_cpe_core *core,
	struct cpe_lsm_session *session,
	struct cpe_lsm_ids *ids)
{
	struct cmi_hdr *hdr;
	struct cpe_param_data *param_d;
	u8 *message;
	u32 pld_size;
	int rc = 0;

	pld_size = sizeof(*hdr) + sizeof(*param_d);

	message = kzalloc(pld_size, GFP_KERNEL);
	if (!message)
		return -ENOMEM;

	hdr = (struct cmi_hdr *) message;
	param_d = (struct cpe_param_data *)
			(((u8 *) message) + sizeof(*hdr));

	if (fill_lsm_cmd_header_v0_inband(hdr,
				session->id,
				sizeof(*param_d),
				CPE_LSM_SESSION_CMD_SET_PARAMS_V2)) {
		rc = -EINVAL;
		goto err_ret;
	}
	wcd_cpe_set_param_data(param_d, ids, 0,
			       CPE_LSM_SESSION_CMD_SET_PARAMS_V2);
	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	rc = wcd_cpe_cmi_send_lsm_msg(core, session, message);
	if (rc)
		dev_err(core->dev,
			"%s: snd_model_deregister failed, %d\n",
			__func__, rc);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
err_ret:
	kfree(message);
	return rc;
}

static int wcd_cpe_send_custom_param(
	struct wcd_cpe_core *core,
	struct cpe_lsm_session *session,
	void *data, u32 msg_size)
{
	u8 *msg;
	struct cmi_hdr *hdr;
	u8 *msg_pld;
	int rc;

	if (msg_size > CMI_INBAND_MESSAGE_SIZE) {
		dev_err(core->dev,
			"%s: out of band custom params not supported\n",
			__func__);
		return -EINVAL;
	}

	msg = kzalloc(sizeof(*hdr) + msg_size, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = (struct cmi_hdr *) msg;
	msg_pld = msg + sizeof(struct cmi_hdr);

	if (fill_lsm_cmd_header_v0_inband(hdr,
				session->id,
				msg_size,
				CPE_LSM_SESSION_CMD_SET_PARAMS_V2)) {
		rc = -EINVAL;
		goto err_ret;
	}

	memcpy(msg_pld, data, msg_size);
	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	rc = wcd_cpe_cmi_send_lsm_msg(core, session, msg);
	if (rc)
		dev_err(core->dev,
			"%s: custom params send failed, err = %d\n",
			 __func__, rc);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
err_ret:
	kfree(msg);
	return rc;
}

static int wcd_cpe_set_one_param(void *core_handle,
	struct cpe_lsm_session *session, struct lsm_params_info *p_info,
	void *data, enum LSM_PARAM_TYPE param_type)
{
	struct wcd_cpe_core *core = core_handle;
	int rc = 0;
	struct cpe_lsm_ids ids;

	memset(&ids, 0, sizeof(ids));
	ids.module_id = p_info->module_id;
	ids.param_id = p_info->param_id;

	switch (param_type) {
	case LSM_ENDPOINT_DETECT_THRESHOLD:
		rc = wcd_cpe_send_param_epd_thres(core, session,
						data, &ids);
		break;
	case LSM_OPERATION_MODE: {
		struct cpe_lsm_ids connectport_ids;

		rc = wcd_cpe_send_param_opmode(core, session,
					data, &ids);
		if (rc)
			break;

		connectport_ids.module_id = LSM_MODULE_ID_FRAMEWORK;
		connectport_ids.param_id = LSM_PARAM_ID_CONNECT_TO_PORT;

		rc = wcd_cpe_send_param_connectport(core, session, NULL,
				       &connectport_ids, CPE_AFE_PORT_1_TX);
		if (rc)
			dev_err(core->dev,
				"%s: send_param_connectport failed, err %d\n",
				__func__, rc);
		break;
	}
	case LSM_GAIN:
		rc = wcd_cpe_send_param_gain(core, session, data, &ids);
		break;
	case LSM_MIN_CONFIDENCE_LEVELS:
		rc = wcd_cpe_send_param_conf_levels(core, session, &ids);
		break;
	case LSM_REG_SND_MODEL:
		rc = wcd_cpe_send_param_snd_model(core, session, &ids);
		break;
	case LSM_DEREG_SND_MODEL:
		rc = wcd_cpe_send_param_dereg_model(core, session, &ids);
		break;
	case LSM_CUSTOM_PARAMS:
		rc = wcd_cpe_send_custom_param(core, session,
					       data, p_info->param_size);
		break;
	default:
		pr_err("%s: wrong param_type 0x%x\n",
			__func__, p_info->param_type);
	}

	if (rc)
		dev_err(core->dev,
			"%s: send_param(%d) failed, err %d\n",
			 __func__, p_info->param_type, rc);
	return rc;
}

/*
 * wcd_cpe_lsm_set_params: set the parameters for lsm service
 * @core: handle to cpe core
 * @session: session for which the parameters are to be set
 * @detect_mode: mode for detection
 * @detect_failure: flag indicating failure detection enabled/disabled
 *
 */
static int wcd_cpe_lsm_set_params(struct wcd_cpe_core *core,
	struct cpe_lsm_session *session,
	enum lsm_detection_mode detect_mode, bool detect_failure)
{
	struct cpe_lsm_ids ids;
	struct snd_lsm_detect_mode det_mode;

	int ret = 0;

	/* Send lsm calibration */
	ret = wcd_cpe_send_lsm_cal(core, session);
	if (ret) {
		pr_err("%s: fail to sent acdb cal, err = %d",
			__func__, ret);
		goto err_ret;
	}

	/* Send operation mode */
	ids.module_id = CPE_LSM_MODULE_ID_VOICE_WAKEUP;
	ids.param_id = CPE_LSM_PARAM_ID_OPERATION_MODE;
	det_mode.mode = detect_mode;
	det_mode.detect_failure = detect_failure;
	ret = wcd_cpe_send_param_opmode(core, session,
					&det_mode, &ids);
	if (ret)
		dev_err(core->dev,
			"%s: Failed to set opmode, err=%d\n",
			__func__, ret);

err_ret:
	return ret;
}

static int wcd_cpe_lsm_set_data(void *core_handle,
				struct cpe_lsm_session *session,
				enum lsm_detection_mode detect_mode,
				bool detect_failure)
{
	struct wcd_cpe_core *core = core_handle;
	struct cpe_lsm_ids ids;
	int ret = 0;

	if (session->num_confidence_levels > 0) {
		ret = wcd_cpe_lsm_set_params(core, session, detect_mode,
				       detect_failure);
		if (ret) {
			dev_err(core->dev,
				"%s: lsm set params failed, rc = %d\n",
				__func__, ret);
			goto err_ret;
		}

		ids.module_id = CPE_LSM_MODULE_ID_VOICE_WAKEUP;
		ids.param_id = CPE_LSM_PARAM_ID_MIN_CONFIDENCE_LEVELS;
		ret = wcd_cpe_send_param_conf_levels(core, session, &ids);
		if (ret) {
			dev_err(core->dev,
				"%s: lsm confidence levels failed, rc = %d\n",
				__func__, ret);
			goto err_ret;
		}
	} else {
		dev_dbg(core->dev,
			"%s: no conf levels to set\n",
			__func__);
	}

err_ret:
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

	ret = wcd_cpe_lsm_set_data(core_handle, session,
				   detect_mode, detect_failure);
	if (ret) {
		dev_err(core->dev,
			"%s: fail to set lsm data, err = %d\n",
			__func__, ret);
		return ret;
	}

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

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

	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &obm_msg);
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

	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cmd_dereg_snd_model);
	if (ret)
		dev_err(core->dev,
			"%s: failed to send dereg_snd_model cmd\n",
			__func__);
end_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}

/*
 * wcd_cpe_lsm_get_afe_out_port_id: get afe output port id
 * @core_handle: handle to the CPE core
 * @session: session for which port id needs to get
 */
static int wcd_cpe_lsm_get_afe_out_port_id(void *core_handle,
					   struct cpe_lsm_session *session)
{
	struct wcd_cpe_core *core = core_handle;
	struct snd_soc_codec *codec;
	int rc = 0;

	if (!core || !core->codec) {
		pr_err("%s: Invalid handle to %s\n",
			__func__,
			(!core) ? "core" : "codec");
		rc = -EINVAL;
		goto done;
	}

	if (!session) {
		dev_err(core->dev, "%s: Invalid session\n",
			__func__);
		rc = -EINVAL;
		goto done;
	}

	if (!core->cpe_cdc_cb ||
		!core->cpe_cdc_cb->get_afe_out_port_id) {
		session->afe_out_port_id = WCD_CPE_AFE_OUT_PORT_2;
		dev_dbg(core->dev,
			"%s: callback not defined, default port_id = %d\n",
			__func__, session->afe_out_port_id);
		goto done;
	}

	codec = core->codec;
	rc = core->cpe_cdc_cb->get_afe_out_port_id(codec,
						   &session->afe_out_port_id);
	if (rc) {
		dev_err(core->dev,
			"%s: failed to get port id, err = %d\n",
			__func__, rc);
		goto done;
	}
	dev_dbg(core->dev, "%s: port_id: %d\n", __func__,
		session->afe_out_port_id);

done:
	return rc;
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

	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cmd_lsm_start);
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

	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cmd_lsm_stop);
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
	void *core_handle, void *client_data,
	void (*event_cb) (void *, u8, u8, u8 *))
{
	struct cpe_lsm_session *session;
	int i, session_id = -1;
	struct wcd_cpe_core *core = core_handle;
	bool afe_register_service = false;
	int ret = 0;

	/*
	 * Even if multiple listen sessions can be
	 * allocated, the AFE service registration
	 * should be done only once as CPE can only
	 * have one instance of AFE service.
	 *
	 * If this is the first session to be allocated,
	 * only then register the afe service.
	 */
	if (!wcd_cpe_lsm_session_active())
		afe_register_service = true;

	for (i = 1; i <= WCD_CPE_LSM_MAX_SESSIONS; i++) {
		if (!lsm_sessions[i]) {
			session_id = i;
			break;
		}
	}

	if (session_id < 0) {
		dev_err(core->dev,
			"%s: max allowed sessions already allocated\n",
			__func__);
		return NULL;
	}

	ret = wcd_cpe_vote(core, true);
	if (ret) {
		dev_err(core->dev,
			"%s: Failed to enable cpe, err = %d\n",
			__func__, ret);
		return NULL;
	}

	session = kzalloc(sizeof(struct cpe_lsm_session), GFP_KERNEL);
	if (!session) {
		dev_err(core->dev,
			"%s: failed to allocate session, no memory\n",
			__func__);
		goto err_session_alloc;
	}

	session->id = session_id;
	session->event_cb = event_cb;
	session->cmi_reg_handle = cmi_register(wcd_cpe_cmi_lsm_callback,
						CMI_CPE_LSM_SERVICE_ID);
	if (!session->cmi_reg_handle) {
		dev_err(core->dev,
			"%s: Failed to register LSM service with CMI\n",
			__func__);
		goto err_ret;
	}
	session->priv_d = client_data;
	mutex_init(&session->lsm_lock);
	if (afe_register_service) {
		/* Register for AFE Service */
		core->cmi_afe_handle = cmi_register(wcd_cpe_cmi_afe_cb,
						CMI_CPE_AFE_SERVICE_ID);
		wcd_cpe_initialize_afe_port_data();
		if (!core->cmi_afe_handle) {
			dev_err(core->dev,
				"%s: Failed to register AFE service with CMI\n",
				__func__);
			goto err_afe_svc_reg;
		}

		/* Once AFE service is registered, send the mode command */
		ret = wcd_cpe_afe_svc_cmd_mode(core,
				AFE_SVC_EXPLICIT_PORT_START);
		if (ret)
			goto err_afe_mode_cmd;
	}

	session->lsm_mem_handle = 0;
	init_completion(&session->cmd_comp);

	lsm_sessions[session_id] = session;
	return session;

err_afe_mode_cmd:
	cmi_deregister(core->cmi_afe_handle);

err_afe_svc_reg:
	cmi_deregister(session->cmi_reg_handle);
	mutex_destroy(&session->lsm_lock);

err_ret:
	kfree(session);

err_session_alloc:
	wcd_cpe_vote(core, false);
	return NULL;
}

/*
 * wcd_cpe_lsm_config_lab_latency: send lab latency value
 * @core: handle to wcd_cpe_core
 * @session: lsm session
 * @latency: the value of latency for lab setup in msec
 */
static int wcd_cpe_lsm_config_lab_latency(
		struct wcd_cpe_core *core,
		struct cpe_lsm_session *session,
		u32 latency)
{
	int ret = 0, pld_size = CPE_PARAM_LSM_LAB_LATENCY_SIZE;
	struct cpe_lsm_lab_latency_config cpe_lab_latency;
	struct cpe_lsm_lab_config *lab_lat = &cpe_lab_latency.latency_cfg;
	struct cpe_param_data *param_d = &lab_lat->param;
	struct cpe_lsm_ids ids;

	if (fill_lsm_cmd_header_v0_inband(&cpe_lab_latency.hdr, session->id,
		(u8) pld_size, CPE_LSM_SESSION_CMD_SET_PARAMS_V2)) {
		pr_err("%s: Failed to create header\n", __func__);
		return -EINVAL;
	}
	if (latency == 0x00 || latency > WCD_CPE_LAB_MAX_LATENCY) {
		pr_err("%s: Invalid latency %u\n",
			__func__, latency);
		return -EINVAL;
	} else {
		lab_lat->latency = latency;
	}

	lab_lat->minor_ver = 1;
	ids.module_id = CPE_LSM_MODULE_ID_LAB;
	ids.param_id = CPE_LSM_PARAM_ID_LAB_CONFIG;
	wcd_cpe_set_param_data(param_d, &ids,
			       PARAM_SIZE_LSM_LATENCY_SIZE,
			       CPE_LSM_SESSION_CMD_SET_PARAMS_V2);

	pr_debug("%s: Module 0x%x Param 0x%x size %zu pld_size 0x%x\n",
		  __func__, lab_lat->param.module_id,
		 lab_lat->param.param_id, PARAM_SIZE_LSM_LATENCY_SIZE,
		 pld_size);

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cpe_lab_latency);
	if (ret != 0)
		pr_err("%s: lsm_set_params failed, error = %d\n",
		       __func__, ret);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
	return ret;
}

/*
 * wcd_cpe_lsm_lab_control: enable/disable lab
 * @core: handle to wcd_cpe_core
 * @session: lsm session
 * @enable: Indicates whether to enable / disable lab
 */
static int wcd_cpe_lsm_lab_control(
		void *core_handle,
		struct cpe_lsm_session *session,
		bool enable)
{
	struct wcd_cpe_core *core = core_handle;
	int ret = 0, pld_size = CPE_PARAM_SIZE_LSM_LAB_CONTROL;
	struct cpe_lsm_control_lab cpe_lab_enable;
	struct cpe_lsm_lab_enable *lab_enable = &cpe_lab_enable.lab_enable;
	struct cpe_param_data *param_d = &lab_enable->param;
	struct cpe_lsm_ids ids;

	pr_debug("%s: enter payload_size = %d Enable %d\n",
		 __func__, pld_size, enable);

	if (fill_lsm_cmd_header_v0_inband(&cpe_lab_enable.hdr, session->id,
		(u8) pld_size, CPE_LSM_SESSION_CMD_SET_PARAMS_V2)) {
		return -EINVAL;
	}
	if (enable == true)
		lab_enable->enable = 1;
	else
		lab_enable->enable = 0;

	ids.module_id = CPE_LSM_MODULE_ID_LAB;
	ids.param_id = CPE_LSM_PARAM_ID_LAB_ENABLE;
	wcd_cpe_set_param_data(param_d, &ids,
			PARAM_SIZE_LSM_CONTROL_SIZE,
			CPE_LSM_SESSION_CMD_SET_PARAMS_V2);

	pr_debug("%s: Module 0x%x, Param 0x%x size %zu pld_size 0x%x\n",
		 __func__, lab_enable->param.module_id,
		 lab_enable->param.param_id, PARAM_SIZE_LSM_CONTROL_SIZE,
		 pld_size);

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &cpe_lab_enable);
	if (ret != 0) {
		pr_err("%s: lsm_set_params failed, error = %d\n",
			__func__, ret);
		WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
		goto done;
	}
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");

	if (lab_enable->enable)
		ret = wcd_cpe_lsm_config_lab_latency(core, session,
					       WCD_CPE_LAB_MAX_LATENCY);
done:
	return ret;
}

/*
 * wcd_cpe_lsm_eob: stop lab
 * @core: handle to wcd_cpe_core
 * @session: lsm session to be deallocated
 */
static int wcd_cpe_lsm_eob(
			struct wcd_cpe_core *core,
			struct cpe_lsm_session *session)
{
	int ret = 0;
	struct cmi_hdr lab_eob;

	if (fill_lsm_cmd_header_v0_inband(&lab_eob, session->id,
		0, CPE_LSM_SESSION_CMD_EOB)) {
		return -EINVAL;
	}

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &lab_eob);
	if (ret != 0)
		pr_err("%s: lsm_set_params failed\n", __func__);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");

	return ret;
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
	int ret = 0;

	if (!session) {
		dev_err(core->dev,
			"%s: Invalid lsm session\n", __func__);
		return -EINVAL;
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

	cmi_deregister(session->cmi_reg_handle);
	mutex_destroy(&session->lsm_lock);
	lsm_sessions[session->id] = NULL;
	kfree(session);

	if (!wcd_cpe_lsm_session_active()) {
		cmi_deregister(core->cmi_afe_handle);
		core->cmi_afe_handle = NULL;
		wcd_cpe_deinitialize_afe_port_data();
	}

	ret = wcd_cpe_vote(core, false);
	if (ret)
		dev_dbg(core->dev,
			"%s: Failed to un-vote cpe, err = %d\n",
			__func__, ret);

	return ret;
}

static int wcd_cpe_lab_ch_setup(void *core_handle,
		struct cpe_lsm_session *session,
		enum wcd_cpe_event event)
{
	struct wcd_cpe_core *core = core_handle;
	struct snd_soc_codec *codec;
	int rc = 0;
	u8 cpe_intr_bits;

	if (!core || !core->codec) {
		pr_err("%s: Invalid handle to %s\n",
			__func__,
			(!core) ? "core" : "codec");
		rc = EINVAL;
		goto done;
	}

	if (!core->cpe_cdc_cb ||
	    !core->cpe_cdc_cb->cdc_ext_clk ||
	    !core->cpe_cdc_cb->lab_cdc_ch_ctl) {
		dev_err(core->dev,
			"%s: Invalid codec callbacks\n",
			__func__);
		rc = -EINVAL;
		goto done;
	}

	codec = core->codec;
	dev_dbg(core->dev,
		"%s: event = 0x%x\n",
		__func__, event);

	switch (event) {
	case WCD_CPE_PRE_ENABLE:
		rc = core->cpe_cdc_cb->cdc_ext_clk(codec, true, false);
		if (rc) {
			dev_err(core->dev,
				"%s: failed to enable cdc clk, err = %d\n",
				__func__, rc);
			goto done;
		}

		rc = core->cpe_cdc_cb->lab_cdc_ch_ctl(codec,
						      true);
		if (rc) {
			dev_err(core->dev,
				"%s: failed to enable cdc port, err = %d\n",
				__func__, rc);
			rc = core->cpe_cdc_cb->cdc_ext_clk(codec, false, false);
			goto done;
		}

		break;

	case WCD_CPE_POST_ENABLE:
		rc = cpe_svc_toggle_lab(core->cpe_handle, true);
		if (rc)
			dev_err(core->dev,
			"%s: Failed to enable lab\n", __func__);
		break;

	case WCD_CPE_PRE_DISABLE:
		/*
		 * Mask the non-fatal interrupts in CPE as they will
		 * be generated during lab teardown and may flood.
		 */
		cpe_intr_bits = ~(core->irq_info.cpe_fatal_irqs & 0xFF);
		if (CPE_ERR_IRQ_CB(core))
			core->cpe_cdc_cb->cpe_err_irq_control(
						core->codec,
						CPE_ERR_IRQ_MASK,
						&cpe_intr_bits);

		rc = core->cpe_cdc_cb->lab_cdc_ch_ctl(codec,
						      false);
		if (rc)
			dev_err(core->dev,
				"%s: failed to disable cdc port, err = %d\n",
				__func__, rc);
		break;

	case WCD_CPE_POST_DISABLE:
		rc = wcd_cpe_lsm_eob(core, session);
		if (rc)
			dev_err(core->dev,
				"%s: eob send failed, err = %d\n",
				__func__, rc);

		/* Continue teardown even if eob failed */
		rc = cpe_svc_toggle_lab(core->cpe_handle, false);
		if (rc)
			dev_err(core->dev,
			"%s: Failed to disable lab\n", __func__);

		/* Continue with disabling even if toggle lab fails */
		rc = core->cpe_cdc_cb->cdc_ext_clk(codec, false, false);
		if (rc)
			dev_err(core->dev,
				"%s: failed to disable cdc clk, err = %d\n",
				__func__, rc);

		/* Unmask non-fatal CPE interrupts */
		cpe_intr_bits = ~(core->irq_info.cpe_fatal_irqs & 0xFF);
		if (CPE_ERR_IRQ_CB(core))
			core->cpe_cdc_cb->cpe_err_irq_control(
						core->codec,
						CPE_ERR_IRQ_UNMASK,
						&cpe_intr_bits);
		break;

	default:
		dev_err(core->dev,
			"%s: Invalid event 0x%x\n",
			__func__, event);
		rc = -EINVAL;
		break;
	}

done:
	return rc;
}

static int wcd_cpe_lsm_set_fmt_cfg(void *core_handle,
			struct cpe_lsm_session *session)
{
	int ret;
	struct cpe_lsm_output_format_cfg out_fmt_cfg;
	struct wcd_cpe_core *core = core_handle;

	ret = wcd_cpe_is_valid_lsm_session(core, session, __func__);
	if (ret)
		goto done;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");

	memset(&out_fmt_cfg, 0, sizeof(out_fmt_cfg));
	if (fill_lsm_cmd_header_v0_inband(&out_fmt_cfg.hdr,
			session->id, OUT_FMT_CFG_CMD_PAYLOAD_SIZE,
			CPE_LSM_SESSION_CMD_TX_BUFF_OUTPUT_CONFIG)) {
		ret = -EINVAL;
		goto err_ret;
	}

	out_fmt_cfg.format = session->out_fmt_cfg.format;
	out_fmt_cfg.packing = session->out_fmt_cfg.pack_mode;
	out_fmt_cfg.data_path_events = session->out_fmt_cfg.data_path_events;

	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &out_fmt_cfg);
	if (ret)
		dev_err(core->dev,
			"%s: lsm_set_output_format_cfg failed, err = %d\n",
			__func__, ret);

err_ret:
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
done:
	return ret;
}

static void wcd_cpe_snd_model_offset(void *core_handle,
		struct cpe_lsm_session *session, size_t *offset)
{
	*offset = sizeof(struct cpe_param_data);
}

static int wcd_cpe_lsm_set_media_fmt_params(void *core_handle,
					  struct cpe_lsm_session *session,
					  struct lsm_hw_params *param)
{
	struct cpe_lsm_media_fmt_param media_fmt;
	struct cmi_hdr *msg_hdr = &media_fmt.hdr;
	struct wcd_cpe_core *core = core_handle;
	struct cpe_param_data *param_d = &media_fmt.param;
	struct cpe_lsm_ids ids;
	int ret;

	memset(&media_fmt, 0, sizeof(media_fmt));
	if (fill_lsm_cmd_header_v0_inband(msg_hdr,
				session->id,
				CPE_MEDIA_FMT_PLD_SIZE,
				CPE_LSM_SESSION_CMD_SET_PARAMS_V2)) {
		ret = -EINVAL;
		goto done;
	}

	memset(&ids, 0, sizeof(ids));
	ids.module_id = CPE_LSM_MODULE_FRAMEWORK;
	ids.param_id = CPE_LSM_PARAM_ID_MEDIA_FMT;

	wcd_cpe_set_param_data(param_d, &ids, CPE_MEDIA_FMT_PARAM_SIZE,
			       CPE_LSM_SESSION_CMD_SET_PARAMS_V2);

	media_fmt.minor_version = 1;
	media_fmt.sample_rate = param->sample_rate;
	media_fmt.num_channels = param->num_chs;
	media_fmt.bit_width = param->bit_width;

	WCD_CPE_GRAB_LOCK(&session->lsm_lock, "lsm");
	ret = wcd_cpe_cmi_send_lsm_msg(core, session, &media_fmt);
	if (ret)
		dev_err(core->dev,
			"%s: Set_param(media_format) failed, err=%d\n",
			__func__, ret);
	WCD_CPE_REL_LOCK(&session->lsm_lock, "lsm");
done:
	return ret;
}

static int wcd_cpe_lsm_set_port(void *core_handle,
				struct cpe_lsm_session *session, void *data)
{
	u32 port_id;
	int ret;
	struct cpe_lsm_ids ids;
	struct wcd_cpe_core *core = core_handle;

	ret = wcd_cpe_is_valid_lsm_session(core, session, __func__);
	if (ret)
		goto done;

	if (!data) {
		dev_err(core->dev, "%s: data is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	port_id = *(u32 *)data;
	dev_dbg(core->dev, "%s: port_id: %d\n", __func__, port_id);

	memset(&ids, 0, sizeof(ids));
	ids.module_id = LSM_MODULE_ID_FRAMEWORK;
	ids.param_id = LSM_PARAM_ID_CONNECT_TO_PORT;

	ret = wcd_cpe_send_param_connectport(core, session, NULL,
					     &ids, port_id);
	if (ret)
		dev_err(core->dev,
			"%s: send_param_connectport failed, err %d\n",
			__func__, ret);
done:
	return ret;
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
	lsm_ops->lsm_get_afe_out_port_id = wcd_cpe_lsm_get_afe_out_port_id;
	lsm_ops->lsm_start = wcd_cpe_cmd_lsm_start;
	lsm_ops->lsm_stop = wcd_cpe_cmd_lsm_stop;
	lsm_ops->lsm_lab_control = wcd_cpe_lsm_lab_control;
	lsm_ops->lab_ch_setup = wcd_cpe_lab_ch_setup;
	lsm_ops->lsm_set_data = wcd_cpe_lsm_set_data;
	lsm_ops->lsm_set_fmt_cfg = wcd_cpe_lsm_set_fmt_cfg;
	lsm_ops->lsm_set_one_param = wcd_cpe_set_one_param;
	lsm_ops->lsm_get_snd_model_offset = wcd_cpe_snd_model_offset;
	lsm_ops->lsm_set_media_fmt_params = wcd_cpe_lsm_set_media_fmt_params;
	lsm_ops->lsm_set_port = wcd_cpe_lsm_set_port;

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
 * @core: wcd cpe core handle
 * @port_cfg: configuration data for the afe port
 *	      for which this message is to be sent
 * @message: actual message with header and payload
 *
 * Port specific lock needs to be acquired before this
 * function can be invoked
 */
static int wcd_cpe_cmi_send_afe_msg(
	struct wcd_cpe_core *core,
	struct wcd_cmi_afe_port_data *port_d,
	void *message)
{
	int ret = 0;
	struct cmi_hdr *hdr = message;

	pr_debug("%s: sending message with opcode 0x%x\n",
		__func__, hdr->opcode);

	if (unlikely(!wcd_cpe_is_online_state(core))) {
		dev_err(core->dev, "%s: CPE offline\n", __func__);
		return 0;
	}

	if (CMI_HDR_GET_OBM_FLAG(hdr))
		wcd_cpe_bus_vote_max_bw(core, true);

	ret = cmi_send_msg(message);
	if (ret) {
		pr_err("%s: cmd 0x%x send failed, err = %d\n",
			__func__, hdr->opcode, ret);
		goto rel_bus_vote;
	}

	ret = wait_for_completion_timeout(&port_d->afe_cmd_complete,
					  CMI_CMD_TIMEOUT);
	if (ret > 0) {
		pr_debug("%s: command 0x%x, received response 0x%x\n",
			 __func__, hdr->opcode, port_d->cmd_result);
		if (port_d->cmd_result == CMI_SHMEM_ALLOC_FAILED)
			port_d->cmd_result = CPE_ENOMEMORY;
		if (port_d->cmd_result > 0)
			pr_err("%s: CPE returned error[%s]\n",
				__func__, cpe_err_get_err_str(
				port_d->cmd_result));
		ret = cpe_err_get_lnx_err_code(port_d->cmd_result);
		goto rel_bus_vote;
	} else {
		pr_err("%s: command 0x%x send timed out\n",
			__func__, hdr->opcode);
		ret = -ETIMEDOUT;
		goto rel_bus_vote;
	}

rel_bus_vote:
	reinit_completion(&port_d->afe_cmd_complete);

	if (CMI_HDR_GET_OBM_FLAG(hdr))
		wcd_cpe_bus_vote_max_bw(core, false);

	return ret;
}



/*
 * wcd_cpe_afe_shmem_alloc: allocate the cpe memory for afe service
 * @core: handle to cpe core
 * @port_cfg: configuration data for the port which needs
 *	      memory to be allocated on CPE
 * @size: size of the memory to be allocated
 */
static int wcd_cpe_afe_shmem_alloc(
	struct wcd_cpe_core *core,
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

	ret = wcd_cpe_cmi_send_afe_msg(core, port_d, &cmd_shmem_alloc);
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
 * @core: handle to cpe core
 * @port_d: configuration data for the port which needs
 *	      memory to be deallocated on CPE
 * The memory handle to be de-allocated is saved in the
 * port configuration data
 */
static int wcd_cpe_afe_shmem_dealloc(
	struct wcd_cpe_core *core,
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
	ret = wcd_cpe_cmi_send_afe_msg(core, port_d, &cmd_dealloc);
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
 * wcd_cpe_send_afe_cal: send the acdb calibration to AFE port
 * @core: handle to cpe core
 * @port_d: configuration data for the port for which the
 *	      calibration needs to be appplied
 */
static int wcd_cpe_send_afe_cal(void *core_handle,
		struct wcd_cmi_afe_port_data *port_d)
{

	struct cal_block_data *afe_cal = NULL;
	struct wcd_cpe_core *core = core_handle;
	struct cmi_obm_msg obm_msg;
	void *inb_msg = NULL;
	void *msg;
	int rc = 0;
	bool is_obm_msg;

	if (core->cal_data[WCD_CPE_LSM_CAL_AFE] == NULL) {
		pr_err("%s: LSM cal not allocated!\n",
			__func__);
		rc = -EINVAL;
		goto rel_cal_mutex;
	}

	mutex_lock(&core->cal_data[WCD_CPE_LSM_CAL_AFE]->lock);
	afe_cal = cal_utils_get_only_cal_block(
			core->cal_data[WCD_CPE_LSM_CAL_AFE]);
	if (!afe_cal) {
		pr_err("%s: failed to get afe cal block\n",
			__func__);
		rc = -EINVAL;
		goto rel_cal_mutex;
	}

	if (afe_cal->cal_data.size == 0) {
		dev_dbg(core->dev, "%s: No AFE cal to send\n",
			__func__);
		rc = 0;
		goto rel_cal_mutex;
	}

	is_obm_msg = (afe_cal->cal_data.size >
		      CMI_INBAND_MESSAGE_SIZE) ? true : false;

	if (is_obm_msg) {
		struct cmi_hdr *hdr = &(obm_msg.hdr);
		struct cmi_obm *pld = &(obm_msg.pld);

		rc = wcd_cpe_afe_shmem_alloc(core, port_d,
					afe_cal->cal_data.size);
		if (rc) {
			dev_err(core->dev,
				"%s: AFE shmem alloc fail %d\n",
				__func__, rc);
			goto rel_cal_mutex;
		}

		rc = fill_afe_cmd_header(hdr, port_d->port_id,
					 CPE_AFE_CMD_SET_PARAM,
					 CPE_AFE_PARAM_PAYLOAD_SIZE,
					 true);
		if (rc) {
			dev_err(core->dev,
				"%s: invalid params for header, err = %d\n",
				__func__, rc);
			wcd_cpe_afe_shmem_dealloc(core, port_d);
			goto rel_cal_mutex;
		}

		pld->version = 0;
		pld->size = afe_cal->cal_data.size;
		pld->data_ptr.kvaddr = afe_cal->cal_data.kvaddr;
		pld->mem_handle = port_d->mem_handle;
		msg = &obm_msg;

	} else {
		u8 *msg_pld;
		struct cmi_hdr *hdr;
		inb_msg = kzalloc(sizeof(struct cmi_hdr) +
					afe_cal->cal_data.size,
				  GFP_KERNEL);
		if (!inb_msg) {
			dev_err(core->dev,
				"%s: no memory for afe cal inband\n",
				__func__);
			rc = -ENOMEM;
			goto rel_cal_mutex;
		}

		hdr = (struct cmi_hdr *) inb_msg;

		rc = fill_afe_cmd_header(hdr, port_d->port_id,
					 CPE_AFE_CMD_SET_PARAM,
					 CPE_AFE_PARAM_PAYLOAD_SIZE,
					 false);
		if (rc) {
			dev_err(core->dev,
				"%s: invalid params for header, err = %d\n",
				__func__, rc);
			kfree(inb_msg);
			inb_msg = NULL;
			goto rel_cal_mutex;
		}

		msg_pld = ((u8 *) inb_msg) + sizeof(struct cmi_hdr);
		memcpy(msg_pld, afe_cal->cal_data.kvaddr,
		       afe_cal->cal_data.size);

		msg = inb_msg;
	}

	rc = wcd_cpe_cmi_send_afe_msg(core, port_d, msg);
	if (rc)
		pr_err("%s: afe cal for listen failed, rc = %d\n",
			__func__, rc);

	if (is_obm_msg) {
		wcd_cpe_afe_shmem_dealloc(core, port_d);
		port_d->mem_handle = 0;
	} else {
		kfree(inb_msg);
		inb_msg = NULL;
	}

rel_cal_mutex:
	mutex_unlock(&core->cal_data[WCD_CPE_LSM_CAL_AFE]->lock);
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

static int wcd_cpe_afe_svc_cmd_mode(void *core_handle,
				    u8 mode)
{
	struct cpe_afe_svc_cmd_mode afe_mode;
	struct wcd_cpe_core *core = core_handle;
	struct wcd_cmi_afe_port_data *afe_port_d;
	int ret;

	afe_port_d = &afe_ports[0];
	/*
	 * AFE SVC mode command is for the service and not port
	 * specific, hence use AFE port as 0 so the command will
	 * be applied to all AFE ports on CPE.
	 */
	afe_port_d->port_id = 0;

	WCD_CPE_GRAB_LOCK(&afe_port_d->afe_lock, "afe");
	memset(&afe_mode, 0, sizeof(afe_mode));
	if (fill_afe_cmd_header(&afe_mode.hdr, afe_port_d->port_id,
				CPE_AFE_SVC_CMD_LAB_MODE,
				CPE_AFE_CMD_MODE_PAYLOAD_SIZE,
				false)) {
		ret = -EINVAL;
		goto err_ret;
	}

	afe_mode.mode = mode;

	ret = wcd_cpe_cmi_send_afe_msg(core, afe_port_d, &afe_mode);
	if (ret)
		dev_err(core->dev,
			"%s: afe_svc_mode cmd failed, err = %d\n",
			__func__, ret);

err_ret:
	WCD_CPE_REL_LOCK(&afe_port_d->afe_lock, "afe");
	return ret;
}

static int wcd_cpe_afe_cmd_port_cfg(void *core_handle,
		struct wcd_cpe_afe_port_cfg *afe_cfg)
{
	struct cpe_afe_cmd_port_cfg port_cfg_cmd;
	struct wcd_cpe_core *core = core_handle;
	struct wcd_cmi_afe_port_data *afe_port_d;
	int ret;

	ret = wcd_cpe_is_valid_port(core, afe_cfg, __func__);
	if (ret)
		goto done;

	afe_port_d = &afe_ports[afe_cfg->port_id];
	afe_port_d->port_id = afe_cfg->port_id;

	WCD_CPE_GRAB_LOCK(&afe_port_d->afe_lock, "afe");
	memset(&port_cfg_cmd, 0, sizeof(port_cfg_cmd));
	if (fill_afe_cmd_header(&port_cfg_cmd.hdr,
			afe_cfg->port_id,
			CPE_AFE_PORT_CMD_GENERIC_CONFIG,
			CPE_AFE_CMD_PORT_CFG_PAYLOAD_SIZE,
			false)) {
		ret = -EINVAL;
		goto err_ret;
	}

	port_cfg_cmd.bit_width = afe_cfg->bit_width;
	port_cfg_cmd.num_channels = afe_cfg->num_channels;
	port_cfg_cmd.sample_rate = afe_cfg->sample_rate;

	if (afe_port_d->port_id == CPE_AFE_PORT_3_TX)
		port_cfg_cmd.buffer_size = WCD_CPE_EC_PP_BUF_SIZE;
	else
		port_cfg_cmd.buffer_size = AFE_OUT_BUF_SIZE(afe_cfg->bit_width,
							afe_cfg->sample_rate);

	ret = wcd_cpe_cmi_send_afe_msg(core, afe_port_d, &port_cfg_cmd);
	if (ret)
		dev_err(core->dev,
			"%s: afe_port_config failed, err = %d\n",
			__func__, ret);

err_ret:
	WCD_CPE_REL_LOCK(&afe_port_d->afe_lock, "afe");
done:
	return ret;
}

/*
 * wcd_cpe_afe_set_params: set the parameters for afe port
 * @afe_cfg: configuration data for the port for which the
 *	      parameters are to be set
 */
static int wcd_cpe_afe_set_params(void *core_handle,
		struct wcd_cpe_afe_port_cfg *afe_cfg, bool afe_mad_ctl)
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

	ret = wcd_cpe_send_afe_cal(core, afe_port_d);
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
	hw_mad_ctrl->param.p_size.sr.param_size = PARAM_SIZE_AFE_HW_MAD_CTRL;
	hw_mad_ctrl->param.p_size.sr.reserved = 0;
	hw_mad_ctrl->minor_version = 1;
	hw_mad_ctrl->mad_type = MAD_TYPE_AUDIO;
	hw_mad_ctrl->mad_enable = afe_mad_ctl;

	port_cfg->param.module_id = CPE_AFE_MODULE_AUDIO_DEV_INTERFACE;
	port_cfg->param.param_id = CPE_AFE_PARAM_ID_GENERIC_PORT_CONFIG;
	port_cfg->param.p_size.sr.param_size = PARAM_SIZE_AFE_PORT_CFG;
	port_cfg->param.p_size.sr.reserved = 0;
	port_cfg->minor_version = 1;
	port_cfg->bit_width = afe_cfg->bit_width;
	port_cfg->num_channels = afe_cfg->num_channels;
	port_cfg->sample_rate = afe_cfg->sample_rate;

	ret = wcd_cpe_cmi_send_afe_msg(core, afe_port_d, &afe_params);
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
	ret = wcd_cpe_cmi_send_afe_msg(core, afe_port_d, &hdr);
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
	ret = wcd_cpe_cmi_send_afe_msg(core, afe_port_d, &hdr);
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
	ret = wcd_cpe_cmi_send_afe_msg(core, afe_port_d, &hdr);
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
	ret = wcd_cpe_cmi_send_afe_msg(core, afe_port_d, &hdr);
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
	afe_ops->afe_port_cmd_cfg = wcd_cpe_afe_cmd_port_cfg;

	return 0;
}
EXPORT_SYMBOL(wcd_cpe_get_afe_ops);

MODULE_DESCRIPTION("WCD CPE Core");
MODULE_LICENSE("GPL v2");
