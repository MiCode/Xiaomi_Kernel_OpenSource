#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/hwinfo.h>

typedef struct {
	unsigned int touch_info:4;
	unsigned int soc_info:4;
	unsigned int ddr_info:4;
	unsigned int emmc_info:8;
	unsigned int cpu_info:2;
	unsigned int pmic_info:2;
	unsigned int panel_info:4;
	unsigned int tp_maker_info:3;
	unsigned int fp_info:1;
} HW_INFO;

static HW_INFO hw_info;

extern unsigned int get_hw_version_devid(void);

static int hwinfo_proc_show(struct seq_file *m, void *v)
{
	switch (hw_info.emmc_info) {
	case 0x11:
		seq_printf(m, "EMMC: Toshiba\n");
		break;
	case 0x15:
		seq_printf(m, "EMMC: Samsung\n");
		break;
	case 0x45:
		seq_printf(m, "EMMC: Sandisk\n");
		break;
	case 0x90:
		seq_printf(m, "EMMC: Hynix\n");
		break;
	default:
		seq_printf(m, "EMMC: Unknown %x\n", hw_info.emmc_info);
		break;
	}

	switch (hw_info.ddr_info) {
	case 0x01:
		seq_printf(m, "DDR: Samsung\n"); /* 0000 0001B */
		break;
	case 0x02:
		seq_printf(m, "DDR: Hynix\n"); /* 0000 0110B */
		break;
	case 0x03:
		seq_printf(m, "DDR: Elpida\n"); /* 0000 0011B */
		break;
	case 0x04:
		seq_printf(m, "DDR: Micron\n"); /* 1111 1111B */
		break;
	case 0x05:
		seq_printf(m, "DDR: Nanya\n"); /* 0000 0101B */
		break;
	case 0x06:
		seq_printf(m, "DDR: Intel\n"); /* 0000 1110B */
		break;
	default:
		seq_printf(m, "DDR: Unknown %x\n", hw_info.ddr_info);
		break;
	}


	switch (hw_info.touch_info) {
	case 1:
		seq_printf(m, "TOUCH IC: Synaptics\n");
		break;
	case 2:
		seq_printf(m, "TOUCH IC: Atmel\n");
		break;
	case 3:
		seq_printf(m, "TOUCH IC: Focaltech\n");
		break;
	default:
		seq_printf(m, "TOUCH IC: Unknown %x\n", hw_info.touch_info);
		break;
	}

	switch (hw_info.tp_maker_info) {
	case 1:
		seq_printf(m, "TP Maker: Biel\n");
		break;
	case 4:
		seq_printf(m, "TP Maker: Ofilm\n");
		break;
	default:
		seq_printf(m, "TP Maker: Unknown %x\n", hw_info.tp_maker_info);
		break;
	}

	switch (hw_info.panel_info) {
	case 1:
		seq_printf(m, "LCD: AUO FHD R61322 VIDEO PANEL\n");
		break;
	case 4:
		seq_printf(m, "LCD: TIANMA FHD R63350 VIDEO PANEL\n");
		break;
	default:
		seq_printf(m, "LCD: Unknown %x\n", hw_info.panel_info);
		break;
	}

	switch (hw_info.fp_info) {
	case 0:
		seq_printf(m, "Fingerprint: Goodix\n");
		break;
	case 1:
		seq_printf(m, "Fingerprint: FPC\n");
		break;
	default:
		seq_printf(m, "Fingerprint: Unknown %x\n", hw_info.fp_info);
		break;
	}

	return 0;
}


void update_hardware_info(unsigned int type, unsigned int value)
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
	case TYPE_TP_MAKER:
		hw_info.tp_maker_info = value;
		break;
	case TYPE_FP:
		hw_info.fp_info = value;
		break;
	default:
		break;
	}
}

unsigned int get_hardware_info(unsigned int type)
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
	case TYPE_TP_MAKER:
		ret = hw_info.tp_maker_info;
		break;
	case TYPE_FP:
		ret = hw_info.fp_info;
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

static int cpumaxfreq_show(struct seq_file *m, void *v)
{
	seq_printf(m, "2.00\n");
	return 0;
}

static int cpumaxfreq_open(struct inode *inode, struct file *file)
{
	return single_open(file, &cpumaxfreq_show, NULL);
}

static const struct file_operations proc_cpumaxfreq_operations = {
	.open       = cpumaxfreq_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = seq_release,
};


static int __init proc_hwinfo_init(void)
{
	proc_create("cpumaxfreq", 0, NULL, &proc_cpumaxfreq_operations);
	proc_create("hwinfo", 0, NULL, &hwinfo_proc_fops);
	return 0;
}
module_init(proc_hwinfo_init);
