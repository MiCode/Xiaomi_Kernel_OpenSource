/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#include "mtk_thermal_ipi.h"
#include "mach/mtk_thermal.h"
#include "tscpu_settings.h"
#include "linux/mutex.h"
#include <linux/proc_fs.h>
#include <linux/uidgid.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include "mt-plat/mtk_thermal_monitor.h"

#if THERMAL_ENABLE_TINYSYS_SSPM || THERMAL_ENABLE_ONLY_TZ_SSPM
static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
/* ipi_send() return code
 * IPI_DONE 0
 * IPI_RETURN 1
 * IPI_BUSY -1
 * IPI_TIMEOUT_AVL -2
 * IPI_TIMEOUT_ACK -3
 * IPI_MODULE_ID_ERROR -4
 * IPI_HW_ERROR -5
 */
static DEFINE_MUTEX(thermo_sspm_mutex);
static int is_thermal_ipi_registered;
static int ack_data;

static int register_thermal_ipi(void)
{
	int ret;

	ret = mtk_ipi_register(&sspm_ipidev, IPIS_C_THERMAL, NULL, NULL,
		(void *)&ack_data);
	if (ret != 0) {
		tscpu_printk("%s error ret:%d\n", __func__, ret);
		return -1;
	}

	is_thermal_ipi_registered = 1;

	return 0;
}

unsigned int thermal_to_sspm(
	unsigned int cmd, struct thermal_ipi_data *thermal_data)
{
	int ackData = -1;
	int ret;

	mutex_lock(&thermo_sspm_mutex);

	if (!is_thermal_ipi_registered) {
		if (register_thermal_ipi() != 0)
			goto end;
	}

	switch (cmd) {
	case THERMAL_IPI_INIT_GRP1:
	case THERMAL_IPI_INIT_GRP2:
	case THERMAL_IPI_INIT_GRP3:
	case THERMAL_IPI_INIT_GRP4:
	case THERMAL_IPI_INIT_GRP5:
	case THERMAL_IPI_INIT_GRP6:
		thermal_data->cmd = cmd;
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_POLLING, thermal_data, THERMAL_SLOT_NUM, 2000);
		if (ret != 0)
			tscpu_printk("send init cmd(%d) error ret:%d\n",
				cmd, ret);
		else if (ack_data < 0)
			tscpu_printk("cmd(%d) return error(%d)\n",
				cmd, ack_data);

		ackData = ack_data;

		break;
	case THERMAL_IPI_LVTS_INIT_GRP1:
		thermal_data->cmd = cmd;
		tscpu_printk("cmd(%d) lvts efuse to SSPM (%d)\n",
				cmd, ack_data);

		ackData = ack_data;

		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_POLLING, thermal_data, THERMAL_SLOT_NUM, 2000);


		if (ret != 0)
			tscpu_printk("sspm_ipi_send err cmd %d,ret:%d - %d\n",
					cmd, ret, ackData);
		else if (ackData < 0)
			tscpu_printk("cmd(%d) return error(%d)\n",
				cmd, ackData);
		break;

	case THERMAL_IPI_GET_TEMP:
		thermal_data->cmd = cmd;
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_POLLING, thermal_data, THERMAL_SLOT_NUM, 2000);
		if (ret != 0)
			tscpu_printk("send get_temp cmd(%d) error ret:%d\n",
				cmd, ret);
		else if (ack_data < 0)
			tscpu_printk("cmd(%d) return error(%d)\n",
				cmd, ack_data);

		ackData = ack_data;

		break;
	case THERMAL_IPI_SET_BIG_FREQ_THRESHOLD:
	case THERMAL_IPI_GET_BIG_FREQ_THRESHOLD:
		thermal_data->cmd = cmd;

		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_POLLING, thermal_data, THERMAL_SLOT_NUM, 2000);

		if (ret != 0)
			tscpu_printk("mtk_ipi_send_compl error ret:%d - %d\n",
					cmd, ret);
		else if (ackData < 0)
			tscpu_printk("cmd(%d) return error(%d)\n",
				cmd, ackData);
		break;

	case THERMAL_IPI_SET_DIS_THERMAL_THROTTLE:
		thermal_data->cmd = cmd;
		tscpu_printk("cmd(%d) disable SSPM thermal throttle(%d)\n",
				cmd, ack_data);

		ackData = ack_data;

		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_POLLING, thermal_data, THERMAL_SLOT_NUM, 2000);

		if (ret != 0)
			tscpu_printk("sspm_ipi_send err cmd %d,ret:%d - %d\n",
					cmd, ret, ackData);
		else if (ackData < 0)
			tscpu_printk("cmd(%d) return error(%d)\n",
				cmd, ackData);
		break;
	case THERMAL_IPI_SUSPEND_RESUME_NOTIFY:
		thermal_data->cmd = cmd;
		//tscpu_printk("cmd(%d) kernel suspend/resume(%d)\n",
		//		cmd, ack_data);

		ackData = ack_data;

		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_POLLING, thermal_data, THERMAL_SLOT_NUM, 2000);

		if (ret != 0)
			tscpu_printk("sspm_ipi_send err cmd %d,ret:%d - %d\n",
					cmd, ret, ackData);
		else if (ackData < 0)
			tscpu_printk("cmd(%d) return error(%d)\n",
				cmd, ackData);
		break;

	default:
		tscpu_printk("cmd(%d) wrong!!\n", cmd);
		break;
	}

end:
	mutex_unlock(&thermo_sspm_mutex);

	return ackData; /** It's weird here. What should be returned? */
}

/* ipi_send() return code
 * IPI_DONE 0
 * IPI_RETURN 1
 * IPI_BUSY -1
 * IPI_TIMEOUT_AVL -2
 * IPI_TIMEOUT_ACK -3
 * IPI_MODULE_ID_ERROR -4
 * IPI_HW_ERROR -5
 */
int atm_to_sspm(unsigned int cmd, int data_len,
struct thermal_ipi_data *thermal_data, int *ackData)
{
	int ret = -1;

	if (data_len < 1 || data_len > 3) {
		*ackData = -1;
		return ret;
	}

	mutex_lock(&thermo_sspm_mutex);

	if (!is_thermal_ipi_registered) {
		if (register_thermal_ipi() != 0)
			goto end;
	}

	switch (cmd) {
	case THERMAL_IPI_SET_ATM_CFG_GRP1:
	case THERMAL_IPI_SET_ATM_CFG_GRP2:
	case THERMAL_IPI_SET_ATM_CFG_GRP3:
	case THERMAL_IPI_SET_ATM_CFG_GRP4:
	case THERMAL_IPI_SET_ATM_CFG_GRP5:
	case THERMAL_IPI_SET_ATM_CFG_GRP6:
	case THERMAL_IPI_SET_ATM_CFG_GRP7:
	case THERMAL_IPI_SET_ATM_CFG_GRP8:
	case THERMAL_IPI_SET_ATM_TTJ:
	case THERMAL_IPI_SET_ATM_EN:
	case THERMAL_IPI_GET_ATM_CPU_LIMIT:
	case THERMAL_IPI_GET_ATM_GPU_LIMIT:
		thermal_data->cmd = cmd;
		ret = mtk_ipi_send_compl(&sspm_ipidev, IPIS_C_THERMAL,
			IPI_SEND_POLLING, thermal_data, (data_len+1), 2000);
		if ((ret != 0) || (ack_data < 0))
			tscpu_printk("%s cmd %d ret %d ack %d\n",
				__func__, cmd, ret, ack_data);

		*ackData = ack_data;

		break;

	default:
		tscpu_printk("%s cmd %d err!\n", __func__, cmd);
		break;
	}

end:
	mutex_unlock(&thermo_sspm_mutex);

	return ret;
}

static int get_sspm_tz_temp_read(struct seq_file *m, void *v)
{
	struct thermal_ipi_data thermal_data;

	thermal_data.u.data.arg[0] = 0;
	thermal_data.u.data.arg[1] = 0;
	thermal_data.u.data.arg[2] = 0;

	while (thermal_to_sspm(THERMAL_IPI_GET_TEMP, &thermal_data) != 0)
		udelay(500);

	seq_puts(m, "Show current temperature in SSPM UART log\n");

	return 0;
}

static int get_sspm_tz_temp_open(struct inode *inode, struct file *file)
{
	return single_open(file, get_sspm_tz_temp_read, NULL);
}

static const struct file_operations get_sspm_tz_temp_fops = {
	.owner = THIS_MODULE,
	.open = get_sspm_tz_temp_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static ssize_t set_sspm_big_limit_threshold_write
(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[128];
	int len = 0;
	int arrayIndex, bigCoreTj, bigCoreExitTj, bigCoreFreqUpperBound;
	struct thermal_ipi_data thermal_data;


	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (sscanf(desc, "%d %d %d", &arrayIndex, &bigCoreTj,
		&bigCoreFreqUpperBound) == 3) {
		if ((arrayIndex >= BIG_CORE_THRESHOLD_ARRAY_SIZE) ||
			(arrayIndex < 0)) {
			tscpu_printk("%s invalid array index: %s\n",
				__func__, desc);

			return -EINVAL;
		}
		thermal_data.u.data.arg[0] = arrayIndex;
		thermal_data.u.data.arg[1] = bigCoreTj;
		thermal_data.u.data.arg[2] = bigCoreFreqUpperBound;

		while (thermal_to_sspm(THERMAL_IPI_SET_BIG_FREQ_THRESHOLD,
			&thermal_data) != 0)
			udelay(500);

		return count;
	} else if (sscanf(desc, "%d %d", &arrayIndex, &bigCoreExitTj) == 2) {
		if (arrayIndex != -1) {
			tscpu_printk("%s invalid array index: %s\n",
				__func__, desc);

			return -EINVAL;
		}

		thermal_data.u.data.arg[0] = arrayIndex;
		thermal_data.u.data.arg[1] = bigCoreExitTj;
		thermal_data.u.data.arg[2] = 0;

		while (thermal_to_sspm(THERMAL_IPI_SET_BIG_FREQ_THRESHOLD,
			&thermal_data) != 0)
			udelay(500);

		return count;
	}

	tscpu_printk("%s bad argument: %s\n", __func__, desc);
	return -EINVAL;

}

static int set_sspm_big_limit_threshold_read(struct seq_file *m, void *v)
{
	struct thermal_ipi_data thermal_data;

	seq_puts(m, "Use this command to change big core freq limit threshold\n");
	seq_puts(m, "   echo arrayIndex BigCoreTj bigCoreFreqUpperBound >\n");
	seq_puts(m, "      /proc/driver/thermal/set_sspm_big_limit_threshold\n");
	seq_printf(m, "   arrayIndex is not larger than or equal to %d\n",
		BIG_CORE_THRESHOLD_ARRAY_SIZE);
	seq_puts(m, "   BigCoreTj is in m'C\n");
	seq_puts(m, "   BigCoreFreqUpperBound is in MHz\n");
	seq_puts(m, "   For example:\n");
	seq_puts(m, "   echo 0 85000 2050 > /proc/driver/thermal/set_sspm_big_limit_threshold\n");
	seq_puts(m, "Use this command to change exit point\n");
	seq_puts(m, "   echo -1 BigCoreExitTj > /proc/driver/thermal/set_sspm_big_limit_threshold\n");
	seq_puts(m, "   BigCoreExitTj is in m'C\n");
	seq_puts(m, "   For example:\n");
	seq_puts(m, "   echo -1 75000 > /proc/driver/thermal/set_sspm_big_limit_threshold\n");

	thermal_data.u.data.arg[0] = 0;
	thermal_data.u.data.arg[1] = 0;
	thermal_data.u.data.arg[2] = 0;

	while (thermal_to_sspm(THERMAL_IPI_GET_BIG_FREQ_THRESHOLD,
		&thermal_data) != 0)
		udelay(500);

	seq_puts(m, "Show big core frequency thresholds in SSPM UART log\n");

	return 0;
}

static int set_sspm_big_limit_threshold_open(struct inode *inode,
	struct file *file)
{
	return single_open(file, set_sspm_big_limit_threshold_read, NULL);
}

static const struct file_operations set_sspm_big_limit_threshold_fops = {
	.owner = THIS_MODULE,
	.open = set_sspm_big_limit_threshold_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = set_sspm_big_limit_threshold_write,
	.release = single_release,
};
static int __init thermal_ipi_init(void)
{
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *thermal_ipi_dir = NULL;

	tscpu_printk("[%s]\n", __func__);


	thermal_ipi_dir = mtk_thermal_get_proc_drv_therm_dir_entry();
	if (!thermal_ipi_dir) {
		tscpu_printk(
			"[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	} else {
		entry =
		    proc_create("get_sspm_tz_temp", 444, thermal_ipi_dir,
				&get_sspm_tz_temp_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
		entry =
		    proc_create("set_sspm_big_limit_threshold", 664,
			thermal_ipi_dir, &set_sspm_big_limit_threshold_fops);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	return 0;
}

static void __exit thermal_ipi_exit(void)
{
	tscpu_printk("[%s]\n", __func__);
}

module_init(thermal_ipi_init);
module_exit(thermal_ipi_exit);
#endif
