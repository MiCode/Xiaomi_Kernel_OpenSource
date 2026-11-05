/* include order: 
 * private headers files shall follow standard headers. 
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>

#include "sandbox.h"

static struct sandbox_whitelist whitelist[SANDBOX_MAX_RT_THREADS] = {0};

int sandbox_whitelist_audit(struct task_struct *p)
{
	int i = 0;
	for (i = 0; i < SANDBOX_MAX_RT_THREADS; i++) {
		if (strncmp(p->comm, whitelist[i].name, TASK_COMM_LEN) == 0) {
			/* whitelist hit.*/
			whitelist[i].num += 1;
			if (whitelist[i].num > whitelist[i].maxAllowedNum)
				return -1;
		}
	}

	/* return code definition: 0 = audit pass; -1 = audit failure;*/
	return 0;
}
