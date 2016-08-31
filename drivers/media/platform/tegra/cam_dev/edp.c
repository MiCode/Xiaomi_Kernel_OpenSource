/*
 * edp.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define CAMERA_DEVICE_INTERNAL

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/edp.h>

#include <media/nvc.h>
#include <media/camera.h>

void camera_edp_lowest(struct camera_device *cdev)
{
	struct camera_edp_cfg *cam_edp = &cdev->edpc;
	struct edp_client *edp_client = &cam_edp->edp_client;

	if (!cam_edp->edpc_en)
		return;

	cam_edp->edp_state = edp_client->num_states - 1;
	dev_dbg(cdev->dev, "%s %d\n", __func__, cam_edp->edp_state);
	if (edp_update_client_request(edp_client, cam_edp->edp_state, NULL)) {
		dev_err(cdev->dev, "THIS IS NOT LIKELY HAPPEN!\n");
		dev_err(cdev->dev, "UNABLE TO SET LOWEST EDP STATE!\n");
	}
}

static void camera_edp_throttle(unsigned int new_state, void *priv_data)
{
	struct camera_device *cdev = priv_data;

	if (cdev->edpc.shutdown)
		cdev->edpc.shutdown(cdev);
}

void camera_edp_register(struct camera_device *cdev)
{
	struct edp_manager *edp_manager;
	struct camera_edp_cfg *cam_edp = &cdev->edpc;
	struct edp_client *edp_client = &cam_edp->edp_client;
	int ret;

	if (!edp_client->num_states) {
		dev_notice(cdev->dev, "%s: NO edp states defined.\n", __func__);
		return;
	}
	cam_edp->edpc_en = 0;

	snprintf(edp_client->name, sizeof(edp_client->name) - 1, "%x%02x%s",
		cdev->client->adapter->nr, cdev->client->addr, cdev->name);
	edp_client->name[EDP_NAME_LEN - 1] = 0;
	edp_client->private_data = cdev;
	edp_client->throttle = camera_edp_throttle;

	dev_dbg(cdev->dev, "%s: %s, e0 = %d, p %d\n", __func__,
		edp_client->name, edp_client->e0_index, edp_client->priority);
	for (ret = 0; ret < edp_client->num_states; ret++)
		dev_dbg(cdev->dev, "e%d = %d mA",
			ret - edp_client->e0_index, edp_client->states[ret]);

	edp_manager = edp_get_manager("battery");
	if (!edp_manager) {
		dev_err(cdev->dev, "unable to get edp manager: battery\n");
		return;
	}

	ret = edp_register_client(edp_manager, edp_client);
	if (ret) {
		dev_err(cdev->dev, "unable to register edp client\n");
		return;
	}

	cam_edp->edpc_en = 1;
	/* set to lowest state at init */
	camera_edp_lowest(cdev);
}

int camera_edp_req(struct camera_device *cdev, unsigned new_state)
{
	struct camera_edp_cfg *cam_edp = &cdev->edpc;
	unsigned approved;
	int ret = 0;

	if (!cam_edp->edpc_en)
		return 0;

	dev_dbg(cdev->dev, "%s %d\n", __func__, new_state);
	ret = edp_update_client_request(
		&cam_edp->edp_client, new_state, &approved);
	if (ret) {
		dev_err(cdev->dev, "E state transition failed\n");
		return ret;
	}

	cam_edp->edp_state = approved;
	if (approved > new_state) {
		dev_err(cdev->dev, "EDP no enough current\n");
		return -EAGAIN;
	}

	return 0;
}
