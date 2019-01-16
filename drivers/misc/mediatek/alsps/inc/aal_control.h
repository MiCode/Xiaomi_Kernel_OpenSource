#ifndef __AAL_CONTROL_H__
#define __AAL_CONTROL_H__

#define AAL_TAG                  "[ALS/AAL]"
#define AAL_LOG(fmt, args...)	 printk(KERN_ERR AAL_TAG fmt, ##args)
#define AAL_FUN(f)               printk(KERN_INFO 	AAL_TAG"%s\n", __FUNCTION__)
#define AAL_ERR(fmt, args...)    printk(KERN_ERR  	AAL_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#endif
extern int aal_use;



