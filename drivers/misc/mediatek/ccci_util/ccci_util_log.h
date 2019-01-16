#ifndef __CCCI_UTIL_LOG_H__
#define __CCCI_UTIL_LOG_H__

// No MD id message part
#define CCCI_UTIL_DBG_MSG(fmt, args...) \
do { \
	printk(KERN_DEBUG "[ccci0/util]" fmt, ##args); \
} while(0)

#define CCCI_UTIL_INF_MSG(fmt, args...) \
do { \
	printk(KERN_NOTICE "[ccci0/util]" fmt, ##args); \
} while(0)

#define CCCI_UTIL_ERR_MSG(fmt, args...) \
do { \
	printk(KERN_ERR "[ccci0/util]" fmt, ##args); \
} while(0)

// With MD id message part
#define CCCI_UTIL_DBG_MSG_WITH_ID(id, fmt, args...) \
do { \
	printk(KERN_DEBUG "[ccci%d/util]" fmt, (id+1), ##args); \
} while(0)

#define CCCI_UTIL_INF_MSG_WITH_ID(id, fmt, args...) \
do { \
	printk(KERN_NOTICE "[ccci%d/util]" fmt, (id+1), ##args); \
} while(0)

#define CCCI_UTIL_ERR_MSG_WITH_ID(id, fmt, args...) \
do { \
	printk(KERN_ERR "[ccci%d/util]" fmt, (id+1), ##args); \
} while(0)


#endif //__CCCI_UTIL_LOG_H__
