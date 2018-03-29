#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/list.h>
#include <linux/semaphore.h>
#include <linux/completion.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <asm/cacheflush.h>
#include <linux/delay.h>
#include <linux/irq.h>

#include "ut_keymaster_service.h"
#include "../tz_driver/teei_id.h"

#define MESSAGE_LENGTH 4096

/**
@breif this message buf is used for keymaster message transmit.
*/
unsigned long ut_keymaster_message_buf;
/**
@breif this message buf is used for keymaster message transmit for REE to TEE.
*/
unsigned long ut_keymaster_fmessage_buf;
/**
@breif this message buf is used for keymaster message transmit for TEE to REE.
*/
unsigned long ut_keymaster_bmessage_buf;

struct ut_keymaster_addr_struct {
	unsigned long phy_addr;
	unsigned long f_phy_addr;
	unsigned long b_phy_addr;
};

struct ut_keymaster_addr_struct keymaster_cmdbuf_entry;


static void secondary_init_keymaster_cmdbuf(void *info)
{
	struct ut_keymaster_addr_struct *cd = (struct ut_keymaster_addr_struct *)info;

	/* with a rmb() */
	rmb();

	pr_debug("[%s][%d] keymaster addr= %lx,  keymaster f addr = %lx, keymaster b addr = %lx\n", __func__, __LINE__,
		(unsigned long)cd->phy_addr, (unsigned long)cd->f_phy_addr,
		(unsigned long)cd->b_phy_addr);

	/*
	n_init_t_fc_buf(cd->phy_addr, cd->fdrv_phy_addr, 0);
	n_init_t_fc_buf(cd->bdrv_phy_addr, cd->tlog_phy_addr, 0);
	chx need add  the smc call function to transmit the physical addr from REE to TEE
	*/

	/* with a wmb() */
	wmb();
}


static void init_keymaster_cmd_buf(unsigned long phy_address, unsigned long f_phy_address,
				unsigned long b_phy_address)
{
	int cpu_id = 0;
	keymaster_cmdbuf_entry.phy_addr = phy_address;
	keymaster_cmdbuf_entry.f_phy_addr = f_phy_address;
	keymaster_cmdbuf_entry.b_phy_addr = b_phy_address;


	/* with a wmb() */
	wmb();

	get_online_cpus();
	cpu_id = get_current_cpuid();
	smp_call_function_single(cpu_id, secondary_init_keymaster_cmdbuf, (void *)(&keymaster_cmdbuf_entry), 1);
	put_online_cpus();

	/* with a rmb() */
	rmb();
}


long create_keymaster_cmd_buf(void)
{
	unsigned long irq_status = 0;

	ut_keymaster_message_buf =  (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));

	if (ut_keymaster_message_buf == NULL) {
		pr_err("[%s][%d] Create message buffer failed!\n", __FILE__, __LINE__);
		return -ENOMEM;
	}

	ut_keymaster_fmessage_buf =  (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));

	if (ut_keymaster_fmessage_buf == NULL) {
		pr_err("[%s][%d] Create fdrv message buffer failed!\n", __FILE__, __LINE__);
		free_pages(ut_keymaster_message_buf, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		return -ENOMEM;
	}

	ut_keymaster_bmessage_buf = (unsigned long) __get_free_pages(GFP_KERNEL, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));

	if (ut_keymaster_bmessage_buf == NULL) {
		pr_err("[%s][%d] Create bdrv message buffer failed!\n", __FILE__, __LINE__);
		free_pages(ut_keymaster_message_buf, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		free_pages(ut_keymaster_fmessage_buf, get_order(ROUND_UP(MESSAGE_LENGTH, SZ_4K)));
		return -ENOMEM;
	}


	pr_debug("[%s][%d] message = %lx,  fdrv message = %lx, bdrv_message = %lx\n", __func__, __LINE__,
		(unsigned long)virt_to_phys(ut_keymaster_message_buf),
		(unsigned long)virt_to_phys(ut_keymaster_fmessage_buf),
		(unsigned long)virt_to_phys(ut_keymaster_bmessage_buf));

	init_keymaster_cmd_buf((unsigned long)virt_to_phys(ut_keymaster_message_buf), (unsigned long)virt_to_phys(ut_keymaster_fmessage_buf),
				(unsigned long)virt_to_phys(ut_keymaster_bmessage_buf));

	return 0;
}



