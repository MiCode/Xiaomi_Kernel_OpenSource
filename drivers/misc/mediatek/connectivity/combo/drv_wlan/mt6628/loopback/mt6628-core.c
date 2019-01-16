/*
 * linux/drivers/mmc/card/mt6620.c - MT6620 SDIO WIFI/BT/GPS/FM driver
 *
 * @file mt6620.c
 * @author Chih-pin Wu
 * @description
 * @date October 20, 2009
 * Copyright:	MediaTek Inc.
 *
 */
#if defined(MT6628)
#include "mt6628.h"
#include "mt6628_reg.h"
#endif
#if defined(MT6620)
#include "mt6620.h"
#include "mt6620_reg.h"
#endif
#include "hif_sdio.h"

#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/time.h>

#define MAX_RW_SIZE	(128*1024)

#define TX_PATH_BLKSZ_WORKAROUND 1
#define RX_PATH_BLKSZ_WORKAROUND 1

#define LOOPBACK_DEV_MAJOR_NUMBER   (200)

#if defined(MT6620) || defined(MT6628)
typedef enum _ENUM_WMTHWVER_TYPE_T {
	WMTHWVER_MT6620_E1 = 0x0,
	WMTHWVER_MT6620_E2 = 0x1,
	WMTHWVER_MT6620_E3 = 0x2,
	WMTHWVER_MT6620_E4 = 0x3,
	WMTHWVER_MT6620_E5 = 0x4,
	WMTHWVER_MT6620_E6 = 0x5,
	WMTHWVER_MT6620_E7 = 0x6,
	WMTHWVER_MT6620_MAX,
	WMTHWVER_INVALID = 0xff
} ENUM_WMTHWVER_TYPE_T, *P_ENUM_WMTHWVER_TYPE_T;

extern ENUM_WMTHWVER_TYPE_T mtk_wcn_wmt_hwver_get(void
    );
#endif

static DEFINE_SEMAPHORE(mt6620_init_sem);
static DEFINE_SEMAPHORE(mt6620_sem);
static DRIVER_PRIVATE_T mt6620_t;

/* IRQ Handler */
static INT32 mt6620_irq_handler(MTK_WCN_HIF_SDIO_CLTCTX cltCtx)
{
	down(&mt6620_sem);

	/* disabling interrupt */
	mtk_wcn_hif_sdio_writel(cltCtx, MCR_WHLPCR, 0x2);
	mt6620_t.irq_en = 0;

	/* marking */
	mt6620_t.irq.irq = 1;

	up(&mt6620_sem);

	return 0;
}

static ssize_t mt6620_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	size_t newSize;
	ssize_t ret;

	if (size > MAX_RW_SIZE)
		return -EINVAL;

#if	(RX_PATH_BLKSZ_WORKAROUND == 1)
	if (size > mt6620_t.func->blk_sz) {
		unsigned blksz = mt6620_t.func->blk_sz;

		newSize = (size + (blksz - 1)) / blksz * blksz;
	} else
		newSize = size;
#else
	newSize = size;
#endif

	if (down_interruptible(&mt6620_sem)) {
		return -ERESTARTSYS;
	}

	ret =
	    mtk_wcn_hif_sdio_read_buf(mt6620_t.cltCtx, mt6620_t.addr, (PUINT32) (mt6620_t.buf),
				      newSize);

	if (ret == 0) {
		if (copy_to_user(buf, mt6620_t.buf, size))
			ret = -EFAULT;
		else
			ret = size;
	} else {
		printk(KERN_INFO "%s(): io error code = %d\n", __func__, (int)ret);
		ret = -EIO;
	}

	up(&mt6620_sem);

	return ret;
}

static ssize_t mt6620_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	size_t newSize;
	ssize_t ret;

	if (size > MAX_RW_SIZE)
		return -EINVAL;

#if	(TX_PATH_BLKSZ_WORKAROUND == 1)
	if (size > mt6620_t.func->blk_sz) {
		unsigned blksz = mt6620_t.func->blk_sz;

		newSize = (size + (blksz - 1)) / blksz * blksz;
	} else
		newSize = size;
#else
	newSize = size;
#endif

	if (copy_from_user(mt6620_t.buf, buf, size))
		return -EFAULT;

	if (newSize > size)	/* pad with zero */
		memset(&mt6620_t.buf[size], 0, sizeof(char) * newSize - size);

	if (down_interruptible(&mt6620_sem)) {
		return -ERESTARTSYS;
	}

	ret =
	    mtk_wcn_hif_sdio_write_buf(mt6620_t.cltCtx, mt6620_t.addr, (PUINT32) (mt6620_t.buf),
				       newSize);

	if (ret == 0) {
		ret = size;
	} else {
		printk(KERN_INFO "%s(): io error code = %d\n", __func__, (int)ret);
		ret = -EIO;
	}

	up(&mt6620_sem);

	return ret;
}

static long mt6620_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int err = 0;
	int retval = 0;
	u8 data;
	unsigned int tmp;

	if (_IOC_TYPE(cmd) != MT6620_IOC_MAGIC)
		return -ENOTTY;
	else if (_IOC_NR(cmd) > MT6620_IOC_MAXNR)
		return -ENOTTY;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE, (void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ, (void __user *)arg, _IOC_SIZE(cmd));
	if (err)
		return -EFAULT;

	switch (cmd) {
	case MT6620_IOC_GET_FUNC_FOCUS:
	case MT6620_IOC_SET_FUNC_FOCUS:
	case MT6620_IOC_GET_SDBUS_WIDTH:
	case MT6620_IOC_SET_SDBUS_WIDTH:
	case MT6620_IOC_GET_BUS_CLOCK:
	case MT6620_IOC_SET_BUS_CLOCK:
		retval = -EINVAL;
		break;

	case MT6620_IOC_READ_DIRECT:
		if (down_interruptible(&mt6620_sem)) {
			return -ERESTARTSYS;
		}

		retval = mtk_wcn_hif_sdio_readb(mt6620_t.cltCtx, mt6620_t.addr, &data);

		up(&mt6620_sem);

		if (retval == 0)
			retval = __put_user(data, (u8 __user *) arg);

		break;

	case MT6620_IOC_WRITE_DIRECT:
		__get_user(data, (u8 __user *) arg);

		if (down_interruptible(&mt6620_sem)) {
			return -ERESTARTSYS;
		}

		retval = mtk_wcn_hif_sdio_writeb(mt6620_t.cltCtx, mt6620_t.addr, data);

		up(&mt6620_sem);

		break;

	case MT6620_IOC_GET_ADDR:
		tmp = mt6620_t.addr;
		__put_user(tmp, (u32 __user *) arg);
		break;

	case MT6620_IOC_SET_ADDR:
		__get_user(tmp, (u32 __user *) arg);
		if (tmp <= 0xFF)
			mt6620_t.addr = tmp;
		else
			retval = -EINVAL;
		break;

	case MT6620_IOC_SET_FIFO_MODE:
	case MT6620_IOC_SET_INCR_MODE:
	case MT6620_IOC_GET_BLOCK_SIZE:
	case MT6620_IOC_SET_BLOCK_SIZE:
		retval = -EINVAL;
		break;

	case MT6620_IOC_QUERY_IRQ_LEVEL:
		if (down_interruptible(&mt6620_sem))
			retval = -ERESTARTSYS;
		else if (mt6620_t.irq.irq == 1) {
			unsigned char buffer[88];

			if (mt6620_t.enhance_int.totalBytes != 0) {
				int i, offset = 0;

				if (mtk_wcn_hif_sdio_read_buf(mt6620_t.cltCtx, MCR_WHISR,
							      (PUINT32) (buffer),
							      mt6620_t.enhance_int.totalBytes +
							      sizeof(unsigned int))) {
					/* I/O error */
					up(&mt6620_sem);
					return -EIO;
				}

				mt6620_t.irq.u4HISR |= *(uint32_t *) (&(buffer[offset]));
				offset += sizeof(uint32_t);

				/* WTSR0/WTSR1 */
				if (mt6620_t.enhance_int.totalBytes >= (12)) {
					int tqStatus[2];
					memcpy(&(tqStatus[0]), &(buffer[offset]), 8);

					/* increase */
					mt6620_t.irq.rTxInfo.u.ucTQ0Cnt +=
					    ((tqStatus[0] >> 0) & 0xff);
					mt6620_t.irq.rTxInfo.u.ucTQ1Cnt +=
					    ((tqStatus[0] >> 8) & 0xff);
					mt6620_t.irq.rTxInfo.u.ucTQ2Cnt +=
					    ((tqStatus[0] >> 16) & 0xff);
					mt6620_t.irq.rTxInfo.u.ucTQ3Cnt +=
					    ((tqStatus[0] >> 24) & 0xff);
					mt6620_t.irq.rTxInfo.u.ucTQ4Cnt +=
					    ((tqStatus[1] >> 0) & 0xff);
					mt6620_t.irq.rTxInfo.u.ucTQ5Cnt +=
					    ((tqStatus[1] >> 8) & 0xff);

					offset += 8;
				}
				/* RX0NUM/RX1NUM */
				if (mt6620_t.enhance_int.totalBytes >= (16)) {
					memcpy(&(mt6620_t.irq.rRxInfo.au4RxStatusRaw[0]),
					       &(buffer[offset]), 4);
					offset += 4;
				}
				/* RX0 LEN0-15 / RX1 LEN0-15 */
				if (mt6620_t.enhance_int.totalBytes >=
				    (16 + mt6620_t.enhance_int.rxNum * 4)) {
					for (i = 0; i < mt6620_t.enhance_int.rxNum; i++) {
						mt6620_t.irq.rRxInfo.u.au2Rx0Len[i] =
						    *(uint16_t *) (&(buffer[offset]));
						offset += sizeof(uint16_t);
					}

					for (i = 0; i < mt6620_t.enhance_int.rxNum; i++) {
						mt6620_t.irq.rRxInfo.u.au2Rx1Len[i] =
						    *(uint16_t *) (&(buffer[offset]));
						offset += sizeof(uint16_t);
					}
				}
				/* D2HRM0R / D2HRM1R */
				if (mt6620_t.enhance_int.totalBytes >=
				    (16 + mt6620_t.enhance_int.rxNum * 4 + 8)) {
					memcpy(&(mt6620_t.irq.u4RcvMailbox0), &(buffer[offset]), 4);
					offset += 4;

					memcpy(&(mt6620_t.irq.u4RcvMailbox1), &(buffer[offset]), 4);
					offset += 4;
				}
				/* @FIXME: WHISR could be configured as write-1-clear .... */
				/* if so, we need to clear WHISR here .. */
			} else {
				if (mtk_wcn_hif_sdio_read_buf
				    (mt6620_t.cltCtx, MCR_WHISR, (PUINT32) (buffer),
				     sizeof(unsigned int))) {
					/* I/O error */
					up(&mt6620_sem);
					return -EIO;
				}

				mt6620_t.irq.u4HISR |= *(uint32_t *) (&(buffer[0]));
			}

			if (__copy_to_user
			    ((void __user *)arg, &(mt6620_t.irq), sizeof(INTR_DATA_STRUCT_T)))
				retval = -EIO;

			memset(&(mt6620_t.irq), 0, sizeof(INTR_DATA_STRUCT_T));
			up(&mt6620_sem);
		} else {
			if (__copy_to_user
			    ((void __user *)arg, &(mt6620_t.irq), sizeof(INTR_DATA_STRUCT_T)))
				retval = -EIO;

			up(&mt6620_sem);
		}

		break;

	case MT6620_IOC_SET_INT_ENHANCED:
		do {
			struct int_enhance_arg_t tmp;

			if (copy_from_user
			    (&tmp, (void __user *)arg, sizeof(struct int_enhance_arg_t)))
				retval = -EIO;
			else if (tmp.rxNum > 16 || tmp.totalBytes > 84)
				retval = -EINVAL;
			else {
				unsigned int dwWHCR;

				if (down_interruptible(&mt6620_sem))
					retval = -ERESTARTSYS;
				else {
					mt6620_t.enhance_int.rxNum = tmp.rxNum;
					mt6620_t.enhance_int.totalBytes = tmp.totalBytes;

					if (mtk_wcn_hif_sdio_read_buf
					    (mt6620_t.cltCtx, MCR_WHCR, (PUINT32) (&dwWHCR),
					     sizeof(unsigned int))) {
						/* I/O error */
						up(&mt6620_sem);
						return -EIO;
					}
					dwWHCR =
					    (dwWHCR & 0x00010007) |
					    ((mt6620_t.enhance_int.rxNum << 4) & 0xf0);

					if (mtk_wcn_hif_sdio_write_buf
					    (mt6620_t.cltCtx, MCR_WHCR, (PUINT32) (&dwWHCR),
					     sizeof(unsigned int))) {
						/* I/O error */
						up(&mt6620_sem);
						return -EIO;
					}
					up(&mt6620_sem);
				}
			}
		} while (0);

		break;
	case MT6620_IOC_ENABLE_INTERRUPT:
		if (mt6620_t.irq_en == 0) {
			if (down_interruptible(&mt6620_sem))
				retval = -ERESTARTSYS;
			else {
				if (mtk_wcn_hif_sdio_writel(mt6620_t.cltCtx, MCR_WHLPCR, 0x1)) {
					/* I/O error */
					up(&mt6620_sem);
					return -EIO;
				}

				mt6620_t.irq_en = 1;
				up(&mt6620_sem);
			}
		}
		break;

	case MT6620_IOC_DISABLE_INTERRUPT:
		if (mt6620_t.irq_en == 1) {
			if (down_interruptible(&mt6620_sem))
				retval = -ERESTARTSYS;
			else {
				if (mtk_wcn_hif_sdio_writel(mt6620_t.cltCtx, MCR_WHLPCR, 0x2)) {
					/* I/O error */
					up(&mt6620_sem);
					return -EIO;
				}

				mt6620_t.irq_en = 0;
				up(&mt6620_sem);
			}
		}
		break;

	default:
		retval = -EINVAL;
		break;
	}

	return retval;
}


static int mt6620_open(struct inode *inodep, struct file *filp)
{
	/* enable interrupt */
	memset(&(mt6620_t.irq), 0, sizeof(INTR_DATA_STRUCT_T));
	mt6620_t.enhance_int.rxNum = 0;
	mt6620_t.enhance_int.totalBytes = 0;

	mtk_wcn_hif_sdio_writel(mt6620_t.cltCtx, MCR_WHLPCR, 0x1);
	mt6620_t.irq_en = 1;

	return 0;
}


static int mt6620_release(struct inode *inodep, struct file *filp)
{
	return 0;
}

static const struct file_operations mt6620_fops = {
	.owner = THIS_MODULE,
	.read = mt6620_read,
	.write = mt6620_write,
	.unlocked_ioctl = mt6620_ioctl,
	.open = mt6620_open,
	.release = mt6620_release,
};

static INT32 mt6620_probe(MTK_WCN_HIF_SDIO_CLTCTX cltCtx,
			  const MTK_WCN_HIF_SDIO_FUNCINFO *prFuncInfo)
{
	int ret;
	printk("[loopback]: mt662x_probe++\n");
	down(&mt6620_init_sem);

	mt6620_t.func = (MTK_WCN_HIF_SDIO_FUNCINFO *) prFuncInfo;
	mt6620_t.cltCtx = cltCtx;

	mt6620_t.addr = 0;
	mt6620_t.focus = prFuncInfo->func_num;
	mt6620_t.buf = (char *)kmalloc(sizeof(char) * MAX_RW_SIZE, GFP_KERNEL);
	memset(&(mt6620_t.irq), 0, sizeof(INTR_DATA_STRUCT_T));
	mt6620_t.enhance_int.rxNum = 0;
	mt6620_t.enhance_int.totalBytes = 0;

#if defined(MT6620)
	switch (mtk_wcn_wmt_hwver_get()) {
	case WMTHWVER_MT6620_E1:
	case WMTHWVER_MT6620_E2:
	case WMTHWVER_MT6620_E3:
	case WMTHWVER_MT6620_E4:
	case WMTHWVER_MT6620_E5:
		firmware_download("/etc/firmware/WIFI_RAM_CODE", cltCtx);
		break;
	case WMTHWVER_MT6620_E6:
	default:
		firmware_download("/etc/firmware/WIFI_RAM_CODE_E6", cltCtx);
		break;
	}
#elif defined(MT5931)
	firmware_download("/etc/firmware/WIFI_RAM_CODE", cltCtx);
#elif defined(MT6628)
	firmware_download("/etc/firmware/WIFI_RAM_CODE_MT6628", cltCtx);
#endif

	/* 1. initialize cdev */
	cdev_init(&(mt6620_t.cdev), &mt6620_fops);
	mt6620_t.cdev.owner = THIS_MODULE;

	/* 2. add character device */
	ret = cdev_add(&(mt6620_t.cdev), mt6620_t.device_number, 1);

	up(&mt6620_init_sem);
	mtk_wcn_hif_sdio_enable_irq(cltCtx, MTK_WCN_BOOL_TRUE);
	printk("[loopback]: mt662x_probe--\n");
	return 0;
}

static INT32 mt6620_remove(MTK_WCN_HIF_SDIO_CLTCTX cltCtx)
{
	printk("[loopback]: mt662x_remove++\n");
	down(&mt6620_init_sem);

	/* 1. send power ctrl command to firmware */
	/* firmware_power_off(cltCtx); /* not-necessary, WMT will power off IC */ */

	/* 2. removal of character device */
	cdev_del(&(mt6620_t.cdev));

	/* 3. unregister card */
	mt6620_t.func = NULL;
	kfree(mt6620_t.buf);
	mt6620_t.buf = NULL;

	up(&mt6620_init_sem);
	printk("[loopback]: mt662x_remvoe--\n");
	return 0;
}

static MTK_WCN_HIF_SDIO_FUNCINFO funcInfo[] = {
#if defined(MT6620)
	/* MT6620 */
	{MTK_WCN_HIF_SDIO_FUNC(0x037a, 0x020a, 0x1, 512)},
	{MTK_WCN_HIF_SDIO_FUNC(0x037a, 0x020c, 0x2, 512)},
	{MTK_WCN_HIF_SDIO_FUNC(0x037a, 0x018a, 0x1, 512)},
	{MTK_WCN_HIF_SDIO_FUNC(0x037a, 0x018c, 0x2, 512)},
#elif defined(MT5931)
	/* MT5931 */
	{MTK_WCN_HIF_SDIO_FUNC(0x037a, 0x5931, 0x1, 512)},
#elif defined(MT6628)
	/* MT6628 */
	{MTK_WCN_HIF_SDIO_FUNC(0x037a, 0x6628, 0x1, 512)},
#endif
};

static MTK_WCN_HIF_SDIO_CLTINFO cltInfo = {
	.func_tbl = funcInfo,
	.func_tbl_size = sizeof(funcInfo) / sizeof(MTK_WCN_HIF_SDIO_FUNCINFO),
	.hif_clt_probe = mt6620_probe,
	.hif_clt_remove = mt6620_remove,
	.hif_clt_irq = mt6620_irq_handler,
};

static int __init mt6620_init(void)
{
	int ret;

	/* 1. allocate major number dynamically */
	ret = alloc_chrdev_region(&(mt6620_t.device_number), 0, 1, "mtk-wifi-loopback");

	if (ret != 0) {
		printk("==> alloc_chrdev_region() failure!\n");
		return ret;
	}

	/* 2. register to SDIO wrapper */
	ret = mtk_wcn_hif_sdio_client_reg(&cltInfo);

	if (ret == HIF_SDIO_ERR_SUCCESS)
		return 0;
	else {
		printk("%s(): mtk_wcn_hif_sdio_client_reg returned as %d\n", __func__, ret);
		unregister_chrdev_region(mt6620_t.device_number, 1);

		return ret;
	}
}

static void __exit mt6620_exit(void)
{
	/* 1. unregister from SDIO wrapper */
	mtk_wcn_hif_sdio_client_unreg(&cltInfo);

	/* 2. free device number */
	unregister_chrdev_region(mt6620_t.device_number, 1);
}
module_init(mt6620_init);
module_exit(mt6620_exit);

MODULE_AUTHOR("Chih-pin Wu <cp.wu@mediatek.com>");
MODULE_LICENSE("GPL");
