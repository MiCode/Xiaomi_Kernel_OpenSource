#include <generated/autoconf.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/blkdev.h>
#include <linux/mmc/host.h>
#include <linux/mmc/card.h>
#include <linux/mmc/core.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>
#include <linux/mmc/sdio.h>
#include <linux/dma-mapping.h>

#include <mach/dma.h>
#include <mach/board.h>		/* FIXME */
/* #include <asm/tcm.h> */

#include "mt_sd.h"
#include <linux/mmc/sd_misc.h>
#include "board-custom.h"
#include "drivers/mmc/card/queue.h"

#ifndef FPGA_PLATFORM
#include <mach/mt_clkmgr.h>
/* #include "mach/mt6575_clkmgr_internal.h" */
#endif

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#ifdef CONFIG_MTK_EMMC_SUPPORT
/*#include "partition_define.h"*/

/* extern struct excel_info PartInfoEmmc[PART_NUM]; */

extern u32 g_emmc_mode_switch;
#endif

#define PARTITION_NAME_LENGTH    (64)
#define DRV_NAME_MISC            "misc-sd"

#define DEBUG_MMC_IOCTL   0
#define DEBUG_MSDC_SSC    1
/*
 * For simple_sd_ioctl
 */
#define FORCE_IN_DMA (0x11)
#define FORCE_IN_PIO (0x10)
#define FORCE_NOTHING (0x0)

static int dma_force[HOST_MAX_NUM] =	/* used for sd ioctrol */
{
	FORCE_NOTHING,
	FORCE_NOTHING,
	FORCE_NOTHING,
	FORCE_NOTHING,
};

#define dma_is_forced(host_id)     (dma_force[host_id] & 0x10)
#define get_forced_transfer_mode(host_id)  (dma_force[host_id] & 0x01)


static u32 *sg_msdc_multi_buffer;

static int simple_sd_open(struct inode *inode, struct file *file)
{
	return 0;
}

extern int mmc_switch(struct mmc_card *card, u8 set, u8 index, u8 value, unsigned int timeout_ms);
extern int mmc_send_ext_csd(struct mmc_card *card, u8 *ext_csd);
extern struct msdc_host *mtk_msdc_host[];
extern struct msdc_host *msdc_get_host(int host_function, bool boot, bool secondary);
extern int msdc_reinit(struct msdc_host *host);
extern int msdc_get_reserve(void);
extern u32 msdc_get_capacity(int get_emmc_total);
extern struct gendisk *mmc_get_disk(struct mmc_card *card);
#ifndef FPGA_PLATFORM
extern void msdc_get_driving(struct msdc_host *host, struct msdc_ioctl *msdc_ctl);
#endif

static int sd_ioctl_reinit(struct msdc_ioctl *msdc_ctl)
{
	struct msdc_host *host = msdc_get_host(MSDC_SD, 0, 0);
	if (NULL != host)
		return msdc_reinit(host);
	else
		return -EINVAL;
}

void msdc_check_init_done(void)
{
	struct msdc_host *host = NULL;
	host = msdc_get_host(MSDC_EMMC, 1, 0);
	BUG_ON(!host);
	BUG_ON(!host->mmc);
	host->mmc->card_init_wait(host->mmc);
	BUG_ON(!host->mmc->card);
	pr_err("[%s]: get the emmc init done signal\n", __func__);
	return;
}

static int simple_sd_ioctl_multi_rw(struct msdc_ioctl *msdc_ctl)
{
	char l_buf[512];
	struct scatterlist msdc_sg;
	struct mmc_data msdc_data;
	struct mmc_command msdc_cmd;
	struct mmc_command msdc_stop;
	int ret = 0;

#ifdef MTK_MSDC_USE_CMD23
	struct mmc_command msdc_sbc;
#endif
    
    struct mmc_request  msdc_mrq;
    struct msdc_host *host_ctl;

    if(!msdc_ctl)
        return -EINVAL; 
		
    host_ctl = mtk_msdc_host[msdc_ctl->host_num];
    BUG_ON(!host_ctl);
    BUG_ON(!host_ctl->mmc);
    BUG_ON(!host_ctl->mmc->card);

    mmc_claim_host(host_ctl->mmc);

#if DEBUG_MMC_IOCTL
	pr_debug("user want access %d partition\n", msdc_ctl->partition);
#endif

	ret = mmc_send_ext_csd(host_ctl->mmc->card, l_buf);
	if (ret) {
		pr_debug("mmc_send_ext_csd error, multi rw\n");
		goto multi_end;
	}
#ifdef CONFIG_MTK_EMMC_SUPPORT
	switch (msdc_ctl->partition) {
	case EMMC_PART_BOOT1:
		if (0x1 != (l_buf[179] & 0x7)) {
			/* change to access boot partition 1 */
			l_buf[179] &= ~0x7;
			l_buf[179] |= 0x1;
			mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
		}
		break;
	case EMMC_PART_BOOT2:
		if (0x2 != (l_buf[179] & 0x7)) {
			/* change to access boot partition 2 */
			l_buf[179] &= ~0x7;
			l_buf[179] |= 0x2;
			mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
		}
		break;
	default:
		/* make sure access partition is user data area */
		if (0 != (l_buf[179] & 0x7)) {
			/* set back to access user area */
			l_buf[179] &= ~0x7;
			l_buf[179] |= 0x0;
			mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
		}
		break;
	}
#endif
	if(msdc_ctl->total_size > host_ctl->mmc->max_req_size) { //64*1024){
		msdc_ctl->result = -1;
		goto multi_end;
	}

	memset(&msdc_data, 0, sizeof(struct mmc_data));
	memset(&msdc_mrq, 0, sizeof(struct mmc_request));
	memset(&msdc_cmd, 0, sizeof(struct mmc_command));
	memset(&msdc_stop, 0, sizeof(struct mmc_command));

#ifdef MTK_MSDC_USE_CMD23
	memset(&msdc_sbc, 0, sizeof(struct mmc_command));
#endif

	msdc_mrq.cmd = &msdc_cmd;
	msdc_mrq.data = &msdc_data;

	if (msdc_ctl->trans_type)
		dma_force[host_ctl->id] = FORCE_IN_DMA;
	else
		dma_force[host_ctl->id] = FORCE_IN_PIO;

	if (msdc_ctl->iswrite) {
		msdc_data.flags = MMC_DATA_WRITE;
		msdc_cmd.opcode = MMC_WRITE_MULTIPLE_BLOCK;
		msdc_data.blocks = msdc_ctl->total_size / 512;
		if (MSDC_CARD_DUNM_FUNC != msdc_ctl->opcode) {
			if (copy_from_user(sg_msdc_multi_buffer, msdc_ctl->buffer, msdc_ctl->total_size)) {
				dma_force[host_ctl->id] = FORCE_NOTHING;
				ret = -EFAULT;
				goto multi_end;
			}
		} else {
			/* called from other kernel module */
			memcpy(sg_msdc_multi_buffer, msdc_ctl->buffer, msdc_ctl->total_size);
		}
	} else {
		msdc_data.flags = MMC_DATA_READ;
		msdc_cmd.opcode = MMC_READ_MULTIPLE_BLOCK;
		msdc_data.blocks = msdc_ctl->total_size / 512;
		memset(sg_msdc_multi_buffer, 0, msdc_ctl->total_size);
	}

#ifdef MTK_MSDC_USE_CMD23
	if ((mmc_card_mmc(host_ctl->mmc->card)
		 || (mmc_card_sd(host_ctl->mmc->card)
		 && host_ctl->mmc->card->scr.cmds & SD_SCR_CMD23_SUPPORT))
		&& !(host_ctl->mmc->card->quirks & MMC_QUIRK_BLK_NO_CMD23)) {
		msdc_mrq.sbc = &msdc_sbc;
		msdc_mrq.sbc->opcode = MMC_SET_BLOCK_COUNT;
#ifdef MTK_MSDC_USE_CACHE
		/* if ioctl access cacheable partition data, there is on flush mechanism in msdc driver
		 * so do reliable write .*/
		if (mmc_card_mmc(host_ctl->mmc->card)
			&& (host_ctl->mmc->card->ext_csd.cache_ctrl & 0x1)
			&& (msdc_cmd.opcode == MMC_WRITE_MULTIPLE_BLOCK))
			msdc_mrq.sbc->arg = msdc_data.blocks | (1 << 31);
		else
			msdc_mrq.sbc->arg = msdc_data.blocks;
#else
		msdc_mrq.sbc->arg = msdc_data.blocks;
#endif
		msdc_mrq.sbc->flags = MMC_RSP_R1 | MMC_CMD_AC;
	}
#endif

	msdc_cmd.arg = msdc_ctl->address;

	if (!mmc_card_blockaddr(host_ctl->mmc->card)) {
		pr_debug("this device use byte address!!\n");
		msdc_cmd.arg <<= 9;
	}
	msdc_cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	msdc_stop.opcode = MMC_STOP_TRANSMISSION;
	msdc_stop.arg = 0;
	msdc_stop.flags = MMC_RSP_SPI_R1B | MMC_RSP_R1B | MMC_CMD_AC;

	msdc_data.stop = &msdc_stop;
	msdc_data.blksz = 512;
	msdc_data.sg = &msdc_sg;
	msdc_data.sg_len = 1;

#if DEBUG_MMC_IOCTL
	pr_debug("total size is %d\n", msdc_ctl->total_size);
#endif
	sg_init_one(&msdc_sg, sg_msdc_multi_buffer, msdc_ctl->total_size);
	mmc_set_data_timeout(&msdc_data, host_ctl->mmc->card);
	mmc_wait_for_req(host_ctl->mmc, &msdc_mrq);

	if (!msdc_ctl->iswrite) {
		if (MSDC_CARD_DUNM_FUNC != msdc_ctl->opcode) {
			if (copy_to_user(msdc_ctl->buffer, sg_msdc_multi_buffer, msdc_ctl->total_size)) {
				dma_force[host_ctl->id] = FORCE_NOTHING;
				ret = -EFAULT;
				goto multi_end;
			}
		} else {
			/* called from other kernel module */
			memcpy(msdc_ctl->buffer, sg_msdc_multi_buffer, msdc_ctl->total_size);
		}
	}

	if (msdc_ctl->partition) {
		ret = mmc_send_ext_csd(host_ctl->mmc->card, l_buf);
		if (ret) {
			pr_debug("mmc_send_ext_csd error, multi rw2\n");
			goto multi_end;
		}

		if (l_buf[179] & 0x7) {
			/* set back to access user area */
			l_buf[179] &= ~0x7;
			l_buf[179] |= 0x0;
			mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
		}
	}

multi_end:
	mmc_release_host(host_ctl->mmc);
	if (ret)
		msdc_ctl->result = ret;

	if (msdc_cmd.error)
		msdc_ctl->result = msdc_cmd.error;

	if (msdc_data.error) {
		msdc_ctl->result = msdc_data.error;
	} else {
		msdc_ctl->result = 0;
	}
	dma_force[host_ctl->id] = FORCE_NOTHING;
	return msdc_ctl->result;

}

static int simple_sd_ioctl_single_rw(struct msdc_ioctl *msdc_ctl)
{
	char l_buf[512];
	struct scatterlist msdc_sg;
	struct mmc_data msdc_data;
	struct mmc_command msdc_cmd;
	struct mmc_request msdc_mrq;
	struct msdc_host *host_ctl;
	int  ret = 0;
    if(!msdc_ctl)
        return -EINVAL; 
		
	host_ctl = mtk_msdc_host[msdc_ctl->host_num];
	BUG_ON(!host_ctl);
	BUG_ON(!host_ctl->mmc);
	BUG_ON(!host_ctl->mmc->card);

#ifdef MTK_MSDC_USE_CACHE
	if (msdc_ctl->iswrite && mmc_card_mmc(host_ctl->mmc->card)
		&& (host_ctl->mmc->card->ext_csd.cache_ctrl & 0x1))
		return simple_sd_ioctl_multi_rw(msdc_ctl);
#endif

	mmc_claim_host(host_ctl->mmc);

#if DEBUG_MMC_IOCTL
	pr_debug("user want access %d partition\n", msdc_ctl->partition);
#endif

	ret = mmc_send_ext_csd(host_ctl->mmc->card, l_buf);
	if (ret) {
		pr_debug("mmc_send_ext_csd error, single rw\n");
		goto single_end;
	}
#ifdef CONFIG_MTK_EMMC_SUPPORT
	switch (msdc_ctl->partition) {
	case EMMC_PART_BOOT1:
		if (0x1 != (l_buf[179] & 0x7)) {
			/* change to access boot partition 1 */
			l_buf[179] &= ~0x7;
			l_buf[179] |= 0x1;
			mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
		}
		break;
	case EMMC_PART_BOOT2:
		if (0x2 != (l_buf[179] & 0x7)) {
			/* change to access boot partition 2 */
			l_buf[179] &= ~0x7;
			l_buf[179] |= 0x2;
			mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
		}
		break;
	default:
		/* make sure access partition is user data area */
		if (0 != (l_buf[179] & 0x7)) {
			/* set back to access user area */
			l_buf[179] &= ~0x7;
			l_buf[179] |= 0x0;
			mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
		}
		break;
	}
#endif
	if (msdc_ctl->total_size > 512) {
		msdc_ctl->result = -1;
		goto single_end;
	}
#if DEBUG_MMC_IOCTL
	pr_debug("start MSDC_SINGLE_READ_WRITE !!\n");
#endif
	memset(&msdc_data, 0, sizeof(struct mmc_data));
	memset(&msdc_mrq, 0, sizeof(struct mmc_request));
	memset(&msdc_cmd, 0, sizeof(struct mmc_command));

	msdc_mrq.cmd = &msdc_cmd;
	msdc_mrq.data = &msdc_data;

	if (msdc_ctl->trans_type)
		dma_force[host_ctl->id] = FORCE_IN_DMA;
	else
		dma_force[host_ctl->id] = FORCE_IN_PIO;

	if (msdc_ctl->iswrite) {
		msdc_data.flags = MMC_DATA_WRITE;
		msdc_cmd.opcode = MMC_WRITE_BLOCK;
		msdc_data.blocks = msdc_ctl->total_size / 512;
		if (MSDC_CARD_DUNM_FUNC != msdc_ctl->opcode) {
			if (copy_from_user(sg_msdc_multi_buffer, msdc_ctl->buffer, 512)) {
				dma_force[host_ctl->id] = FORCE_NOTHING;
				ret = -EFAULT;
				goto single_end;
			}
		} else {
			/* called from other kernel module */
			memcpy(sg_msdc_multi_buffer, msdc_ctl->buffer, 512);
		}
	} else {
		msdc_data.flags = MMC_DATA_READ;
		msdc_cmd.opcode = MMC_READ_SINGLE_BLOCK;
		msdc_data.blocks = msdc_ctl->total_size / 512;

		memset(sg_msdc_multi_buffer, 0, 512);
	}

	msdc_cmd.arg = msdc_ctl->address;

	if (!mmc_card_blockaddr(host_ctl->mmc->card)) {
		pr_debug("the device is used byte address!\n");
		msdc_cmd.arg <<= 9;
	}

	msdc_cmd.flags = MMC_RSP_SPI_R1 | MMC_RSP_R1 | MMC_CMD_ADTC;

	msdc_data.stop = NULL;
	msdc_data.blksz = 512;
	msdc_data.sg = &msdc_sg;
	msdc_data.sg_len = 1;

#if DEBUG_MMC_IOCTL
	pr_debug("single block: ueser buf address is 0x%p!\n", msdc_ctl->buffer);
#endif
	sg_init_one(&msdc_sg, sg_msdc_multi_buffer, msdc_ctl->total_size);
	mmc_set_data_timeout(&msdc_data, host_ctl->mmc->card);

	mmc_wait_for_req(host_ctl->mmc, &msdc_mrq);

	if (!msdc_ctl->iswrite) {
		if (MSDC_CARD_DUNM_FUNC != msdc_ctl->opcode) {
			if (copy_to_user(msdc_ctl->buffer, sg_msdc_multi_buffer, 512)) {
				dma_force[host_ctl->id] = FORCE_NOTHING;
				ret = -EFAULT;
				goto single_end;
			}
		} else {
			/* called from other kernel module */
			memcpy(msdc_ctl->buffer, sg_msdc_multi_buffer, 512);
		}
	}

	if (msdc_ctl->partition) {
		ret = mmc_send_ext_csd(host_ctl->mmc->card, l_buf);
		if (ret) {
			pr_debug("mmc_send_ext_csd error, single rw2\n");
			goto single_end;
		}

		if (l_buf[179] & 0x7) {
			/* set back to access user area */
			l_buf[179] &= ~0x7;
			l_buf[179] |= 0x0;
			mmc_switch(host_ctl->mmc->card, 0, 179, l_buf[179], 1000);
		}
	}

single_end:
	mmc_release_host(host_ctl->mmc);
	if (ret)
		msdc_ctl->result = ret;

	if (msdc_cmd.error)
		msdc_ctl->result = msdc_cmd.error;

	if (msdc_data.error)
		msdc_ctl->result = msdc_data.error;
	else
		msdc_ctl->result = 0;

	dma_force[host_ctl->id] = FORCE_NOTHING;
	return msdc_ctl->result;
}

int simple_sd_ioctl_rw(struct msdc_ioctl *msdc_ctl)
{
	if((msdc_ctl->total_size > 512) && (msdc_ctl->total_size <= MAX_REQ_SZ))
		return simple_sd_ioctl_multi_rw(msdc_ctl);
	else
		return simple_sd_ioctl_single_rw(msdc_ctl);
}

static int simple_sd_ioctl_get_cid(struct msdc_ioctl *msdc_ctl)
{
    struct msdc_host *host_ctl;

    if(!msdc_ctl)
        return -EINVAL; 
		
    host_ctl = mtk_msdc_host[msdc_ctl->host_num];

    BUG_ON(!host_ctl);
    BUG_ON(!host_ctl->mmc);
    BUG_ON(!host_ctl->mmc->card);

#if DEBUG_MMC_IOCTL
	pr_debug("user want the cid in msdc slot%d\n", msdc_ctl->host_num);
#endif

	if (copy_to_user(msdc_ctl->buffer, &host_ctl->mmc->card->raw_cid, 16))
		return -EFAULT;

#if DEBUG_MMC_IOCTL
	pr_debug("cid:0x%x,0x%x,0x%x,0x%x\n", 
		 host_ctl->mmc->card->raw_cid[0], host_ctl->mmc->card->raw_cid[1],
		 host_ctl->mmc->card->raw_cid[2], host_ctl->mmc->card->raw_cid[3]);
#endif
	return 0;

}

static int simple_sd_ioctl_get_csd(struct msdc_ioctl *msdc_ctl)
{
    struct msdc_host *host_ctl;
    
    if(!msdc_ctl)
        return -EINVAL; 
		
    host_ctl = mtk_msdc_host[msdc_ctl->host_num];

    BUG_ON(!host_ctl);
    BUG_ON(!host_ctl->mmc);
    BUG_ON(!host_ctl->mmc->card);

#if DEBUG_MMC_IOCTL
	pr_debug("user want the csd in msdc slot%d\n", msdc_ctl->host_num);
#endif

	if (copy_to_user(msdc_ctl->buffer, &host_ctl->mmc->card->raw_csd, 16))
		return -EFAULT;

#if DEBUG_MMC_IOCTL
	pr_debug("csd:0x%x,0x%x,0x%x,0x%x\n",
		 host_ctl->mmc->card->raw_csd[0], host_ctl->mmc->card->raw_csd[1],
		 host_ctl->mmc->card->raw_csd[2], host_ctl->mmc->card->raw_csd[3]);
#endif
	return 0;

}

static int simple_sd_ioctl_get_excsd(struct msdc_ioctl *msdc_ctl)
{
	char l_buf[512];
	struct msdc_host *host_ctl;

#if DEBUG_MMC_IOCTL
	int i;
#endif

    if(!msdc_ctl)
        return -EINVAL; 
		   
    host_ctl = mtk_msdc_host[msdc_ctl->host_num];

    BUG_ON(!host_ctl);
    BUG_ON(!host_ctl->mmc);
    BUG_ON(!host_ctl->mmc->card);

    mmc_claim_host(host_ctl->mmc);

#if DEBUG_MMC_IOCTL
	pr_debug("user want the extend csd in msdc slot%d\n", msdc_ctl->host_num);
#endif
	mmc_send_ext_csd(host_ctl->mmc->card, l_buf);
	if (copy_to_user(msdc_ctl->buffer, l_buf, 512))
		return -EFAULT;

#if DEBUG_MMC_IOCTL
	for (i = 0; i < 512; i++) {
		pr_debug("%x", l_buf[i]);
		if (0 == ((i + 1) % 16)) {
			pr_debug("\n");
		}
	}
#endif

	if (copy_to_user(msdc_ctl->buffer, l_buf, 512))
		return -EFAULT;

	mmc_release_host(host_ctl->mmc);

	return 0;

}

#ifndef FPGA_PLATFORM
extern void msdc_set_driving(struct msdc_host *host, struct msdc_hw *hw, bool sd_18);
#endif
extern void msdc_dump_padctl(struct msdc_host *host, u32 pin);
extern u32 msdc_host_mode[HOST_MAX_NUM];
extern u32 msdc_host_mode2[HOST_MAX_NUM];
static int simple_sd_ioctl_set_driving(struct msdc_ioctl *msdc_ctl)
{
	void __iomem *base;
	struct msdc_host *host;
#if DEBUG_MMC_IOCTL
	unsigned int l_value;
#endif

	if(!msdc_ctl)
		return -EINVAL; 

	if((msdc_ctl->host_num < 0) || (msdc_ctl->host_num >= HOST_MAX_NUM)){
		pr_err("invalid host num: %d\n", msdc_ctl->host_num);
		return -EINVAL;
	}else if (msdc_ctl->host_num == 0){
#ifndef CFG_DEV_MSDC0
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 1) {
#ifndef CFG_DEV_MSDC1
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 2) {
#ifndef CFG_DEV_MSDC2
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 3) {
#ifndef CFG_DEV_MSDC3
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 4) {
#ifndef CFG_DEV_MSDC4
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
		return -EINVAL;
	}

	host = mtk_msdc_host[msdc_ctl->host_num];
	BUG_ON(!host);
		
	base = host->base;
#ifndef FPGA_PLATFORM
	enable_clock(MT_CG_PERI_MSDC30_0 + host->id, "SD"); 
#if DEBUG_MMC_IOCTL
	pr_debug("set: clk driving is 0x%x\n", msdc_ctl->clk_pu_driving);
	pr_debug("set: cmd driving is 0x%x\n", msdc_ctl->cmd_pu_driving);
	pr_debug("set: dat driving is 0x%x\n", msdc_ctl->dat_pu_driving);
	pr_debug("set: rst driving is 0x%x\n", msdc_ctl->rst_pu_driving);
	pr_debug("set: ds driving is 0x%x\n", msdc_ctl->ds_pu_driving);
#endif
	host->hw->clk_drv = msdc_ctl->clk_pu_driving;
	host->hw->cmd_drv = msdc_ctl->cmd_pu_driving;
	host->hw->dat_drv = msdc_ctl->dat_pu_driving;
	host->hw->rst_drv = msdc_ctl->rst_pu_driving;
	host->hw->ds_drv = msdc_ctl->ds_pu_driving;
	host->hw->clk_drv_sd_18 = msdc_ctl->clk_pu_driving;
	host->hw->cmd_drv_sd_18 = msdc_ctl->cmd_pu_driving;
	host->hw->dat_drv_sd_18 = msdc_ctl->dat_pu_driving;
	msdc_set_driving(host, host->hw, 0);
#if DEBUG_MMC_IOCTL
	msdc_dump_padctl(host, GPIO_CLK_CTRL);
	msdc_dump_padctl(host, GPIO_CMD_CTRL);
	msdc_dump_padctl(host, GPIO_DAT_CTRL);
	msdc_dump_padctl(host, GPIO_RST_CTRL);
	msdc_dump_padctl(host, GPIO_DS_CTRL);
#endif
#endif

	return 0;
}

static int simple_sd_ioctl_get_driving(struct msdc_ioctl *msdc_ctl)
{
    void __iomem *base;
/* unsigned int l_value; */
	struct msdc_host *host;

	if(!msdc_ctl)
		return -EINVAL; 

	if((msdc_ctl->host_num < 0) || (msdc_ctl->host_num >= HOST_MAX_NUM)){
		pr_err("invalid host num: %d\n", msdc_ctl->host_num);
		return -EINVAL;
	}else if (msdc_ctl->host_num == 0){
#ifndef CFG_DEV_MSDC0
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 1) {
#ifndef CFG_DEV_MSDC1
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 2) {
#ifndef CFG_DEV_MSDC2
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 3) {
#ifndef CFG_DEV_MSDC3
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 4) {
#ifndef CFG_DEV_MSDC4
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	}else {
		pr_err("invalid host num: %d\n", msdc_ctl->host_num);
		return -EINVAL;
	}

	host = mtk_msdc_host[msdc_ctl->host_num];
	BUG_ON(!host);
		
	base = host->base;
#ifndef FPGA_PLATFORM
	enable_clock(MT_CG_PERI_MSDC30_0 + host->id, "SD"); 
	msdc_get_driving(mtk_msdc_host[msdc_ctl->host_num], msdc_ctl);
#if DEBUG_MMC_IOCTL
	pr_debug("read: clk driving is 0x%x\n", msdc_ctl->clk_pu_driving);
	pr_debug("read: cmd driving is 0x%x\n", msdc_ctl->cmd_pu_driving);
	pr_debug("read: dat driving is 0x%x\n", msdc_ctl->dat_pu_driving);
	pr_debug("read: rst driving is 0x%x\n", msdc_ctl->rst_pu_driving);
	pr_debug("read: ds driving is 0x%x\n", msdc_ctl->ds_pu_driving);
#endif
#endif

	return 0;
}

static int simple_sd_ioctl_sd30_mode_switch(struct msdc_ioctl *msdc_ctl)
{
/* u32 base; */
/* struct msdc_hw hw; */
	int id; 
#if DEBUG_MMC_IOCTL
	unsigned int l_value;
#endif

	if(!msdc_ctl)
		return -EINVAL; 
    id = msdc_ctl->host_num;

	if((msdc_ctl->host_num < 0) || (msdc_ctl->host_num >= HOST_MAX_NUM)){
		pr_err("invalid host num: %d\n", msdc_ctl->host_num);
		return -EINVAL;
	}else if (msdc_ctl->host_num == 0){
#ifndef CFG_DEV_MSDC0
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 1) {
#ifndef CFG_DEV_MSDC1
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 2) {
#ifndef CFG_DEV_MSDC2
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 3) {
#ifndef CFG_DEV_MSDC3
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	} else if (msdc_ctl->host_num == 4) {
#ifndef CFG_DEV_MSDC4
		pr_err("host%d is not config\n", msdc_ctl->host_num);
		return -EINVAL;
#endif
	}else {
		pr_err("invalid host num: %d\n", msdc_ctl->host_num);
		return -EINVAL;
	}

	switch (msdc_ctl->sd30_mode) {
	case SDHC_HIGHSPEED:
		msdc_host_mode[id] |= MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED;
		msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR12) & (~MMC_CAP_UHS_SDR25) &
			(~MMC_CAP_UHS_SDR50) & (~MMC_CAP_UHS_DDR50) &
			(~MMC_CAP_1_8V_DDR) & (~MMC_CAP_UHS_SDR104);
#if 1				/* CONFIG_EMMC_50_FEATURE */
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
		pr_debug("[****SD_Debug****]host will support Highspeed\n");
		break;
	case UHS_SDR12:
		msdc_host_mode[id] |= MMC_CAP_UHS_SDR12;
		msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR25) & (~MMC_CAP_UHS_SDR50) &
			(~MMC_CAP_UHS_DDR50) & (~MMC_CAP_1_8V_DDR) & (~MMC_CAP_UHS_SDR104);
#if 1				/* CONFIG_EMMC_50_FEATURE */
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
		pr_debug("[****SD_Debug****]host will support UHS-SDR12\n");
		break;
	case UHS_SDR25:
		msdc_host_mode[id] |= MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25;
		msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR50) & (~MMC_CAP_UHS_DDR50) &
			(~MMC_CAP_1_8V_DDR) & (~MMC_CAP_UHS_SDR104);
#if 1				/* CONFIG_EMMC_50_FEATURE */
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
		pr_debug("[****SD_Debug****]host will support UHS-SDR25\n");
		break;
	case UHS_SDR50:
		msdc_host_mode[id] |= MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 | MMC_CAP_UHS_SDR50;
		msdc_host_mode[id] &= (~MMC_CAP_UHS_DDR50) & (~MMC_CAP_1_8V_DDR) &
			(~MMC_CAP_UHS_SDR104);
#if 1				/* CONFIG_EMMC_50_FEATURE */
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
		pr_debug("[****SD_Debug****]host will support UHS-SDR50\n");
		break;
	case UHS_SDR104:
		msdc_host_mode[id] |= MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
			MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_DDR50 |
			MMC_CAP_1_8V_DDR | MMC_CAP_UHS_SDR104;
		msdc_host_mode2[id] |= MMC_CAP2_HS200_1_8V_SDR;
#if 1				/* CONFIG_EMMC_50_FEATURE */
		msdc_host_mode2[id] &= (~MMC_CAP2_HS400_1_8V_DDR);
#endif
		pr_debug("[****SD_Debug****]host will support UHS-SDR104\n");
		break;
	case UHS_DDR50:
		msdc_host_mode[id] |= MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
			MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR;
		msdc_host_mode[id] &= (~MMC_CAP_UHS_SDR50) & (~MMC_CAP_UHS_SDR104);
#if 1				/* CONFIG_EMMC_50_FEATURE */
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR) & (~MMC_CAP2_HS400_1_8V_DDR);
#else
		msdc_host_mode2[id] &= (~MMC_CAP2_HS200_1_8V_SDR);
#endif
		pr_debug("[****SD_Debug****]host will support UHS-DDR50\n");
		break;
#if 1				/* CONFIG_EMMC_50_FEATURE */
	case 6:		/* EMMC_HS400: */
		msdc_host_mode[id] |= MMC_CAP_UHS_SDR12 | MMC_CAP_UHS_SDR25 |
			MMC_CAP_UHS_SDR50 | MMC_CAP_UHS_DDR50 |
			MMC_CAP_1_8V_DDR | MMC_CAP_UHS_SDR104;
		msdc_host_mode2[id] |= MMC_CAP2_HS200_1_8V_SDR | MMC_CAP2_HS400_1_8V_DDR;
		pr_debug("[****SD_Debug****]host will support EMMC_HS400\n");
		break;
#endif
	default:
		pr_debug("[****SD_Debug****]invalid sd30_mode:%d\n", msdc_ctl->sd30_mode);
		break;
	}

	pr_debug(KERN_ERR "\n [%s]: msdc%d, line:%d, msdc_host_mode2=%d\n", 
		 __func__, id, __LINE__, msdc_host_mode2[id]);
	switch (msdc_ctl->sd30_drive) {
	case DRIVER_TYPE_A:
		msdc_host_mode[id] |= MMC_CAP_DRIVER_TYPE_A;
		msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_C) & (~MMC_CAP_DRIVER_TYPE_D);
		pr_debug("[****SD_Debug****]host will support DRIVING TYPE A\n");
		break;
	case DRIVER_TYPE_B:
		msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_A) &
			(~MMC_CAP_DRIVER_TYPE_C) & (~MMC_CAP_DRIVER_TYPE_D);
		pr_debug("[****SD_Debug****]host will support DRIVING TYPE B\n");
		break;
	case DRIVER_TYPE_C:
		msdc_host_mode[id] |= MMC_CAP_DRIVER_TYPE_C;
		msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_A) & (~MMC_CAP_DRIVER_TYPE_D);
		pr_debug("[****SD_Debug****]host will support DRIVING TYPE C\n");
		break;
	case DRIVER_TYPE_D:
		msdc_host_mode[id] |= MMC_CAP_DRIVER_TYPE_D;
		msdc_host_mode[id] &= (~MMC_CAP_DRIVER_TYPE_A) & (~MMC_CAP_DRIVER_TYPE_C);
		pr_debug("[****SD_Debug****]host will support DRIVING TYPE D\n");
		break;
	default:
		pr_debug("[****SD_Debug****]invalid sd30_drive:%d\n", msdc_ctl->sd30_drive);
		break;
	}
#if 0
	switch (msdc_ctl->sd30_max_current) {
	case MAX_CURRENT_200:
		msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200;
		msdc_host_mode[id] &= (~MMC_CAP_MAX_CURRENT_400)& 
			(~MMC_CAP_MAX_CURRENT_600) & (~MMC_CAP_MAX_CURRENT_800);
		pr_debug("[****SD_Debug****]host will support MAX_CURRENT_200\n");
		break;
	case MAX_CURRENT_400:
		msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200 | MMC_CAP_MAX_CURRENT_400;
		msdc_host_mode[id] &= (~MMC_CAP_MAX_CURRENT_600) & (~MMC_CAP_MAX_CURRENT_800);
		pr_debug("[****SD_Debug****]host will support MAX_CURRENT_400\n");
		break;
	case MAX_CURRENT_600:
		msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200 | 
			MMC_CAP_MAX_CURRENT_400 | MMC_CAP_MAX_CURRENT_600;
		msdc_host_mode[id] &= (~MMC_CAP_MAX_CURRENT_800);
		pr_debug("[****SD_Debug****]host will support MAX_CURRENT_600\n");
		break;
	case MAX_CURRENT_800:
		msdc_host_mode[id] |= MMC_CAP_MAX_CURRENT_200 |
			MMC_CAP_MAX_CURRENT_400 | MMC_CAP_MAX_CURRENT_600 |
			MMC_CAP_MAX_CURRENT_800;
		pr_debug("[****SD_Debug****]host will support MAX_CURRENT_800\n");
		break;
	default:
		pr_debug("[****SD_Debug****]invalid sd30_max_current:%d\n",
			 msdc_ctl->sd30_max_current);
		break;
	}
	switch (msdc_ctl->sd30_power_control) {
	case SDXC_NO_POWER_CONTROL:
		msdc_host_mode[id] &= (~MMC_CAP_SET_XPC_330) &
			(~MMC_CAP_SET_XPC_300) & (~MMC_CAP_SET_XPC_180);
		pr_debug("[****SD_Debug****]host will not support SDXC power control\n");
		break;
	case SDXC_POWER_CONTROL:
		msdc_host_mode[id] |= MMC_CAP_SET_XPC_330 |
			MMC_CAP_SET_XPC_300 | MMC_CAP_SET_XPC_180;
		pr_debug("[****SD_Debug****]host will support SDXC power control\n");
		break;
	default:
		pr_debug("[****SD_Debug****]invalid sd30_power_control:%d\n",
			 msdc_ctl->sd30_power_control);
		break;
	}
#endif

#ifdef CONFIG_MTK_EMMC_SUPPORT
	/* just for emmc slot */
	if (msdc_ctl->host_num == 0) {
		g_emmc_mode_switch = 1;
	} else {
		g_emmc_mode_switch = 0;
	}
#endif
	return 0;
}

/*  to ensure format operate is clean the emmc device fully(partition erase) */
typedef struct mbr_part_info {
	unsigned int start_sector;
	unsigned int nr_sects;
	unsigned int part_no;
	unsigned char *part_name;
} MBR_PART_INFO_T;

#define MBR_PART_NUM  6
#define __MMC_ERASE_ARG        0x00000000
#define __MMC_TRIM_ARG         0x00000001
#define __MMC_DISCARD_ARG      0x00000003

struct __mmc_blk_data {
	spinlock_t lock;
	struct gendisk *disk;
	struct mmc_queue queue;

	unsigned int usage;
	unsigned int read_only;
};

int msdc_get_info(STORAGE_TPYE storage_type, GET_STORAGE_INFO info_type, struct storage_info *info)
{
	struct msdc_host *host = NULL;
	int host_function = 0;
	bool boot = 0;
	
	if(!info)
		return -EINVAL; 

	switch (storage_type) {
	case EMMC_CARD_BOOT:
		host_function = MSDC_EMMC;
		boot = MSDC_BOOT_EN;
		break;
	case EMMC_CARD:
		host_function = MSDC_EMMC;
		break;
	case SD_CARD_BOOT:
		host_function = MSDC_SD;
		boot = MSDC_BOOT_EN;
		break;
	case SD_CARD:
		host_function = MSDC_SD;
		break;
	default:
		pr_err(KERN_ERR "No supported storage type!");
		return 0;
		break;
	}
	host = msdc_get_host(host_function, boot, 0);
	BUG_ON(!host);
	BUG_ON(!host->mmc);
	BUG_ON(!host->mmc->card);
	switch (info_type) {
	case CARD_INFO:
		if (host->mmc && host->mmc->card)
			info->card = host->mmc->card;
		else {
			pr_err(KERN_ERR "CARD was not ready<get card>!");
			return 0;
		}
		break;
	case DISK_INFO:
		if (host->mmc && host->mmc->card)
			info->disk = mmc_get_disk(host->mmc->card);
		else {
			pr_err(KERN_ERR "CARD was not ready<get disk>!");
			return 0;
		}
		break;
	case EMMC_USER_CAPACITY:
		info->emmc_user_capacity = msdc_get_capacity(0);
		break;
	case EMMC_CAPACITY:
		info->emmc_capacity = msdc_get_capacity(1);
		break;
	case EMMC_RESERVE:
#ifdef CONFIG_MTK_EMMC_SUPPORT
		info->emmc_reserve = msdc_get_reserve();
#endif
		break;
	default:
		pr_err(KERN_ERR "Please check INFO_TYPE");
		return 0;
	}
	return 1;
}

#ifdef CONFIG_MTK_EMMC_SUPPORT
#ifdef CONFIG_MTK_GPT_SCHEME_SUPPORT
static int simple_mmc_get_disk_info(struct mbr_part_info* mpi, unsigned char* name)
{
    char* no_partition_name = "n/a";
    struct disk_part_iter piter;
    struct hd_struct *part;
    struct msdc_host *host;
    struct gendisk *disk;
    struct __mmc_blk_data *md;
    
    /* emmc always in slot0 */
    host = msdc_get_host(MSDC_EMMC,MSDC_BOOT_EN,0);
    BUG_ON(!host);
    BUG_ON(!host->mmc);
    BUG_ON(!host->mmc->card);

    md = mmc_get_drvdata(host->mmc->card);
    BUG_ON(!md);
    BUG_ON(!md->disk);

    disk = md->disk;
    
    /* use this way to find partition info is to avoid handle addr transfer in scatter file 
     * and 64bit address calculate */
    disk_part_iter_init(&piter, disk, 0);
    while ((part = disk_part_iter_next(&piter))){
#if DEBUG_MMC_IOCTL
	pr_debug("part_name = %s name = %s\n", part->info->volname, name);
#endif                
	if (!strncmp(part->info->volname, name, PARTITION_NAME_LENGTH)){
            mpi->start_sector = part->start_sect;           
            mpi->nr_sects = part->nr_sects;           
            mpi->part_no = part->partno; 
            if (part->info){
                mpi->part_name = part->info->volname;
            } else {
                mpi->part_name = no_partition_name;
            }

            disk_part_iter_exit(&piter);
            return 0;
        }
    }
    disk_part_iter_exit(&piter);

    return 1;
}

#else
static int simple_mmc_get_disk_info(struct mbr_part_info *mpi, unsigned char *name)
{
    int i = 0;
    char* no_partition_name = "n/a";
    struct disk_part_iter piter;
    struct hd_struct *part;
    struct msdc_host *host;
    struct gendisk *disk;
    struct __mmc_blk_data *md;
    
    if(!name || !mpi)
        return -EINVAL; 

    /* emmc always in slot0 */
    host = msdc_get_host(MSDC_EMMC,MSDC_BOOT_EN,0);
    BUG_ON(!host);
    BUG_ON(!host->mmc);
    BUG_ON(!host->mmc->card);

    md = mmc_get_drvdata(host->mmc->card);
    BUG_ON(!md);
    BUG_ON(!md->disk);

    disk = md->disk;
    
    /* use this way to find partition info is to avoid handle addr transfer in scatter file 
     * and 64bit address calculate */
    disk_part_iter_init(&piter, disk, 0);
    while ((part = disk_part_iter_next(&piter))){
        for (i = 0; i < PART_NUM; i++) {
			if ((PartInfo[i].partition_idx != 0)
				&& (PartInfo[i].partition_idx == part->partno)) {
#if DEBUG_MMC_IOCTL
				pr_debug("part_name = %s    name = %s\n", PartInfo[i].name, name);
#endif
				if (!strncmp(PartInfo[i].name, name, PARTITION_NAME_LENGTH)) {
					mpi->start_sector = part->start_sect;
					mpi->nr_sects = part->nr_sects;
					mpi->part_no = part->partno;
					if (i < PART_NUM) {
						mpi->part_name = PartInfo[i].name;
					} else {
						mpi->part_name = no_partition_name;
					}

					disk_part_iter_exit(&piter);
					return 0;
				}

				break;
			}
		}
	}
	disk_part_iter_exit(&piter);

	return 1;
}
#endif

/* call mmc block layer interface for userspace to do erase operate */
static int simple_mmc_erase_func(unsigned int start, unsigned int size)
{
    struct msdc_host *host;
    unsigned int arg; 
        
    /* emmc always in slot0 */
    host = msdc_get_host(MSDC_EMMC,MSDC_BOOT_EN,0);
    BUG_ON(!host);
    BUG_ON(!host->mmc);
    BUG_ON(!host->mmc->card);
                        
    mmc_claim_host(host->mmc);

	if (mmc_can_discard(host->mmc->card)) {
		arg = __MMC_DISCARD_ARG;
	} else if (mmc_can_trim(host->mmc->card)) {
		arg = __MMC_TRIM_ARG;
	} else if (mmc_can_erase(host->mmc->card)) {
		/* mmc_erase() will remove the erase group un-aligned part,
		 * msdc_command_start() will do trim for old combo erase un-aligned issue
		 */
		arg = __MMC_ERASE_ARG;
	} else {
		pr_err("[%s]: emmc card can't support trim / discard / erase\n", __func__);
        goto end;
    }
    
	pr_debug("[%s]: start=0x%x, size=%d, arg=0x%x, can_trim=(0x%x),EXT_CSD_SEC_GB_CL_EN=0x%lx\n",
		 __func__, start, size, arg, host->mmc->card->ext_csd.sec_feature_support,
		 EXT_CSD_SEC_GB_CL_EN);
    mmc_erase(host->mmc->card, start, size, arg);

#if DEBUG_MMC_IOCTL
	pr_debug("[%s]: erase done....arg=0x%x\n", __func__, arg);
#endif
 end:
	mmc_release_host(host->mmc);

	return 0;
}
#endif

static int simple_mmc_erase_partition(unsigned char *name)
{
#ifdef CONFIG_MTK_EMMC_SUPPORT
	struct mbr_part_info mbr_part;
	int l_ret = -1;

	BUG_ON(!name);
	/* just support erase cache & data partition now */
	if (('u' == *name && 's' == *(name + 1) && 'r' == *(name + 2) && 'd' == *(name + 3) &&
	     'a' == *(name + 4) && 't' == *(name + 5) && 'a' == *(name + 6)) ||
	    ('c' == *name && 'a' == *(name + 1) && 'c' == *(name + 2) && 'h' == *(name + 3)
	     && 'e' == *(name + 4))) {
		/* find erase start address and erase size, just support high capacity emmc card now */
		l_ret = simple_mmc_get_disk_info(&mbr_part, name);


		if (l_ret == 0) {
			/* do erase */
			pr_debug("erase %s start sector: 0x%x size: 0x%x\n", mbr_part.part_name,
				 mbr_part.start_sector, mbr_part.nr_sects);
			simple_mmc_erase_func(mbr_part.start_sector, mbr_part.nr_sects);
		}
	}

	return 0;
#else
	return 0;
#endif

}

static int simple_mmc_erase_partition_wrap(struct msdc_ioctl *msdc_ctl)
{
	unsigned char name[PARTITION_NAME_LENGTH];

    if(!msdc_ctl)
        return -EINVAL; 
		
	if(msdc_ctl->total_size > PARTITION_NAME_LENGTH)
		return -EFAULT;

	if (copy_from_user(name, (unsigned char *)msdc_ctl->buffer, msdc_ctl->total_size))
		return -EFAULT;

	return simple_mmc_erase_partition(name);
}

static long simple_sd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct msdc_ioctl *msdc_ctl = (struct msdc_ioctl *)arg;
	int ret;
	if (msdc_ctl == NULL) {
		switch (cmd) {
			/* #ifdef MTK_SD_REINIT_SUPPORT */
		case MSDC_REINIT_SDCARD:
			ret = sd_ioctl_reinit(msdc_ctl);
			break;
			/* #endif */
		default :
			pr_err("mt_sd_ioctl:this opcode value is illegal!!\n");
			return -EINVAL;
		}
		return ret;
	} else {
		switch (msdc_ctl->opcode) {
		case MSDC_SINGLE_READ_WRITE:
			msdc_ctl->result = simple_sd_ioctl_single_rw(msdc_ctl);
			break;
		case MSDC_MULTIPLE_READ_WRITE:
			msdc_ctl->result = simple_sd_ioctl_multi_rw(msdc_ctl);
			break;
		case MSDC_GET_CID:
			msdc_ctl->result = simple_sd_ioctl_get_cid(msdc_ctl);
			break;
		case MSDC_GET_CSD:
			msdc_ctl->result = simple_sd_ioctl_get_csd(msdc_ctl);
			break;
		case MSDC_GET_EXCSD:
			msdc_ctl->result = simple_sd_ioctl_get_excsd(msdc_ctl);
			break;
		case MSDC_DRIVING_SETTING:
			pr_debug("in ioctl to change driving\n");
			if (1 == msdc_ctl->iswrite) {
				msdc_ctl->result = simple_sd_ioctl_set_driving(msdc_ctl);
			} else {
				msdc_ctl->result = simple_sd_ioctl_get_driving(msdc_ctl);
			}
			break;
		case MSDC_ERASE_PARTITION:
			msdc_ctl->result = simple_mmc_erase_partition_wrap(msdc_ctl);
			break;
		case MSDC_SD30_MODE_SWITCH:
			msdc_ctl->result = simple_sd_ioctl_sd30_mode_switch(msdc_ctl);
			break;
		default:
			pr_err("simple_sd_ioctl:this opcode value is illegal!!\n");
			return -EINVAL;
		}

		return msdc_ctl->result;
	}
}

static struct file_operations simple_msdc_em_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = simple_sd_ioctl,
	.open = simple_sd_open,
};

static struct miscdevice simple_msdc_em_dev[] = {
	{
	 .minor = MISC_DYNAMIC_MINOR,
	 .name = "misc-sd",
	 .fops = &simple_msdc_em_fops,
	 }
};

static int simple_sd_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_debug("in misc_sd_probe function\n");

	return ret;
}

static int simple_sd_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver simple_sd_driver = {
	.probe = simple_sd_probe,
	.remove = simple_sd_remove,

	.driver = {
		   .name = DRV_NAME_MISC,
		   .owner = THIS_MODULE,
		   },
};

static int __init simple_sd_init(void)
{
	int ret;

	sg_msdc_multi_buffer = (u32 *) kmalloc(64 * 1024, GFP_KERNEL);
	if (sg_msdc_multi_buffer == NULL)
		pr_err("allock 64KB memory failed\n");


	ret = platform_driver_register(&simple_sd_driver);
	if (ret) {
		pr_err(DRV_NAME_MISC ": Can't register driver\n");
		return ret;
	}
	pr_debug(DRV_NAME_MISC ": MediaTek simple SD/MMC Card Driver\n");

	/*msdc0 is for emmc only, just for emmc */
	/* ret = misc_register(&simple_msdc_em_dev[host->id]); */
	ret = misc_register(&simple_msdc_em_dev[0]);
	if (ret) {
		pr_err("register MSDC Slot[0] misc driver failed (%d)\n", ret);
		return ret;
	}

	return 0;
}

static void __exit simple_sd_exit(void)
{
	if (sg_msdc_multi_buffer != NULL) {
		kfree(sg_msdc_multi_buffer);
		sg_msdc_multi_buffer = NULL;
	}

    misc_deregister(&simple_msdc_em_dev[0]);
    
    platform_driver_unregister(&simple_sd_driver);
}

module_init(simple_sd_init);
module_exit(simple_sd_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("simple MediaTek SD/MMC Card Driver");
MODULE_AUTHOR("feifei.wang <feifei.wang@mediatek.com>");
