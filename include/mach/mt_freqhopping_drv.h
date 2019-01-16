#ifndef __FREQHOPPING_DRV_H
#define __FREQHOPPING_DRV_H

#include <linux/proc_fs.h>
#include "mach/mt_freqhopping.h"

// move to /mediatek/platform/prj, can config. by prj.
//#define MEMPLL_SSC 0
//#define MAINPLL_SSC 1

/* Export API */
int mt_freqhopping_devctl(unsigned int cmd, void* args);

struct mt_fh_hal_proc_func{
	
    int (*clk_gen_read)(char *, char **, off_t, int, int *, void *);
    int (*clk_gen_write)(struct file *,const char *, unsigned long, void *);
    int (*dramc_read)(struct seq_file* m, void* v);		//(char *, char **, off_t, int, int *, void *);
    int (*dramc_write)(struct file *,const char *, unsigned long, void *);
    int (*dumpregs_read)(struct seq_file* m, void* v);//(char *, char **, off_t, int, int *, void *);
    int (*dvfs_read)(struct seq_file* m, void* v);		//(char *, char **, off_t, int, int *, void *);
    int (*dvfs_write)(struct file *,const char *, unsigned long, void *);

};

struct mt_fh_hal_driver{

	fh_pll_t 		*fh_pll;
	struct freqhopping_ssc 	*fh_usrdef;
	unsigned int 		mempll;
	unsigned int 		lvdspll;
	unsigned int 		mainpll;
	unsigned int 		msdcpll;
	unsigned int 		mmpll;
	unsigned int 		vencpll;
	unsigned int 		pll_cnt;

	struct mt_fh_hal_proc_func proc;

	void (*mt_fh_hal_init)(void);
	int  (*mt_fh_hal_ctrl)(struct freqhopping_ioctl* , bool);
	void (*mt_fh_lock)(unsigned long *);
	void (*mt_fh_unlock)(unsigned long *);
	int  (*mt_fh_get_init)(void);
	void (*mt_fh_popod_restore)(void);
	void (*mt_fh_popod_save)(void);
	
	int  (*mt_l2h_mempll)(void);
	int  (*mt_h2l_mempll)(void);
	int  (*mt_dfs_armpll)(unsigned int , unsigned int);
	int  (*mt_dfs_mmpll)(unsigned int);
	int  (*mt_dfs_vencpll)(unsigned int);
    int  (*mt_dfs_mpll)(unsigned int);
    int  (*mt_dfs_mempll)(unsigned int);
	int  (*mt_is_support_DFS_mode)(void);
	int  (*mt_l2h_dvfs_mempll)(void);
	int  (*mt_h2l_dvfs_mempll)(void);	
	int  (*mt_dram_overclock)(int);
	int  (*mt_get_dramc)(void);
	void (*mt_fh_default_conf)(void);

	void (*ioctl)(unsigned int ctlid, void* );
};

//define ctlid for ioctl()
#define FH_IO_PROC_READ 0x001

enum FH_DEVCTL_CMD_ID {
 FH_DCTL_CMD_ID               = 0x1000,
 FH_DCTL_CMD_DVFS             = 0x1001,
 FH_DCTL_CMD_DVFS_SSC_ENABLE  = 0x1002,
 FH_DCTL_CMD_DVFS_SSC_DISABLE = 0x1003,
 FH_DCTL_CMD_SSC_ENABLE       = 0x1004,
 FH_DCTL_CMD_SSC_DISABLE      = 0x1005, 
 FH_DCTL_CMD_MAX
};


//define structure for correspoinding ctlid
typedef struct{
    struct seq_file* m;
    void* v;
    fh_pll_t * pll;
}FH_IO_PROC_READ_T;

struct mt_fh_hal_driver* mt_get_fh_hal_drv(void);

#define FH_BUG_ON(x) \
do {    \
		if((x)){ \
			printk("BUGON %s:%d %s:%d\n",__FUNCTION__,__LINE__,current->comm,current->pid); \
        	} \
} while(0);

#endif
