#ifndef __DISP_DRV_LOG_H__
#define __DISP_DRV_LOG_H__
#include "display_recorder.h"
#include "debug.h"
    ///for kernel
    #include <linux/xlog.h>

    #define DISP_LOG_PRINT(level, sub_module, fmt, arg...)      \
        do {                                                    \
            xlog_printk(level, "DISP/"sub_module, fmt, ##arg);  \
        }while(0)

    #define LOG_PRINT(level, module, fmt, arg...)               \
        do {                                                    \
            xlog_printk(level, module, fmt, ##arg);             \
        }while(0)

extern unsigned int dprec_error_log_len;
extern unsigned int dprec_error_log_id;
extern unsigned int dprec_error_log_buflen;
extern char dprec_error_log_buffer[];

#define DISPMSG(string, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, string, ##args);	\
		if (g_mobilelog)					\
			pr_debug("[DISP]"string, ##args);		\
	} while (0)
#define DISPDBG(string, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, string, ##args);	\
		if (g_mobilelog)					\
			pr_debug("disp/"string, ##args);		\
	} while (0)
#define DISPERR  	DISPPR_ERROR
#if 0
	do {\
		printk("[DISP][%s #%d]ERROR:"string,__func__, __LINE__, ##args); \
		if(0)dprec_error_log_len += scnprintf(dprec_error_log_buffer+dprec_error_log_len, dprec_error_log_buflen-dprec_error_log_len, "[%d][%s #%d]"string,dprec_error_log_id++, __func__, __LINE__, ##args) ; \
	}while(0)
#endif

#define DISPFUNC() pr_debug("[DISP]func|%s\n", __func__)	/* default on, err msg */
#define DISPDBGFUNC() DISPDBG("[DISP]func|%s\n", __func__)  //default on, err msg

#define DISPCHECK(string, args...)					\
	do {								\
		dprec_logger_pr(DPREC_LOGGER_DEBUG, string, ##args);	\
		pr_debug("[DISPCHECK]"string, ##args);			\
	} while (0)

#define DISPPR_HWOP(string, args...)  //dprec_logger_pr(DPREC_LOGGER_HWOP, string, ##args);
#define DISPPR_ERROR(string, args...)  do{dprec_logger_pr(DPREC_LOGGER_ERROR, string, ##args);printk("[DISP][%s #%d]ERROR:"string,__func__, __LINE__, ##args); }while(0)
#define DISPPR_FENCE(string, args...)  do{dprec_logger_pr(DPREC_LOGGER_FENCE, string, ##args);xlog_printk(ANDROID_LOG_DEBUG,   "fence/",  string, ##args); }while(0)

#define disp_aee_print(string, args...) do{\
    char disp_name[100];\
    snprintf(disp_name,100, "[DISP]"string, ##args); \
    aee_kernel_warning_api(__FILE__, __LINE__, DB_OPT_DEFAULT | DB_OPT_MMPROFILE_BUFFER, disp_name, "[DISP] error"string, ##args);  \
	xlog_printk(ANDROID_LOG_ERROR, "M4U", "error: "string,##args);  \
}while(0)



#endif // __DISP_DRV_LOG_H__
