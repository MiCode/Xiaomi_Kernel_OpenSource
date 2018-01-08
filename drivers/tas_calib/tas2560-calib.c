#define pr_fmt(fmt) "%s: " fmt, __func__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/semaphore.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <sound/smart_amp.h>


/* Holds the Packet data required for processing */
struct tas_dsp_pkt {
	u8 slave_id;
	u8 book;
	u8 page;
	u8 offset;
	u8 data[TAS_PAYLOAD_SIZE * 4];
};

static int isSFR(u32 index)
{
	switch (index) {
	case 3810:
	case 3811:
	case 3812:
	case 3813:
	case 3814:
	case 3815:
	case 3816:
		index = 1;
		break;
	default:
		index = 0;
		break;
	};
	return index;
}

static int smartamp_params_ctrl(uint8_t *input, u8 dir, u8 count)
{
	u32 length = count / 4;
	u32 paramid = 0;
	u32 index;
	int ret = 0;
	struct tas_dsp_pkt packet;

	pr_err("smartAmp-drv: %s", __func__);
	memset(&packet, 0, sizeof(struct tas_dsp_pkt));
	memcpy(&packet, input, sizeof(struct tas_dsp_pkt));
	index = (packet.page - 1) * 30 + (packet.offset - 8) / 4;
	pr_err("index = %d\n", index);
	if ((index < 0 || index > MAX_DSP_PARAM_INDEX) && !isSFR(index)) {
		pr_err("invalid index  %d!\n", index);
		return -EINVAL;
	}

	if (packet.slave_id == SLAVE1)
		paramid = (paramid | (index) | (length << 16) | (1 << 24));
	else if (packet.slave_id == SLAVE2)
		paramid = (paramid | (index) | (length << 16) | (2 << 24));
	else
		pr_err("smartAmp-Drv: Wrong slaveid = %x\n", packet.slave_id);

	ret = afe_smartamp_algo_ctrl(packet.data, paramid,
			dir, length * 4, packet.slave_id);
	if (ret)
		pr_err("%s Slave 0x%x params failed from afe\n",
			dir == TAS_GET_PARAM ? "get" : "set", packet.slave_id);

	memcpy(input, &packet, sizeof(struct tas_dsp_pkt));
	return ret;
}

static int tas_calib_open(struct inode *inode, struct file *fd)
{
	pr_err("smartAmp-drv: %s", __func__);
	return 0;
}

static ssize_t tas_calib_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *offp)
{
	int rc = 0;
	pr_err("smartAmp-drv: %s", __func__);
	rc = smartamp_params_ctrl((uint8_t *)buffer, TAS_SET_PARAM, count);
	return rc;
}

static ssize_t tas_calib_read(struct file *file, char __user *buffer,
		size_t count, loff_t *ptr)
{
	int rc;
	pr_err("smartAmp-drv: %s", __func__);
	rc = smartamp_params_ctrl((uint8_t *)buffer, TAS_GET_PARAM, count);
	if (rc < 0)
		count = rc;
	return count;
}

static long tas_calib_ioctl(struct file *filp, uint cmd, ulong arg)
{
	pr_err("smartAmp-drv: %s", __func__);
	return 0;
}

static int tas_calib_release(struct inode *inode, struct file *fd)
{
	pr_err("smartAmp-drv: %s", __func__);
	return 0;
}

const struct file_operations tas_calib_fops = {
	.owner			= THIS_MODULE,
	.open			= tas_calib_open,
	.write			= tas_calib_write,
	.read			= tas_calib_read,
	.release		= tas_calib_release,
	.unlocked_ioctl	= tas_calib_ioctl,
};

static struct miscdevice tas_calib_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "tas_calib",
	.fops = &tas_calib_fops,
};

static int __init tas_calib_init(void)
{
	int rc;
	pr_err("smartAmp-drv: %s", __func__);
	rc = misc_register(&tas_calib_misc);
	if (rc)
		pr_err("register calib misc failed\n");
	return rc;
}

static void __exit tas_calib_exit(void)
{
	pr_err("smartAmp-drv: %s", __func__);
	misc_deregister(&tas_calib_misc);
}

module_init(tas_calib_init);
module_exit(tas_calib_exit);

MODULE_AUTHOR("XYZ");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("XYZ");
