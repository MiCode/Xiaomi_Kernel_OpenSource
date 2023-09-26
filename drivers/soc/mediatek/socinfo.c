#include <linux/export.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include  <linux/init.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/types.h>
#include  <soc/mediatek/socinfo.h>

#define TAG "[xiaomi socinfo]: "
#define HWINFO_CMDLINE "hwversion="
#define HWCOUNTRY_CMDLINE "hwc="

struct socinfo_v0 {
	int hw_product;
	int hw_version_major;
	int hw_version_minor;
	char country[15+1];
} socinfo;

int get_hw_product(void)
{
	return socinfo.hw_product;
}
EXPORT_SYMBOL(get_hw_product);

int get_hw_version_major(void)
{
	return socinfo.hw_version_major;
}
EXPORT_SYMBOL(get_hw_version_major);

int get_hw_version_minor(void)
{
	return socinfo.hw_version_minor;
}
EXPORT_SYMBOL(get_hw_version_minor);

char *get_hw_country(void)
{
	return socinfo.country;
}
EXPORT_SYMBOL(get_hw_country);

static inline void socinfo_struct_init(void)
{
	socinfo.hw_product = -1;
	socinfo.hw_version_major = -1;
	socinfo.hw_version_minor = -1;
	memset(socinfo.country, '\0', ARRAY_SIZE(socinfo.country));
	return;
}

int __init socinfo_init(void)
{
	char *hwversion_ptr = strstr(saved_command_line, HWINFO_CMDLINE);
	char *hwcountry_ptr = strstr(saved_command_line, HWCOUNTRY_CMDLINE);
	int n = 0;

	socinfo_struct_init();
	if (hwversion_ptr) {
		n = sscanf(hwversion_ptr, HWINFO_CMDLINE"%d.%d.%d", &socinfo.hw_product, &socinfo.hw_version_major, &socinfo.hw_version_minor);
		if (n == 3) {
			pr_info("%s hw_product = %d,hw_version_major = %d, hw_version_minor = %d\n", TAG, socinfo.hw_product, socinfo.hw_version_major, socinfo.hw_version_minor);
		} else {
			pr_info("%scan not get hwversion info for detail!\n", TAG);
			socinfo_struct_init();
		}

	} else {
		pr_info("%scan not get hwversion info from cmdline\n", TAG);
	}

	if (hwcountry_ptr) {
		sscanf(hwcountry_ptr, HWCOUNTRY_CMDLINE"%s", socinfo.country, sizeof(socinfo.country));
		socinfo.country[sizeof(socinfo.country) - 1] = '\0';
		pr_info("%s hwc = %s\n", TAG, socinfo.country);
	} else {
		pr_info("%scan not get hwcountry info from cmdline\n", TAG);
	}

	return 0;
}
subsys_initcall(socinfo_init);
