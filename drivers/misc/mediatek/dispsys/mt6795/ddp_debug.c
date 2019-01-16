#define LOG_TAG "DEBUG"

#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>
#include <linux/aee.h>
#include <linux/disp_assert_layer.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/time.h>

#include <mach/m4u.h>
#include <mach/m4u_port.h>

#include "disp_drv_ddp.h"

#include "ddp_debug.h"
#include "ddp_reg.h"
#include "ddp_drv.h"
#include "ddp_wdma.h"
#include "ddp_hal.h"
#include "ddp_path.h"
#include "ddp_aal.h"
#include "ddp_pwm.h"
#include "ddp_info.h"
#include "ddp_dsi.h"
#include "ddp_od.h"

#include "ddp_manager.h"
#include "ddp_log.h"
#include "ddp_met.h"
#include "display_recorder.h"
#include "disp_session.h"

#pragma GCC optimize("O0")

// ---------------------------------------------------------------------------
//  External variable declarations
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
//  Debug Options
// ---------------------------------------------------------------------------

static struct dentry *debugfs;
static struct dentry *debugDir;


static struct dentry *debugfs_dump;

static const long int DEFAULT_LOG_FPS_WND_SIZE = 30;
static int debug_init;


unsigned char pq_debug_flag=0;
unsigned char aal_debug_flag=0;

static unsigned int dbg_log_level;
static unsigned int irq_log_level;
static unsigned int dump_to_buffer;

unsigned int gOVLBackground = 0xFF000000;
unsigned int gUltraEnable = 1;
static char STR_HELP[] =
    "USAGE:\n"
    "       echo [ACTION]>/d/dispsys\n"
    "ACTION:\n"
    "       regr:addr\n              :regr:0xf400c000\n"
    "       regw:addr,value          :regw:0xf400c000,0x1\n"
    "       dbg_log:0|1|2            :0 off, 1 dbg, 2 all\n"
    "       irq_log:0|1              :0 off, !0 on\n"
    "       met_on:[0|1],[0|1],[0|1] :fist[0|1]on|off,other [0|1]direct|decouple\n"
    "       backlight:level\n"
    "       dump_aal:arg\n"
    "       mmp\n"  
    "       dump_reg:moduleID\n"
    "       dump_path:mutexID\n"  
    "       dpfd_ut1:channel\n"
    ;
// ---------------------------------------------------------------------------
//  Command Processor
// ---------------------------------------------------------------------------
static char dbg_buf[2048];
static unsigned int is_reg_addr_valid(unsigned int isVa, unsigned long addr)
{
    unsigned int i=0;

    for(i=0;i<DISP_REG_NUM;i++)
    {
        if((isVa==1) && (addr>=dispsys_reg[i]) && (addr<dispsys_reg[i]+0x1000) )
			break;
        if((isVa==0) && (addr>=ddp_reg_pa_base[i]) && (addr<ddp_reg_pa_base[i]+0x1000) )
			break;		
    }
    
	if(i<DISP_REG_NUM)
	{
	    DDPMSG("addr valid, isVa=0x%x, addr=0x%lx, module=%s!\n", isVa, addr, ddp_get_reg_module_name(i));
	    return 1;
	}
	else
	{
        DDPERR("is_reg_addr_valid return fail, isVa=0x%x, addr=0x%lx!\n", isVa, addr);
        return 0;
	}
}

extern int disp_create_session (disp_session_config*config);
extern int disp_destroy_session (disp_session_config* config);
static void process_dbg_opt(const char *opt)
{
    char *buf = dbg_buf + strlen(dbg_buf);
    static disp_session_config config;
    
    if (0 == strncmp(opt, "regr:", 5))
    {
        char *p = (char *)opt + 5;
        unsigned long addr = (unsigned long) simple_strtoul(p, &p, 16);

        if(is_reg_addr_valid(1, addr)==1)// (addr >= 0xf0000000U && addr <= 0xff000000U) 
        {
            unsigned int regVal = DISP_REG_GET(addr);
            DDPMSG("regr: 0x%lx = 0x%08X\n", addr, regVal);
            sprintf(buf, "regr: 0x%lx = 0x%08X\n", addr, regVal);
        } else {
            sprintf(buf, "regr, invalid address 0x%lx\n", addr);
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "regw:", 5))
    {
        char *p = (char *)opt + 5;
        unsigned long addr = (unsigned long) simple_strtoul(p, &p, 16);
        unsigned int val = (unsigned int) simple_strtoul(p + 1, &p, 16);
        if(is_reg_addr_valid(1, addr)==1)// (addr >= 0xf0000000U && addr <= 0xff000000U) 
        {
            unsigned int regVal;
            DISP_CPU_REG_SET(addr, val);
            regVal = DISP_REG_GET(addr);
            DDPMSG("regw: 0x%lx, 0x%08X = 0x%08X\n", addr, val, regVal);
            sprintf(buf, "regw: 0x%lx, 0x%08X = 0x%08X\n", addr, val, regVal);
        } else {
            sprintf(buf, "regw, invalid address 0x%lx \n", addr);
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "dbg_log:", 8))
    {
        char *p = (char *)opt + 8;
        unsigned int enable = (unsigned int) simple_strtoul(p, &p, 10);
        if (enable)
            dbg_log_level = 1;
        else
            dbg_log_level = 0;

        sprintf(buf, "dbg_log: %d\n", dbg_log_level);
    }
    else if (0 == strncmp(opt, "irq_log:", 8))
    {
        char *p = (char *)opt + 8;
        unsigned int enable = (unsigned int) simple_strtoul(p, &p, 10);
        if (enable)
            irq_log_level = 1;
        else
            irq_log_level = 0;
        
        sprintf(buf, "irq_log: %d\n", irq_log_level);        
    }
    else if (0 == strncmp(opt, "met_on:", 7))
    {
        char *p = (char *)opt + 7;
        int met_on = (int) simple_strtoul(p, &p, 10);
        int rdma0_mode = (int) simple_strtoul(p + 1, &p, 10);
        int rdma1_mode = (int) simple_strtoul(p + 1, &p, 10);
        ddp_init_met_tag(met_on,rdma0_mode,rdma1_mode);
        DDPMSG("process_dbg_opt, met_on=%d,rdma0_mode %d, rdma1 %d \n", met_on,rdma0_mode,rdma1_mode);
        sprintf(buf, "met_on:%d,rdma0_mode:%d,rdma1_mode:%d\n", met_on,rdma0_mode,rdma1_mode);
    }    
    else if (0 == strncmp(opt, "backlight:", 10))
    {
        char *p = (char *)opt + 10;
        unsigned int level = (unsigned int) simple_strtoul(p, &p, 10);

        if (level) 
        {
            disp_bls_set_backlight(level);
            sprintf(buf, "backlight: %d\n", level); 
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "pwm0:", 5) || 0 == strncmp(opt, "pwm1:", 5))
    {
        char *p = (char *)opt + 5;
        unsigned int level = (unsigned int)simple_strtoul(p, &p, 10);

        if (level) 
        {
            disp_pwm_id_t pwm_id = DISP_PWM0;
            if (opt[3] == '1')
                pwm_id = DISP_PWM1;
                
            disp_pwm_set_backlight(pwm_id, level);
            sprintf(buf, "PWM 0x%x : %d\n", pwm_id, level); 
        } else {
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "aal_dbg:", 8))
    {
        aal_dbg_en = (int)simple_strtoul(opt + 8, NULL, 10);
        sprintf(buf, "aal_dbg_en = 0x%x\n", aal_dbg_en); 
    }
    else if (0 == strncmp(opt, "aal_test:", 9))
    {
        aal_test(opt + 9, buf);
    }
    else if (0 == strncmp(opt, "pwm_test:", 9))
    {
        disp_pwm_test(opt + 9, buf);
    }
    else if (0 == strncmp(opt, "od_test:", 8))
    {
        od_test(opt + 8, buf);
    }
    else if (0 == strncmp(opt, "dump_reg:", 9))
    {
        char *p = (char *)opt + 9;
        unsigned int module = (unsigned int) simple_strtoul(p, &p, 10);
        DDPMSG("process_dbg_opt, module=%d \n", module);
        if (module<DISP_MODULE_NUM) 
        {
            ddp_dump_reg(module);
            sprintf(buf, "dump_reg: %d\n", module); 
        } else {
            DDPMSG("process_dbg_opt2, module=%d \n", module);
            goto Error;
        }
    }
    else if (0 == strncmp(opt, "dump_path:", 10))
    {
        char *p = (char *)opt + 10;
        unsigned int mutex_idx = (unsigned int) simple_strtoul(p, &p, 10);
        DDPMSG("process_dbg_opt, path mutex=%d \n", mutex_idx);
        dpmgr_debug_path_status(mutex_idx);
        sprintf(buf, "dump_path: %d\n", mutex_idx);
    }
    else if (0 == strncmp(opt, "debug:", 6))
    {
        char *p = (char *)opt + 6;
        unsigned int enable = (unsigned int) simple_strtoul(p, &p, 10);
        if(enable==1)
        {
            DDPMSG("[DDP] debug=1, trigger AEE\n");
            aee_kernel_exception("DDP-TEST-ASSERT", "[DDP] DDP-TEST-ASSERT");
        }
        else if(enable==2)
        {
           ddp_mem_test();
        }
        else if(enable==3)
        {
           ddp_lcd_test();
        }
        else if(enable==4)
        {
            DDPAEE("test 4");
        }
        else if(enable==5)
        {
        }
        else if(enable==6)
        {
            unsigned int i = 0;
            int * modules = ddp_get_scenario_list(DDP_SCENARIO_PRIMARY_DISP);
            int module_num = ddp_get_module_num(DDP_SCENARIO_PRIMARY_DISP);
            printk("dump path status:");
            for(i=0;i<module_num;i++)
            {
                printk("%s-", ddp_get_module_name(modules[i]));
            }
            printk("\n");
            ddp_dump_analysis(DISP_MODULE_CONFIG);
            ddp_dump_analysis(DISP_MODULE_MUTEX);            
            for( i=0; i< module_num;i++)
            {
                ddp_dump_analysis(modules[i]);
            }
             
            ddp_dump_reg(DISP_MODULE_CONFIG);    
            ddp_dump_reg(DISP_MODULE_MUTEX);
            for( i=0; i< module_num;i++)
            {
                ddp_dump_reg(modules[i]);
            }   
        }
        else if(enable==7)
        {
            if(dbg_log_level<3)
               dbg_log_level++;
            else
                dbg_log_level=0;
            printk("DDP: dbg_log_level=%d\n", dbg_log_level);    
            sprintf(buf, "dbg_log_level: %d\n", dbg_log_level);
        }
        else if(enable==8)
        {
        }
        else if(enable==9)
        {
            gOVLBackground = 0xFF0000FF;
            printk("DDP: gOVLBackground=%d\n", gOVLBackground);    
            sprintf(buf, "gOVLBackground: %d\n", gOVLBackground);
        }
        else if(enable==10)
        {
            gOVLBackground = 0xFF000000;
            printk("DDP: gOVLBackground=%d\n", gOVLBackground);    
            sprintf(buf, "gOVLBackground: %d\n", gOVLBackground);
        }
        else if(enable==11)
        {
            unsigned int i=0;
            char *buf_temp = buf;
            
            for(i=0;i<DISP_REG_NUM;i++)
            {
                DDPDUMP("i=%d, module=%s, reg_va=0x%lx \n", i, ddp_get_reg_module_name(i), dispsys_reg[i]);
                sprintf(buf_temp, "i=%d, module=%s, reg_va=0x%lx \n", i, ddp_get_reg_module_name(i), dispsys_reg[i]);
                buf_temp += strlen(buf_temp);
            }
        }
        else if(enable==12)
        {
            if(gUltraEnable==0)
              gUltraEnable = 1;
            else
              gUltraEnable = 0;
            printk("DDP: gUltraEnable=%d\n", gUltraEnable);    
            sprintf(buf, "gUltraEnable: %d\n", gUltraEnable);              
        }
#if 0
        else if(enable==13)
        {
            int ovl_status = ovl_get_status();            
            config.type = DISP_SESSION_MEMORY;
            config.device_id = 0;
            disp_create_session(&config);
            printk("old status=%d, ovl1 status=%d\n", ovl_status, ovl_get_status()); 
            sprintf(buf, "old status=%d, ovl1 status=%d\n", ovl_status, ovl_get_status()); 
        }
        else if(enable==14)
        {
            int ovl_status = ovl_get_status();
            disp_destroy_session(&config);
            printk("old status=%d, ovl1 status=%d\n", ovl_status, ovl_get_status()); 
            sprintf(buf, "old status=%d, ovl1 status=%d\n", ovl_status, ovl_get_status()); 
        }
#endif
    }
    else if (0 == strncmp(opt, "mmp", 3))
    {
        init_ddp_mmp_events();
    }
    else
    {
        dbg_buf[0]='\0';
	    goto Error;
    }

    return;

Error:
    DDPERR("parse command error!\n%s\n\n%s", opt, STR_HELP);
}


static void process_dbg_cmd(char *cmd)
{
    char *tok;
    
    DDPDBG("cmd: %s\n", cmd);
    memset(dbg_buf, 0, sizeof(dbg_buf));
	while ((tok = strsep(&cmd, " ")) != NULL) {
        process_dbg_opt(tok);
    }
}


// ---------------------------------------------------------------------------
//  Debug FileSystem Routines
// ---------------------------------------------------------------------------

static ssize_t debug_open(struct inode *inode, struct file *file)
{
    file->private_data = inode->i_private;
    return 0;
}


static char cmd_buf[512];

static ssize_t debug_read(struct file *file,
                          char __user *ubuf, size_t count, loff_t *ppos)
{
    DDPMSG("debug_read %zu\n", count);
    if (strlen(dbg_buf))
        return simple_read_from_buffer(ubuf, count, ppos, dbg_buf, strlen(dbg_buf));
    else
        return simple_read_from_buffer(ubuf, count, ppos, STR_HELP, strlen(STR_HELP));
        
}


static ssize_t debug_write(struct file *file,
                           const char __user *ubuf, size_t count, loff_t *ppos)
{
    const int debug_bufmax = sizeof(cmd_buf) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax) 
        count = debug_bufmax;

	if (copy_from_user(&cmd_buf, ubuf, count))
		return -EFAULT;

	cmd_buf[count] = 0;

    process_dbg_cmd(cmd_buf);

    return ret;
}


static struct file_operations debug_fops = {
	.read  = debug_read,
    .write = debug_write,
	.open  = debug_open,
};

static ssize_t debug_dump_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{

    dprec_logger_dump_reset();
    dump_to_buffer = 1;
    /* dump all */
    dpmgr_debug_path_status(-1);
    dump_to_buffer = 0;
    return simple_read_from_buffer(buf, size, ppos, dprec_logger_get_dump_addr(), dprec_logger_get_dump_len());
}


static const struct file_operations debug_fops_dump = {
    .read = debug_dump_read,
};

void ddp_debug_init(void)
{
    if(!debug_init)
    {
        debug_init = 1;
        debugfs = debugfs_create_file("dispsys",
            S_IFREG|S_IRUGO, NULL, (void *)0, &debug_fops);

        
        debugDir = debugfs_create_dir("disp", NULL);
        if (debugDir)
        {

            debugfs_dump = debugfs_create_file("dump", 
                S_IFREG|S_IRUGO, debugDir, NULL, &debug_fops_dump);
        }
    }
}

unsigned int ddp_debug_analysis_to_buffer(void)
{
    return dump_to_buffer;
}

unsigned int ddp_debug_dbg_log_level(void)
{
    return dbg_log_level;
}

unsigned int ddp_debug_irq_log_level(void)
{
    return irq_log_level;
}


void ddp_debug_exit(void)
{
    debugfs_remove(debugfs);
    debugfs_remove(debugfs_dump);
    debug_init = 0;
}

int ddp_mem_test(void)
{
    return -1;
}

int ddp_lcd_test(void)
{
    return -1;
}
