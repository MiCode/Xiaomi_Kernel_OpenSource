#include "../../kernel/kern_oem.h"

extern void millet_sig(int sig, struct task_struct *killer,
		struct task_struct *dst);

void mi_sig_hook(int sig, struct task_struct *killer, struct task_struct *dst)
{
	/*You can add your sig filter hook in here*/
	millet_sig(sig, killer, dst);
}

struct kernel_oem_hook_s oem_kernel_hook = {
	.sig = mi_sig_hook,
};
