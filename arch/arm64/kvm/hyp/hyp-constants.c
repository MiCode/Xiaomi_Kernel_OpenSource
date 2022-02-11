// SPDX-License-Identifier: GPL-2.0-only

#include <linux/kbuild.h>
#include <nvhe/memory.h>
#include <nvhe/pkvm.h>

int main(void)
{
	DEFINE(STRUCT_HYP_PAGE_SIZE,	sizeof(struct hyp_page));
	DEFINE(KVM_SHADOW_VM_SIZE,	sizeof(struct kvm_shadow_vm));
	DEFINE(SHADOW_VCPU_STATE_SIZE,	sizeof(struct shadow_vcpu_state));
	return 0;
}
