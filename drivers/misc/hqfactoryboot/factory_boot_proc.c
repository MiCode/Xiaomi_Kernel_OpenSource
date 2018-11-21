/*   Inc. (C) 2011. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS (" SOFTWARE")
 * RECEIVED FROM  AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER ON
 * AN "AS-IS" BASIS ONLY.  EXPRESSLY DISCLAIMS ANY AND ALL WARRANTIES,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR NONINFRINGEMENT.
 * NEITHER DOES  PROVIDE ANY WARRANTY WHATSOEVER WITH RESPECT TO THE
 * SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY, INCORPORATED IN, OR
 * SUPPLIED WITH THE  SOFTWARE, AND RECEIVER AGREES TO LOOK ONLY TO SUCH
 * THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO. RECEIVER EXPRESSLY ACKNOWLEDGES
 * THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES
 * CONTAINED IN  SOFTWARE.  SHALL ALSO NOT BE RESPONSIBLE FOR ANY
 * SOFTWARE RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND 'S ENTIRE AND
 * CUMULATIVE LIABILITY WITH RESPECT TO THE  SOFTWARE RELEASED HEREUNDER WILL BE,
 * AT 'S OPTION, TO REVISE OR REPLACE THE  SOFTWARE AT ISSUE,
 * OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE CHARGE PAID BY RECEIVER TO
 *  FOR SUCH  SOFTWARE AT ISSUE.
 *
 */

#include <linux/proc_fs.h>
#include <linux/module.h>
#include <linux/seq_file.h>

static struct proc_dir_entry *factory_boot_proc;
static unsigned int boot_into_factory;

static int factory_boot_proc_show(struct seq_file *file, void *data)
{
	seq_printf(file, "%d\n", boot_into_factory);

	return 0;
}

static int factory_boot_proc_open (struct inode *inode, struct file *file)
{
	return single_open(file, factory_boot_proc_show, inode->i_private);
}

static const struct file_operations factory_boot_proc_fops =
{
	.open       = factory_boot_proc_open,
	.read       = seq_read,
	.llseek     = seq_lseek,
	.release    = seq_release,
};

static int __init get_boot_mode(char *str)
{
	if (strcmp("factory_boot", str) == 0) {
		boot_into_factory = 1;
	}

	return 0;
}

__setup("androidboot.factory=", get_boot_mode);

static int __init factory_boot_init(void)
{
	factory_boot_proc = proc_create("boot_status", 0644, NULL, &factory_boot_proc_fops);
	if (factory_boot_proc == NULL) {
		pr_err("[%s]: create_proc_entry factory_boot_proc failed!\n", __func__);
	}

	return 0;
}

core_initcall(factory_boot_init);

MODULE_DESCRIPTION(" Boot Into Factory Driver");
MODULE_LICENSE("GPL");
