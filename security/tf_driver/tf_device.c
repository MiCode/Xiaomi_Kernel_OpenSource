/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * Copyright (C) 2011-2013 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <linux/atomic.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/page-flags.h>
#include <linux/pm.h>
#include <linux/syscore_ops.h>
#include <linux/vmalloc.h>
#include <linux/signal.h>
#ifdef CONFIG_ANDROID
#include <linux/device.h>
#endif

#include "tf_protocol.h"
#include "tf_defs.h"
#include "tf_util.h"
#include "tf_conn.h"
#include "tf_comm.h"
#ifdef CONFIG_TF_ZEBRA
#include <plat/cpu.h>
#include "tf_zebra.h"
#endif
#ifdef CONFIG_TF_DRIVER_CRYPTO_FIPS
#include "tf_crypto.h"
#endif

#include "s_version.h"

#define CREATE_TRACE_POINTS
#include <trace/events/nvsecurity.h>

/*----------------------------------------------------------------------------
 * Forward Declarations
 *----------------------------------------------------------------------------*/

/*
 * Creates and registers the device to be managed by the specified driver.
 *
 * Returns zero upon successful completion, or an appropriate error code upon
 * failure.
 */
static int tf_device_register(void);


/*
 * Implements the device Open callback.
 */
static int tf_device_open(
		struct inode *inode,
		struct file *file);


/*
 * Implements the device Release callback.
 */
static int tf_device_release(
		struct inode *inode,
		struct file *file);


/*
 * Implements the device ioctl callback.
 */
static long tf_device_ioctl(
		struct file *file,
		unsigned int ioctl_num,
		unsigned long ioctl_param);


/*
 * Implements the device shutdown callback.
 */
static void tf_device_shutdown(void);


/*
 * Implements the device suspend callback.
 */
static int tf_device_suspend(void);


/*
 * Implements the device resume callback.
 */
static void tf_device_resume(void);


/*---------------------------------------------------------------------------
 * Module Parameters
 *---------------------------------------------------------------------------*/

/*
 * The device major number used to register a unique character device driver.
 * Let the default value be 122
 */
static int device_major_number = 122;

module_param(device_major_number, int, 0000);
MODULE_PARM_DESC(device_major_number,
	"The device major number used to register a unique character "
	"device driver");

#ifdef CONFIG_TF_TRUSTZONE
/*
 * This software interrupt line is used by the S-World interrupt system
 * to inform N-World that it must schedule S-World as soon as it is possible.
 */
static int soft_interrupt = 191;    /*-1;*/
/*
 * Interrupt line #191 is not used by the hardware of Tegra3 and Tegra4.
 * The same signaling interrupt must be specified in S-World (GIC driver)
 */

module_param(soft_interrupt, int, 0000);
MODULE_PARM_DESC(soft_interrupt,
	"The softint interrupt line used by the Secure world");
#endif

#ifdef CONFIG_TF_DRIVER_DEBUG_SUPPORT
unsigned tf_debug_level = UINT_MAX;
module_param_named(debug, tf_debug_level, uint, 0644);
#endif

#ifdef CONFIG_TF_DRIVER_CRYPTO_FIPS
char *tf_integrity_hmac_sha256_expected_value;
module_param_named(hmac_sha256, tf_integrity_hmac_sha256_expected_value,
		   charp, 0444);

#ifdef CONFIG_TF_DRIVER_FAULT_INJECTION
unsigned tf_fault_injection_mask;
module_param_named(fault, tf_fault_injection_mask, uint, 0644);
#endif

int tf_self_test_blkcipher_align;
module_param_named(post_align, tf_self_test_blkcipher_align, int, 0644);
int tf_self_test_blkcipher_use_vmalloc;
module_param_named(post_vmalloc, tf_self_test_blkcipher_use_vmalloc, int, 0644);
#endif

#ifdef CONFIG_ANDROID
static struct class *tf_class;
#endif

/*----------------------------------------------------------------------------
 * Global Variables
 *----------------------------------------------------------------------------*/

/*
 * tf_driver character device definitions.
 * read and write methods are not defined
 * and will return an error if used by user space
 */
static const struct file_operations g_tf_device_file_ops = {
	.owner = THIS_MODULE,
	.open = tf_device_open,
	.release = tf_device_release,
	.unlocked_ioctl = tf_device_ioctl,
	.llseek = no_llseek,
};


static struct syscore_ops g_tf_device_syscore_ops = {
	.shutdown = tf_device_shutdown,
	.suspend = tf_device_suspend,
	.resume = tf_device_resume,
};

/* The single device supported by this driver */
static struct tf_device g_tf_dev;

/*----------------------------------------------------------------------------
 * Implementations
 *----------------------------------------------------------------------------*/

struct tf_device *tf_get_device(void)
{
	return &g_tf_dev;
}

/*
 * sysfs entries
 */
struct tf_sysfs_entry {
	struct attribute attr;
	ssize_t (*show)(struct tf_device *, char *);
	ssize_t (*store)(struct tf_device *, const char *, size_t);
};

/*
 * sysfs entry showing allocation stats
 */
static ssize_t info_show(struct tf_device *dev, char *buf)
{
	struct tf_device_stats *dev_stats = &dev->stats;

	return snprintf(buf, PAGE_SIZE,
		"stat.memories.allocated: %d\n"
		"stat.pages.allocated:    %d\n"
		"stat.pages.locked:       %d\n",
		atomic_read(&dev_stats->stat_memories_allocated),
		atomic_read(&dev_stats->stat_pages_allocated),
		atomic_read(&dev_stats->stat_pages_locked));
}
static struct tf_sysfs_entry tf_info_entry = __ATTR_RO(info);

#ifdef CONFIG_TF_ZEBRA
/*
 * sysfs entry showing whether secure world is up and running
 */
static ssize_t tf_started_show(struct tf_device *dev, char *buf)
{
	int tf_started = test_bit(TF_COMM_FLAG_PA_AVAILABLE,
		&dev->sm.flags);

	return snprintf(buf, PAGE_SIZE, "%s\n", tf_started ? "yes" : "no");
}
static struct tf_sysfs_entry tf_started_entry =
	__ATTR_RO(tf_started);

static ssize_t workspace_addr_show(struct tf_device *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", dev->workspace_addr);
}
static struct tf_sysfs_entry tf_workspace_addr_entry =
	__ATTR_RO(workspace_addr);

static ssize_t workspace_size_show(struct tf_device *dev, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%08x\n", dev->workspace_size);
}
static struct tf_sysfs_entry tf_workspace_size_entry =
	__ATTR_RO(workspace_size);
#endif

static ssize_t tf_attr_show(struct kobject *kobj, struct attribute *attr,
	char *page)
{
	struct tf_sysfs_entry *entry = container_of(attr, struct tf_sysfs_entry,
		attr);
	struct tf_device *dev = container_of(kobj, struct tf_device, kobj);

	if (!entry->show)
		return -EIO;

	return entry->show(dev, page);
}

static ssize_t tf_attr_store(struct kobject *kobj, struct attribute *attr,
	const char *page, size_t length)
{
	struct tf_sysfs_entry *entry = container_of(attr, struct tf_sysfs_entry,
		attr);
	struct tf_device *dev = container_of(kobj, struct tf_device, kobj);

	if (!entry->store)
		return -EIO;

	return entry->store(dev, page, length);
}

static void tf_kobj_release(struct kobject *kobj) {}

static struct attribute *tf_default_attrs[] = {
	&tf_info_entry.attr,
#ifdef CONFIG_TF_ZEBRA
	&tf_started_entry.attr,
	&tf_workspace_addr_entry.attr,
	&tf_workspace_size_entry.attr,
#endif
	NULL,
};
static const struct sysfs_ops tf_sysfs_ops = {
	.show	= tf_attr_show,
	.store	= tf_attr_store,
};
static struct kobj_type tf_ktype = {
	.release	= tf_kobj_release,
	.sysfs_ops	= &tf_sysfs_ops,
	.default_attrs	= tf_default_attrs
};

/*----------------------------------------------------------------------------*/

#if defined(MODULE) && defined(CONFIG_TF_ZEBRA)
static char *smc_mem;
module_param(smc_mem, charp, S_IRUGO);
#endif

/*
 * First routine called when the kernel module is loaded
 */
static int __init tf_device_register(void)
{
	int error;
	struct tf_device *dev = &g_tf_dev;

	dprintk(KERN_INFO "tf_device_register()\n");

	/*
	 * Initialize the device
	 */
	dev->dev_number = MKDEV(device_major_number,
		TF_DEVICE_MINOR_NUMBER);
	cdev_init(&dev->cdev, &g_tf_device_file_ops);
	dev->cdev.owner = THIS_MODULE;

	INIT_LIST_HEAD(&dev->connection_list);
	spin_lock_init(&dev->connection_list_lock);

#if defined(MODULE) && defined(CONFIG_TF_ZEBRA)
	error = (*tf_comm_early_init)();
	if (error)
		goto module_early_init_failed;

	error = tf_device_mshield_init(smc_mem);
	if (error)
		goto mshield_init_failed;

#ifdef CONFIG_TF_DRIVER_CRYPTO_FIPS
	error = tf_crypto_hmac_module_init();
	if (error)
		goto hmac_init_failed;

	error = tf_self_test_register_device();
	if (error)
		goto self_test_register_device_failed;
#endif
#endif

	/* register the sysfs object driver stats */
	error = kobject_init_and_add(&dev->kobj,  &tf_ktype, NULL, "%s",
		 TF_DEVICE_BASE_NAME);
	if (error) {
		printk(KERN_ERR "tf_device_register(): "
			"kobject_init_and_add failed (error %d)!\n", error);
		kobject_put(&dev->kobj);
		goto kobject_init_and_add_failed;
	}

	/*
	 * Register the system device.
	 */
	register_syscore_ops(&g_tf_device_syscore_ops);

	/*
	 * Register the char device.
	 */
	printk(KERN_INFO "Registering char device %s (%u:%u)\n",
		TF_DEVICE_BASE_NAME,
		MAJOR(dev->dev_number),
		MINOR(dev->dev_number));
	error = register_chrdev_region(dev->dev_number, 1,
		TF_DEVICE_BASE_NAME);
	if (error != 0) {
		printk(KERN_ERR "tf_device_register():"
			" register_chrdev_region failed (error %d)!\n",
			error);
		goto register_chrdev_region_failed;
	}

	error = cdev_add(&dev->cdev, dev->dev_number, 1);
	if (error != 0) {
		printk(KERN_ERR "tf_device_register(): "
			"cdev_add failed (error %d)!\n",
			error);
		goto cdev_add_failed;
	}

	/*
	 * Initialize the communication with the Secure World.
	 */
#ifdef CONFIG_TF_TRUSTZONE
	dev->sm.soft_int_irq = soft_interrupt;
#endif
	error = tf_init(&g_tf_dev.sm);
	if (error != S_SUCCESS) {
		dprintk(KERN_ERR "tf_device_register(): "
			"tf_init failed (error %d)!\n",
			error);
		goto init_failed;
	}

#ifdef CONFIG_TF_DRIVER_CRYPTO_FIPS
	error = tf_self_test_post_init(&(g_tf_dev.kobj));
	/* N.B. error > 0 indicates a POST failure, which will not
	   prevent the module from loading. */
	if (error < 0) {
		dprintk(KERN_ERR "tf_device_register(): "
			"tf_self_test_post_vectors failed (error %d)!\n",
			error);
		goto post_failed;
	}
#endif

#ifdef CONFIG_ANDROID
	tf_class = class_create(THIS_MODULE, TF_DEVICE_BASE_NAME);
	device_create(tf_class, NULL,
		dev->dev_number,
		NULL, TF_DEVICE_BASE_NAME);
#endif

#ifdef CONFIG_TF_ZEBRA
	/*
	 * Initializes the /dev/tf_ctrl device node.
	 */
	error = tf_ctrl_device_register();
	if (error)
		goto ctrl_failed;
#endif

#ifdef CONFIG_TF_DRIVER_DEBUG_SUPPORT
	address_cache_property((unsigned long) &tf_device_register);
#endif
	/*
	 * Successful completion.
	 */

	dprintk(KERN_INFO "tf_device_register(): Success\n");
	return 0;

	/*
	 * Error: undo all operations in the reverse order
	 */
#ifdef CONFIG_TF_ZEBRA
ctrl_failed:
#endif
#ifdef CONFIG_TF_DRIVER_CRYPTO_FIPS
	tf_self_test_post_exit();
post_failed:
#endif
init_failed:
	cdev_del(&dev->cdev);
cdev_add_failed:
	unregister_chrdev_region(dev->dev_number, 1);
register_chrdev_region_failed:
	unregister_syscore_ops(&g_tf_device_syscore_ops);
kobject_init_and_add_failed:
	kobject_del(&g_tf_dev.kobj);

#if defined(MODULE) && defined(CONFIG_TF_ZEBRA)
#ifdef CONFIG_TF_DRIVER_CRYPTO_FIPS
	tf_self_test_unregister_device();
self_test_register_device_failed:
	tf_crypto_hmac_module_exit();
hmac_init_failed:
#endif
	tf_device_mshield_exit();
mshield_init_failed:
module_early_init_failed:
#endif
	dprintk(KERN_INFO "tf_device_register(): Failure (error %d)\n",
		error);
	return error;
}

/*----------------------------------------------------------------------------*/

static int tf_device_open(struct inode *inode, struct file *file)
{
	int error;
	struct tf_device *dev = &g_tf_dev;
	struct tf_connection *connection = NULL;

	dprintk(KERN_INFO "tf_device_open(%u:%u, %p)\n",
		imajor(inode), iminor(inode), file);

	/* Dummy lseek for non-seekable driver */
	error = nonseekable_open(inode, file);
	if (error != 0) {
		dprintk(KERN_ERR "tf_device_open(%p): "
			"nonseekable_open failed (error %d)!\n",
			file, error);
		goto error;
	}

#ifndef CONFIG_ANDROID
	/*
	 * Check file flags. We only autthorize the O_RDWR access
	 */
	if (file->f_flags != O_RDWR) {
		dprintk(KERN_ERR "tf_device_open(%p): "
			"Invalid access mode %u\n",
			file, file->f_flags);
		error = -EACCES;
		goto error;
	}
#endif

	/*
	 * Open a new connection.
	 */

	error = tf_open(dev, file, &connection);
	if (error != 0) {
		dprintk(KERN_ERR "tf_device_open(%p): "
			"tf_open failed (error %d)!\n",
			file, error);
		goto error;
	}

	file->private_data = connection;

	/*
	 * Send the CreateDeviceContext command to the secure
	 */
	error = tf_create_device_context(connection);
	if (error != 0) {
		dprintk(KERN_ERR "tf_device_open(%p): "
			"tf_create_device_context failed (error %d)!\n",
			file, error);
		goto error1;
	}

	/*
	 * Successful completion.
	 */

	dprintk(KERN_INFO "tf_device_open(%p): Success (connection=%p)\n",
		file, connection);
	return 0;

	/*
	 * Error handling.
	 */

error1:
	tf_close(connection);
error:
	dprintk(KERN_INFO "tf_device_open(%p): Failure (error %d)\n",
		file, error);
	return error;
}

/*----------------------------------------------------------------------------*/

static int tf_device_release(struct inode *inode, struct file *file)
{
	struct tf_connection *connection;

	dprintk(KERN_INFO "tf_device_release(%u:%u, %p)\n",
		imajor(inode), iminor(inode), file);

	connection = tf_conn_from_file(file);
	tf_close(connection);

	dprintk(KERN_INFO "tf_device_release(%p): Success\n", file);
	return 0;
}

/*----------------------------------------------------------------------------*/

static long tf_device_ioctl(struct file *file, unsigned int ioctl_num,
	unsigned long ioctl_param)
{
	int result = S_SUCCESS;
	struct tf_connection *connection;
	union tf_command command;
	struct tf_command_header header;
	union tf_answer answer;
	u32 command_size;
	u32 answer_size;
	void *user_answer;

	dprintk(KERN_INFO "tf_device_ioctl(%p, %u, %p)\n",
		file, ioctl_num, (void *) ioctl_param);

	switch (ioctl_num) {
	case IOCTL_TF_GET_VERSION:
		/* ioctl is asking for the driver interface version */
		result = TF_DRIVER_INTERFACE_VERSION;
		goto exit;

#ifdef CONFIG_TF_ION
	case IOCTL_TF_ION_REGISTER: {
		int ion_register;
		/* ioctl is asking to register an ion handle */
		if (copy_from_user(&ion_register,
				(int *) ioctl_param,
				sizeof(int))) {
			dprintk(KERN_ERR "tf_device_ioctl(%p): "
				"copy_from_user failed\n",
				file);
			result = -EFAULT;
			goto exit;
		}

		connection = tf_conn_from_file(file);
		BUG_ON(connection == NULL);

		/* Initialize ION connection */
		if (connection->ion_client == NULL) {
			connection->ion_client = ion_client_create(
						zebra_ion_device,
						(1 << ION_HEAP_TYPE_CARVEOUT),
						"tf");
		}

		if (connection->ion_client == NULL) {
			dprintk(KERN_ERR "tf_device_ioctl(%p): "
				"unable to create ion client\n",
				file);
			result = -EFAULT;
			goto exit;
		}

		/*
		 * TODO: We should use a reference count on this handle in order
		 * to not unregistered it while using it.
		 */
	       return (long)ion_import_fd(connection->ion_client, ion_register);
	}

	case IOCTL_TF_ION_UNREGISTER: {
		int ion_register;
		/* ioctl is asking to unregister an ion handle */

		if (copy_from_user(&ion_register,
				(int *) ioctl_param,
				sizeof(int))) {
			dprintk(KERN_ERR "tf_device_ioctl(%p): "
				"copy_from_user failed\n",
				file);
			result = -EFAULT;
			goto exit;
		}

		connection = tf_conn_from_file(file);
		BUG_ON(connection == NULL);

		if (connection->ion_client == NULL) {
			dprintk(KERN_ERR "tf_device_ioctl(%p): "
				"ion client does not exist\n",
				file);
			result = -EFAULT;
			goto exit;
		}

		ion_free(connection->ion_client,
			(struct ion_handle *) ion_register);

		return S_SUCCESS;
	}
#endif

	case IOCTL_TF_EXCHANGE:
		/*
		 * ioctl is asking to perform a message exchange with the Secure
		 * Module
		 */

		/*
		 * Make a local copy of the data from the user application
		 * This routine checks the data is readable
		 *
		 * Get the header first.
		 */
		if (copy_from_user(&header,
				(struct tf_command_header *)ioctl_param,
				sizeof(struct tf_command_header))) {
			dprintk(KERN_ERR "tf_device_ioctl(%p): "
				"Cannot access ioctl parameter %p\n",
				file, (void *) ioctl_param);
			result = -EFAULT;
			goto exit;
		}

		/* size in words of u32 */
		command_size = header.message_size +
			sizeof(struct tf_command_header)/sizeof(u32);
		if (command_size > sizeof(command)/sizeof(u32)) {
			dprintk(KERN_ERR "tf_device_ioctl(%p): "
				"Buffer overflow: too many bytes to copy %d\n",
				file, command_size);
			result = -EFAULT;
			goto exit;
		}

		if (copy_from_user(&command,
				(union tf_command *)ioctl_param,
				command_size * sizeof(u32))) {
			dprintk(KERN_ERR "tf_device_ioctl(%p): "
				"Cannot access ioctl parameter %p\n",
				file, (void *) ioctl_param);
			result = -EFAULT;
			goto exit;
		}

		connection = tf_conn_from_file(file);
		BUG_ON(connection == NULL);

		/*
		 * The answer memory space address is in the operation_id field
		 */
		user_answer = (void *) command.header.operation_id;

		atomic_inc(&(connection->pending_op_count));

		dprintk(KERN_WARNING "tf_device_ioctl(%p): "
			"Sending message type  0x%08x\n",
			file, command.header.message_type);

		switch (command.header.message_type) {
		case TF_MESSAGE_TYPE_OPEN_CLIENT_SESSION:
			result = tf_open_client_session(connection,
				&command, &answer);
			break;

		case TF_MESSAGE_TYPE_CLOSE_CLIENT_SESSION:
			result = tf_close_client_session(connection,
				&command, &answer);
			break;

		case TF_MESSAGE_TYPE_REGISTER_SHARED_MEMORY:
			result = tf_register_shared_memory(connection,
				&command, &answer);
			break;

		case TF_MESSAGE_TYPE_RELEASE_SHARED_MEMORY:
			result = tf_release_shared_memory(connection,
				&command, &answer);
			break;

		case TF_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND:
			trace_invoke_client_command(NVSEC_INVOKE_CMD_START);
			result = tf_invoke_client_command(connection,
				&command, &answer);
			trace_invoke_client_command(NVSEC_INVOKE_CMD_DONE);
			break;

		case TF_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND:
			result = tf_cancel_client_command(connection,
				&command, &answer);
			break;

		default:
			dprintk(KERN_ERR "tf_device_ioctl(%p): "
				"Incorrect message type (0x%08x)!\n",
				connection, command.header.message_type);
			result = -EOPNOTSUPP;
			break;
		}

		atomic_dec(&(connection->pending_op_count));

		if (result != 0) {
			dprintk(KERN_WARNING "tf_device_ioctl(%p): "
				"Operation returning error code 0x%08x)!\n",
				file, result);
			goto exit;
		}

		/*
		 * Copy the answer back to the user space application.
		 * The driver does not check this field, only copy back to user
		 * space the data handed over by Secure World
		 */
		answer_size = answer.header.message_size +
			sizeof(struct tf_answer_header)/sizeof(u32);
		if (copy_to_user(user_answer,
				&answer, answer_size * sizeof(u32))) {
			dprintk(KERN_WARNING "tf_device_ioctl(%p): "
				"Failed to copy back the full command "
				"answer to %p\n", file, user_answer);
			result = -EFAULT;
			goto exit;
		}

		/* successful completion */
		dprintk(KERN_INFO "tf_device_ioctl(%p): Success\n", file);
		break;

	case  IOCTL_TF_GET_DESCRIPTION: {
		/* ioctl asking for the version information buffer */
		struct tf_version_information_buffer *pInfoBuffer;

		dprintk(KERN_INFO "IOCTL_TF_GET_DESCRIPTION:(%p, %u, %p)\n",
			file, ioctl_num, (void *) ioctl_param);

		pInfoBuffer =
			((struct tf_version_information_buffer *) ioctl_param);

		dprintk(KERN_INFO "IOCTL_TF_GET_DESCRIPTION1: "
			"driver_description=\"%64s\"\n", S_VERSION_STRING);

		if (copy_to_user(pInfoBuffer->driver_description,
				S_VERSION_STRING,
				strlen(S_VERSION_STRING) + 1)) {
			dprintk(KERN_ERR "tf_device_ioctl(%p): "
				"Fail to copy back the driver description "
				"to  %p\n",
				file, pInfoBuffer->driver_description);
			result = -EFAULT;
			goto exit;
		}

		dprintk(KERN_INFO "IOCTL_TF_GET_DESCRIPTION2: "
			"secure_world_description=\"%64s\"\n",
			tf_get_description(&g_tf_dev.sm));

		if (copy_to_user(pInfoBuffer->secure_world_description,
				tf_get_description(&g_tf_dev.sm),
				TF_DESCRIPTION_BUFFER_LENGTH)) {
			dprintk(KERN_WARNING "tf_device_ioctl(%p): "
				"Failed to copy back the secure world "
				"description to %p\n",
				file, pInfoBuffer->secure_world_description);
			result = -EFAULT;
			goto exit;
		}
		break;
	}

	default:
		dprintk(KERN_ERR "tf_device_ioctl(%p): "
			"Unknown IOCTL code 0x%08x!\n",
			file, ioctl_num);
		result = -EOPNOTSUPP;
		goto exit;
	}

exit:
	return result;
}

/*----------------------------------------------------------------------------*/

static void tf_device_shutdown(void)
{
	if (0 > tf_power_management(&g_tf_dev.sm, TF_POWER_OPERATION_SHUTDOWN))
		dprintk(KERN_ERR "tf_device_shutdown failing\n");
}

/*----------------------------------------------------------------------------*/

static int tf_device_suspend(void)
{
	dprintk(KERN_INFO "tf_device_suspend: Enter\n");
	return tf_power_management(&g_tf_dev.sm,
		TF_POWER_OPERATION_HIBERNATE);
}


/*----------------------------------------------------------------------------*/

static void tf_device_resume(void)
{
	if (0 > tf_power_management(&g_tf_dev.sm, TF_POWER_OPERATION_RESUME))
		dprintk(KERN_ERR "tf_device_resume failing\n");
}


/*----------------------------------------------------------------------------*/

module_init(tf_device_register);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Trusted Logic S.A.");
