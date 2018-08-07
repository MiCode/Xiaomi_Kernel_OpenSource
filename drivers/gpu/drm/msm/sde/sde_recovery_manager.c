/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "sde_recovery_manager.h"
#include  "sde_kms.h"


static struct recovery_mgr_info *rec_mgr;

static ssize_t sde_recovery_mgr_rda_clients_attr(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	ssize_t len = 0;
	struct list_head *pos;
	struct recovery_client_db *temp = NULL;

	mutex_lock(&rec_mgr->rec_lock);

	len = snprintf(buf, PAGE_SIZE, "Clients:\n");

	list_for_each(pos, &rec_mgr->client_list) {
		temp = list_entry(pos, struct recovery_client_db, list);

		len += snprintf(buf + len, PAGE_SIZE - len, "%s\n",
					temp->client_info.name);
	}

	mutex_unlock(&rec_mgr->rec_lock);

	return len;
}

static DEVICE_ATTR(clients, S_IRUGO, sde_recovery_mgr_rda_clients_attr, NULL);

static struct attribute *recovery_attrs[] = {
	&dev_attr_clients.attr,
	NULL,
};

static struct attribute_group recovery_mgr_attr_group = {
	.attrs = recovery_attrs,
};

static void sde_recovery_mgr_notify(bool err_state)
{
	char *envp[2];
	char *uevent_str = kzalloc(SZ_4K, GFP_KERNEL);

	if (uevent_str == NULL) {
		DRM_ERROR("failed to allocate event string\n");
		return;
	}
	if (err_state == true)
		snprintf(uevent_str, MAX_REC_UEVENT_LEN,
				"DISPLAY_ERROR_RECOVERED\n");
	else
		snprintf(uevent_str, MAX_REC_UEVENT_LEN,
				"DISPLAY_CRITICAL_ERROR\n");

	DRM_DEBUG("generating uevent [%s]\n", uevent_str);

	envp[0] = uevent_str;
	envp[1] = NULL;

	mutex_lock(&rec_mgr->dev->mode_config.mutex);
	kobject_uevent_env(&rec_mgr->dev->primary->kdev->kobj,
			KOBJ_CHANGE, envp);
	mutex_unlock(&rec_mgr->dev->mode_config.mutex);
	kfree(uevent_str);
}

static void sde_recovery_mgr_recover(int err_code)
{
	struct list_head *pos;
	struct recovery_client_db *c = NULL;
	int tmp_err, rc, pre, post, i;
	bool found = false;
	static bool rec_flag = true;

	mutex_lock(&rec_mgr->rec_lock);
	list_for_each(pos, &rec_mgr->client_list) {
		c = list_entry(pos, struct recovery_client_db, list);

		mutex_unlock(&rec_mgr->rec_lock);

		for (i = 0; i < MAX_REC_ERR_SUPPORT; i++) {
			tmp_err = c->client_info.err_supported[i].
							reported_err_code;
			if (tmp_err == err_code) {
				found = true;
				break;
			}
		}

		if (found == true) {

			pre = c->client_info.err_supported[i].pre_err_code;
			if (pre && pre != '0')
				sde_recovery_mgr_recover(pre);

			if (c->client_info.recovery_cb) {
				rc = c->client_info.recovery_cb(err_code,
							&c->client_info);
				if (rc) {
					pr_err("%s failed to recover error %d\n",
						__func__, err_code);
					rec_flag = false;
				} else {
					pr_debug("%s Recovery successful[%d]\n",
						__func__, err_code);
				}
			}

			post = c->client_info.err_supported[i].post_err_code;
			if (post && post != '0')
				sde_recovery_mgr_recover(post);

		}
		mutex_lock(&rec_mgr->rec_lock);

		if (found)
			break;
	}

	if (rec_flag) {
		pr_debug("%s successful full recovery\n", __func__);
		sde_recovery_mgr_notify(true);
	}

	mutex_unlock(&rec_mgr->rec_lock);
}

static void sde_recovery_mgr_event_work(struct work_struct *work)
{
	struct list_head *pos, *q;
	struct recovery_event_db *temp_event;
	int err_code;

	if (!rec_mgr) {
		pr_err("%s recovery manager is NULL\n", __func__);
		return;
	}

	mutex_lock(&rec_mgr->rec_lock);

	list_for_each_safe(pos, q, &rec_mgr->event_list) {
		temp_event = list_entry(pos, struct recovery_event_db, list);

		err_code = temp_event->err;

		rec_mgr->recovery_ongoing = true;

		mutex_unlock(&rec_mgr->rec_lock);

		/* notify error */
		sde_recovery_mgr_notify(false);
		/* recover error */
		sde_recovery_mgr_recover(err_code);

		mutex_lock(&rec_mgr->rec_lock);

		list_del(pos);
		kfree(temp_event);
	}

	rec_mgr->recovery_ongoing = false;
	mutex_unlock(&rec_mgr->rec_lock);

}

int sde_recovery_set_events(int err)
{
	int rc = 0;
	struct list_head *pos;
	struct recovery_event_db *temp;
	bool found = false;

	mutex_lock(&rec_mgr->rec_lock);

	/* check if there is same event in the list */
	list_for_each(pos, &rec_mgr->event_list) {
		temp = list_entry(pos, struct recovery_event_db, list);
		if (err == temp->err) {
			found = true;
			pr_info("%s error %d is already present in list\n",
					__func__, err);
			break;
		}
	}

	if (!found) {
		temp = kzalloc(sizeof(struct recovery_event_db), GFP_KERNEL);
		if (!temp) {
			pr_err("%s out of memory\n", __func__);
			rc = -ENOMEM;
			goto out;
		}
		temp->err = err;

		list_add_tail(&temp->list, &rec_mgr->event_list);
		queue_work(rec_mgr->event_queue, &rec_mgr->event_work);
	}

out:
	mutex_unlock(&rec_mgr->rec_lock);
	return rc;
}

int sde_recovery_client_register(struct recovery_client_info *client)
{
	int rc = 0;
	struct list_head *pos;
	struct recovery_client_db *c = NULL;
	bool found = false;

	if (!rec_mgr) {
		pr_err("%s recovery manager is not initialized\n", __func__);
		return -EPERM;
	}

	if (!strlen(client->name)) {
		pr_err("%s client name is empty\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&rec_mgr->rec_lock);

	/* check if there is same client */
	list_for_each(pos, &rec_mgr->client_list) {
		c = list_entry(pos, struct recovery_client_db, list);
		if (!strcmp(c->client_info.name,
			client->name)) {
			found = true;
			break;
		}
	}

	if (!found) {
		c = kzalloc(sizeof(*c), GFP_KERNEL);
		if (!c) {
			pr_err("%s out of memory for client", __func__);
			rc = -ENOMEM;
			goto out;
		}
	} else {
		pr_err("%s client = %s is already registered\n",
				__func__, client->name);
		client->handle = c;
		goto out;
	}

	memcpy(&(c->client_info), client, sizeof(struct recovery_client_info));

	list_add_tail(&c->list, &rec_mgr->client_list);
	rec_mgr->num_of_clients++;

	client->handle = c;

out:
	mutex_unlock(&rec_mgr->rec_lock);
	return rc;
}

int sde_recovery_client_unregister(void *handle)
{
	struct list_head *pos, *q, *pos1;
	struct recovery_client_db *temp_client;
	struct recovery_event_db *temp;
	int client_err = 0;
	bool found = false;
	bool found_pending = false;
	int i, rc = 0;
	struct recovery_client_info *client =
			&((struct recovery_client_db *)handle)->client_info;

	if (!handle) {
		pr_err("%s handle is NULL\n", __func__);
		return -EINVAL;
	}

	if (!strlen(client->name)) {
		pr_err("%s client name is empty\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&rec_mgr->rec_lock);

	if (rec_mgr->recovery_ongoing) {
		pr_err("%s SDE Executing Recovery, Failed! Unregister client %s\n",
							__func__, client->name);
		goto out;
	}

	/* check if client is present in the list */
	list_for_each_safe(pos, q, &rec_mgr->client_list) {
		temp_client = list_entry(pos, struct recovery_client_db, list);
		if (!strcmp(temp_client->client_info.name, client->name)) {
			found = true;

			/* free any pending event for this client */
			list_for_each(pos1, &rec_mgr->event_list) {
				temp = list_entry(pos1,
					struct recovery_event_db, list);

				found_pending = false;
				for (i = 0; i < MAX_REC_ERR_SUPPORT; i++) {
					client_err = temp_client->
						client_info.err_supported[i].
						reported_err_code;
					if (temp->err == client_err)
						found_pending = true;
				}

				if (found_pending) {
					list_del(pos1);
					kfree(temp);
				}
			}

			list_del(pos);
			kfree(temp_client);
			rec_mgr->num_of_clients--;
			break;
		}
	}

	if (!found) {
		pr_err("%s can't find the client[%s] from db\n",
					__func__, client->name);
		rc = -EFAULT;
	}

out:
	mutex_unlock(&rec_mgr->rec_lock);
	return rc;
}

int sde_init_recovery_mgr(struct drm_device *dev)
{
	struct recovery_mgr_info *rec = NULL;
	int rc = 0;

	if (!dev || !dev->dev_private) {
		SDE_ERROR("drm device node invalid\n");
		return -EINVAL;
	}

	rec = kzalloc(sizeof(struct recovery_mgr_info), GFP_KERNEL);
	if (!rec)
		return -ENOMEM;

	mutex_init(&rec->rec_lock);

	rec->dev = dev;
	rc = sysfs_create_group(&dev->primary->kdev->kobj,
				&recovery_mgr_attr_group);
	if (rc) {
		pr_err("%s sysfs_create_group fails=%d", __func__, rc);
		rec->sysfs_created = false;
	} else {
		rec->sysfs_created = true;
	}

	INIT_LIST_HEAD(&rec->event_list);
	INIT_LIST_HEAD(&rec->client_list);
	INIT_WORK(&rec->event_work, sde_recovery_mgr_event_work);
	rec->event_queue = create_workqueue("recovery_event");

	if (IS_ERR_OR_NULL(rec->event_queue)) {
		pr_err("%s unable to create queue; errno = %ld",
			__func__, PTR_ERR(rec->event_queue));
		rec->event_queue = NULL;
		rc = -EFAULT;
		goto err;
	}

	rec_mgr = rec;

	return rc;

err:
	mutex_destroy(&rec->rec_lock);
	if (rec->sysfs_created)
		sysfs_remove_group(&rec_mgr->dev->primary->kdev->kobj,
				&recovery_mgr_attr_group);
	kfree(rec);
	return rc;
}
