// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/uaccess.h>
#include <linux/compat.h>

#include <common/mdla_device.h>
#include <common/mdla_ioctl.h>
#include <common/mdla_cmd_proc.h>
#include <common/mdla_power_ctrl.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>


static int (*mdla_ioctl_perf)(struct file *filp,
		u32 command, unsigned long arg, bool need_pwr_on);

static long mdla_ioctl_config(unsigned long arg)
{
	long retval = 0;
	struct ioctl_config cfg;

	if (copy_from_user(&cfg, (void *) arg, sizeof(cfg)))
		return -EFAULT;

	switch (cfg.op) {
	case MDLA_CFG_NONE:
		break;
	case MDLA_CFG_TIMEOUT_GET:
		cfg.arg[0] = mdla_dbg_read_u32(FS_TIMEOUT);
		cfg.arg_count = 1;
		break;
	case MDLA_CFG_TIMEOUT_SET:
		if (cfg.arg_count == 1)
			mdla_dbg_write_u32(FS_TIMEOUT, cfg.arg[0]);
		break;
	case MDLA_CFG_FIFO_SZ_GET:
		cfg.arg[0] = 1;
		cfg.arg_count = 1;
		break;
	case MDLA_CFG_FIFO_SZ_SET:
		return -EINVAL;
	case MDLA_CFG_GSM_INFO:
		break;
	default:
		return -EINVAL;
	}

	if (copy_to_user((void *) arg, &cfg, sizeof(cfg)))
		return -EFAULT;

	return retval;
}

static long mdla_ioctl(struct file *filp, unsigned int command,
		unsigned long arg)
{
	long retval = 0;
	struct ioctl_run_cmd_sync cmd_data_sync;

	switch (command) {
	case IOCTL_MALLOC:
	case IOCTL_FREE:
	case IOCTL_ION_KMAP:
	case IOCTL_ION_KUNMAP:
		mdla_err("%s: Not support memory control\n", __func__);
		break;
	case IOCTL_RUN_CMD_SYNC:
		if (copy_from_user(&cmd_data_sync, (void *) arg,
				sizeof(cmd_data_sync))) {
			return -EFAULT;
		}
		mdla_cmd_debug("%s: RUN_CMD_SYNC: core_id=%d, kva=%p, mva=0x%08x, phys_to_virt=%p\n",
			__func__,
			cmd_data_sync.mdla_id,
			(void *)cmd_data_sync.req.buf.kva,
			cmd_data_sync.req.buf.mva,
			phys_to_virt(cmd_data_sync.req.buf.mva));

		if (core_id_is_invalid(cmd_data_sync.mdla_id))
			return -EFAULT;

		retval = mdla_cmd_ops_get()->ut_run_sync(&cmd_data_sync.req,
					&cmd_data_sync.res,
					mdla_get_device(cmd_data_sync.mdla_id));

		if (copy_to_user((void *) arg,
				&cmd_data_sync, sizeof(cmd_data_sync)))
			return -EFAULT;
		break;
	case IOCTL_RUN_CMD_ASYNC:
	case IOCTL_WAIT_CMD:
		/* Not support command type */
		break;

	case IOCTL_PERF_SET_EVENT:
	case IOCTL_PERF_GET_EVENT:
	case IOCTL_PERF_GET_CNT:
	case IOCTL_PERF_UNSET_EVENT:
	case IOCTL_PERF_RESET_CNT:
	case IOCTL_PERF_RESET_CYCLE:
	case IOCTL_PERF_SET_MODE:
		if (mdla_ioctl_perf)
			retval = mdla_ioctl_perf(filp, command, arg, true);
		break;
	case IOCTL_PERF_GET_START:
	case IOCTL_PERF_GET_END:
	case IOCTL_PERF_GET_CYCLE:
		if (mdla_ioctl_perf)
			retval = mdla_ioctl_perf(filp, command, arg, false);
		break;
	case IOCTL_CONFIG:
		return mdla_ioctl_config(arg);
	default:
			return -EINVAL;
	}

	return retval;
}


static long mdla_compat_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg)
{
	return mdla_ioctl(file, cmd, (unsigned long) compat_ptr(arg));
}

static int mdla_open(struct inode *inodep, struct file *filep)
{
	mdla_drv_debug("%s(): Device has been opened\n", __func__);
	return 0;
}

static int mdla_release(struct inode *inodep, struct file *filep)
{
	mdla_drv_debug("%s(): Device successfully closed\n", __func__);
	return 0;
}

static const struct file_operations fops = {
	.open = mdla_open,
	.unlocked_ioctl = mdla_ioctl,
	.compat_ioctl = mdla_compat_ioctl,
	.release = mdla_release,
};

const struct file_operations *mdla_ioctl_get_fops(void)
{
	return &fops;
}


void mdla_ioctl_register_perf_handle(int (*pmu_ioctl)(struct file *filp,
						u32 command,
						unsigned long arg,
						bool need_pwr_on))
{
	if (pmu_ioctl)
		mdla_ioctl_perf	= pmu_ioctl;
}

void mdla_ioctl_unregister_perf_handle(void)
{
	mdla_ioctl_perf	= NULL;
}

