#ifndef __HQ_PRINTK__
#define __HQ_PRINTK__

#define TAG                     "[HQ_CHG][CM]"
#define hq_err(fmt, ...)        pr_err(TAG ":%s:" fmt, __func__, ##__VA_ARGS__)
#define hq_info(fmt, ...)       pr_info(TAG ":%s:" fmt, __func__, ##__VA_ARGS__)
#define hq_debug(fmt, ...)      pr_debug(TAG ":%s:" fmt, __func__, ##__VA_ARGS__)

#endif
