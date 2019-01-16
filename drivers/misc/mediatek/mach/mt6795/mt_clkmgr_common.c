#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/proc_fs.h>

#include <mach/mt_clkmgr.h>

int enable_clock(enum cg_clk_id id, char *name)
{
	int err;
	
	err = mt_enable_clock(id, name);
	
	return err;
}
EXPORT_SYMBOL(enable_clock);


int disable_clock(enum cg_clk_id id, char *name)
{
	int err;
	
	err = mt_disable_clock(id, name);
	
	return err;
}
EXPORT_SYMBOL(disable_clock);

