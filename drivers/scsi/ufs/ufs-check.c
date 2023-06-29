#include "ufshcd.h"
#include <linux/of.h>
#include <linux/reboot.h>

void fill_wb_gb(struct ufs_hba *hba, unsigned int segsize, unsigned int unitsize, unsigned int rawval)
{
	int unit;

	if (!hba) {
		pr_err("HBA is null.\n");
		return;
	}
	unit = (segsize * unitsize) >> 1;
	hba->check.wb_gb = (rawval * unit) >> 20;
}
void fill_hpb_gb(struct ufs_hba *hba, unsigned short lu_rgs, int rg_size)
{
	if (hba)
		hba->check.hpb_gb = (lu_rgs * (1ULL << rg_size) * 512) >> 30;
}
void fill_total_gb(struct ufs_hba *hba,  unsigned long long rawval)
{
	if (hba)
		hba->check.total_gb = rawval >> 21;
}
/*
 * Get hw level
 */
extern char *saved_command_line;
static int get_hardware_level(u8 *hwlevel)
{
	char *ptr = (char *)strnstr(saved_command_line, "androidboot.hwlevel=", strlen(saved_command_line));

	if (ptr) {
		memcpy(hwlevel, (ptr + strlen("androidboot.hwlevel=")), 4);
		pr_info("%s: hwlevel is %s\n", __func__, hwlevel);
		return 0;
	}
	else
		return 1;
}

static int check_if_bug_on()
{
	char hwlevel[5] = {'\0'};
	int ret = -1;
	char *reboot_bootloader = "bootloader";

	ret =  get_hardware_level(hwlevel);
	if (!ret && (!strncmp(hwlevel, "P2.1", strlen("P2.1"))
		|| !strncmp(hwlevel, "MP", strlen("MP")))) {
		pr_info("%s: P2.1/P2.2/MP phone is not enable write booster feature, factory image can not turn on!\n",__func__);
		pr_info("%s: If you have any question, please contact memory group!\n", __func__);
		machine_restart(reboot_bootloader);
	}
}
int check_wb_hpb_size(struct ufs_hba *hba)
{
	int ret = -1;
	int total_gb[] = {64, 128, 256, 512};
	int wb_gb[] = {6, 12, 24, 48};
	int hpb_gb = 32;
	int i;
	int total_size_f = 0;

	if (hba->check.total_gb > 512 && hba->check.total_gb <=1024) {
		total_size_f = 1024;
	} else if (hba->check.total_gb > 256) {
		total_size_f = 512;
	} else if (hba->check.total_gb > 128) {
		total_size_f = 256;
	} else if (hba->check.total_gb > 64) {
		total_size_f = 128;
	} else if (hba->check.total_gb > 32) {
		total_size_f = 64;
	} else if (hba->check.total_gb > 16) {
		total_size_f = 32;
	} else if (hba->check.total_gb > 8) {
		total_size_f = 16;
	} else {
		pr_info("ufs total size unknow:%dGB \n", hba->check.total_gb);
		return ret;
	}
	pr_info("ufs total:%dGB wb:%dGB hpb:%dGB\n",
		total_size_f, hba->check.wb_gb, hba->check.hpb_gb);
	for (i = 0; i < ARRAY_SIZE(total_gb); i++) {
		if (total_gb[i] == total_size_f) {
			if (wb_gb[i] == hba->check.wb_gb && hpb_gb == hba->check.hpb_gb)
				return i;
		}
	}

	return -1;
}
void check_hpb_and_tw_provsion(struct ufs_hba *hba)
{
	struct device *dev = hba->dev;
	struct device_node *np = dev->of_node;

	if (of_property_read_bool(np, "disable-wb_hpb"))
		dev_err(hba->dev, "%s: disabled wb_hpb\n", __func__);
	else
		check_if_bug_on();
}

