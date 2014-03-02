/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <asm/io.h>
#include <asm/div64.h>

#define PP2S_MODULE_NAME "PP2S"
#define WC_MODULE_NAME   "wallclock"

/*
 * Logging macros...
 */
#define LOG_DRVR_DEBUG(args...) pr_debug(args)
#define LOG_DRVR_ERR(args...)   pr_err(args)

/*
 * The following defines how many sysfs nodes the driver is creating.
 */
#define NUM_PP2S_SYSFS_REG_NODES 1

#define NUM_WC_SYSFS_REG_NODES 6

#define ATTR_PTR_SIZE sizeof(struct attribute *)

/*
 * The following deal with register locations in virtual memory.
 *
 * More specifically, we're mapping in base addresses and a length
 * (that come from the device tree) during the probe and then the
 * registers are just offsets from their respective base...
 */
#define BASEADDR_PLUS(base, off)                        \
	((void *) ((u32) (base) + (u32) (off)))

#define WC_SECS_ADDR       BASEADDR_PLUS(time_bank,  0x00000014)
#define WC_NSECS_ADDR      BASEADDR_PLUS(time_bank,  0x00000018)
#define WC_CONTROL_ADDR    BASEADDR_PLUS(cntrl_bank, 0x00000020)
#define WC_PULSECNT_ADDR   BASEADDR_PLUS(cntrl_bank, 0x00000024)
#define WC_CLOCKCNT_ADDR   BASEADDR_PLUS(cntrl_bank, 0x0000002C)
#define WC_SNAPSHOT_ADDR   BASEADDR_PLUS(cntrl_bank, 0x00000028)

/*
 * Number of bits in the nanosecond register to be used for the
 * storage of leap seconds...
 */
#define BITS_FOR_LEAP 6

/*
 * The rate the free running clock runs at...
 */
#define CLK_RATE 122880000

/*
 * The difference (in seconds) between the unix and gps epochs...
 */
#define UNIX_EPOCH_TO_GPS_EPOCH_GAP 315964819

/*
 * These macros assist with creating sysfs attributes...
 */
#define MAKE_RO_ATTR(_name, _ptr, _index)                             \
	(_ptr)->kobj_attr_##_name.attr.name = __stringify(_name);         \
	(_ptr)->kobj_attr_##_name.attr.mode = S_IRUSR;                    \
	(_ptr)->kobj_attr_##_name.show = read_##_name;                    \
	(_ptr)->kobj_attr_##_name.store = NULL;                           \
	sysfs_attr_init(&(_ptr)->kobj_attr_##_name.attr);                 \
	(_ptr)->attr_grp.attrs[_index] = &(_ptr)->kobj_attr_##_name.attr;

#define MAKE_RW_ATTR(_name, _ptr, _index)                             \
	(_ptr)->kobj_attr_##_name.attr.name = __stringify(_name);         \
	(_ptr)->kobj_attr_##_name.attr.mode = S_IRUSR|S_IWUSR;            \
	(_ptr)->kobj_attr_##_name.show = read_##_name;                    \
	(_ptr)->kobj_attr_##_name.store = write_##_name;                  \
	sysfs_attr_init(&(_ptr)->kobj_attr_##_name.attr);                 \
	(_ptr)->attr_grp.attrs[_index] = &(_ptr)->kobj_attr_##_name.attr;

/*
 * The following will break any pending poll/select(s) on a sysfs
 * node...
 */
#define RELEASE_SELECT_ON_ATTR(_ptr, _attr)                     \
	sysfs_notify((_ptr)->kobj, NULL, __stringify(_attr))

/*
 * The following is for the creation of pp2s sysfs entries.
 */
struct pp2s_sysfs_t {
	struct kobject        *kobj;
	struct attribute_group attr_grp;

	struct kobj_attribute  kobj_attr_pp2s_cnt;
	unsigned long long     pp2s_cnt;
};

/*
 * The following is for the creation of wallclock sysfs entries.
 */
struct wc_sysfs_t {
	struct kobject        *kobj;
	struct attribute_group attr_grp;
	struct kobj_attribute  kobj_attr_cntrl_reg;
	u32                    cntrl_reg;
	struct kobj_attribute  kobj_attr_secs_reg;
	u32                    secs_reg;
	struct kobj_attribute  kobj_attr_nsecs_reg;
	u32                    nsecs_reg;
	struct kobj_attribute  kobj_attr_pulsecnt_reg;
	u32                    pulsecnt_reg;
	struct kobj_attribute  kobj_attr_clockcnt_reg;
	u32                    clockcnt_reg;
	struct kobj_attribute  kobj_attr_snapshot_reg;
	u32                    snapshot_reg;
};

/*
 * Some rudimentary internal state...
 */
static int                      pp2s_irq;
static struct pp2s_sysfs_t      pp2s_sysfs;
static struct workqueue_struct *workqueue;
static struct work_struct       bottom_work;
static struct wc_sysfs_t        wc_sysfs;

/*
 * The following will be set to point at the two regions where the
 * wall clock registers in question live.
 *
 * More specifically:
 *
 *   *time_bank : Will be used to point at the region which holds two
 *                time registers (ie. the base time or "epoch" when
 *                the time was set), and
 *
 *   *cntrl_bank : A set of registers used to make current wall clock
 *                 time calculations relative to the base time...
 */
static void *time_bank;
static void *cntrl_bank;

/*
 * The following function takes GPS time from the FTM and converts it
 * to unix time.
 *
 * OF NOTE:
 *
 *   Upon hard sync, TFCS seeds the FTM time registers used by this
 *   function with GPS time.
 *
 *   GPS time is stored in the registers in the following way:
 *
 *   1) Two registers, referred to as the base time registers, are
 *      seeded with the time that the first PP2S arrived.  These
 *      registers never change value once set.
 *
 *   2) Two other register, "pulse count" and "snap shot," hold
 *      dynamic values that, when combined with the base time, yield
 *      the actual GPS time.
 *
 *   The "pulse count" count register holds the number of PP2S pulses
 *   that have transpired prior to the register being read.  The "snap
 *   shot" register is a latch register whose value is set by simply
 *   doing a read of the "pulse count" register.  The value being
 *   latched is that of the "clock register" which is a free running
 *   clock that's running at 122.88 MHz.  Its value goes back to 0 and
 *   free runs again on every PP2S pusle.
 *
 * Given the above, we initally calculate GPS time which then gets
 * converted to unix time thusly:
 */
static void gps_to_unix_time(
	struct timespec *ts_ptr)
{
	u32 pulses = readl_relaxed(WC_PULSECNT_ADDR);
	u32 clkCnt = readl_relaxed(WC_SNAPSHOT_ADDR);
	u32 secs   = readl_relaxed(WC_SECS_ADDR);
	u32 nsecs  = readl_relaxed(WC_NSECS_ADDR);
	u32 leaps;
	u32 rmndr, unused;
	u64 q;

	/*
	 * NOTE:
	 *
	 *   There is some bit manipulation of the nano second value
	 *   below.  This is because the nsecs register is doing duel
	 *   duty.  It not only stores nano seconds, but also, the first
	 *   BITS_FOR_LEAP low order bits are used to store leap seconds.
	 *   The remaining high order bits hold a scaled (by a factor of
	 *   16) version of nano seconds.
	 */
	leaps = nsecs & ((1 << BITS_FOR_LEAP) - 1);

	nsecs >>= BITS_FOR_LEAP;
	nsecs <<= 4;

	/*
	 * Convert PP2S pules to seconds and add that in. NOTE: The first
	 * pulse ignored, since it's the epoch...
	 */
	secs += ((pulses - 1) * 2);

	/*
	 * Now convert the free running clock into seconds and nanoseconds
	 * and add that in as well...
	 */
	secs += (u32) div_u64_rem((u64) clkCnt, CLK_RATE, &rmndr);

	q = div_u64_rem((u64) NSEC_PER_SEC, CLK_RATE, &unused);

	nsecs += (u32) ((u64) rmndr * q);

	/*
	 * And finally, the actual conversion from GPS to unix time...
	 */
	ts_ptr->tv_sec  = secs + (UNIX_EPOCH_TO_GPS_EPOCH_GAP - leaps);
	ts_ptr->tv_nsec = nsecs;
}

/*
 * Given a register address and a storage location address, this
 * routine will read a value from the register address and store it at
 * the location address.
 */
static ssize_t register_read(
	const char *caller_ptr,
	void       *reg_addr,
	char       *valfromreg_ptr)
{
	u32 val = readl_relaxed(reg_addr);

	val = scnprintf(valfromreg_ptr, PAGE_SIZE, "%u", val);

	LOG_DRVR_DEBUG(
		"%s -> read %s (from addr %p) with %u bytes\n",
		caller_ptr, valfromreg_ptr, reg_addr, val);

	return val;
}

/*
 * Given a register address and an address of a value, this routine
 * will take the value and copy it to the register address...
 */
static ssize_t register_write(
	const char *caller_ptr,
	void       *reg_addr,
	const char *valtoreg_ptr)
{
	u32 val = 0;

	sscanf(valtoreg_ptr, "%u", &val);

	writel_relaxed(val, reg_addr);

	LOG_DRVR_DEBUG(
		"%s -> wrote %u (to addr %p) with %u bytes\n",
		caller_ptr, val, reg_addr, sizeof(val));

	return strlen(valtoreg_ptr);
}

/*
 * The sysfs routine which is called when an application issues a read
 * on the cntrl_reg sysfs node...
 */
static ssize_t read_cntrl_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	char                  *buf)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_read(__func__, WC_CONTROL_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a
 * write on the cntrl_reg sysfs node...
 */
static ssize_t write_cntrl_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	const char            *buf,
	size_t                 count)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_write(__func__, WC_CONTROL_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a read
 * on the secs_reg sysfs node...
 */
static ssize_t read_secs_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	char                  *buf)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_read(__func__, WC_SECS_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a
 * write on the secs_reg sysfs node...
 */
static ssize_t write_secs_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	const char            *buf,
	size_t                 count)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_write(__func__, WC_SECS_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a read
 * on the nsecs_reg sysfs node...
 */
static ssize_t read_nsecs_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	char                  *buf)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_read(__func__, WC_NSECS_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a
 * write on the nsecs_reg sysfs node...
 */
static ssize_t write_nsecs_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	const char            *buf,
	size_t                 count)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_write(__func__, WC_NSECS_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a read
 * on the pulsecnt_reg sysfs node...
 */
static ssize_t read_pulsecnt_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	char                  *buf)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_read(__func__, WC_PULSECNT_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a
 * write on the pulsecnt_reg sysfs node...
 */
static ssize_t write_pulsecnt_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	const char            *buf,
	size_t                 count)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_write(__func__, WC_PULSECNT_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a read
 * on the clockcnt_reg sysfs node...
 */
static ssize_t read_clockcnt_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	char                  *buf)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_read(__func__, WC_CLOCKCNT_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a
 * write on the clockcnt_reg sysfs node...
 */
static ssize_t write_clockcnt_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	const char            *buf,
	size_t                 count)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_write(__func__, WC_CLOCKCNT_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a read
 * on the snapshot_reg sysfs node...
 */
static ssize_t read_snapshot_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	char                  *buf)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_read(__func__, WC_SNAPSHOT_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a
 * write on the snapshot_reg sysfs node...
 */
static ssize_t write_snapshot_reg(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	const char            *buf,
	size_t                 count)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return register_write(__func__, WC_SNAPSHOT_ADDR, buf);
}

/*
 * The sysfs routine which is called when an application issues a read
 * on the pp2s_cnt sysfs node...
 */
static ssize_t read_pp2s_cnt(
	struct kobject        *kobj,
	struct kobj_attribute *attr,
	char                  *buf)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	return read_pulsecnt_reg(kobj, attr, buf);
}

/*
 * The pp2s interrupt handler top half schedules the following to be
 * run upon reception of the pp2s interrupt, so that any polls or
 * selects on the pp2s_cnt sysfs node can be released...
 */
static void pp2s_intr_bottom(
	struct work_struct *work)
{
	static int done = -1;

	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	/*
	 * On the first PP2S interrupt, GPS time will be set in the FTM
	 * and we'll have the first pass through this function.  At that
	 * point, it is required to take GPS time, convert it to unix
	 * time, and to set it in the kernel, hence...
	 */
	if (done <= 0) {
		struct timespec ts;
		gps_to_unix_time(&ts);
		done = !do_settimeofday(&ts);
		if (!done)
			LOG_DRVR_ERR("do_settimeofday() failed\n");
	}

	RELEASE_SELECT_ON_ATTR(&pp2s_sysfs, pp2s_cnt);
}

/*
 * The following is called upon receipt of the pp2s interrupt.  It
 * simply schedules pp2s_intr_bottom to be called...
 */
static irqreturn_t pp2s_intr_top(
	int   irq,
	void *arbDatPtr)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	INIT_WORK(&bottom_work, pp2s_intr_bottom);

	queue_work(workqueue, &bottom_work);

	pp2s_sysfs.pp2s_cnt++;

	return IRQ_HANDLED;
}

/*
 * The following will probe the device tree file for this driver's IRQ
 * number.  Once found, it will be enabled after a top half interrupt
 * handler is associated with the IRQ.
 *
 * This routine will also create the pp2s_cnt sysfs node...
 */
static int pp2s_probe(
	struct platform_device *pdev)
{
	struct resource *r;
	int              ret = 0;

	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	r = platform_get_resource_byname(pdev, IORESOURCE_IRQ, "pp2s_irq");

	if (r == 0) {
		LOG_DRVR_ERR(
			"%s: platform_get_resource_byname() failed\n",
			__func__);
		ret = -ENODEV;
		goto perr0;
	}

	pp2s_irq = r->start;

	LOG_DRVR_DEBUG(
		"%s: retrieved irq (%d) from device tree\n",
		__func__, pp2s_irq);

	ret = request_irq(
		pp2s_irq,
		pp2s_intr_top,
		IRQF_TRIGGER_RISING,
		PP2S_MODULE_NAME,
		0);

	if (ret != 0) {
		LOG_DRVR_ERR(
			"%s: request_irq() failed\n",
			__func__);
		goto perr0;
	}

	enable_irq(pp2s_irq);

	/*
	 * Create the sysfs obj...
	 */
	pp2s_sysfs.kobj = kobject_create_and_add("pp2s", kernel_kobj);

	if (pp2s_sysfs.kobj == 0) {
		ret = -ENOMEM;
		goto perr1;
	}

	/*
	 * Allocate space for sysfs attrs...
	 */
	pp2s_sysfs.attr_grp.attrs =
		kzalloc(ATTR_PTR_SIZE * NUM_PP2S_SYSFS_REG_NODES, GFP_KERNEL);

	if (pp2s_sysfs.attr_grp.attrs == 0) {
		ret = -ENOMEM;
		goto perr2;
	}

	/*
	 * Add sysfs attr...
	 */
	MAKE_RO_ATTR(pp2s_cnt, &pp2s_sysfs, 0);

	/*
	 * Create sysfs group...
	 */
	ret = sysfs_create_group(pp2s_sysfs.kobj, &pp2s_sysfs.attr_grp);

	if (ret != 0)
		goto perr3;

	return ret;

perr3:
	kzfree(pp2s_sysfs.attr_grp.attrs);
	pp2s_sysfs.attr_grp.attrs = 0;
perr2:
	kobject_put(pp2s_sysfs.kobj);
	pp2s_sysfs.kobj = 0;
perr1:
	disable_irq(pp2s_irq);
	free_irq(pp2s_irq, 0);
perr0:
	return ret;
}

/*
 * This routine will read the device tree file and initialize a few
 * important wallclock data structures...
 */
static int wc_probe(
	struct platform_device *pdev)
{
	struct resource *r;
	int              ret = 0;

	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	/*
	 * Find the address of the time bank and map it in...
	 */
	r = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "wallclock_time_bank");

	if (r == 0) {
		LOG_DRVR_ERR(
			"%s: platform_get_resource_byname(time_bank) failed\n",
			__func__);
		ret = -ENODEV;
		goto werr0;
	}

	LOG_DRVR_DEBUG(
		"%s: time_bank start(%d) size(%d)\n",
		__func__, r->start, resource_size(r));

	time_bank = ioremap_nocache(r->start, resource_size(r));

	if (time_bank == 0) {
		LOG_DRVR_ERR(
			"%s: ioremap(time_bank) failed\n",
			__func__);
		ret = -ENOMEM;
		goto werr0;
	}

	/*
	 * Find the address of the control bank and map it in...
	 */
	r = platform_get_resource_byname(
		pdev, IORESOURCE_MEM, "wallclock_cntrl_bank");

	if (r == 0) {
		LOG_DRVR_ERR(
			"%s: platform_get_resource_byname(cntrl_bank) failed\n",
			__func__);
		ret = -ENODEV;
		goto werr1;
	}

	LOG_DRVR_DEBUG(
		"%s: cntrl_bank start(%d) size(%d)\n",
		__func__, r->start, resource_size(r));

	cntrl_bank = ioremap_nocache(r->start, resource_size(r));

	if (cntrl_bank == 0) {
		LOG_DRVR_ERR(
			"%s: ioremap(cntrl_bank) failed\n",
			__func__);
		ret = -ENOMEM;
		goto werr1;
	}

	/*
	 * Create the sysfs obj...
	 */
	wc_sysfs.kobj = kobject_create_and_add("wall_clock", kernel_kobj);

	if (wc_sysfs.kobj == 0) {
		ret = -ENOMEM;
		goto werr2;
	}

	/*
	 * Allocate space for sysfs attrs...
	 */
	wc_sysfs.attr_grp.attrs =
		kzalloc(ATTR_PTR_SIZE * NUM_WC_SYSFS_REG_NODES, GFP_KERNEL);

	if (wc_sysfs.attr_grp.attrs == 0) {
		ret = -ENOMEM;
		goto werr3;
	}

	/*
	 * Add sysfs attrs...
	 */
	MAKE_RW_ATTR(cntrl_reg,    &wc_sysfs, 0);
	MAKE_RW_ATTR(secs_reg,     &wc_sysfs, 1);
	MAKE_RW_ATTR(nsecs_reg,    &wc_sysfs, 2);
	MAKE_RW_ATTR(pulsecnt_reg, &wc_sysfs, 3);
	MAKE_RW_ATTR(clockcnt_reg, &wc_sysfs, 4);
	MAKE_RW_ATTR(snapshot_reg, &wc_sysfs, 5);

	/*
	 * Create sysfs group...
	 */
	ret = sysfs_create_group(wc_sysfs.kobj, &wc_sysfs.attr_grp);

	if (ret != 0)
		goto werr4;

	return ret;

werr4:
	kzfree(wc_sysfs.attr_grp.attrs);
	wc_sysfs.attr_grp.attrs = 0;
werr3:
	kobject_put(wc_sysfs.kobj);
	wc_sysfs.kobj = 0;
werr2:
	iounmap(cntrl_bank);
	cntrl_bank = 0;
werr1:
	iounmap(time_bank);
	time_bank = 0;
werr0:
	return ret;
}

/*
 * Free up pp2s resources upon driver removal.
 */
static int pp2s_remove(
	struct platform_device *pdev)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	kzfree(pp2s_sysfs.attr_grp.attrs);
	pp2s_sysfs.attr_grp.attrs = 0;

	kobject_put(pp2s_sysfs.kobj);
	pp2s_sysfs.kobj = 0;

	disable_irq(pp2s_irq);
	free_irq(pp2s_irq, 0);

	return 0;
}

/*
 * Free up wallclock resources upon driver removal.
 */
static int wc_remove(
	struct platform_device *pdev)
{
	LOG_DRVR_DEBUG("Entering %s\n", __func__);

	kzfree(wc_sysfs.attr_grp.attrs);
	wc_sysfs.attr_grp.attrs = 0;

	kobject_put(wc_sysfs.kobj);
	wc_sysfs.kobj = 0;

	iounmap(time_bank);
	time_bank = 0;

	iounmap(cntrl_bank);
	cntrl_bank = 0;

	return 0;
}

/*
 * All of the following are standard platform driver initialization
 * calls...
 */
static struct of_device_id pp2s_match_table[] = {
	{ .compatible = "qcom,pp2s", },
	{}
};

static struct platform_driver pp2s_driver = {
	.probe  = pp2s_probe,
	.remove = pp2s_remove,
	.driver = {
		.name           = PP2S_MODULE_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = pp2s_match_table,
	},
};

static struct of_device_id wc_match_table[] = {
	{ .compatible = "qcom,wallclock", },
	{}
};

static struct platform_driver wc_driver = {
	.probe  = wc_probe,
	.remove = wc_remove,
	.driver = {
		.name           = WC_MODULE_NAME,
		.owner          = THIS_MODULE,
		.of_match_table = wc_match_table,
	},
};

/*
 * Called when device driver module loaded...
 */
static int __init pp2s_init(void)
{
	int ret;

	LOG_DRVR_DEBUG(
		"%s: PP2S interrupt notifier initialization\n",
		__func__);

	memset(&pp2s_sysfs, 0, sizeof(pp2s_sysfs));

	workqueue = create_singlethread_workqueue("pp2s_wq");

	if (workqueue == 0) {
		LOG_DRVR_ERR(
			"%s: create_singlethread_workqueue() failed\n",
			__func__);
		return -ENOMEM;
	}

	ret = platform_driver_register(&pp2s_driver);

	if (ret) {
		LOG_DRVR_ERR(
		  "%s: Registration of %s failed\n",
		  __func__,
		  PP2S_MODULE_NAME);
		return ret;
	}

	ret = platform_driver_register(&wc_driver);

	if (ret) {
		LOG_DRVR_ERR(
		  "%s: Registration of %s failed\n",
		  __func__,
		  WC_MODULE_NAME);
		return ret;
	}

	return 0;
}

/*
 * Called when device driver module unloaded...
 */
static void __exit pp2s_exit(void)
{
	LOG_DRVR_DEBUG(
		"%s: PP2S interrupt notifier de-initialization\n",
		__func__);

	platform_driver_unregister(&pp2s_driver);
	platform_driver_unregister(&wc_driver);
}

MODULE_DESCRIPTION("FSM PP2S Interrupt Notification Driver");

module_init(pp2s_init);
module_exit(pp2s_exit);
