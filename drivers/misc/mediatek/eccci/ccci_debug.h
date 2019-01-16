#ifndef __CCCI_DEBUG_H__
#define __CCCI_DEBUG_H__

// tag defination
#define CORE "cor"
#define BM "bfm"
#define NET "net"
#define CHAR "chr"
#define SYSFS "sfs"
#define KERN "ken"
#define IPC "ipc"
#define RPC "rpc"

extern unsigned int ccci_debug_enable;
extern int ccci_log_write(const char *fmt, ...);

#define CCCI_DBG_MSG(idx, tag, fmt, args...) \
do { \
	if(ccci_debug_enable == 1) \
		printk(KERN_DEBUG "[ccci%d/" tag "]" fmt, (idx+1), ##args); \
	else if(ccci_debug_enable == 2) \
		printk(KERN_NOTICE "[ccci%d/" tag "]" fmt, (idx+1), ##args); \
} while(0)
#define CCCI_INF_MSG(idx, tag, fmt, args...) printk(KERN_NOTICE"[ccci%d/" tag "]" fmt, (idx+1), ##args)
#define CCCI_RAM_MSG(idx, tag, fmt, args...) printk(KERN_DEBUG"[ccci%d/" tag "]" fmt, (idx+1), ##args)
#define CCCI_ERR_MSG(idx, tag, fmt, args...) printk(KERN_ERR "[ccci%d/err/" tag "]" fmt, (idx+1), ##args)

// only for ccci_dfo.c
#define CCCI_DBG_COM_MSG(fmt, args...)		 CCCI_DBG_MSG(0, "com", fmt, ##args)
#define CCCI_INF_COM_MSG(fmt, args...)		 CCCI_INF_MSG(0, "com", fmt, ##args)
#define CCCI_ERR_COM_MSG(fmt, args...)		 CCCI_ERR_MSG(0, "com", fmt, ##args)

// raw prink is only used to dump memory and statistic data

//#define CCCI_STATISTIC

#endif //__CCCI_DEBUG_H__
