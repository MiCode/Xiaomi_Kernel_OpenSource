#include "mi_memory.h"

#define MCB_NAME "mi_memory"
#define MCB_CNT 1
#define MCB_CDEV

struct mcb_t *mcb = NULL;
static const struct attribute_group *mcb_sysfs_groups[] = {
    &mmc_sysfs_group,
    NULL,
};

#ifdef MCB_CDEV
static struct file_operations mcb_fops = {
    .owner =  THIS_MODULE,

};
#endif
static int __init mcb_init(void)
{
    int ret = 0;

    pr_err("[mi-memory]: mcb init start\n");
    mcb = kmalloc(sizeof(struct mcb_t), GFP_KERNEL);
    if (!mcb) {
        pr_err("mcb malloc fail.\n");
        return -ENOMEM;
    }

    /*register/alloc device id*/
    if (mcb->major) {
        mcb->devid = MKDEV(mcb->major, 0);
        register_chrdev_region(mcb->devid, MCB_CNT, MCB_NAME);
    } else {
        alloc_chrdev_region(&mcb->devid, 0, MCB_CNT, MCB_NAME);
        mcb->major = MAJOR(mcb->devid);
        mcb->minor = MINOR(mcb->devid);
    }

#ifdef MCB_CDEV
    /*cdev init*/
    mcb->cdev.owner = THIS_MODULE;
    cdev_init(&mcb->cdev, &mcb_fops);
    /*cdev add*/
    cdev_add(&mcb->cdev, mcb->devid, MCB_CNT);
#endif

    mcb->mem_class = class_create(THIS_MODULE, MCB_NAME);
    if (IS_ERR(mcb->mem_class)) {
        ret = -1;
        pr_err("[mi-memory]: creat mcb class Failed!\n");
        goto out;
    }

    /*create device*/
    mcb->device = device_create(mcb->mem_class, NULL, mcb->devid, NULL, MCB_NAME);
    if (IS_ERR(mcb->device)) {
        ret = -1;
        pr_err("[mi-memory]: creat mcb device Failed!\n");
        goto class_unreg;
    }

    /*create attributes*/
    ret = sysfs_create_groups(&mcb->device->kobj, mcb_sysfs_groups);
    if(ret) {
        ret = -1;
        pr_err("[mi-memory]: creat sysfs groups Failed!\n");
        goto memdev_unreg;
    }
    pr_err("[mi-memory]: mcb init end\n");
    return 0;

memdev_unreg:
    device_destroy(mcb->mem_class, mcb->devid);
class_unreg:
    class_destroy(mcb->mem_class);
out:
#ifdef MCB_CDEV
    cdev_del(&mcb->cdev);
#endif
    unregister_chrdev_region(mcb->devid, MCB_CNT);
    kfree(mcb);
    return ret;
}

static void __exit mcb_exit(void)
{
#ifdef MCB_CDEV
    cdev_del(&mcb->cdev);
#endif
    sysfs_remove_groups(&mcb->device->kobj, mcb_sysfs_groups);
    device_destroy(mcb->mem_class, mcb->devid);
    class_destroy(mcb->mem_class);
    unregister_chrdev_region(mcb->devid, MCB_CNT);
    kfree(mcb);
}
late_initcall(mcb_init)
module_exit(mcb_exit);
