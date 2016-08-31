/* -----------------------------------------------------------------------------
 * Copyright (c) 2011 Ozmo Inc
 * Released under the GNU General Public License Version 2 (GPLv2).
 * -----------------------------------------------------------------------------
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#include "ozpd.h"
#include "ozcdev.h"
#include "ozproto.h"
#include "oztrace.h"
#include "ozkobject.h"
#include "ozappif.h"

static ssize_t devices_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int i, count, s;
	unsigned state;
	int ret = 0;
	u8 devices[(ETH_ALEN + sizeof(unsigned)) * OZ_MAX_PDS];

	count = oz_get_pd_status_list(devices, OZ_MAX_PDS);
	s = sprintf(buf, "Total: %d\n", count);
	buf += s;
	ret += s;
	for (i = 0; i < count; i++) {
		ret += sprintf(buf, "%pm", (void *)&devices[i * (ETH_ALEN +
						sizeof(unsigned))]);
		buf += (ETH_ALEN * 2);
		ret += sprintf(buf++, "\t");
		memcpy(&state, &devices[(i * (ETH_ALEN + sizeof(unsigned))) +
						ETH_ALEN], sizeof(unsigned));
		switch (state) {
		case OZ_PD_S_IDLE:
			s = sprintf(buf, "IDLE\n");
			buf += s;
			ret += s;
			break;
		case OZ_PD_S_CONNECTED:
			s = sprintf(buf, "CONNECTED\n");
			buf += s;
			ret += s;
			break;
		case OZ_PD_S_STOPPED:
			s = sprintf(buf, "STOPPED\n");
			buf += s;
			ret += s;
			break;
		case OZ_PD_S_SLEEP:
			s = sprintf(buf, "SLEEP\n");
			buf += s;
			ret += s;
			break;
		}

	}
	return ret;
}

u8 oz_str_to_hex(const char *st)
{
	u8 t1 = 0;
	char arr[3];
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39))
	char **pt = NULL;
#endif

	memcpy(arr, st, 2);
	arr[2] = '\0';
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 39))
	t1 = (u8) simple_strtoul(arr, pt, 16);
#else
	if (kstrtou8(arr, 16, &t1))
		oz_trace("Invalid string received\n");
#endif
	return t1;
}

static ssize_t stop_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int i;
	u8 mac_addr[6];
	struct oz_pd *pd;

	if (count >= 12) {
		for (i = 0; i < 6; i++) {
			mac_addr[i] = oz_str_to_hex(buf);
			buf += 2;
		}

		pd = oz_pd_find(mac_addr);
		if (pd && (!(pd->state & OZ_PD_S_CONNECTED))) {
			oz_pd_stop(pd);
			oz_pd_put(pd);
		} else
			oz_pd_put(pd);
	}

	return count;
}

static ssize_t select_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int i;
	int ret = 0;
	u8 mac_addr[6];

	oz_get_active_pd(mac_addr);

	for (i = 0; i < 6; i++) {
		ret += sprintf(buf, "%02x", mac_addr[i]);
		buf += 2;
	}
	ret += sprintf(buf, "\n");
	return ret;
}

static ssize_t select_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	int i;
	u8 mac_addr[6];

	if (count >= 12) {
		for (i = 0; i < 6; i++) {
			mac_addr[i] = oz_str_to_hex(buf);
			buf += 2;
		}

		oz_set_active_pd(mac_addr);
	}
	return count;
}

static ssize_t bind_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	char nw_list[OZ_MAX_NW_IF * OZ_MAX_BINDING_LEN] = {0};
	int count, i, s;
	int ret = 0;

	count = oz_get_binding_list(nw_list, OZ_MAX_NW_IF);
	for (i = 0; i < count; i++) {
		s = sprintf(buf, "%s\n", nw_list + (i * OZ_MAX_BINDING_LEN));
		ret += s;
		buf += s;
	}
		return ret;
}

static ssize_t bind_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	char name[OZ_MAX_BINDING_LEN];
	char *p = NULL;

	memcpy(name, buf + 2, count);
	p = strstr(name, "\n");
	if (p)
		*p = '\0';

	switch (*buf) {
	case 'a':
		oz_binding_add(name);
		break;
	case 'r':
		oz_binding_remove(name);
		break;
	}
	return count;
}

static ssize_t mode_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	u8 mode;
	int ret;

	mode = oz_get_serial_mode();
	ret = sprintf(buf, "0x%02x\n", mode);
	return ret;
}

static ssize_t mode_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	u8 new_mode;
	if (count >= 4) {
		new_mode = oz_str_to_hex(buf + 2);
		oz_set_serial_mode(new_mode);
	} else {
		printk(KERN_ERR "Invalid mode\n");
	}
	return count;
}

static ssize_t debug_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int ret = 0;
	u32 debug = g_debug;
	int i;

	for (i = 0; i < 'Z'-'A'+1; i++) {
		if (debug & (1<<i))
			*(buf + ret++) = 'A' + i;
		}

	return ret;
}

static ssize_t debug_store(struct kobject *kobj, struct kobj_attribute *attr,
				const char *buf, size_t count)
{
	u32 new_debug = 0;
	const char *t = buf;

	if (count > 1 && count < 33) {
		while (*t) {
			char symbol = *t;
			if ('A' <= symbol && symbol <= 'Z')
				new_debug |= 1<<(symbol - 'A');
			t++;
		}

		if (0 != new_debug) {
			g_debug = new_debug;
		}
		else
			printk(KERN_ERR "Invalid debug\n");
	} else {
		if (1 == count && *t == '\0')
			g_debug = 0;
	}
	
	return count;
}

static ssize_t fptr_show(struct kobject *kobj, struct kobj_attribute *attr,
				char *buf)
{
	int ret;

	ret = sprintf(buf, "p->oz_protocol_init = 0x%p\n", oz_protocol_init);
	return ret;

}

static struct kobj_attribute devices_attribute =
	__ATTR(devices, 0400, devices_show, NULL);

static struct kobj_attribute stop_attribute =
	__ATTR(stop, 0200, NULL, stop_store);

static struct kobj_attribute select_attribute =
	__ATTR(select, 0600, select_show, select_store);

static struct kobj_attribute bind_attribute =
	__ATTR(bind, 0600, bind_show, bind_store);

static struct kobj_attribute mode_attribute =
	__ATTR(mode, 0600, mode_show, mode_store);

static struct kobj_attribute debug_attribute =
	__ATTR(debug, 0600, debug_show, debug_store);

static struct kobj_attribute fptr_attribute =
	__ATTR(fptr, 0400, fptr_show, NULL);

static struct attribute *attrs[] = {
	&devices_attribute.attr,
	&stop_attribute.attr,
	&select_attribute.attr,
	&bind_attribute.attr,
	&mode_attribute.attr,
	&debug_attribute.attr,
	&fptr_attribute.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

void oz_create_sys_entry(void)
{
	int retval;

	retval = sysfs_create_group(&g_oz_wpan_dev->kobj, &attr_group);
	if (retval)
		oz_trace("Can not create attribute group\n");

}

void oz_destroy_sys_entry(void)
{
	sysfs_remove_group(&g_oz_wpan_dev->kobj, &attr_group);
}
