#ifndef __LX_PRINTK__
#define __LX_PRINTK__

#define TAG                     "[CHG][CM]"
#define lx_err(fmt, ...)        pr_err(TAG ":%s:" fmt, __func__, ##__VA_ARGS__)
#define lx_info(fmt, ...)       pr_info(TAG ":%s:" fmt, __func__, ##__VA_ARGS__)
#define lx_debug(fmt, ...)      pr_debug(TAG ":%s:" fmt, __func__, ##__VA_ARGS__)

#endif
