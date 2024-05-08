#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include "io_limit.h"

static struct kmem_cache *limit_node_cachep;
static struct rb_root limit_tree = RB_ROOT;
static DEFINE_SPINLOCK(limit_tree_lock);
static int limit_tree_size = 0;

static bool is_pid_valid(pid_t pid)
{
	struct task_struct *task;
	struct pid *pid_struct;

	pid_struct = find_get_pid(pid);
	if (pid_struct == NULL)
		return false;
	task = get_pid_task(pid_struct, PIDTYPE_PID);
	put_pid(pid_struct);
	if (task == NULL)
		return false;
	put_task_struct(task);

	return true;
}

struct limit_node {
	int pid;
	struct rb_node rb_node;
};

static struct limit_node *__get_limit_node(pid_t pid)
{
	struct rb_node *node = limit_tree.rb_node;
	struct limit_node *ret = NULL;

	while (node) {
		ret = rb_entry(node, struct limit_node, rb_node);
		if (ret->pid > pid)
			node = node->rb_right;
		else if (ret->pid < pid)
			node = node->rb_left;
		else
			return ret;
	}
	return NULL;
}

static struct limit_node *__insert_limit_node(struct limit_node *node)
{
	struct rb_node **p = &limit_tree.rb_node;
	struct rb_node *parent = NULL;
	struct limit_node *ret = NULL;

	while (*p) {
		parent = *p;
		ret = rb_entry(*p, struct limit_node, rb_node);
		if (ret->pid > node->pid)
			p = &parent->rb_right;
		else if (ret->pid < node->pid)
			p = &parent->rb_left;
		else
			return ret;
	}
	rb_link_node(&node->rb_node, parent, p);
	rb_insert_color(&node->rb_node, &limit_tree);
	limit_tree_size++;
	return NULL;
}

struct limit_node *__del_limit_node(pid_t pid)
{
	struct limit_node *node = __get_limit_node(pid);

	if (node) {
		rb_erase(&node->rb_node, &limit_tree);
		limit_tree_size--;
	}
	return node;
}

static inline struct limit_node *limit_node_alloc(gfp_t gfp)
{
	return limit_node_cachep != NULL ?
		       kmem_cache_alloc(limit_node_cachep, gfp) :
		       NULL;
}

static void limit_node_free(struct limit_node *node)
{
	if (limit_node_cachep != NULL)
		kmem_cache_free(limit_node_cachep, node);
}

static void insert_limit_node(pid_t pid)
{
	struct limit_node *node = limit_node_alloc(GFP_KERNEL);

	if (node) {
		unsigned long flags;

		node->pid = pid;
		spin_lock_irqsave(&limit_tree_lock, flags);
		if (__insert_limit_node(node)) {
			limit_node_free(node);
			node = NULL;
		}
		spin_unlock_irqrestore(&limit_tree_lock, flags);
	} else {
		pr_err("%s:%u: [mi_io_limit] Failed to allocate memory: %d\n",
		       __FILE__, __LINE__, -ENOMEM);
	}
}

void del_limit_node(pid_t pid)
{
	struct limit_node *node;
	unsigned long flags;

	spin_lock_irqsave(&limit_tree_lock, flags);
	node = __del_limit_node(pid);
	spin_unlock_irqrestore(&limit_tree_lock, flags);
	if (node) {
		limit_node_free(node);
		node = NULL;
	}
}

bool check_pid_in_limit_list(pid_t pid)
{
	struct limit_node *node = NULL;
	unsigned long flags;

	spin_lock_irqsave(&limit_tree_lock, flags);
	node = __get_limit_node(pid);
	spin_unlock_irqrestore(&limit_tree_lock, flags);

	return node ? true : false;
}

bool init_limit_node_cachep(void)
{
	limit_node_cachep = kmem_cache_create(
		"limit_node", sizeof(struct limit_node), 0,
		SLAB_HWCACHE_ALIGN | SLAB_PANIC | SLAB_ACCOUNT, NULL);

	return limit_node_cachep != NULL;
}

void free_limit_node_cachep(void)
{
	kmem_cache_destroy(limit_node_cachep);
	limit_node_cachep = NULL;
}

int rbtree_show(char *buf)
{
	int len;
	struct limit_node *entry;
	struct rb_node *node = rb_first(&limit_tree);

	len = 0;
	for (; node; node = rb_next(node)) {
		entry = rb_entry(node, struct limit_node, rb_node);
		len += snprintf(buf + len, PAGE_SIZE - len, "%d\n", entry->pid);
		if (len >= PAGE_SIZE) {
			break;
		}
	}

	return len;
}

int rbtree_store(const char *buf, size_t len)
{
	pid_t pid;

	if (sscanf(buf, "%u", &pid) != 1)
		return -EINVAL;

	if (!is_pid_valid(pid)) {
		return -EINVAL;
	}

	if (check_pid_in_limit_list(pid)) {
		del_limit_node(pid);
	} else {
		insert_limit_node(pid);
	}

	return len;
}
