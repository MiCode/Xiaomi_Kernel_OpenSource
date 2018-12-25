/* -*- mode: c; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* vi: set ts=8 sw=8 sts=8: */
/*************************************************************************/ /*!
@File
@Codingstyle    LinuxKernel
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#include <linux/debugfs.h>

#include "drm_pdp_drv.h"

#define PDP_DEBUGFS_DISPLAY_ENABLED "display_enabled"

static int display_enabled_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;

	return 0;
}

static ssize_t display_enabled_read(struct file *file,
				    char __user *user_buffer,
				    size_t count,
				    loff_t *position_ptr)
{
	struct drm_device *dev = file->private_data;
	struct pdp_drm_private *dev_priv = dev->dev_private;
	loff_t position = *position_ptr;
	char buffer[] = "N\n";
	size_t buffer_size = ARRAY_SIZE(buffer);
	int err;

	if (position < 0)
		return -EINVAL;
	else if (position >= buffer_size || count == 0)
		return 0;

	if (dev_priv->display_enabled)
		buffer[0] = 'Y';

	if (count > buffer_size - position)
		count = buffer_size - position;

	err = copy_to_user(user_buffer, &buffer[position], count);
	if (err)
		return -EFAULT;

	*position_ptr = position + count;

	return count;
}

static ssize_t display_enabled_write(struct file *file,
				     const char __user *user_buffer,
				     size_t count,
				     loff_t *position)
{
	struct drm_device *dev = file->private_data;
	struct pdp_drm_private *dev_priv = dev->dev_private;
	char buffer[3];
	int err;

	count = min(count, ARRAY_SIZE(buffer) - 1);

	err = copy_from_user(buffer, user_buffer, count);
	if (err)
		return -EFAULT;
	buffer[count] = '\0';

	if (!strtobool(buffer, &dev_priv->display_enabled) && dev_priv->crtc)
		pdp_crtc_set_plane_enabled(dev_priv->crtc, dev_priv->display_enabled);

	return count;
}

static const struct file_operations pdp_display_enabled_fops = {
	.owner = THIS_MODULE,
	.open = display_enabled_open,
	.read = display_enabled_read,
	.write = display_enabled_write,
	.llseek = default_llseek,
};

static int pdp_debugfs_create(struct drm_minor *minor, const char *name,
			      umode_t mode, const struct file_operations *fops)
{
	struct drm_info_node *node;

	/*
	 * We can't get access to our driver private data when this function is
	 * called so we fake up a node so that we can clean up entries later on.
	 */
	node = kzalloc(sizeof(*node), GFP_KERNEL);
	if (!node)
		return -ENOMEM;

	node->dent = debugfs_create_file(name, mode, minor->debugfs_root,
					 minor->dev, fops);
	if (!node->dent) {
		kfree(node);
		return -ENOMEM;
	}

	node->minor = minor;
	node->info_ent = (void *) fops;

	mutex_lock(&minor->debugfs_lock);
	list_add(&node->list, &minor->debugfs_list);
	mutex_unlock(&minor->debugfs_lock);

	return 0;
}

int pdp_debugfs_init(struct drm_minor *minor)
{
	int err;

	err = pdp_debugfs_create(minor, PDP_DEBUGFS_DISPLAY_ENABLED,
				 S_IFREG | S_IRUGO | S_IWUSR,
				 &pdp_display_enabled_fops);
	if (err) {
		DRM_INFO("failed to create '%s' debugfs entry\n",
			 PDP_DEBUGFS_DISPLAY_ENABLED);
	}

	return err;
}

void pdp_debugfs_cleanup(struct drm_minor *minor)
{
	drm_debugfs_remove_files((struct drm_info_list *) &pdp_display_enabled_fops,
				 1, minor);
}
