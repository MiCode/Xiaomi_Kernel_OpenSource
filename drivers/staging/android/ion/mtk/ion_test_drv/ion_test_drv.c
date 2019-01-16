#include <linux/err.h>
#include <linux/ion.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/ion_drv.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/aee.h>

#define ION_TEST_DEVNAME "ion_test"

static dev_t ion_test_devno;
static struct cdev *ion_test_cdev;
static struct class *ion_test_class = NULL;

static struct ion_client* gClient = NULL;

static char* g_pBuf = NULL;

static int ion_test_release(struct inode *inode, struct file *file)
{
    printk("ion_test_drv: ion_test_release()\n");
	return 0;
}

static int ion_test_open(struct inode *inode, struct file *file)
{
    char* pBuf = NULL;
    ion_phys_addr_t phy_addr;
    size_t len;
    struct ion_handle* handle;
    struct ion_mm_data mm_data;
    if (gClient)
    {
        handle = ion_alloc(gClient, 0x1000, 1, ION_HEAP_MULTIMEDIA_MASK, 0);
        if (IS_ERR_OR_NULL(handle))
        {
            printk("ion_test_drv: Cannot allocate buffer. handle=0x%08X\n", (unsigned int) handle);
            return 0;
        }
        pBuf = ion_map_kernel(gClient, handle);
        if (IS_ERR_OR_NULL(pBuf))
        {
            printk("ion_test_drv: Cannot map buffer to kernel. pBuf=0x%08X\n", (unsigned int) pBuf);
            return 0;
        }
        mm_data.config_buffer_param.kernel_handle = handle;
        mm_data.config_buffer_param.eModuleID = 2;
        mm_data.config_buffer_param.security = 1;
        mm_data.config_buffer_param.coherent = 0;
        mm_data.mm_cmd = ION_MM_CONFIG_BUFFER;
        if (ion_kernel_ioctl(gClient, ION_CMD_MULTIMEDIA, (unsigned long)&mm_data) < 0)
        {
            printk("ion_test_drv: Config buffer failed.\n");
        }
        if (ion_phys(gClient, handle, &phy_addr, &len) < 0)
        {
            printk("ion_test_drv: Cannot get physical address.\n");
            return 0;
        }
        printk("ion_test_drv: Physical Address: 0x%08X Len: 0x%X\n", (unsigned int) phy_addr, len);
        //ion_unmap_kernel(gClient, handle);
        //ion_free(gClient, handle);
    }
	return 0;
}

static ssize_t ion_test_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	return 0;
}

static ssize_t ion_test_write(struct file *file, const char __user *data, size_t len, loff_t *ppos)
{
	return 0;
}

static long ion_test_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{

    struct ion_handle* handle;
    char* pBuf;
    int i;
    printk("ion_test_drv: ion_test_ioctl() fd=%d\n", (int) arg);
    handle = ion_import_dma_buf(gClient, (int)arg);
    if (IS_ERR_OR_NULL(handle))
    {
        printk("ion_test_drv: Cannot import buffer. handle=0x%08X\n", (unsigned int)handle);
        return 0;
    }
    pBuf = (char*) ion_map_kernel(gClient, handle);
    if (IS_ERR_OR_NULL(pBuf))
    {
        printk("ion_test_drv: Cannot map kernel. pBuf=0x%08X\n", (unsigned int)pBuf);
        return 0;
    }

    for (i=0; i<1024*1024*20+256; i+=4)
    {
        if(*(volatile unsigned int*)(pBuf+i) != i)
        {
            aee_kernel_warning("ION_UT","kernel read data error");
            printk("ion_test_drv: read data error from kernel!!!!!!!!!!!!\n\n");
        }
    }
    ion_unmap_kernel(gClient, handle);
/*
    struct task_struct* pTask;
    printk("ion_test_drv: ioctl(%d)\n", arg);
    pTask = find_task_by_pid_ns(arg, &init_pid_ns);
    printk("ion_test_drv: Current task is 0x%08X pid=%d\n", (unsigned int) current, current->pid);
    if (pTask)
        printk("ion_test_drv: Target task is 0x%08X pid=%d\n", (unsigned int) pTask, pTask->pid);
    else
        printk("ion_test_drv: Target task is NULL\n");
*/
    return 0;
}

static int ion_test_mmap(struct file *file, struct vm_area_struct * vma)
{
    if (g_pBuf)
    {
	    unsigned long pfn = __phys_to_pfn(virt_to_phys(g_pBuf));
	    return remap_pfn_range(vma, vma->vm_start, pfn + vma->vm_pgoff,
			           vma->vm_end - vma->vm_start,
			           vma->vm_page_prot);

    }
    return 0;
}

struct file_operations ion_test_fops = {
	.owner   = THIS_MODULE,
	.unlocked_ioctl   = ion_test_ioctl,
	.open    = ion_test_open,    
	.release = ion_test_release,
	.read    = ion_test_read,
	.write   = ion_test_write,
	.mmap    = ion_test_mmap,
};

static int __init ion_test_init(void)
{
	struct class_device *class_dev = 0;
    int ret;
    printk("ion_test_drv: ion_test_init()\n");
	ret = alloc_chrdev_region(&ion_test_devno, 0, 1, ION_TEST_DEVNAME);

	ion_test_cdev = cdev_alloc();
	ion_test_cdev->owner = THIS_MODULE;
	ion_test_cdev->ops = &ion_test_fops;
	ret = cdev_add(ion_test_cdev, ion_test_devno, 1);
	ion_test_class = class_create(THIS_MODULE, ION_TEST_DEVNAME);
	class_dev = (struct class_device *)device_create(ion_test_class, NULL, ion_test_devno, NULL, ION_TEST_DEVNAME);

    if (g_ion_device)
    {
        gClient = ion_client_create(g_ion_device, 0xFFFFFFFF, "ion_test_drv_client");
        if (IS_ERR_OR_NULL(gClient))
        {
            printk("ion_test_drv: Cannot create client. gClient=0x%08X\n", (unsigned int) gClient);
            return 0;
        }
    }
    else
        printk("ion_test_drv: ion device not initialized.\n");
    g_pBuf = kzalloc(0x1000, GFP_KERNEL);
	return 0;
}

static void __exit ion_test_exit(void)
{
    printk("ion_test_drv: ion_test_exit()\n");
	device_destroy(ion_test_class, ion_test_devno);
	class_destroy(ion_test_class);
	cdev_del(ion_test_cdev);
	unregister_chrdev_region(ion_test_devno, 1);
    if (gClient)
        ion_client_destroy(gClient);
}

module_init(ion_test_init);
module_exit(ion_test_exit);

MODULE_DESCRIPTION("ION test driver");
MODULE_AUTHOR("MTK80344");
MODULE_LICENSE("GPL");
