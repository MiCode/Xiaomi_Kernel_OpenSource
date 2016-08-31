/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/sysedp.h>
#include <linux/err.h>
#include <trace/events/sysedp.h>
#include "sysedp_internal.h"

static struct kobject sysedp_kobj;
static struct kset *consumers_kset;

struct sysedp_consumer_attribute {
	struct attribute attr;
	ssize_t (*show)(struct sysedp_consumer *c, char *buf);
	ssize_t (*store)(struct sysedp_consumer *c,
			const char *buf, size_t count);
};


static ssize_t states_show(struct sysedp_consumer *c, char *s)
{
	unsigned int i;
	int cnt = 0;
	const int sz = sizeof(*c->states) * 3 + 2;

	for (i = 0; i < c->num_states && (cnt + sz) < PAGE_SIZE; i++)
		cnt += sprintf(s + cnt, "%s%u", i ? " " : "", c->states[i]);

	cnt += sprintf(s + cnt, "\n");
	return cnt;
}

static ssize_t ocpeaks_show(struct sysedp_consumer *c, char *s)
{
	unsigned int i;
	int cnt = 0;
	unsigned int *p;
	const int sz = sizeof(*p) * 3 + 2;

	p = c->ocpeaks ? c->ocpeaks : c->states;

	for (i = 0; i < c->num_states && (cnt + sz) < PAGE_SIZE; i++)
		cnt += sprintf(s + cnt, "%s%u", i ? " " : "", p[i]);

	cnt += sprintf(s + cnt, "\n");
	return cnt;
}


static ssize_t current_show(struct sysedp_consumer *c, char *s)
{
	return sprintf(s, "%u\n", c->states[c->state]);
}

static ssize_t state_show(struct sysedp_consumer *c, char *s)
{
	return sprintf(s, "%u\n", c->state);
}

static ssize_t state_store(struct sysedp_consumer *c, const char *s,
			   size_t count)
{
	unsigned int new_state;

	if (sscanf(s, "%u", &new_state) != 1)
		return -EINVAL;

	sysedp_set_state(c, new_state);

	return count;
}

static struct sysedp_consumer_attribute attr_current = {
	.attr = { .name = "current", .mode = 0444 },
	.show = current_show
};
static struct sysedp_consumer_attribute attr_state = __ATTR(state, 0660,
							    state_show,
							    state_store);
static struct sysedp_consumer_attribute attr_states = __ATTR_RO(states);
static struct sysedp_consumer_attribute attr_ocpeaks = __ATTR_RO(ocpeaks);

static struct attribute *consumer_attrs[] = {
	&attr_current.attr,
	&attr_state.attr,
	&attr_states.attr,
	&attr_ocpeaks.attr,
	NULL
};

static struct sysedp_consumer *to_consumer(struct kobject *kobj)
{
	return container_of(kobj, struct sysedp_consumer, kobj);
}

static ssize_t consumer_attr_show(struct kobject *kobj,
		struct attribute *attr,	char *buf)
{
	ssize_t r = -EINVAL;
	struct sysedp_consumer *c;
	struct sysedp_consumer_attribute *cattr;

	c = to_consumer(kobj);
	cattr = container_of(attr, struct sysedp_consumer_attribute, attr);
	if (c && cattr) {
		if (cattr->show)
			r = cattr->show(c, buf);
	}

	return r;
}

static ssize_t consumer_attr_store(struct kobject *kobj,
		struct attribute *attr, const char *buf, size_t count)
{
	ssize_t r = -EINVAL;
	struct sysedp_consumer *c;
	struct sysedp_consumer_attribute *cattr;

	c = to_consumer(kobj);
	cattr = container_of(attr, struct sysedp_consumer_attribute, attr);
	if (c && cattr) {
		if (cattr->store)
			r = cattr->store(c, buf, count);
	}

	return r;
}

static const struct sysfs_ops consumer_sysfs_ops = {
	.show = consumer_attr_show,
	.store = consumer_attr_store
};

static struct kobj_type ktype_consumer = {
	.sysfs_ops = &consumer_sysfs_ops,
	.default_attrs = consumer_attrs
};

int sysedp_consumer_add_kobject(struct sysedp_consumer *consumer)
{
	int ret;

	consumer->kobj.kset = consumers_kset;
	kobject_init(&consumer->kobj, &ktype_consumer);

	ret = kobject_add(&consumer->kobj, NULL, consumer->name);
	if (ret) {
		pr_err("%s: failed to add sysfs consumer entry\n",
		       consumer->name);
		return ret;
	}

	ret = kobject_uevent(&consumer->kobj, KOBJ_ADD);
	if (ret) {
		pr_err("%s: failed to send uevent\n",
		       consumer->name);
		kobject_put(&consumer->kobj);
		return ret;
	}
	return 0;
}

void sysedp_consumer_remove_kobject(struct sysedp_consumer *consumer)
{
	kobject_put(&consumer->kobj);
}

struct sysedp_attribute {
	struct attribute attr;
	ssize_t (*show)(char *buf);
	ssize_t (*store)(const char *buf, size_t count);
};

static unsigned int *get_tokenized_data(const char *buf,
					unsigned int *num_tokens)
{
	const char *cp;
	int i;
	unsigned int ntokens = 1;
	unsigned int *tokenized_data;
	int err = -EINVAL;

	if (!buf || *buf == 0)
		goto err;

	cp = buf;
	while ((cp = strpbrk(cp + 1, ",;")))
		if (*cp == ';')
			break;
		else
			ntokens++;

	tokenized_data = kmalloc(ntokens * sizeof(unsigned int),
				 GFP_KERNEL);
	if (!tokenized_data) {
		err = -ENOMEM;
		goto err;
	}

	cp = buf;
	i = 0;
	while (i < ntokens) {
		if (sscanf(cp, "%u", &tokenized_data[i++]) != 1)
			goto err_kfree;
		cp = strpbrk(cp, ",;");
		if (!cp || *cp == ';')
			break;
		cp++;
	}

	if (i != ntokens)
		goto err_kfree;

	*num_tokens = ntokens;
	return tokenized_data;

err_kfree:
	kfree(tokenized_data);
err:
	return ERR_PTR(err);
}


static ssize_t consumer_register_store(const char *s, size_t count)
{
	size_t name_len;
	unsigned int *states = 0;
	unsigned int *ocpeaks = 0;
	unsigned int num_states;
	unsigned int num_ocpeaks;
	struct sysedp_consumer *consumer = 0;
	const char *s2;
	int err;

	name_len = strcspn(s, " \n");
	if (name_len > SYSEDP_NAME_LEN-1)
		return -EINVAL;

	states = get_tokenized_data(s + name_len, &num_states);
	if (IS_ERR_OR_NULL(states))
		return PTR_ERR(states);

	/* Parse for optional 2nd table (peak values) */
	s2 = strpbrk(s + name_len, ";");
	if (s2) {
		ocpeaks = get_tokenized_data(s2 + 1, &num_ocpeaks);
		if (IS_ERR_OR_NULL(ocpeaks)) {
			err = PTR_ERR(ocpeaks);
			ocpeaks = 0;
			goto err_kfree;
		}
		if (num_states != num_ocpeaks) {
			err = -EINVAL;
			goto err_kfree;
		}

	}

	consumer = kzalloc(sizeof(*consumer), GFP_KERNEL);
	if (IS_ERR_OR_NULL(consumer)) {
		err = PTR_ERR(consumer);
		consumer = 0;
		goto err_kfree;
	}

	memcpy(consumer->name, s, name_len);
	consumer->name[name_len] = 0;
	consumer->states = states;
	consumer->ocpeaks = ocpeaks;
	consumer->num_states = num_states;
	consumer->removable = 1;

	err = sysedp_register_consumer(consumer);
	if (err)
		goto err_kfree;

	return count;
err_kfree:
	kfree(states);
	kfree(ocpeaks);
	kfree(consumer);
	return err;
}

static ssize_t consumer_unregister_store(const char *s, size_t count)
{
	char name[SYSEDP_NAME_LEN];
	size_t n;
	struct sysedp_consumer *consumer;

	n = count > SYSEDP_NAME_LEN - 1 ? SYSEDP_NAME_LEN - 1 : count;
	strncpy(name, s, n);
	name[n] = 0;
	consumer = sysedp_get_consumer(strim(name));

	if (!consumer)
		return -EINVAL;

	if (!consumer->removable)
		return -EINVAL;


	sysedp_unregister_consumer(consumer);
	kfree(consumer->states);
	kfree(consumer->ocpeaks);
	kfree(consumer);

	return count;
}

static ssize_t margin_show(char *s)
{
	return sprintf(s, "%d\n", margin);
}

static ssize_t margin_store(const char *s, size_t count)
{
	int val;
	if (sscanf(s, "%d", &val) != 1)
		return -EINVAL;

	mutex_lock(&sysedp_lock);
	margin = val;
	_sysedp_refresh();
	mutex_unlock(&sysedp_lock);

	return count;
}

static ssize_t avail_budget_show(char *s)
{
	return sprintf(s, "%u\n", avail_budget);
}

static struct sysedp_attribute attr_consumer_register =
	__ATTR(consumer_register, 0220, NULL, consumer_register_store);
static struct sysedp_attribute attr_consumer_unregister =
	__ATTR(consumer_unregister, 0220, NULL, consumer_unregister_store);
static struct sysedp_attribute attr_margin =
	__ATTR(margin, 0660, margin_show, margin_store);
static struct sysedp_attribute attr_avail_budget = __ATTR_RO(avail_budget);

static struct attribute *sysedp_attrs[] = {
	&attr_consumer_register.attr,
	&attr_consumer_unregister.attr,
	&attr_margin.attr,
	&attr_avail_budget.attr,
	NULL
};

static ssize_t sysedp_attr_show(struct kobject *kobj,
				struct attribute *_attr, char *buf)
{
	ssize_t r = -EINVAL;
	struct sysedp_attribute *attr;
	attr = container_of(_attr, struct sysedp_attribute, attr);
	if (attr && attr->show)
		r = attr->show(buf);
	return r;
}

static ssize_t sysedp_attr_store(struct kobject *kobj, struct attribute *_attr,
				 const char *buf, size_t count)
{
	ssize_t r = -EINVAL;
	struct sysedp_attribute *attr;
	attr = container_of(_attr, struct sysedp_attribute, attr);
	if (attr && attr->store)
		r = attr->store(buf, count);
	return r;
}

static const struct sysfs_ops sysedp_sysfs_ops = {
	.show = sysedp_attr_show,
	.store = sysedp_attr_store
};

static struct kobj_type ktype_sysedp = {
	.sysfs_ops = &sysedp_sysfs_ops,
	.default_attrs = sysedp_attrs
};

static const struct kset_uevent_ops sysedp_uevent_ops = {
};


int sysedp_init_sysfs(void)
{
	int ret;
	struct kobject *parent = NULL;

#ifdef CONFIG_PM
	parent = power_kobj;
#endif

	ret = kobject_init_and_add(&sysedp_kobj, &ktype_sysedp,
				   parent, "sysedp");
	if (ret) {
		pr_err("sysedp_init_sysfs: initialization failed\n");
		return ret;
	}

	consumers_kset = kset_create_and_add("consumers", &sysedp_uevent_ops,
					     &sysedp_kobj);
	if (!consumers_kset) {
		pr_err("sysedp_init_sysfs: consumers kset init failed\n");
		return -EFAULT;
	}
	return 0;

}
