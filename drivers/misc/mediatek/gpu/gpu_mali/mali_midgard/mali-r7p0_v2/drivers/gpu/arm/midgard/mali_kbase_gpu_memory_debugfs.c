/*
 *
 * (C) COPYRIGHT 2012-2015 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */



#include <mali_kbase_gpu_memory_debugfs.h>

#ifdef CONFIG_DEBUG_FS

/* MTK Memory Hal */
#include <mtk_gpu_utility.h> /*Test Code*/
#define MTK_MEMINFO_SIZE 150
typedef struct
{
	int pid;
	int used_pages;
} mtk_gpu_meminfo_type;

static mtk_gpu_meminfo_type mtk_gpu_meminfo[150];
static int mtk_gpu_memory_total = 0;
/* MTK Memory Hal */


/** Show callback for the @c gpu_memory debugfs file.
 *
 * This function is called to get the contents of the @c gpu_memory debugfs
 * file. This is a report of current gpu memory usage.
 *
 * @param sfile The debugfs entry
 * @param data Data associated with the entry
 *
 * @return 0 if successfully prints data in debugfs entry file
 *         -1 if it encountered an error
 */

static int kbasep_gpu_memory_seq_show(struct seq_file *sfile, void *data)
{
	ssize_t ret = 0;
	struct list_head *entry;
	const struct list_head *kbdev_list;
	int i = 0;
	for(i = 0; i < MTK_MEMINFO_SIZE; i++) {
		mtk_gpu_meminfo[i].pid = 0;
		mtk_gpu_meminfo[i].used_pages = 0;
	}

	kbdev_list = kbase_dev_list_get();
	list_for_each(entry, kbdev_list) {
		struct kbase_device *kbdev = NULL;
		struct kbasep_kctx_list_element *element;

		kbdev = list_entry(entry, struct kbase_device, entry);
		/* output the total memory usage and cap for this device */
		ret = seq_printf(sfile, "%-16s  %10u\n",
				kbdev->devname,
				atomic_read(&(kbdev->memdev.used_pages)));
		mtk_gpu_memory_total = atomic_read(&(kbdev->memdev.used_pages));
		mutex_lock(&kbdev->kctx_list_lock);

		i = 0;
		list_for_each_entry(element, &kbdev->kctx_list, link) {
			/* output the memory usage and cap for each kctx
			* opened on this device */
			ret = seq_printf(sfile, "  %s-0x%p %10u %10u\n", \
				"kctx",
				element->kctx, \
				atomic_read(&(element->kctx->used_pages)),
				element->kctx->tgid);
			mtk_gpu_meminfo[i].pid = element->kctx->tgid;
			mtk_gpu_meminfo[i].used_pages = (int)atomic_read(&(element->kctx->used_pages));
			i++;
		}
		mutex_unlock(&kbdev->kctx_list_lock);
	}
	kbase_dev_list_put(kbdev_list);
	//mtk_dump_gpu_memory_usage(); /*MTK dump gpu memory test code*/
	return ret;
}

/*
 *  File operations related to debugfs entry for gpu_memory
 */
static int kbasep_gpu_memory_debugfs_open(struct inode *in, struct file *file)
{
	return single_open(file, kbasep_gpu_memory_seq_show , NULL);
}

static const struct file_operations kbasep_gpu_memory_debugfs_fops = {
	.open = kbasep_gpu_memory_debugfs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 *  Initialize debugfs entry for gpu_memory
 */
void kbasep_gpu_memory_debugfs_init(struct kbase_device *kbdev)
{
	debugfs_create_file("gpu_memory", S_IRUGO,
			kbdev->mali_debugfs_directory, NULL,
			&kbasep_gpu_memory_debugfs_fops);
	return;
}

KBASE_EXPORT_TEST_API(kbase_dump_gpu_memory_usage)
bool kbase_dump_gpu_memory_usage()
{
	int i = 0;

	//output the total memory usage and cap for this device
	pr_warn(KERN_DEBUG "%10s\t%16s\n", "PID", "Memory by Page");
	pr_warn(KERN_DEBUG "============================\n");
	
	for(i = 0; (i < MTK_MEMINFO_SIZE) && (mtk_gpu_meminfo[i].pid != 0); i++) {
		pr_warn(KERN_DEBUG "%10d\t%16d\n", mtk_gpu_meminfo[i].pid, \
                                        mtk_gpu_meminfo[i].used_pages);
        }

	pr_warn(KERN_DEBUG "============================\n");
	pr_warn(KERN_DEBUG "%10s\t%16u\n", \
			"Total", \
			mtk_gpu_memory_total);
	pr_warn(KERN_DEBUG "============================\n");
	return true;
}

#else
/*
 * Stub functions for when debugfs is disabled
 */
void kbasep_gpu_memory_debugfs_init(struct kbase_device *kbdev)
{
	return;
}
#endif
