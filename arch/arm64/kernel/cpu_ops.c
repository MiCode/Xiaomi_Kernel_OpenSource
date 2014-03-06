/*
 * CPU kernel entry/exit control
 *
 * Copyright (C) 2013 ARM Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <asm/cpu_ops.h>
#include <asm/smp_plat.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/string.h>

const struct cpu_operations *cpu_ops[NR_CPUS];
extern struct cpu_operations *__cpu_method_of_table[];
extern struct cpu_operations *__cpu_method_of_table_end[];

const struct cpu_operations * __init cpu_get_ops(const char *name)
{
	const struct cpu_operations **start = (void *)__cpu_method_of_table;
	const struct cpu_operations **end = (void *)__cpu_method_of_table_end;

	while (start < end) {
		if (!strcmp((*start)->name, name))
			return *start;
		start++;
	};

	return NULL;
}

/*
 * Read a cpu's enable method from the device tree and record it in cpu_ops.
 */
int __init cpu_read_ops(struct device_node *dn, int cpu)
{
	const char *enable_method = of_get_property(dn, "enable-method", NULL);
	if (!enable_method) {
		/*
		 * The boot CPU may not have an enable method (e.g. when
		 * spin-table is used for secondaries). Don't warn spuriously.
		 */
		if (cpu != 0)
			pr_err("%s: missing enable-method property\n",
				dn->full_name);
		return -ENOENT;
	}

	cpu_ops[cpu] = cpu_get_ops(enable_method);
	if (!cpu_ops[cpu]) {
		pr_warn("%s: unsupported enable-method property: %s\n",
			dn->full_name, enable_method);
		return -EOPNOTSUPP;
	}

	return 0;
}

void __init cpu_read_bootcpu_ops(void)
{
	struct device_node *dn = of_get_cpu_node(0, NULL);
	if (!dn) {
		pr_err("Failed to find device node for boot cpu\n");
		return;
	}
	cpu_read_ops(dn, 0);
}
