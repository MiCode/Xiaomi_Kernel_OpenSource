#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/hwinfo.h>

typedef struct {
	unsigned int touch_info:4;
	unsigned int soc_info:4;
	unsigned int ddr_info:2;
	unsigned int emmc_info:2;
	unsigned int cpu_info:2;
	unsigned int pmic_info:2;
	unsigned int panel_info:2;
	unsigned int reserved_bit:14;
} HW_INFO;

static HW_INFO hw_info;

static int hwinfo_proc_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%x\n",
			*(unsigned int *)&hw_info);

	switch (hw_info.emmc_info) {
	case 0x01:
		seq_printf(m, "EMMC: Toshiba\n");
		break;
	case 0x02:
		seq_printf(m, "EMMC: Sandisk\n");
		break;
	default:
		seq_printf(m, "EMMC: UNKNOWN\n");
		break;
	}

	if (hw_info.ddr_info == 0x01)
		seq_printf(m, "DDR: Samsung\n");
	else if (hw_info.ddr_info == 0x03)
		seq_printf(m, "DDR: Elpida\n");
	else if (hw_info.ddr_info == 0x02)
		seq_printf(m, "DDR: Hynix\n");
	else
		seq_printf(m, "DDR: UNKNOWN\n");

	switch (hw_info.panel_info) {
	case 0x00:
		seq_printf(m, "LCD: AUO Panel\n");
		break;
	case 0x01:
		seq_printf(m, "LCD: Sharp Panel\n");
		break;
	default:
		break;
	}
	return 0;
}

void update_hardware_info (unsigned int type, unsigned int value)
{
	switch (type) {
	case TYPE_TOUCH:
		hw_info.touch_info = value;
		break;
	case TYPE_SOC:
		hw_info.soc_info = value;
		break;
	case TYPE_DDR:
		hw_info.ddr_info = value;
		break;
	case TYPE_EMMC:
		hw_info.emmc_info = value;
		break;
	case TYPE_CPU:
		hw_info.cpu_info = value;
		break;
	case TYPE_PMIC:
		hw_info.pmic_info = value;
		break;
	case TYPE_PANEL:
		hw_info.panel_info = value;
		break;
	default:
		break;
	}
}

unsigned int get_hardware_info (unsigned int type)
{
	unsigned int ret = 0xFF;

	switch (type) {
	case TYPE_TOUCH:
		ret = hw_info.touch_info;
		break;
	case TYPE_SOC:
		ret = hw_info.soc_info;
		break;
	case TYPE_DDR:
		ret = hw_info.ddr_info;
		break;
	case TYPE_EMMC:
		ret = hw_info.emmc_info;
		break;
	case TYPE_CPU:
		ret = hw_info.cpu_info;
		break;
	case TYPE_PMIC:
		ret = hw_info.pmic_info;
		break;
	case TYPE_PANEL:
		ret = hw_info.panel_info;
		break;
	default:
		break;
	}

	return ret;
}

static int hwinfo_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hwinfo_proc_show, NULL);
}

static const struct file_operations hwinfo_proc_fops = {
	.open		= hwinfo_proc_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_hwinfo_init(void)
{
	proc_create("hwinfo", 0, NULL, &hwinfo_proc_fops);
	return 0;
}
module_init(proc_hwinfo_init);
