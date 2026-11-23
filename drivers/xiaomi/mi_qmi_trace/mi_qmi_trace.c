// SPDX-License-Identifier: GPL-2.0
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/init.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/slab.h>
#include "mi_qmi_trace.h"

DEFINE_SPINLOCK(list_lock);
int device_state = DEVICE_STATE_DEFAULT;
Node *sleep_head = NULL;
Node *night_head = NULL;
Node *day_head = NULL;
void appendNode(const char *hex_service_id, const char *qmi_message_id);
static void kfree_list(Node *head);

Node *createNode(const char *str)
{
	Node *newNode = (Node *)kmalloc(sizeof(Node), GFP_KERNEL);
	if (!newNode) {
		pr_err("Memory allocation failed for Node! Requested size: %zu bytes\n",
		       sizeof(Node));
		return NULL;
	}
	newNode->qmi = kstrdup(str, GFP_KERNEL);
	if (!newNode->qmi) {
		pr_err("String allocation failed for: %s\n", str);
		kfree(newNode);
		return NULL;
	}
	newNode->count = 1;
	newNode->next = NULL;
	pr_info("Created new node: %s (state=%d)\n", str, device_state);
	return newNode;
}

Node *findNodeByQmi(Node *head, const char *qmi)
{
	Node *temp = head;
	size_t len = strlen(qmi);
	while (temp) {
		if (strncmp(temp->qmi, qmi, len) == 0 &&
		    strlen(temp->qmi) == len) {
			return temp;
		}
		temp = temp->next;
	}
	return NULL;
}

static void kfree_list(Node *head)
{
	Node *curr = head;
	while (curr) {
		Node *next = curr->next;
		if (curr->qmi) {
			kfree(curr->qmi);
		}
		kfree(curr);
		curr = next;
	}
}

void appendNode(const char *hex_service_id, const char *qmi_message_id)
{
	if (!hex_service_id || !qmi_message_id) {
		pr_err("NULL pointer detected\n");
		return;
	}

	Node **head = NULL;
	switch (device_state) {
	case DEVICE_STATE_SLEEP:
		head = &sleep_head;
		break;
	case DEVICE_STATE_NIGHT:
		head = &night_head;
		break;
	case DEVICE_STATE_DAY:
		head = &day_head;
		break;
	default:
		return;
	}

	size_t qmi_len = strlen(qmi_message_id);
	char prefix[3], suffix[3];
	strncpy(prefix, qmi_message_id, 2);
	prefix[2] = '\0';
	strncpy(suffix, qmi_message_id + qmi_len - 2, 2);
	suffix[2] = '\0';

	char combined_id[64];
	int len = snprintf(combined_id, sizeof(combined_id), "%s-%s%s",
			   hex_service_id, suffix, prefix);

	if (len < 0 || (size_t)len >= sizeof(combined_id)) {
		pr_err("ID truncation: %s-%s%s\n", hex_service_id, suffix,
		       prefix);
		return;
	}

	spin_lock(&list_lock);
	Node *foundNode = findNodeByQmi(*head, combined_id);
	if (foundNode) {
		foundNode->count += 1;
		spin_unlock(&list_lock);
		pr_info("Count++: %s => %d (state=%d)\n", combined_id,
			foundNode->count, device_state);
		return;
	}

	spin_unlock(&list_lock);

	Node *newNode = createNode(combined_id);
	if (!newNode) {
		pr_err("Create failed: %s\n", combined_id);
		return;
	}

	spin_lock(&list_lock);
	if (*head == NULL) {
		*head = newNode;
		pr_info("New list head: %s (state=%d)\n", combined_id,
			device_state);
	} else {
		Node *temp = *head;
		while (temp->next) {
			temp = temp->next;
		}
		temp->next = newNode;
		pr_info("New node: %s (state=%d)\n", combined_id, device_state);
	}
	spin_unlock(&list_lock);
}
EXPORT_SYMBOL(appendNode);

static ssize_t qmi_read(struct file *file, char __user *buf, size_t count,
			loff_t *pos)
{
	if (*pos > 0) {
		return 0;
	}

	char output[1024];
	int len = 0;
	bool first = true;

	spin_lock(&list_lock);

	Node *curr = sleep_head;
	while (curr) {
		if (!first) {
			len += scnprintf(output + len, sizeof(output) - len,
					 ",");
		} else {
			first = false;
		}
		len += scnprintf(output + len, sizeof(output) - len, "%s:%d:%d",
				 curr->qmi, curr->count, DEVICE_STATE_SLEEP);
		curr = curr->next;
	}

	curr = night_head;
	while (curr) {
		if (!first) {
			len += scnprintf(output + len, sizeof(output) - len,
					 ", ");
		} else {
			first = false;
		}
		len += scnprintf(output + len, sizeof(output) - len, "%s:%d:%d",
				 curr->qmi, curr->count, DEVICE_STATE_NIGHT);
		curr = curr->next;
	}

	curr = day_head;
	while (curr) {
		if (!first) {
			len += scnprintf(output + len, sizeof(output) - len,
					 ", ");
		} else {
			first = false;
		}
		len += scnprintf(output + len, sizeof(output) - len, "%s:%d:%d",
				 curr->qmi, curr->count, DEVICE_STATE_DAY);
		curr = curr->next;
	}

	spin_unlock(&list_lock);

	if (len >= sizeof(output)) {
		pr_warn("Output buffer truncated\n");
		len = sizeof(output) - 1;
	}

	if (copy_to_user(buf, output, len)) {
		pr_err("Copy_to_user failed\n");
		return -EFAULT;
	}

	*pos = len;
	return len;
}

static ssize_t qmi_write(struct file *file, const char __user *buf,
			 size_t count, loff_t *pos)
{
	char input[64] = { 0 };

	if (count > sizeof(input) - 1) {
		return -EINVAL;
	}
	if (copy_from_user(input, buf, count)) {
		return -EFAULT;
	}

	if (count > 0 && input[count - 1] == '\n') {
		input[count - 1] = '\0';
	} else {
		input[count] = '\0';
	}
	pr_info("Input value: %s, size: %zu\n", input, count);

	if (input[0] == '0' && (input[1] == 'x' || input[1] == 'X')) {
		// 十六进制，qmi消息记录
		char data[] = "0300010001001f00";
		appendNode(input, data);
		pr_info("Find qmi: %s\n", input);
	} else {
		// 十进制，更新设备状态
		int new_state = input[0];
		if (new_state >= DEVICE_STATE_DEFAULT &&
		    new_state <= DEVICE_STATE_DAY) {
			pr_info("Device state changed: %d -> %d\n",
				device_state, new_state);
			device_state = new_state;
		} else if (new_state == DEVICE_STATE_CLEAR) {
			pr_info("Received clear command\n");
			Node *temp_sleep, *temp_night, *temp_day;

			spin_lock(&list_lock);
			temp_sleep = sleep_head;
			temp_night = night_head;
			temp_day = day_head;
			sleep_head = NULL;
			night_head = NULL;
			day_head = NULL;
			spin_unlock(&list_lock);

			kfree_list(temp_sleep);
			kfree_list(temp_night);
			kfree_list(temp_day);

			pr_info("All lists cleared successfully\n");
		} else {
			pr_warn("Invalid state value: %d\n", new_state);
		}
	}
	return count;
}

static struct proc_ops mi_qmi_trace_fops = {
	.proc_read = qmi_read,
	.proc_write = qmi_write,
};

static int __init mi_qmi_trace_init(void)
{
	struct proc_dir_entry *entry;
	entry = proc_create("mi_qmi_trace", 0777, NULL, &mi_qmi_trace_fops);
	if (!entry) {
		pr_info("Failed to create proc entry\n");
		return -ENOMEM;
	}
	pr_info("Module loaded: mi_qmi_trace\n");

	return 0;
}
static void __exit mi_qmi_trace_exit(void)
{
	remove_proc_entry("mi_qmi_trace", NULL);
	pr_info("Module unloaded: mi_qmi_trace\n");
}
module_init(mi_qmi_trace_init);
module_exit(mi_qmi_trace_exit);
MODULE_LICENSE("GPL v2");