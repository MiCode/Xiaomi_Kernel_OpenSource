#include <linux/kconfig.h>
#include <linux/memblock.h>
#include <linux/mrdump.h>
#include <linux/reboot.h>
#include <asm/memory.h>
#include <asm/page.h>
#include <asm/io.h>

#define MRDUMP_CB_ADDR (PHYS_OFFSET + 0x1F00000)
#define MRDUMP_CB_SIZE 0x2000

#define LK_LOAD_ADDR (PHYS_OFFSET + 0x1E00000)
#define LK_LOAD_SIZE 0x100000

static void mrdump_hw_enable(bool enabled)
{
}

static void mrdump_reboot(void)
{
	emergency_restart();
}

const struct mrdump_platform mrdump_v1_platform = {
	.hw_enable = mrdump_hw_enable,
	.reboot = mrdump_reboot
};

void mrdump_reserve_memory(void)
{
	struct mrdump_control_block *cblock = NULL;

#if defined(CONFIG_ARCH_MT6752)
#define PRELOADER_ADDR (PHYS_OFFSET + 0x2000000)
#define PRELOADER_SIZE 0x200000

	memblock_reserve(PRELOADER_ADDR, PRELOADER_SIZE);
#endif

	/* We must reserved the lk block, can we pass it from lk? */    
	memblock_reserve(LK_LOAD_ADDR, LK_LOAD_SIZE);

	memblock_reserve(MRDUMP_CB_ADDR, MRDUMP_CB_SIZE);
	cblock = (struct mrdump_control_block *)__va(MRDUMP_CB_ADDR);

	mrdump_platform_init(cblock, &mrdump_v1_platform);
}
