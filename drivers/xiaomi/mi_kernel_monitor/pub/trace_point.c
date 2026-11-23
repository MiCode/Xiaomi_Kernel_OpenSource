

#include <linux/rwsem.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/tracepoint.h>
#include <linux/rculist.h>
#include <linux/module.h>

extern struct mutex *orig_tracepoint_module_list_mutex;
extern struct list_head *orig_tracepoint_module_list;

#include "pub/trace_point.h"

static struct tracepoint *tp_ret;
static void probe_tracepoint(struct tracepoint *tp, void *priv)
{
	char *n = priv;

	if (strcmp(tp->name, n) == 0)
		tp_ret = tp;
}

static void orig_for_each_tracepoint_range(
                tracepoint_ptr_t *begin, tracepoint_ptr_t *end,
                void (*fct)(struct tracepoint *tp, void *priv),
                void *priv)
{
        tracepoint_ptr_t *iter;

        if (!begin)
                return;
        for (iter = begin; iter < end; iter++)
                fct(tracepoint_ptr_deref(iter), priv);
}

static void for_each_moudule_trace_point(
		void (*fct)(struct tracepoint *tp, void *priv),
		void *priv)
{
	struct tp_module *tp_mod;
	struct module *mod;

	mutex_lock(orig_tracepoint_module_list_mutex);

	list_for_each_entry(tp_mod, orig_tracepoint_module_list, list) {
            mod = tp_mod->mod;
            if (mod->num_tracepoints) {
                orig_for_each_tracepoint_range((mod->tracepoints_ptrs),
			mod->tracepoints_ptrs + mod->num_tracepoints, fct, priv);
          }
	}

	mutex_unlock(orig_tracepoint_module_list_mutex);
}

static struct tracepoint *find_tracepoint(const char *name)
{
	tp_ret = NULL;
	for_each_kernel_tracepoint(probe_tracepoint, (void *)name);
	for_each_moudule_trace_point(probe_tracepoint, (void *)name);

	return tp_ret;
}

int hook_tracepoint(const char *name, void *probe, void *data)
{
	struct tracepoint *tp;

	tp = find_tracepoint(name);
	if (!tp)
		return 0;

	return tracepoint_probe_register(tp, probe, data);
}

int unhook_tracepoint(const char *name, void *probe, void *data)
{
	struct tracepoint *tp;
	int ret = 0;

	tp = find_tracepoint(name);
	if (!tp)
		return 0;

	do {
		ret = tracepoint_probe_unregister(tp, probe, data);
	} while (ret == -ENOMEM);

	return ret;
}
