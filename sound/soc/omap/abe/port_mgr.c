/*
 * ALSA SoC OMAP ABE port manager
 *
 * Author: Liam Girdwood <lrg@slimlogic.co.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

//#define DEBUG

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include "port_mgr.h"
#include "abe_main.h"

/* this must match logical ID numbers in port_mgr.h */
static const char *lport_name[] = {
		"dmic0", "dmic1", "dmic2", "pdmdl1", "pdmdl2", "pdmvib",
		"pdmul1", "bt_vx_dl", "bt_vx_ul", "mm_ext_ul", "mm_ext_dl",
		"mm_dl1", "mm_ul1", "mm_ul2", "vx_dl", "vx_ul", "vib", "tones"
};

static DEFINE_MUTEX(port_mgr_mutex);
static struct abe *the_abe = NULL;
static int users = 0;

/*
 * Get the Physical port ID based on the logical port ID
 *
 * FE and BE ports have unique ID's within the driver but share
 * ID's within the ABE. This maps a driver port ID to an ABE port ID.
 */
static int get_physical_id(int logical_id)
{
	switch (logical_id) {
	/* backend ports */
	case OMAP_ABE_BE_PORT_DMIC0:
	case OMAP_ABE_BE_PORT_DMIC1:
	case OMAP_ABE_BE_PORT_DMIC2:
		return DMIC_PORT;
	case OMAP_ABE_BE_PORT_PDM_DL1:
	case OMAP_ABE_BE_PORT_PDM_DL2:
		return PDM_DL_PORT;
	case OMAP_ABE_BE_PORT_PDM_VIB:
		return VIB_DL_PORT;
	case OMAP_ABE_BE_PORT_PDM_UL1:
		return PDM_UL_PORT;
	case OMAP_ABE_BE_PORT_BT_VX_DL:
		return BT_VX_DL_PORT;
	case OMAP_ABE_BE_PORT_BT_VX_UL:
		return BT_VX_UL_PORT;
	case OMAP_ABE_BE_PORT_MM_EXT_UL:
		return MM_EXT_OUT_PORT;
	case OMAP_ABE_BE_PORT_MM_EXT_DL:
		return MM_EXT_IN_PORT;
	/* front end ports */
	case OMAP_ABE_FE_PORT_MM_DL1:
		return MM_DL_PORT;
	case OMAP_ABE_FE_PORT_MM_UL1:
		return MM_UL_PORT;
	case OMAP_ABE_FE_PORT_MM_UL2:
		return MM_UL2_PORT;
	case OMAP_ABE_FE_PORT_VX_DL:
		return VX_DL_PORT;
	case OMAP_ABE_FE_PORT_VX_UL:
		return VX_UL_PORT;
	case OMAP_ABE_FE_PORT_VIB:
		return VIB_DL_PORT;
	case OMAP_ABE_FE_PORT_TONES:
		return TONES_DL_PORT;
	}
	return -EINVAL;
}

/*
 * Get the number of enabled users of the physical port shared by this client.
 * Locks held by callers.
 */
static int port_get_num_users(struct abe *abe, struct omap_abe_port *port)
{
	struct omap_abe_port *p;
	int users = 0;

	list_for_each_entry(p, &abe->ports, list) {
		if (p->physical_id == port->physical_id && p->state == PORT_ENABLED)
			users++;
	}
	return users;
}

static int port_is_open(struct abe *abe, int phy_port)
{
	struct omap_abe_port *p;

	list_for_each_entry(p, &abe->ports, list) {
		if (p->physical_id == phy_port && p->state == PORT_ENABLED)
			return 1;
	}
	return 0;
}

/*
 * Check whether the physical port is enabled for this PHY port ID.
 * Locks held by callers.
 */
int omap_abe_port_is_enabled(struct abe *abe, struct omap_abe_port *port)
{
	struct omap_abe_port *p;
	unsigned long flags;

	spin_lock_irqsave(&abe->lock, flags);

	list_for_each_entry(p, &abe->ports, list) {
		if (p->physical_id == port->physical_id && p->state == PORT_ENABLED) {
			spin_unlock_irqrestore(&abe->lock, flags);
			return 1;
		}
	}

	spin_unlock_irqrestore(&abe->lock, flags);
	return 0;
}
EXPORT_SYMBOL(omap_abe_port_is_enabled);

/*
 * omap_abe_port_enable - enable ABE logical port
 *
 * @abe -  ABE.
 * @port - logical ABE port ID to be enabled.
 */
int omap_abe_port_enable(struct abe *abe, struct omap_abe_port *port)
{
	int ret = 0;
	unsigned long flags;

	/* only enable the physical port iff it is disabled */
	pr_debug("port %s increment count %d\n",
			lport_name[port->logical_id], port->users);

	spin_lock_irqsave(&abe->lock, flags);
	if (port->users == 0 && port_get_num_users(abe, port) == 0) {

		/* enable the physical port */
		pr_debug("port %s phy port %d enabled\n",
			lport_name[port->logical_id], port->physical_id);
		abe_enable_data_transfer(port->physical_id);
	}

	port->state = PORT_ENABLED;
	port->users++;
	spin_unlock_irqrestore(&abe->lock, flags);
	return ret;
}
EXPORT_SYMBOL(omap_abe_port_enable);

/*
 * omap_abe_port_disable - disable ABE logical port
 *
 * @abe -  ABE.
 * @port - logical ABE port ID to be disabled.
 */
int omap_abe_port_disable(struct abe *abe, struct omap_abe_port *port)
{
	int ret = 0;
	unsigned long flags;

	/* only disable the port iff no other users are using it */
	pr_debug("port %s decrement count %d\n",
			lport_name[port->logical_id], port->users);

	spin_lock_irqsave(&abe->lock, flags);
	if (port->users == 1 && port_get_num_users(abe, port) == 1) {
		/* disable the physical port */
		pr_debug("port %s phy port %d disabled\n",
			lport_name[port->logical_id], port->physical_id);

		abe_disable_data_transfer(port->physical_id);
	}

	port->state = PORT_DISABLED;
	port->users--;
	spin_unlock_irqrestore(&abe->lock, flags);
	return ret;
}
EXPORT_SYMBOL(omap_abe_port_disable);

/*
 * omap_abe_port_open - open ABE logical port
 *
 * @abe -  ABE.
 * @logical_id - logical ABE port ID to be opened.
 */
struct omap_abe_port *omap_abe_port_open(struct abe *abe, int logical_id)
{
	struct omap_abe_port *port;
	unsigned long flags;

#ifdef CONFIG_DEBUG_FS
	char debug_fs_name[32];
#endif

	if (logical_id < 0 || logical_id > OMAP_ABE_MAX_PORT_ID)
		return NULL;

	if (port_is_open(abe, logical_id))
		return NULL;

	port = kzalloc(sizeof(struct omap_abe_port), GFP_KERNEL);
	if (port == NULL)
		return NULL;

	port->logical_id = logical_id;
	port->physical_id = get_physical_id(logical_id);
	port->state = PORT_DISABLED;
	port->abe = abe;

	spin_lock_irqsave(&abe->lock, flags);
	list_add(&port->list, &abe->ports);
	spin_unlock_irqrestore(&abe->lock, flags);
	port->physical_users = port_get_num_users(abe, port);

#ifdef CONFIG_DEBUG_FS
	sprintf(debug_fs_name, "%s_state", lport_name[logical_id]);
	port->debugfs_lstate = debugfs_create_u32(debug_fs_name, 0644,
			abe->debugfs_root, &port->state);
	sprintf(debug_fs_name, "%s_phy", lport_name[logical_id]);
	port->debugfs_lphy = debugfs_create_u32(debug_fs_name, 0644,
			abe->debugfs_root, &port->physical_id);
	sprintf(debug_fs_name, "%s_users", lport_name[logical_id]);
	port->debugfs_lusers = debugfs_create_u32(debug_fs_name, 0644,
			abe->debugfs_root, &port->users);
#endif

	pr_debug("opened port %s\n", lport_name[logical_id]);
	return port;
}
EXPORT_SYMBOL(omap_abe_port_open);

/*
 * omap_abe_port_close - close ABE logical port
 *
 * @port - logical ABE port to be closed (and disabled).
 */
void omap_abe_port_close(struct abe *abe, struct omap_abe_port *port)
{
	unsigned long flags;

	/* disable the port */
	omap_abe_port_disable(abe, port);

	spin_lock_irqsave(&abe->lock, flags);
	list_del(&port->list);
	spin_unlock_irqrestore(&abe->lock, flags);

	pr_debug("closed port %s\n", lport_name[port->logical_id]);
	kfree(port);
}
EXPORT_SYMBOL(omap_abe_port_close);

static struct abe *omap_abe_port_mgr_init(void)
{
	struct abe *abe;

	abe = kzalloc(sizeof(struct abe), GFP_KERNEL);
	if (abe == NULL)
		return NULL;

	spin_lock_init(&abe->lock);

	INIT_LIST_HEAD(&abe->ports);
	the_abe = abe;

#ifdef CONFIG_DEBUG_FS
	abe->debugfs_root = debugfs_create_dir("abe_port", NULL);
	if (!abe->debugfs_root) {
		pr_debug( "Failed to create port manager debugfs directory\n");
	}
#endif
	return abe;
}

static void omap_abe_port_mgr_free(struct abe *abe)
{
#ifdef CONFIG_DEBUG_FS
	debugfs_remove_recursive(abe->debugfs_root);
#endif
	kfree(abe);
	the_abe = NULL;
}

struct abe *omap_abe_port_mgr_get(void)
{
	struct abe * abe;

	mutex_lock(&port_mgr_mutex);

	if (the_abe)
		abe = the_abe;
	else
		abe = omap_abe_port_mgr_init();

	users++;
	mutex_unlock(&port_mgr_mutex);
	return abe;
}
EXPORT_SYMBOL(omap_abe_port_mgr_get);

void omap_abe_port_mgr_put(struct abe *abe)
{
	mutex_lock(&port_mgr_mutex);

	if (users == 0)
		goto out;

	if (--users == 0)
		omap_abe_port_mgr_free(abe);

out:
	mutex_unlock(&port_mgr_mutex);
}
EXPORT_SYMBOL(omap_abe_port_mgr_put);

MODULE_LICENSE("GPL");
