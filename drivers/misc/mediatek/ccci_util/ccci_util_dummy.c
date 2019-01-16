#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>

int __weak exec_ccci_kern_func_by_md_id(int md_id, unsigned int id, char *buf, unsigned int len)
{
printk("[ccci/dummy] %s is not supported!\n",__FUNCTION__);
return 0;
}
int __weak switch_sim_mode(int id, char *buf, unsigned int len)
{
printk("[ccci/dummy] %s is not supported!\n",__FUNCTION__);
return 0;
}
unsigned int __weak get_sim_switch_type(void)
{
printk("[ccci/dummy] %s is not supported!\n",__FUNCTION__);
return 0;
}

unsigned int __weak  get_modem_is_enabled(int md_id) 
{
printk("[ccci/dummy] %s is not supported!\n",__FUNCTION__);
return 0;
}
int __weak register_ccci_sys_call_back(int md_id, unsigned int id, int (*func)(int, int)) 
{
printk("[ccci/dummy] %s is not supported!\n",__FUNCTION__);
return 0;
}
