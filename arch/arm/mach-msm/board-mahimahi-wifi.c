/* linux/arch/arm/mach-msm/board-mahimahi-wifi.c
*/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <asm/mach-types.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/skbuff.h>
#include <linux/wlan_plat.h>

#include "board-mahimahi.h"

int mahimahi_wifi_power(int on);
int mahimahi_wifi_reset(int on);
int mahimahi_wifi_set_carddetect(int on);

#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

static void *mahimahi_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}

int __init mahimahi_init_wifi_mem(void)
{
	int i;

	for(i=0;( i < WLAN_SKB_BUF_NUM );i++) {
		if (i < (WLAN_SKB_BUF_NUM/2))
			wlan_static_skb[i] = dev_alloc_skb(4096);
		else
			wlan_static_skb[i] = dev_alloc_skb(8192);
	}
	for(i=0;( i < PREALLOC_WLAN_NUMBER_OF_SECTIONS );i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			return -ENOMEM;
	}
	return 0;
}

static struct resource mahimahi_wifi_resources[] = {
	[0] = {
		.name		= "bcm4329_wlan_irq",
		.start		= MSM_GPIO_TO_INT(MAHIMAHI_GPIO_WIFI_IRQ),
		.end		= MSM_GPIO_TO_INT(MAHIMAHI_GPIO_WIFI_IRQ),
		.flags          = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE,
	},
};

static struct wifi_platform_data mahimahi_wifi_control = {
	.set_power      = mahimahi_wifi_power,
	.set_reset      = mahimahi_wifi_reset,
	.set_carddetect = mahimahi_wifi_set_carddetect,
	.mem_prealloc	= mahimahi_wifi_mem_prealloc,
};

static struct platform_device mahimahi_wifi_device = {
        .name           = "bcm4329_wlan",
        .id             = 1,
        .num_resources  = ARRAY_SIZE(mahimahi_wifi_resources),
        .resource       = mahimahi_wifi_resources,
        .dev            = {
                .platform_data = &mahimahi_wifi_control,
        },
};

extern unsigned char *get_wifi_nvs_ram(void);
extern int wifi_calibration_size_set(void);

static unsigned mahimahi_wifi_update_nvs(char *str, int add_flag)
{
#define NVS_LEN_OFFSET		0x0C
#define NVS_DATA_OFFSET		0x40
	unsigned char *ptr;
	unsigned len;

	if (!str)
		return -EINVAL;
	ptr = get_wifi_nvs_ram();
	/* Size in format LE assumed */
	memcpy(&len, ptr + NVS_LEN_OFFSET, sizeof(len));
	/* if the last byte in NVRAM is 0, trim it */
	if (ptr[NVS_DATA_OFFSET + len - 1] == 0)
		len -= 1;
	if (add_flag) {
		strcpy(ptr + NVS_DATA_OFFSET + len, str);
		len += strlen(str);
	} else {
		if (strnstr(ptr + NVS_DATA_OFFSET, str, len))
			len -= strlen(str);
	}
	memcpy(ptr + NVS_LEN_OFFSET, &len, sizeof(len));
	wifi_calibration_size_set();
	return 0;
}

static int __init mahimahi_wifi_init(void)
{
	int ret;

	if (!machine_is_mahimahi())
		return 0;

	printk("%s: start\n", __func__);
	mahimahi_wifi_update_nvs("sd_oobonly=1\r\n", 0);
	mahimahi_wifi_update_nvs("btc_params70=0x32\r\n", 1);
	mahimahi_init_wifi_mem();
	ret = platform_device_register(&mahimahi_wifi_device);
        return ret;
}

late_initcall(mahimahi_wifi_init);
