#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <mach/irqs.h>
#include <emd_ctl.h>

static int __init emd_ctl_init(void)
{
    int ret=0;

#ifdef ENABLE_EMD_CTL_DRV_BUILDIN
EMD_MSG_INF("init","emd_ctl_init: device_initcall_sync\n");
#else  // MODULE
EMD_MSG_INF("init","emd_ctl_init: module_init\n");
#endif

    ret = emd_dev_node_init(0);
    if(ret)
    {
       EMD_MSG_INF("init","emd_ctl_init:emd_dev_node_init ret=%d\n",ret);
       goto __ERR;
    }
    ret=emd_cfifo_init(0);
    if(ret)
    {
        EMD_MSG_INF("init","emd_ctl_init:emd_cfifo_init ret=%d\n",ret);
        goto __CFIFO_ERR;
    }
    ret=emd_chr_init(0);
    if(ret)
    {
        EMD_MSG_INF("init","emd_ctl_init:emd_chr_init ret=%d\n",ret);
        goto __CFIFO_ERR;
    }
    ret=emd_spm_init(0);
    if(ret)
    {
        EMD_MSG_INF("init","emd_ctl_init:emd_spm_init ret=%d\n",ret);
        goto __SPM_ERR;
    }
    
__ERR:
    return ret;
__SPM_ERR:
    emd_chr_exit(0);
__CHR_ERR:
    emd_cfifo_exit(0);
__CFIFO_ERR:    
    emd_dev_node_exit(0);

    return ret;    
}

static void __exit emd_ctl_exit(void)
{
    emd_spm_exit(0);
    emd_chr_exit(0);
    emd_cfifo_exit(0);
    emd_dev_node_exit(0);
}

module_init(emd_ctl_init);
module_exit(emd_ctl_exit);

MODULE_DESCRIPTION("ext md ctrl driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("MTK");
