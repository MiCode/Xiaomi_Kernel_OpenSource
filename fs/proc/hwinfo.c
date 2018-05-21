#include <linux/fs.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel_stat.h>
#include <linux/hwinfo.h>
#include <linux/cpu.h>

typedef struct {
	unsigned int touch_info:4;
	unsigned int soc_info:4;
	unsigned int ddr_info:4;
	unsigned int emmc_info:16;
	unsigned int cpu_info:2;
	unsigned int pmic_info:2;
	unsigned int panel_info:4;
	unsigned int tp_maker_info:3;
	unsigned int reserved_bit:1;
} HW_INFO;

static HW_INFO hw_info;

#define A1		1
#define A2		2
#define A4		4
#define A7		7
#define A8		8
#define B7		9

struct cpu_freq_info {
	int hwid;
	char freq[5];
};

/*
 * The below table configs the cpu frequency showed by /proc/cpumaxfreq.
 * And if there is no config freq in this table, /proc/cpumaxfreq will show
 * the max frequency through checking cpu freq_table.
 */
static struct cpu_freq_info cpu_maxfreq_list[] = {
	{A1, "2.15"},
	{A2, "1.80"},
	{A4, "2.35"},
	{A7, "2.15"},
	{A8, "2.35"},
	{B7, "2.35"},
};

extern unsigned int get_hw_version_devid(void);

static int hwinfo_proc_show(struct seq_file *m, void *v)
{
	switch (hw_info.emmc_info) {
	case 0x0198:
		seq_printf(m, "UFS: Toshiba\n");
		break;
	case 0x01ce:
		seq_printf(m, "UFS: Samsung\n");
		break;
	default:
		seq_printf(m, "UFS: Unknown %x\n", hw_info.emmc_info);
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
	if (A1 == get_hw_version_devid()) {
		switch (hw_info.panel_info) {
		case 0:
			seq_printf(m, "LCD: JDI FHD R63452 CMD PANEL\n");
			break;
		case 1:
			seq_printf(m, "LCD: LGD FHD TD4322 CMD PANEL\n");
			break;
		case 2:
			seq_printf(m, "LCD: SHARP FHD NT35695 CMD PANEL\n");
			break;
		case 3:
			seq_printf(m, "LCD: JDI FHD R63452 J1 CMD PANEL\n");
			break;
		default:

			break;
		}
	} else if (A2 == get_hw_version_devid()) {
		switch (hw_info.panel_info) {
		case 0:
			seq_printf(m, "LCD: JDI FHD R63452 CMD PANEL\n");
			break;
		case 1:
			seq_printf(m, "LCD: LGD FHD TD4322 CMD PANEL\n");
			break;
		case 2:
			seq_printf(m, "LCD: SHARP FHD NT35695 CMD PANEL\n");
			break;
		case 3:
			seq_printf(m, "LCD: JDI FHD R63452 J1 CMD PANEL\n");
			break;
		default:

			break;
		}
	} else if (A4 == get_hw_version_devid()) {
		switch (hw_info.panel_info) {
		case 0:
			seq_printf(m, "LCD: dual samsung YOUM Qhd command mode dsi panel\n");
			break;
		case 9:
			seq_printf(m, "LCD: LGD FHD SW43101 VIDEO OLED PANEL\n");
			break;
		case 10:
			seq_printf(m, "LCD: LGD FHD SW43101 P2 VIDEO OLED PANEL\n");
			break;
		default:
			seq_printf(m, "LCD: Unknown %x\n", hw_info.panel_info);
			break;
		}
	} else if (A7 == get_hw_version_devid()) {
		switch (hw_info.panel_info) {
		case 0:
			seq_printf(m, "LCD: JDI FHD R63452 CMD PANEL\n");
			break;
		case 1:
			seq_printf(m, "LCD: LGD FHD TD4322 CMD PANEL\n");
			break;
		case 2:
			seq_printf(m, "LCD: SHARP FHD NT35695 CMD PANEL\n");
			break;
		case 3:
			seq_printf(m, "LCD: JDI FHD R63452 J1 CMD PANEL\n");
			break;
		default:

			break;
		}
	} else if (A8 == get_hw_version_devid()) {
		switch (hw_info.panel_info) {
		case 0:
			seq_printf(m, "LCD: SHARP FHD FTE716 VIDEO PANEL\n");
			break;
		case 5:
			seq_printf(m, "LCD: AUO FHD FTE716 VIDEO PANEL\n");
			break;
		default:

				break;
		}
	} else if (B7 == get_hw_version_devid()) {
		switch (hw_info.panel_info) {
		case 0:
			seq_printf(m, "LCD: JDI FHD R63452 CMD PANEL\n");
			break;
		case 1:
			seq_printf(m, "LCD: SHARP FHD TD4322 CMD PANEL\n");
			break;
		default:

			break;
		}
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

		break;
	}

	switch (hw_info.tp_maker_info) {
	case 1:
		seq_printf(m, "TP Maker: Biel D1\n");
		break;
	case 2:
		seq_printf(m, "TP Maker: Lens\n");
		break;
	case 3:
		seq_printf(m, "TP Maker: TPK\n");
		break;
	case 4:
		seq_printf(m, "TP Maker: Biel TPB\n");
		break;
	case 5:
		seq_printf(m, "TP Maker: Sharp\n");
		break;
	case 6:
		seq_printf(m, "TP Maker: Ofilm\n");
		break;
	default:

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
	int i;
	int cnt;
	int maxfreq;

	cnt = sizeof(cpu_maxfreq_list) / sizeof(struct cpu_freq_info);

	for (i = 0; i < cnt; i++) {
		if (cpu_maxfreq_list[i].hwid == get_hw_version_devid()) {
			seq_printf(m, "%s\n", cpu_maxfreq_list[i].freq);
			return 0;
		}
	}

	maxfreq = get_cpu_maxfreq();
	seq_printf(m, "%d.%d\n",
				maxfreq / 1000000,
				maxfreq % 1000000 / 10000);

	return 0;
}

static int cpumaxfreq_open(struct inode *inode, struct file *file)
{
	return single_open(file, &cpumaxfreq_show, NULL);
}

static const struct file_operations proc_cpumaxfreq_operations = {
	.open		= cpumaxfreq_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.release	= seq_release,
};

static int __init proc_hwinfo_init(void)
{
	proc_create("cpumaxfreq", 0, NULL, &proc_cpumaxfreq_operations);
	proc_create("hwinfo", 0, NULL, &hwinfo_proc_fops);
	return 0;
}
module_init(proc_hwinfo_init);
