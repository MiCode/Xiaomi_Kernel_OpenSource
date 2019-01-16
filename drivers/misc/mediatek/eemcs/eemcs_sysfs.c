#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/string.h>

#include "eemcs_char.h"
#include "eemcs_boot.h"
#include "eemcs_debug.h"
#include "eemcs_fs_ut.h"
#include "eemcs_rpc_ut.h"
#include "eemcs_statistics.h"
#include "eemcs_state.h"
#include "eemcs_expt_ut.h"

extern EEMCS_BOOT_SET eemcs_boot_inst;

/*
 * To show if "bypass CCCI handshake" is enabled
 */
static ssize_t eemcs_sysfs_show_ccci_hs_bypass(struct device *dev, struct device_attribute *attr, char *buf)
{
	DBGLOG(SYSF, INF, "get CCCI handshake bypassed info: %d", eemcs_boot_inst.ccci_hs_bypass);
    return 0;
}

/*
 * To set "bypass CCCI handshake" flag
 */
static ssize_t eemcs_sysfs_set_ccci_hs_bypass(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    KAL_UINT32 bypass = 0;

    bypass = simple_strtol(buf, NULL, 10);
    if (bypass > 0)
        eemcs_boot_inst.ccci_hs_bypass = 1;
    else
        eemcs_boot_inst.ccci_hs_bypass = 0;
	DBGLOG(SYSF, INF, "set CCCI handshake bypassed info: %d", eemcs_boot_inst.ccci_hs_bypass);
    return count;
}
static DEVICE_ATTR(ccci_hs_bypass, S_IRUGO|S_IWUSR, eemcs_sysfs_show_ccci_hs_bypass, eemcs_sysfs_set_ccci_hs_bypass);

#ifdef _EEMCS_FS_UT
/*
 * To dump FS UT information
 */
static ssize_t eemcs_sysfs_show_fs_ut(struct device *dev, struct device_attribute *attr, char *buf)
{
    eemcs_fs_ut_dump();
    return 0;
}

/*
 * Use a positive value to trigger FS UT
 */
static ssize_t eemcs_sysfs_set_fs_ut(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    KAL_UINT32 go_ut = 0;

    go_ut = simple_strtol(buf, NULL, 10);
    if (go_ut >= 1)
        eemcs_fs_ut_trigger();

    return count;
}

static DEVICE_ATTR(fs_ut, S_IRUGO|S_IWUSR, eemcs_sysfs_show_fs_ut, eemcs_sysfs_set_fs_ut);

/*
 * To show the port index currently used in FS UT
 */
static ssize_t eemcs_sysfs_show_fs_ut_port(struct device *dev, struct device_attribute *attr, char *buf)
{
    DBGLOG(SYSF, INF, "FS UT Get Port_idx=%d", eemcs_fs_ut_get_index());
    return 0;
}

/*
 * To set the port index you want to use in FS UT
 */
static ssize_t eemcs_sysfs_set_fs_ut_port(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    KAL_UINT32 index = 0;

    index = simple_strtol(buf, NULL, 10);
    if (eemcs_fs_ut_set_index(index) >= 0)
		DBGLOG(SYSF, INF, "FS UT Set Port_idx=%d", index);
    else
		DBGLOG(SYSF, INF, "FS UT Set Port_idx=%d", index);

    return count;
}
static DEVICE_ATTR(fs_ut_port, S_IRUGO|S_IWUSR, eemcs_sysfs_show_fs_ut_port, eemcs_sysfs_set_fs_ut_port);

#endif // _EEMCS_FS_UT


#ifdef _EEMCS_RPC_UT

/*
 * Use a positive value to trigger RPC UT
 */
static ssize_t eemcs_sysfs_set_rpc_ut(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    KAL_UINT32 go_ut = 0;

    go_ut = simple_strtol(buf, NULL, 10);
    if (go_ut >= 1)
        eemcs_rpc_ut_trigger();

    return count;
}
static DEVICE_ATTR(rpc_ut, S_IRUGO|S_IWUSR, NULL, eemcs_sysfs_set_rpc_ut);


#endif // _EEMCS_RPC_UT


/*
 * Use 1 to print time of EMCSVA test start
 *     0 to print time of EMCSVA test stop
 */
static ssize_t eemcs_sysfs_emcsva_log(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    KAL_UINT32 emcsva_log_set = 0;
    struct timex  txc;
    struct rtc_time tm;
    char c_time_string[30]={0};
    do_gettimeofday(&(txc.time));
    rtc_time_to_tm(txc.time.tv_sec,&tm);
    sprintf(c_time_string,"%04d-%02d-%02d_%02d:%02d:%02d",tm.tm_year+1900,tm.tm_mon+1, tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);

    emcsva_log_set = simple_strtol(buf, NULL, 10);
    if (emcsva_log_set == 1){
		DBGLOG(SYSF, INF, "++EEMCSVA Start: %s %s++", c_time_string, buf+2);
    }
    else if (emcsva_log_set == 0) {
		DBGLOG(SYSF, INF, "--EEMCSVA Stop : %s %s--", c_time_string, buf+2);
    }
    return count;
}
static DEVICE_ATTR(emcsva_log, S_IRUGO|S_IWUSR, NULL, eemcs_sysfs_emcsva_log);


/*
 * To dump Boot restet test information
 */
extern unsigned char* g_md_sta_str[];
static ssize_t eemcs_show_boot_state(struct device *dev, struct device_attribute *attr, char *buf)
{
    DBGLOG(SYSF, INF, "get md_boot_state=%s(%d)", g_md_sta_str[eemcs_boot_get_state()], eemcs_boot_get_state());
    return sprintf(buf,"%d\n",eemcs_boot_get_state());
}

/*
 * Use a positive value to trigger FS UT
 */
static ssize_t eemcs_set_boot_reset_state(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    KAL_UINT32 reset_state = 0;

    reset_state = simple_strtol(buf, NULL, 10);
    if (reset_state >= 0)
    	DBGLOG(SYSF, INF, "set md_boot_state=%s(%d)", g_md_sta_str[reset_state], reset_state);
		eemcs_boot_reset_test(reset_state);

    return count;
}

static DEVICE_ATTR(boot_test, S_IRUGO|S_IWUSR, eemcs_show_boot_state, eemcs_set_boot_reset_state);

/*
 * To show eemcs port statistics
 */
KAL_CHAR *ccci_port_name[CCCI_PORT_NUM_MAX]=
{
    "eemcs_ctrl",
    "eemcs_sys",        /* START_OF_NORMAL_PORT */
    "eemcs_aud",
    "eemcs_meta",
    "eemcs_mux",
    "eemcs_fs",
    "eemcs_pmic",
    "eemcs_uem",
    "eemcs_rpc",
    "eemcs_ipc",
    "eemcs_ipc_uart",
    "eemcs_md_log",
    "eemcs_imsv",    /* ims video */
    "eemcs_imsc",    /* ims control */
    "eemcs_imsa",    /* ims audio */
    "eemcs_imsdc",   /* ims data control */
    "eemcs_muxrp",   /* mux report channel, support ioctl only no i/o*/
    "eemcs_ioctl",   /* ioctl channel, support ioctl only no i/o*/
    "eemcs_ril",     /* rild channel, support ioctl only no i/o*/
    "eemcs_it",      /* END_OF_NORMAL_PORT-1 */
    "ECCMNI1",
    "ECCMNI2",
    "ECCMNI3",
};
static ssize_t eemcs_sysfs_show_statistics(struct device *dev, struct device_attribute *attr, char *buf)
{
    int i = 0;
    char line[121]={0};
    char c_time_string[30]={0};
    int pos = 0;
    struct timeval	tv;
    struct rtc_time tm;
    struct rtc_time tm_now;
    do_gettimeofday(&tv);
    rtc_time_to_tm(tv.tv_sec,&tm_now);
    rtc_time_to_tm(eemcs_statistics[0]->time.tv_sec,&tm);
    sprintf(c_time_string,"%04d-%02d-%02d_%02d:%02d:%02d ~ %02d:%02d - %d sec",
        tm.tm_year+1900,tm.tm_mon+1, tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec,
        tm_now.tm_min,tm_now.tm_sec,
        (unsigned int)(tv.tv_sec - eemcs_statistics[0]->time.tv_sec));
    
    pos += sprintf(buf+pos, "Record Time: %s\n", c_time_string);
    
    pos += sprintf(buf+pos, "%15s | %7s | %7s | %9s | %8s | %8s | %7s | %7s |\n", 
        "CCCI PORT", "TX CNT", "RX CNT", "RX Q(MAX)", "TX TOTAL", "RX TOTAL", "TX DROP", "RX DROP");
    memset(line, '=', 91);
    pos += sprintf(buf+pos, "%s\n", line);
    for (i=0; i< CCCI_PORT_NUM_MAX; i++){
        CCCI_PORT_STATISTICS *port = &eemcs_statistics[0]->port[i];
        CCCI_PORT_STATISTICS *port_total = &eemcs_statistics[0]->port_total[i];
        pos += sprintf(buf+pos, "%15s | %7d | %7d | %3d(%4d) | %8d | %8d | %7d | %7d |\n", 
            ccci_port_name[i], port->cnt[TX], port->cnt[RX],
            port->queue[RX], port_total->queue[RX],
            port_total->cnt[TX], port_total->cnt[RX],
            port_total->drop[TX], port_total->drop[RX]
        );
    }
    
    return pos;
}

/*
 * To set eemcs port statistics timer
 */
static ssize_t eemcs_sysfs_set_statistics(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    KAL_UINT32 input = 0;

    input = simple_strtol(buf, NULL, 10);

    switch(input){
        case 0 :
                eemcs_statistics[0]->start = 0;
            break;
        
        default:
                eemcs_statistics[0]->inteval = input;
                eemcs_statistics[0]->start = 1;
            break;
    }

    return count;
}
static DEVICE_ATTR(eemcs_statistics, S_IRUGO|S_IWUSR, eemcs_sysfs_show_statistics, eemcs_sysfs_set_statistics);


//===================================================================
//  Exception Mode sysfs
//===================================================================

#ifdef __EEMCS_EXPT_SUPPORT__

#define EEMCS_EX_MODE_LEN    16

/*
 * Show current exception mode (if in exception state)
 */
static ssize_t eemcs_sysfs_show_expt_mode(struct device *dev, struct device_attribute *attr, char *buf)
{
    EEMCS_EXCEPTION_STATE mode = EEMCS_EX_INVALID;
    KAL_UINT32 is_exception;
    extern KAL_INT32 get_exception_mode(void);
        
    is_exception = is_exception_mode(&mode);
    mode = get_exception_mode();
    return snprintf(buf, PAGE_SIZE, "[EXPT] %s in Exception and Exception Mode = 0x%X\n", (is_exception?"IS":"NOT"), mode);
}

#ifdef _EEMCS_EXCEPTION_UT

/*
 * Set EEMCS to exception state
 *
 * "reset"      ==> EEMCS_EX_INVALID    = 0,
 * "none"       ==> EEMCS_EX_NONE       = 0,
 * "init"       ==> EEMCS_EX_INIT       = 1,
 * "dhl_ready"  ==> EEMCS_EX_DHL_DL_RDY = 2,
 * "init_done"  ==> EEMCS_EX_INIT_DONE  = 3,
 */
static ssize_t eemcs_sysfs_set_expt_mode(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int ret = -EINVAL;
    char str_mode[EEMCS_EX_MODE_LEN] = {0};
    EEMCS_EXCEPTION_STATE mode = EEMCS_EX_INVALID;
    KAL_UINT32 is_exception;
    DEBUG_LOG_FUNCTION_ENTRY;

    ret = sscanf(buf, "%15s", str_mode);
    if (ret != 1)
        return 0;
    DBGLOG(EXPT, INF, "exception mode command(%s), count=%d", str_mode, count);
    if (strnicmp(str_mode, "reset", EEMCS_EX_MODE_LEN) == 0) {
        change_device_state(EEMCS_INIT);
    } else if (strnicmp(str_mode, "none", EEMCS_EX_MODE_LEN) == 0) {
        set_exception_mode(EEMCS_EX_NONE);
    } else if (strnicmp(str_mode, "init", EEMCS_EX_MODE_LEN) == 0) {
        eemcs_expt_ut_trigger(EX_INIT);
    } else if (strnicmp(str_mode, "dhl_ready", EEMCS_EX_MODE_LEN) == 0) {
        eemcs_expt_ut_trigger(EX_DHL_DL_RDY);
    } else if (strnicmp(str_mode, "init_done", EEMCS_EX_MODE_LEN) == 0) {
        eemcs_expt_ut_trigger(EX_INIT_DONE);
    } else {
        DBGLOG(EXPT, WAR, "Nothing to do !!");
    }
    is_exception = is_exception_mode(&mode);
    DBGLOG(SYSF, INF, "set Exception_Mode=%d when md %s in exception\n", (is_exception?"IS":"NOT"), mode);

    DEBUG_LOG_FUNCTION_LEAVE;
    return count;
}
static DEVICE_ATTR(expt_mode, S_IRUGO|S_IWUSR, eemcs_sysfs_show_expt_mode, eemcs_sysfs_set_expt_mode);
#else // _EEMCS_EXCEPTION_UT
static DEVICE_ATTR(expt_mode, S_IRUGO|S_IWUSR, eemcs_sysfs_show_expt_mode, NULL);
#endif // _EEMCS_EXCEPTION_UT

/*
 * Show exception mode statistics
 */
static ssize_t eemcs_sysfs_show_expt(struct device *dev, struct device_attribute *attr, char *buf)
{
    return eemcs_expt_show_statistics(buf);
}

/*
 * Exception mode control
 */
static ssize_t eemcs_sysfs_set_expt(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    unsigned int ret = -EINVAL;
    char ctrl_cmd[EEMCS_EX_MODE_LEN] = {0};
    DEBUG_LOG_FUNCTION_ENTRY;

    ret = sscanf(buf, "%15s", ctrl_cmd);
    if (ret != 1)
        return 0;
    DBGLOG(SYSF, INF, "exception control command(%s), count=%d", ctrl_cmd, count);
    if (strnicmp(ctrl_cmd, "flush", EEMCS_EX_MODE_LEN) == 0) {
        eemcs_expt_flush();
    } else {
        DBGLOG(SYSF, WAR, "[EXPT] Nothing to do!!");
    }

    DEBUG_LOG_FUNCTION_LEAVE;
    return count;

}
static DEVICE_ATTR(expt, S_IRUGO|S_IWUSR, eemcs_sysfs_show_expt, eemcs_sysfs_set_expt);

#ifdef _EEMCS_EXCEPTION_UT
/*
 * Showing if loopback to CCCI in exception UT is enabled
 */
static ssize_t eemcs_sysfs_show_expt_ut_ccci_lb(struct device *dev, struct device_attribute *attr, char *buf)
{
    return eemcs_expt_sysfs_show_ut_ccci_lb(buf);
}

/*
 * Disable/Enable loopback to CCCI in exception UT
 */
static ssize_t eemcs_sysfs_set_expt_ut_ccci_lb(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
    KAL_UINT32 enable = 0;

    enable = simple_strtol(buf, NULL, 10);
    if (enable >= 1)
        eemcs_expt_enable_ut_ccci_lb(1);
    else
        eemcs_expt_enable_ut_ccci_lb(0);

    return count;
}
static DEVICE_ATTR(expt_ut_ccci_lb, S_IRUGO|S_IWUSR, eemcs_sysfs_show_expt_ut_ccci_lb, eemcs_sysfs_set_expt_ut_ccci_lb);

#endif // _EEMCS_EXCEPTION_UT

#endif // __EEMCS_EXPT_SUPPORT__


void eemcs_sysfs_init(struct class *dev_class)
{
    struct device *dev = NULL;

    if (!IS_ERR(dev_class)) {
        dev = device_create(dev_class, NULL, MKDEV(EEMCS_DEV_MAJOR, END_OF_NORMAL_PORT), NULL, EEMCS_DEV_NAME);
        if (IS_ERR(dev)) {
            DBGLOG(SYSF, ERR, "create device(%s) fail", EEMCS_DEV_NAME);
        } else {
            if (device_create_file(dev, &dev_attr_ccci_hs_bypass) != 0)
                DBGLOG(SYSF, ERR, "create sysfs ccci_hs_bypass fail");

            if (device_create_file(dev, &dev_attr_emcsva_log) != 0)
                DBGLOG(SYSF, ERR, "create sysfs emcsva_log fail");

            if (device_create_file(dev, &dev_attr_boot_test) != 0)
                DBGLOG(SYSF, ERR, "create sysfs boot_test fail");
            
            if (device_create_file(dev, &dev_attr_eemcs_statistics) != 0)
                DBGLOG(SYSF, ERR, "create sysfs eemcs_statistics fail");
#ifdef _EEMCS_RPC_UT
            if (device_create_file(dev, &dev_attr_rpc_ut) != 0)
                DBGLOG(SYSF, ERR, "create sysfs rpc_ut fail");
#endif // _EEMCS_RPC_UT

#ifdef _EEMCS_FS_UT
            if (device_create_file(dev, &dev_attr_fs_ut) != 0)
                DBGLOG(SYSF, ERR, "create sysfs fs_ut fail");
            if (device_create_file(dev, &dev_attr_fs_ut_port) != 0)
                DBGLOG(SYSF, ERR, "create sysfs fs_ut_port fail");
#endif // _EEMCS_FS_UT
#ifdef __EEMCS_EXPT_SUPPORT__
            if (device_create_file(dev, &dev_attr_expt_mode) != 0)
                DBGLOG(SYSF, ERR, "create sysfs expt_mode fail");
            if (device_create_file(dev, &dev_attr_expt) != 0)
                DBGLOG(SYSF, ERR, "create sysfs expt_info fail");
#ifdef _EEMCS_EXCEPTION_UT
            if (device_create_file(dev, &dev_attr_expt_ut_ccci_lb) != 0)
                DBGLOG(SYSF, ERR, "create sysfs expt_ut_ccci_lb fail");
#endif // _EEMCS_EXCEPTION_UT
#endif // __EEMCS_EXPT_SUPPORT__


        }
    }
}

void eemcs_sysfs_exit(struct class *dev_class)
{
    if (!IS_ERR(dev_class)) {
        device_destroy(dev_class, MKDEV(EEMCS_DEV_MAJOR, END_OF_NORMAL_PORT));
    }
}

