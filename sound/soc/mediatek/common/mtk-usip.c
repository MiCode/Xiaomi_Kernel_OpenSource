/* Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#include <linux/notifier.h>

#include <mt-plat/mtk_ccci_common.h>
#include <mt-plat/mtk_meminfo.h>
#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT
#include <adsp_helper.h>
#include "audio_messenger_ipi.h"
#include "audio_task.h"
#include "adsp_ipi.h"
#include "audio_speech_msg_id.h"
#include "mtk-dsp-common.h"
#endif

#define USIP_EMP_IOC_MAGIC 'D'
#define GET_USIP_EMI_SIZE _IOWR(USIP_EMP_IOC_MAGIC, 0xF0, unsigned long long)

#define NUM_MPU_REGION 3

int EMI_TABLE[3][3]
	= {{0, 0, 0x30000}, {1, 0x30000, 0x8000}, {2, 0x38000, 0x28000} };

enum {
	SP_EMI_AP_USIP_PARAMETER,
	SP_EMI_ADSP_USIP_PHONECALL,
	SP_EMI_ADSP_USIP_SMARTPA
};

enum {
	SP_EMI_TYPE,
	SP_EMI_OFFSET,
	SP_EMI_SIZE
};

struct usip_info {
	bool memory_ready;
	size_t memory_size;

	void *memory_area;
	dma_addr_t memory_addr;
	phys_addr_t addr_phy;
};

static struct usip_info usip;


static long usip_ioctl(struct file *fp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	long size_for_spe = 0;

	pr_debug("%s(), cmd 0x%x, arg %lu\n", __func__, cmd, arg);
	pr_debug("%s(), memory_size = %ld addr_phy = %lld\n", __func__,
	usip.memory_size, usip.addr_phy);

	size_for_spe = EMI_TABLE[SP_EMI_AP_USIP_PARAMETER][SP_EMI_SIZE];
	switch (cmd) {

	case GET_USIP_EMI_SIZE:
		if (copy_to_user((void __user *)arg, &size_for_spe,
			sizeof(size_for_spe))) {
			pr_warn("Fail copy to user Ptr:%p, r_sz:%zu",
			(char *)&size_for_spe, sizeof(size_for_spe));
			ret = -1;
		}
		break;

	default:
		pr_debug("%s(), default\n", __func__);
		break;
	}
	return ret;
}

#ifdef CONFIG_COMPAT
static long usip_compat_ioctl(struct file *fp, unsigned int cmd,
unsigned long arg)
{
	long ret;

	if (!fp->f_op || !fp->f_op->unlocked_ioctl)
		return -ENOTTY;
	ret = fp->f_op->unlocked_ioctl(fp, cmd, arg);
	if (ret < 0)
		pr_err("%s(), fail, ret %ld, cmd 0x%x, arg %lu\n",
				__func__, ret, cmd, arg);
	return ret;
}
#endif


int usip_mmap_data(struct usip_info *usip, struct vm_area_struct *area)
{
	long size;
	unsigned long offset;
	size_t align_bytes = PAGE_ALIGN(usip->memory_size);
	unsigned long pfn;
	int ret;


	pr_debug("%s(), memory ready %d, size %zu, align_bytes %zu\n",
	__func__, usip->memory_ready, usip->memory_size, align_bytes);

	if (!usip->memory_ready)
		return -EBADFD;

	size = area->vm_end - area->vm_start;
	offset = area->vm_pgoff << PAGE_SHIFT;

	if ((size_t)size > align_bytes)
		return -EINVAL;
	if (offset > align_bytes - size)
		return -EINVAL;

	/*area->vm_ops = &usip_vm_ops_data;*/
	/*area->vm_private_data = usip;*/

	pfn = usip->addr_phy >> PAGE_SHIFT;
	/* ensure that memory does not get swapped to disk */
	area->vm_flags |= VM_IO;
	/* ensure non-cacheable */
	area->vm_page_prot = pgprot_noncached(area->vm_page_prot);
	ret = remap_pfn_range(area, area->vm_start,
			pfn, size, area->vm_page_prot);
	if (ret)
		pr_err("%s(), ret %d, remap failed 0x%lx, phys_addr %pa -> vm_start 0x%lx\n",
				__func__, ret, pfn,
				&usip->addr_phy,
				area->vm_start);


	/*Comment*/
	smp_mb();

	return ret;
}

static int usip_mmap(struct file *file, struct vm_area_struct *area)
{
	unsigned long offset;
	struct usip_info *usip = file->private_data;

	pr_debug("%s(), vm_flags 0x%lx, vm_pgoff 0x%lx\n",
			__func__, area->vm_flags, area->vm_pgoff);

	offset = area->vm_pgoff << PAGE_SHIFT;
	switch (offset) {
	default:
		return usip_mmap_data(usip, area);
	}
	return 0;
}

static int usip_open(struct inode *inode, struct file *file)
{
	file->private_data = &usip;
	return 0;
}

static const struct file_operations usip_fops = {
	.owner = THIS_MODULE,
	.open = usip_open,
	.unlocked_ioctl = usip_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = usip_compat_ioctl,
#endif
	.mmap = usip_mmap,
};

#ifdef CONFIG_MTK_ECCCI_DRIVER
static struct miscdevice usip_miscdevice = {
	.minor      = MISC_DYNAMIC_MINOR,
	.name       = "usip",
	.fops       = &usip_fops,
};
#endif


#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT
static void usip_send_emi_info_to_dsp(void)
{
	int send_result = 0;
	struct ipi_msg_t ipi_msg;
	long long usip_emi_phy = 0;
	phys_addr_t offset = 0;

	offset = EMI_TABLE[SP_EMI_ADSP_USIP_PHONECALL][SP_EMI_OFFSET];
	usip_emi_phy = usip.addr_phy + offset;

	ipi_msg.magic      = IPI_MSG_MAGIC_NUMBER;
	ipi_msg.task_scene = TASK_SCENE_PHONE_CALL;
	ipi_msg.source_layer  = AUDIO_IPI_LAYER_FROM_KERNEL;
	ipi_msg.target_layer  = AUDIO_IPI_LAYER_TO_DSP;
	ipi_msg.data_type  = AUDIO_IPI_PAYLOAD;
	ipi_msg.ack_type   = AUDIO_IPI_MSG_BYPASS_ACK;
	ipi_msg.msg_id     = IPI_MSG_A2D_GET_EMI_ADDRESS;
	ipi_msg.param1     = sizeof(usip_emi_phy);
	ipi_msg.param2     = 0;


	/* Send EMI Address to Hifi3 Via IPI*/
	adsp_register_feature(VOICE_CALL_FEATURE_ID);
	send_result = audio_send_ipi_msg(
			&ipi_msg, TASK_SCENE_PHONE_CALL,
			AUDIO_IPI_LAYER_TO_DSP, AUDIO_IPI_PAYLOAD,
			AUDIO_IPI_MSG_BYPASS_ACK, IPI_MSG_A2D_GET_EMI_ADDRESS,
			sizeof(usip_emi_phy), 0, (char *)&usip_emi_phy);
	adsp_deregister_feature(VOICE_CALL_FEATURE_ID);

	if (send_result != 0)
		pr_info("%s(), scp_ipi send fail\n", __func__);
	else
		pr_debug("%s(), scp_ipi send succeed\n", __func__);
}

#ifdef CFG_RECOVERY_SUPPORT
static int audio_call_event_receive(
	struct notifier_block *this,
	unsigned long event,
	void *ptr)
{
	switch (event) {
	case ADSP_EVENT_STOP:
		break;
	case ADSP_EVENT_READY:
		usip_send_emi_info_to_dsp();
		break;
	default:
		pr_info("event %lu err", event);
	}
	return 0;
}


static struct notifier_block audio_call_notifier = {
	.notifier_call = audio_call_event_receive,
	.priority = VOICE_CALL_FEATURE_PRI,
};
#endif /* end of CFG_RECOVERY_SUPPORT */
#endif /* end of CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT */

static int __init usip_init(void)
{
	int ret;
	int size_o = 0;
	phys_addr_t r_rw_base;

	unsigned int r_rw_size;

	phys_addr_t srw_base;
	unsigned int srw_size;

	phys_addr_t phys_addr;


#ifdef CONFIG_MTK_ECCCI_DRIVER
	phys_addr = get_smem_phy_start_addr(MD_SYS1,
	SMEM_USER_RAW_USIP, &size_o);

	ret = misc_register(&usip_miscdevice);
	if (ret) {
		pr_err("%s(), cannot register miscdev on minor %d, ret %d\n",
				__func__, usip_miscdevice.minor, ret);
	}

	get_md_resv_mem_info(MD_SYS1, &r_rw_base, &r_rw_size,
	&srw_base, &srw_size);
#else
	phys_addr = 0;
	size_o = 0;
	ret = 0;
	r_rw_base = 0;
	r_rw_size = 0;
	srw_base = 0;
	srw_size = 0;
#endif

	pr_debug("%s(), %lld %d %lld %d %lld", __func__,
	r_rw_base, r_rw_size, srw_base, srw_size, phys_addr);

	/* init usip info */
	usip.memory_size = size_o;
	usip.memory_addr = 0x11220000L;
	usip.addr_phy = phys_addr;
	usip.memory_ready = true;

#ifdef CONFIG_MTK_AURISYS_PHONE_CALL_SUPPORT
#ifdef CFG_RECOVERY_SUPPORT
	adsp_A_register_notify(&audio_call_notifier);
#endif
	usip_send_emi_info_to_dsp();
#endif

	return ret;
}

static void __exit usip_exit(void)
{
#ifdef CONFIG_MTK_ECCCI_DRIVER
	misc_deregister(&usip_miscdevice);
#endif
}

module_init(usip_init);
module_exit(usip_exit);

MODULE_DESCRIPTION("Mediatek uSip memory control");
MODULE_LICENSE("GPL");



