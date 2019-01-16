#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>		/* needed by file_operations* */
#include <linux/miscdevice.h>	/* needed by miscdevice* */
#include <linux/device.h>	/* needed by device_* */
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/fs.h>		/* needed by file_operations* */
#include <linux/slab.h>
#include <asm/io.h>		/* needed by ioremap * */
#include <mach/hardware.h>	/* needed by __io_address */
#include <mach/mt_reg_base.h>
#include <mach/sync_write.h>
#include <mach/mt_gpio.h>
#include <mach/md32_ipi.h>
#include <mach/md32_helper.h>


static MD32_OCD_CMD_CFG md32_ocd_cfg;
#define DM_TMP_ADDR 0x00

enum md32_reg_idx {
	r0=0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15,
	sr, ipc, isr,
	lf, ls0, le0, lc0, ls1, le1, lc1, ls2, le2, lc2,
	ar0g, ar0h, ar0l, ar1g, ar1h, ar1l,
	srm, b0, b1, m0, m1, l0, l1, o0, o1,
	v0l, v1l, v2l, v3l, v0h, v1h, v2h, v3h,
	rc, pc
};

static const char *md32_reg_idx_str[] = {
	"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
    "sr", "ipc", "isr",
    "lf", "ls0", "le0", "lc0", "ls1", "le1", "lc1", "ls2", "le2", "lc2",
	"ar0g", "ar0h", "ar0l", "ar1g", "ar1h", "ar1l",
	"srm", "b0", "b1", "m0", "m1", "l0", "l1", "o0", "o1",
	"v0l", "v1l", "v2l", "v3l", "v0h", "v1h", "v2h", "v3h",
	"rc", "pc"
};



void md32_ocd_iw(u32 command)
{
    while(!MD32_OCD_READY_REG)
        ;

    MD32_OCD_INSTR_REG = MD32_OCD_CMD(command);
    dsb();
    //pr_debug("\n\n\nMD32_OCD_INSTR_REG = 0x%08x\n\n\n", MD32_OCD_INSTR_REG);
    MD32_OCD_INSTR_WR_REG = 1;
    dsb();
    MD32_OCD_INSTR_WR_REG = 0;
    dsb();

    while(!MD32_OCD_READY_REG)
        ;

    return;
}

u32 md32_ocd_dr(u32 reg)
{
    while(!MD32_OCD_READY_REG)
        ;

    MD32_OCD_INSTR_REG = MD32_OCD_CMD(reg);
    dsb();
    MD32_OCD_INSTR_WR_REG = 1;
    dsb();
    MD32_OCD_INSTR_WR_REG = 0;
    dsb();

    while(!MD32_OCD_READY_REG)
        ;

    return MD32_OCD_DATA_PO_REG;
}

void md32_ocd_dw(u32 reg, u32 wdata)
{
    while(!MD32_OCD_READY_REG)
        ;

    MD32_OCD_INSTR_REG = MD32_OCD_CMD(reg);
    dsb();
    MD32_OCD_INSTR_WR_REG = 1;
    dsb();
    MD32_OCD_INSTR_WR_REG = 0;
    dsb();
    MD32_OCD_DATA_PI_REG = wdata;
    dsb();
    MD32_OCD_DATA_WR_REG = 1;
    dsb();
    MD32_OCD_DATA_WR_REG = 0;
    dsb();


    while(!MD32_OCD_READY_REG)
        ;
    return ;
}

u32 md32_read_dmw(u32 addr) {
	u32 data;
	md32_ocd_dw(DBG_ADDR_REG_INSTR,addr);
	md32_ocd_iw(DBG_DMb_LOAD_INSTR); *((char *)(&data)+0) = (char)md32_ocd_dr(DBG_DATA_REG_INSTR);
	md32_ocd_iw(DBG_DMb_LOAD_INSTR); *((char *)(&data)+1) = (char)md32_ocd_dr(DBG_DATA_REG_INSTR);
	md32_ocd_iw(DBG_DMb_LOAD_INSTR); *((char *)(&data)+2) = (char)md32_ocd_dr(DBG_DATA_REG_INSTR);
	md32_ocd_iw(DBG_DMb_LOAD_INSTR); *((char *)(&data)+3) = (char)md32_ocd_dr(DBG_DATA_REG_INSTR);
	return data;
}

void md32_ocd_execute(UINT32 insn) {
    while (0 == (md32_ocd_dr(DBG_STATUS_REG_INSTR) & 0x1));
	md32_ocd_dw(DBG_INSTR_REG_INSTR,insn);
	md32_ocd_iw(DBG_EXECUTE_INSTR);
    while (0 == (md32_ocd_dr(DBG_STATUS_REG_INSTR) & 0x1));
}

INT32 md32_ocd_read_pmw(UINT32 addr) {
	md32_ocd_dw(DBG_ADDR_REG_INSTR,addr);
	md32_ocd_iw(DBG_PMb_LOAD_INSTR);
	return md32_ocd_dr(DBG_INSTR_REG_INSTR);
}

void md32_ocd_write_pmw(UINT32 addr,UINT32 wdata) {
	md32_ocd_dw(DBG_ADDR_REG_INSTR,addr);
	md32_ocd_dw(DBG_INSTR_REG_INSTR,wdata);
	md32_ocd_iw(DBG_PMb_STORE_INSTR);
}

UINT8 md32_ocd_read_dmb(UINT32 addr) {
	md32_ocd_dw(DBG_ADDR_REG_INSTR,addr);
	md32_ocd_iw(DBG_DMb_LOAD_INSTR);
	return (md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff);
}

void md32_ocd_write_dmb(UINT32 addr,UINT8 wdata) {
	md32_ocd_dw(DBG_ADDR_REG_INSTR,addr);
	md32_ocd_dw(DBG_DATA_REG_INSTR,wdata);
	md32_ocd_iw(DBG_DMb_STORE_INSTR);
}

UINT32 md32_ocd_add_sw_break(UINT32 addr) {
	UINT32 insn = md32_ocd_read_pmw(addr & (~0x3));
	if (addr & 0x2) { // unaligned
		md32_ocd_write_pmw(addr & (~0x3), (insn & 0xffff0000) | 0xa003);
	}
	else {
		md32_ocd_write_pmw(addr & (~0x3), (insn & 0xffff) | 0xa0030000);
	}
	return insn;
}

static UINT32 md32_ocd_get_dm_word(UINT32 addr) {
    UINT32 byte0,byte1,byte2,byte3;
	md32_ocd_dw(DBG_ADDR_REG_INSTR,addr);
	md32_ocd_iw(DBG_DMb_LOAD_INSTR); byte0 = (md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff);
	md32_ocd_iw(DBG_DMb_LOAD_INSTR); byte1 = (md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff);
	md32_ocd_iw(DBG_DMb_LOAD_INSTR); byte2 = (md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff);
	md32_ocd_iw(DBG_DMb_LOAD_INSTR); byte3 = (md32_ocd_dr(DBG_DATA_REG_INSTR) & 0xff);

    //pr_debug("%s, byte0=0x%02x, byte1=0x%02x, byte2=0x%02x, byte3=0x%02x\n", __func__, byte0, byte1, byte2, byte3);

	return (byte0 | (byte1 << 8) | (byte2 << 16) | (byte3 << 24));
}

static void md32_ocd_put_dm_word(UINT32 addr, UINT32 data) {
    UINT32 byte0,byte1,byte2,byte3;
	md32_ocd_dw(DBG_ADDR_REG_INSTR,addr);
	byte0 = (data >>  0) & 0xff;
	byte1 = (data >>  8) & 0xff;
	byte2 = (data >> 16) & 0xff;
	byte3 = (data >> 24) & 0xff;
	md32_ocd_dw(DBG_DATA_REG_INSTR,byte0); md32_ocd_iw(DBG_DMb_STORE_INSTR);
	md32_ocd_dw(DBG_DATA_REG_INSTR,byte1); md32_ocd_iw(DBG_DMb_STORE_INSTR);
	md32_ocd_dw(DBG_DATA_REG_INSTR,byte2); md32_ocd_iw(DBG_DMb_STORE_INSTR);
	md32_ocd_dw(DBG_DATA_REG_INSTR,byte3); md32_ocd_iw(DBG_DMb_STORE_INSTR);
}

inline static void md32_ocd_set_r0(UINT32 data) {
#if 0
	md32_ocd_execute(0x0f000000 | ((data >> 16)<<8));
	md32_ocd_execute(0x0d000000 | ((data & 0xffff)<<8));
#else
    if ((data>>20)==0) {
		md32_ocd_execute(0x00000000 | (data<<4));
	}
	else {
		UINT32 hi = (data >> 16);
		UINT32 lo = (data & 0xffff);
		md32_ocd_execute(0x0f000000 | (hi<<8));
		if (lo!=0) md32_ocd_execute(0x0d000000 | (lo<<8));
	}
#endif
}

inline static void md32_ocd_set_r1(UINT32 data) {
#if 0
	md32_ocd_execute(0x0f000001 | ((data >> 16)<<8));
	md32_ocd_execute(0x0d000011 | ((data & 0xffff)<<8));
#else
    if ((data>>20)==0) {
		md32_ocd_execute(0x00000001 | (data<<4));
	}
	else {
		UINT32 hi = (data >> 16);
		UINT32 lo = (data & 0xffff);
		md32_ocd_execute(0x0f000001 | (hi<<8));
		if (lo!=0) md32_ocd_execute(0x0d000011 | (lo<<8));
	}
#endif
}

inline static void md32_ocd_clear_r0(void) {
	md32_ocd_execute(0x00000000);
}

inline UINT32 md32_ocd_read_pc(void) {
	return md32_ocd_dr(DBG_ADDR_REG_INSTR);
}

void md32_ocd_write_pc(UINT32 addr) {
	// setup PC to R0
	md32_ocd_set_r0(addr);
	// jump to r0
	md32_ocd_execute(0x40002030);
	// restore R0
	md32_ocd_clear_r0();
}

UINT32 md32_ocd_read_reg(enum md32_reg_idx r) {
	UINT32 dm_tmp_bak = 0;
	UINT32 r1_tmp_bak = 0;
	UINT32 ret = 0;
	int ret_from_dm = 1;

#if 0
    if(r == r14)
    {
        u32 data;
        u32 pm_data;


        //DW(INST,0x067000e0 /*"sw r14,#0(r0)"*/); IW(EXECUTE); DW(ADDR,0x00000000); IW(DMb_LOAD); DR(DATA); // load r14
        md32_ocd_dw(DBG_INSTR_REG_INSTR, 0x067000e0);
        md32_ocd_iw(DBG_EXECUTE_INSTR);
        md32_ocd_dw(DBG_ADDR_REG_INSTR, 0x00000000);
        md32_ocd_iw(DBG_DMb_LOAD_INSTR);
        data = md32_ocd_dr(DBG_DATA_REG_INSTR);

        md32_ocd_dw(DBG_ADDR_REG_INSTR, 0x0);
        md32_ocd_iw(DBG_PMb_LOAD_INSTR);
        pm_data = md32_ocd_dr(DBG_INSTR_REG_INSTR);

        pr_debug("%s r14=0x%x, pm_data=0x%0x\n", __func__, data, pm_data);
        return data;

    }
    else if(r == r15)
    {
        u32 data, pm_data;
        //DW(INST,0x067000f0 /*"sw r15,#0(r0)"*/); IW(EXECUTE); DW(ADDR,0x00000000); IW(DMb_LOAD); DR(DATA); // load r15
        md32_ocd_dw(DBG_INSTR_REG_INSTR, 0x067000f0);
        md32_ocd_iw(DBG_EXECUTE_INSTR);
        md32_ocd_dw(DBG_ADDR_REG_INSTR, 0x00000000);
        md32_ocd_iw(DBG_DMb_LOAD_INSTR);

        data = md32_ocd_dr(DBG_DATA_REG_INSTR);


        md32_ocd_dw(DBG_ADDR_REG_INSTR, 0x0);
        md32_ocd_iw(DBG_PMb_LOAD_INSTR);
        pm_data = md32_ocd_dr(DBG_INSTR_REG_INSTR);

        pr_debug("%s r15=0x%x, pm_data=0x%0x\n", __func__, data, pm_data);
        return data;
    }
#endif

	// setup DM_TMP_ADDR
	md32_ocd_set_r0(DM_TMP_ADDR);
	// backup DM_TMP
	dm_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	// backup R1
	md32_ocd_execute(0x06700010);
	r1_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);

	switch(r) {
    case r0: ret = 0; ret_from_dm = 0; break;
    case r1: ret = r1_tmp_bak; ret_from_dm = 0; break;
    case r2:   md32_ocd_execute(0x06700020); break;
    case r3:   md32_ocd_execute(0x06700030); break;
    case r4:   md32_ocd_execute(0x06700040); break;
    case r5:   md32_ocd_execute(0x06700050); break;
    case r6:   md32_ocd_execute(0x06700060); break;
    case r7:   md32_ocd_execute(0x06700070); break;
    case r8:   md32_ocd_execute(0x06700080); break;
    case r9:   md32_ocd_execute(0x06700090); break;
    case r10:  md32_ocd_execute(0x067000a0); break;
    case r11:  md32_ocd_execute(0x067000b0); break;
    case r12:  md32_ocd_execute(0x067000c0); break;
    case r13:  md32_ocd_execute(0x067000d0); break;
    case r14:  md32_ocd_execute(0x067000e0); break;
    case r15:  md32_ocd_execute(0x067000f0); break;
#if 0
    case ar0g: md32_ocd_execute(0x40006680); break;
    case ar0h: md32_ocd_execute(0x40006640); break;
    case ar0l: md32_ocd_execute(0x40006600); break;
    case ar1g: md32_ocd_execute(0x40006740); break;
    case ar1h: md32_ocd_execute(0x40006700); break;
    case ar1l: md32_ocd_execute(0x400066c0); break;
    case sr:   md32_ocd_execute(0x04154001); md32_ocd_execute(0x06700010); break;
    case ipc:  md32_ocd_execute(0x04154011); md32_ocd_execute(0x06700010); break;
    case isr:  md32_ocd_execute(0x04154021); md32_ocd_execute(0x06700010); break;
    case lf:   md32_ocd_execute(0x04154041); md32_ocd_execute(0x06700010); break;
    case ls0:  md32_ocd_execute(0x04154051); md32_ocd_execute(0x06700010); break;
    case le0:  md32_ocd_execute(0x04154061); md32_ocd_execute(0x06700010); break;
    case lc0:  md32_ocd_execute(0x04154071); md32_ocd_execute(0x06700010); break;
    case ls1:  md32_ocd_execute(0x04154081); md32_ocd_execute(0x06700010); break;
    case le1:  md32_ocd_execute(0x04154091); md32_ocd_execute(0x06700010); break;
    case lc1:  md32_ocd_execute(0x041540a1); md32_ocd_execute(0x06700010); break;
    case ls2:  md32_ocd_execute(0x041540b1); md32_ocd_execute(0x06700010); break;
    case le2:  md32_ocd_execute(0x041540c1); md32_ocd_execute(0x06700010); break;
    case lc2:  md32_ocd_execute(0x041540d1); md32_ocd_execute(0x06700010); break;
    case srm:  md32_ocd_execute(0x40006001); md32_ocd_execute(0x06700010); break;
    case b0:   md32_ocd_execute(0x40006101); md32_ocd_execute(0x06700010); break;
    case b1:   md32_ocd_execute(0x40006111); md32_ocd_execute(0x06700010); break;
    case m0:   md32_ocd_execute(0x40006121); md32_ocd_execute(0x06700010); break;
    case m1:   md32_ocd_execute(0x40006131); md32_ocd_execute(0x06700010); break;
    case l0:   md32_ocd_execute(0x40006141); md32_ocd_execute(0x06700010); break;
    case l1:   md32_ocd_execute(0x40006151); md32_ocd_execute(0x06700010); break;
    case o0:   md32_ocd_execute(0x40006161); md32_ocd_execute(0x06700010); break;
    case o1:   md32_ocd_execute(0x40006171); md32_ocd_execute(0x06700010); break;
    case v0l:  md32_ocd_execute(0x20001010); md32_ocd_execute(0x06700010); break;
    case v1l:  md32_ocd_execute(0x20001110); md32_ocd_execute(0x06700010); break;
    case v2l:  md32_ocd_execute(0x20001210); md32_ocd_execute(0x06700010); break;
    case v3l:  md32_ocd_execute(0x20001310); md32_ocd_execute(0x06700010); break;
    case v0h:  md32_ocd_execute(0x20001401); md32_ocd_execute(0x06700010); break;
    case v1h:  md32_ocd_execute(0x20001501); md32_ocd_execute(0x06700010); break;
    case v2h:  md32_ocd_execute(0x20001601); md32_ocd_execute(0x06700010); break;
    case v3h:  md32_ocd_execute(0x20001701); md32_ocd_execute(0x06700010); break;
    case rc: ret = 0; ret_from_dm = 0; break;
    case pc: ret = md32_ocd_dr(DBG_ADDR_REG_INSTR); ret_from_dm = 0; break;
#endif
    default: ret = 0xdeadbeef; ret_from_dm = 0;
	}
	if (ret_from_dm) {
		ret = md32_ocd_get_dm_word(DM_TMP_ADDR);
	}
	// restore R0
	md32_ocd_clear_r0();
	// restore R1
	md32_ocd_set_r1(r1_tmp_bak);
	// restore DM_TMP
	md32_ocd_put_dm_word(DM_TMP_ADDR,dm_tmp_bak);
	return ret;
}

static inline void md32_ocd_set_vr(const enum md32_reg_idx r,UINT32 data) {
	UINT32 dm_tmp_bak = 0;
	UINT32 r1_tmp_bak = 0;
	md32_ocd_set_r0(DM_TMP_ADDR);
	dm_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	md32_ocd_execute(0x06700010);
	r1_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	switch(r) {
    case v0l: md32_ocd_execute(0x20001810); md32_ocd_set_r0(data); md32_ocd_execute(0x20001c10); break;
    case v1l: md32_ocd_execute(0x20001910); md32_ocd_set_r0(data); md32_ocd_execute(0x20001d10); break;
    case v2l: md32_ocd_execute(0x20001a10); md32_ocd_set_r0(data); md32_ocd_execute(0x20001e10); break;
    case v3l: md32_ocd_execute(0x20001b10); md32_ocd_set_r0(data); md32_ocd_execute(0x20001f10); break;
    case v0h: md32_ocd_execute(0x20001810); md32_ocd_set_r1(data); md32_ocd_execute(0x20001c10); break;
    case v1h: md32_ocd_execute(0x20001910); md32_ocd_set_r1(data); md32_ocd_execute(0x20001d10); break;
    case v2h: md32_ocd_execute(0x20001a10); md32_ocd_set_r1(data); md32_ocd_execute(0x20001e10); break;
    case v3h: md32_ocd_execute(0x20001b10); md32_ocd_set_r1(data); md32_ocd_execute(0x20001f10); break;
    default: break;
	}
	md32_ocd_clear_r0();
	md32_ocd_set_r1(r1_tmp_bak);
	md32_ocd_put_dm_word(DM_TMP_ADDR,dm_tmp_bak);
}

void md32_ocd_write_reg(enum md32_reg_idx r, UINT32 data) {
	// write R0
	md32_ocd_set_r0(data);
	switch(r) {
    case r0: break;
    case r1:  md32_ocd_execute(0x08000001); break;
    case r2:  md32_ocd_execute(0x08000002); break;
    case r3:  md32_ocd_execute(0x08000003); break;
    case r4:  md32_ocd_execute(0x08000004); break;
    case r5:  md32_ocd_execute(0x08000005); break;
    case r6:  md32_ocd_execute(0x08000006); break;
    case r7:  md32_ocd_execute(0x08000007); break;
    case r8:  md32_ocd_execute(0x08000008); break;
    case r9:  md32_ocd_execute(0x08000009); break;
    case r10: md32_ocd_execute(0x0800000a); break;
    case r11: md32_ocd_execute(0x0800000b); break;
    case r12: md32_ocd_execute(0x0800000c); break;
    case r13: md32_ocd_execute(0x0800000d); break;
    case r14: md32_ocd_execute(0x0800000e); break;
    case r15: md32_ocd_execute(0x0800000f); break;
    case lf : md32_ocd_execute(0x04182004); break;
    case ls0: md32_ocd_execute(0x04182005); break;
    case le0: md32_ocd_execute(0x04182006); break;
    case lc0: md32_ocd_execute(0x04182007); break;
    case ls1: md32_ocd_execute(0x04182008); break;
    case le1: md32_ocd_execute(0x04182009); break;
    case lc1: md32_ocd_execute(0x0418200a); break;
    case ls2: md32_ocd_execute(0x0418200b); break;
    case le2: md32_ocd_execute(0x0418200c); break;
    case lc2: md32_ocd_execute(0x0418200d); break;
    case ipc: md32_ocd_execute(0x04182001); break;
    case isr: md32_ocd_execute(0x04182002); break;
    case sr:  md32_ocd_execute(0x04182000); break;
    case srm: md32_ocd_execute(0x40006010); break;
    case m0:  md32_ocd_execute(0x400060a0); break;
    case m1:  md32_ocd_execute(0x400060b0); break;
    case b0:  md32_ocd_execute(0x40006080); break;
    case b1:  md32_ocd_execute(0x40006090); break;
    case o0:  md32_ocd_execute(0x400060e0); break;
    case o1:  md32_ocd_execute(0x400060f0); break;
    case l0:  md32_ocd_execute(0x400060c0); break;
    case l1:  md32_ocd_execute(0x400060d0); break;
    case v0l:
    case v1l:
    case v2l:
    case v3l:
    case v0h:
    case v1h:
    case v2h:
    case v3h: md32_ocd_set_vr(r,data); break;
    case rc: /* do nothing */ break;
    case pc: md32_ocd_execute(0x40002030); break;
    default: break;
	}
	// restore R0
	md32_ocd_clear_r0();
}

UINT32 md32_ocd_read_mmr(UINT32 addr) {
	UINT32 dm_tmp_bak = 0;
	UINT32 r1_tmp_bak = 0;
	UINT32 ret = 0;
	md32_ocd_set_r0(DM_TMP_ADDR);
	dm_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	md32_ocd_execute(0x06700010);
	r1_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);

	md32_ocd_set_r0(addr & (~0x3));
	md32_ocd_execute(0x06400010); // lw r1,#0(r0)
	md32_ocd_set_r0(DM_TMP_ADDR);
	md32_ocd_execute(0x06700010);
	ret = md32_ocd_get_dm_word(DM_TMP_ADDR);

	md32_ocd_clear_r0();
	md32_ocd_set_r1(r1_tmp_bak);
	md32_ocd_put_dm_word(DM_TMP_ADDR,dm_tmp_bak);
	return ret;
}

void md32_ocd_write_mmr(UINT32 addr, UINT32 data) {
	UINT32 dm_tmp_bak = 0;
	UINT32 r1_tmp_bak = 0;
	md32_ocd_set_r0(DM_TMP_ADDR);
	dm_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);
	md32_ocd_execute(0x06700010);
	r1_tmp_bak = md32_ocd_get_dm_word(DM_TMP_ADDR);

	md32_ocd_set_r0(addr & (~0x3));
	md32_ocd_set_r1(data);
	md32_ocd_execute(0x06700010);

	md32_ocd_clear_r0();
	md32_ocd_set_r1(r1_tmp_bak);
	md32_ocd_put_dm_word(DM_TMP_ADDR,dm_tmp_bak);
}


int md32_ocd_set_break(u32 enable, u32 num, u32 addr)
{
    int err = 0;
    u32 command = 0;
    if(enable)
    {
        /* enable break pointer */
        switch(num){
        case 0:
            command = DBG_BP0_ENABLE_INSTR;
            break;
        case 1:
            command = DBG_BP1_ENABLE_INSTR;
            break;
        case 2:
            command = DBG_BP2_ENABLE_INSTR;
            break;
        case 3:
            command = DBG_BP3_ENABLE_INSTR;
            break;

        default:
            err = 1;
            goto error;
        }
        md32_ocd_dw(DBG_ADDR_REG_INSTR, addr);
        md32_ocd_iw(command);
    }
    else
    {
        /* disable break pointer */
        switch(num){
        case 0:
            command = DBG_BP0_DISABLE_INSTR;
            break;
        case 1:
            command = DBG_BP1_DISABLE_INSTR;
            break;
        case 2:
            command = DBG_BP2_DISABLE_INSTR;
            break;
        case 3:
            command = DBG_BP3_DISABLE_INSTR;
            break;

        default:
            err = 1;
            goto error;

        }
        md32_ocd_iw(command);

    }

error:
    return err;
}

ssize_t md32_ocd_dump_all_cpu_reg(char *buf)
{
    char *ptr = buf;
    int i;

    if(!buf)
        return 0;

    for(i = r14; i <= r15; i++)
    {
        ptr += sprintf(ptr, "%s=0x%08x\n", md32_reg_idx_str[i], md32_ocd_read_reg(i));
    }
    ptr += sprintf(ptr, "dump md32 debug reg\n");
    ptr += sprintf(ptr, "pc=0x%08x, r14=0x%08x, r15=0x%08x\n", MD32_DEBUG_PC_REG, MD32_DEBUG_R14_REG, MD32_DEBUG_R15_REG);

    return ptr - buf;
}


int md32_ocd_execute_cmd(MD32_OCD_CMD_CFG *in_cfg)
{
    MD32_OCD_CMD_CFG *cfg = &md32_ocd_cfg;
	unsigned long irq_flag = 0;

    pr_debug("%s: cmd:%d, addr=0x%x, data=0x%x\n", __func__, in_cfg->cmd, in_cfg->addr, in_cfg->data);

    spin_lock_irqsave(&cfg->spinlock, irq_flag);
    MD32_OCD_BYPASS_JTAG_REG = 0x1;
    dsb();

    cfg->cmd  = in_cfg->cmd;
    cfg->addr = in_cfg->addr;
    cfg->data = in_cfg->data;
    cfg->break_en = in_cfg->break_en;
    cfg->success = 0;

    switch(cfg->cmd) {
    case CMD_MD32_OCD_STOP:
        md32_ocd_iw(DBG_REQUEST_INSTR);
        break;

    case CMD_MD32_OCD_RESUME:
        md32_ocd_iw(DBG_RESUME_INSTR);
        break;

    case CMD_MD32_OCD_STEP:
        md32_ocd_iw(DBG_STEP_INSTR);
        break;

    case CMD_MD32_OCD_READ_MEM:
#if 0
        md32_ocd_dw(DBG_ADDR_REG_INSTR, cfg->addr);
        md32_ocd_iw(DBG_DMb_LOAD_INSTR);
        cfg->data = md32_ocd_dr(DBG_DATA_REG_INSTR);
#else
        cfg->data = md32_ocd_get_dm_word(cfg->addr);
#endif
        break;

    case CMD_MD32_OCD_WRITE_MEM:
#if 0
        md32_ocd_dw(DBG_ADDR_REG_INSTR, cfg->addr);
        md32_ocd_dw(DBG_DATA_REG_INSTR, cfg->data);
        md32_ocd_iw(DBG_DMb_STORE_INSTR);
#else
        md32_ocd_put_dm_word(cfg->addr, cfg->data);
#endif
        break;

    case CMD_MD32_OCD_BREAKPOINT:
        md32_ocd_set_break(cfg->break_en, cfg->data, cfg->addr);
        break;

    case CMD_MD32_OCD_STATUS:
        cfg->data = md32_ocd_dr(DBG_STATUS_REG_INSTR);
        break;
#if 0
    case CMD_MD32_OCD_WRITE_REG:
        md32_ocd_dw(DBG_ADDR_REG_INSTR,tmp_addr);
        md32_ocd_dw(DBG_DATA_REG_INSTR,data);
        md32_ocd_iw(DBG_DMb_STORE_INSTR);
        md32_ocd_dw(DBG_INSTR_REG_INSTR,"lw r1,#tmp_addr(r0)"); /*??? need to translate "lw r1,#tmp_addr(r0)" as m32 instrunction */
        md32_ocd_iw(DBG_EXECUTE_INSTR);
        break;
#endif

    case CMD_MD32_OCD_DW:
        md32_ocd_dw(cfg->addr, cfg->data);
        break;
    case CMD_MD32_OCD_IW:
        md32_ocd_iw(cfg->addr);
        break;
    case CMD_MD32_OCD_DR:
        cfg->data = md32_ocd_dr(cfg->addr);
        break;


    case CMD_MD32_OCD_TEST:
        pr_debug("\n\n\ndr(DBG_STATUS_REG_INSTR)=0x%x\n", md32_ocd_dr(DBG_STATUS_REG_INSTR));
        md32_ocd_dw(DBG_DATA_REG_INSTR, 0xAB);
        cfg->data = md32_ocd_dr(DBG_DATA_REG_INSTR);
        pr_debug("dr(DBG_DATA_REG_INSTR)=0x%x\n", cfg->data);
        break;

    default:
        pr_err("unkonw md32 ocd cmd\n");
        break;
    }

    cfg->success = 1;
    MD32_OCD_BYPASS_JTAG_REG = 0x0;
    dsb();
    spin_unlock_irqrestore(&cfg->spinlock, irq_flag);
    pr_debug("%s, done\n", __func__);

    return 0;

}

const char *md32_ocd_help_msg(void)
{
    static const char help_str[]={
        "md32 ocd command usage:\n"
        "echo [cmd] [address] [data] > md32_ocd\n\n"
        "[cmd]\n"
        "\tstop:   stop md32\n"
        "\tresume: resume md32\n"
        "\tstep:   step md32\n"
        "\twrite:  use md32 to write address with data\n"
        "\t        write [address] [data]\n"
        "\tread:   use md32 to read address\n"
        "\t        read [address]\n"
        "\tbreak:  setup break pointer\n"
        "\t        break [enable/disable] [0-3] [address]\n"
        "\tstatus: show status\n"
        "\tdw:     dw [reg] [data]\n"
        "\tiw:     iw [reg]\n"
        "\tdr:     dr [reg]\n"
        "\t        use cat to get return data\n"
        "\thelp:   show usage\n"
        "\n"
        "ex: <write>\n"
        "echo write 0xD0000084 0x00001234 > md32_ocd\n\n"
        "ex: <dw, iw, dr> to read address 0x00000000\n"
        "dw 0x001 0x00000000\n"
        "iw 0x042\n"
        "dr 0x000\n"
    };

    return help_str;
}




int md32_ocd_input_parse(const char *buf, size_t n, MD32_OCD_CMD_CFG *cfg)
{
    u32 addr=0, data=0, break_en=0;
    enum cmd_md32_ocd cmd = 0;
    char cmd_str[64], cmd_str2[64];
    int err = 0;
    u32 res = sscanf(buf, "%s 0x%x 0x%x", cmd_str, &addr, &data);

    if(res < 1)
    {
        err = 1;
        goto error;
    }

    switch(cmd_str[0]){
    case 'b':
        res = sscanf(buf, "%s %s %x 0x%x", cmd_str, cmd_str2, &data, &addr);
        if(res > 4 || data > 3)
        {
            err = 1;
            goto error;
        }

        break_en = cmd_str2[0] == 'e' ? 1 : 0; // string is 'enable'
        cmd = CMD_MD32_OCD_BREAKPOINT;
        break;

    case 'i':
        if(strncmp(cmd_str, "iw", 2) == 0)
        {
            res = sscanf(buf, "%s 0x%x", cmd_str, &addr);
            if(res != 2)
            {
                err = 1;
                goto error;
            }

            cmd = CMD_MD32_OCD_IW;
        }
        break;

    case 'd':
        if(strncmp(cmd_str, "dw", 2) == 0)
        {
            res = sscanf(buf, "%s 0x%x 0x%x", cmd_str, &addr, &data);
            if(res != 3)
            {
                err = 1;
                goto error;
            }

            cmd = CMD_MD32_OCD_DW;
        }
        else if(strncmp(cmd_str, "dr", 2) == 0)
        {
            res = sscanf(buf, "%s 0x%x", cmd_str, &addr);
            if(res != 2)
            {
                err = 1;
                goto error;
            }
            cmd = CMD_MD32_OCD_DR;
        }
        break;

    case 's':
        if(strncmp(cmd_str, "stop", 4) == 0)
        {
            cmd = CMD_MD32_OCD_STOP;
        }
        else if(strncmp(cmd_str, "status", 6) == 0)
        {
            cmd = CMD_MD32_OCD_STATUS;
        }
        else
        {
            /*step*/
            cmd = CMD_MD32_OCD_STEP;
        }
        break;

    case 'r':
        if(strncmp(cmd_str, "read", 4) == 0)
        {
            res = sscanf(buf, "%s 0x%x", cmd_str, &addr);
            if(res != 2)
            {
                err = 1;
                goto error;
            }
            cmd = CMD_MD32_OCD_READ_MEM;
        }
        else
        {
            cmd = CMD_MD32_OCD_RESUME;
        }
        break;

    case 'h':
        cmd = CMD_MD32_OCD_HELP;
        break;

    case 'w':
        res = sscanf(buf, "%s 0x%x 0x%x", cmd_str, &addr, &data);
        if(res != 3)
        {
            err = 1;
            goto error;
        }
        cmd = CMD_MD32_OCD_WRITE_MEM;

        break;
    case 't':
        cmd = CMD_MD32_OCD_TEST;
        break;

    default:
        err = 1;
        break;
    }

    if(!err)
    {
        cfg->cmd  = cmd;
        cfg->addr = addr;
        cfg->data = data;
        cfg->break_en = break_en;
    }

error:
    return err;
}


static ssize_t md32_ocd_show(struct device *kobj, struct device_attribute *attr, char *buf)
{
    int size = 0;
    char *ptr = buf;
    unsigned long irq_flag;
    MD32_OCD_CMD_CFG *cfg = &md32_ocd_cfg;
    unsigned int sw_rstn;


    sw_rstn = readl((void __iomem *)MD32_BASE);

    if(sw_rstn == 0x0)
    {
        ptr += sprintf(ptr, "MD32 not enabled\n");
        goto error;
    }

    spin_lock_irqsave(&cfg->spinlock, irq_flag);

    ptr += sprintf(ptr, "[%s] ", cfg->success ? "OK" : "FAIL");

    ptr += sprintf(ptr, "command: ");

    switch(cfg->cmd){
    case CMD_MD32_OCD_STOP:
        ptr += sprintf(ptr, "stop");
        break;

    case CMD_MD32_OCD_STEP:
        ptr += sprintf(ptr, "step");
        break;

    case CMD_MD32_OCD_RESUME:
        ptr += sprintf(ptr, "resume");
        break;

    case CMD_MD32_OCD_HELP:
        ptr += sprintf(ptr, "help\n");
        ptr += sprintf(ptr, "%s", md32_ocd_help_msg());
        break;

    case CMD_MD32_OCD_WRITE_MEM:
        ptr += sprintf(ptr, "write addr=0x%08x, data=0x%08x", cfg->addr, cfg->data);
        break;

    case CMD_MD32_OCD_READ_MEM:
        ptr += sprintf(ptr, "read addr=0x%08x, data=0x%08x", cfg->addr, cfg->data);
        break;
    case CMD_MD32_OCD_TEST:
        ptr += sprintf(ptr, "test addr=0x%08x, data=0x%08x", cfg->addr, cfg->data);
        break;

    case CMD_MD32_OCD_BREAKPOINT:
        ptr += sprintf(ptr, "break");
        if(cfg->break_en)
        {
            ptr += sprintf(ptr, "enable %d addr=0x%08x", cfg->data, cfg->addr);
        }
        else
        {
            ptr += sprintf(ptr, "disable %d", cfg->data);
        }

        break;

    case CMD_MD32_OCD_STATUS:
        ptr += sprintf(ptr, "status data=0x%08x\n", cfg->data);

        if (0==(cfg->data & (0x1<<DBG_MODE_INDX))) {
            ptr += sprintf(ptr, "md32 is running\n");
        }
        else
        {
            if (0!=(cfg->data & (0x1<<(DBG_MODE_INDX+1+0)))) {
                ptr += sprintf(ptr, "hit hardware breakpoint 0\n");
            }
            if (0!=(cfg->data & (0x1<<(DBG_MODE_INDX+1+1)))) {
                ptr += sprintf(ptr, "hit hardware breakpoint 1\n");
            }
            if (0!=(cfg->data & (0x1<<(DBG_MODE_INDX+1+2)))) {
                ptr += sprintf(ptr, "hit hardware breakpoint 2\n");
            }
            if (0!=(cfg->data & (0x1<<(DBG_MODE_INDX+1+3)))) {
                ptr += sprintf(ptr, "hit hardware breakpoint 3\n");
            }
            if (0!=(cfg->data & (0x1<<DBG_BP_HIT_INDX))) {
                ptr += sprintf(ptr, "hit hardware breakpoint\n");
            }
            if (0!=(cfg->data & (0x1<<DBG_SWBREAK_INDX))) {
                ptr += sprintf(ptr, "hit software breakpoint\n");
            }

            ptr += md32_ocd_dump_all_cpu_reg(ptr);

        }
        break;
    case CMD_MD32_OCD_DW:
        ptr += sprintf(ptr, "dw reg=0x%08x, data=0x%08x", cfg->addr, cfg->data);
        break;


    case CMD_MD32_OCD_DR:
        ptr += sprintf(ptr, "dr reg=0x%08x, data=0x%08x", cfg->addr, cfg->data);
        break;

    case CMD_MD32_OCD_IW:
        ptr += sprintf(ptr, "iw reg=0x%08x", cfg->addr);
        break;


    default:
        ptr += sprintf(ptr, "unknow");
        break;
    }

    ptr += sprintf(ptr, "\n");

    spin_unlock_irqrestore(&cfg->spinlock, irq_flag);

error:
    size = ptr - buf;
    return size;
}

static ssize_t md32_ocd_store(struct device *kobj, struct device_attribute *attr, const char *buf, size_t n)
{
    MD32_OCD_CMD_CFG cfg;
    unsigned int sw_rstn;

    sw_rstn = readl((void __iomem *)MD32_BASE);

    if(sw_rstn == 0x0)
    {
        pr_err("MD32 not enabled\n");
        return -EINVAL;
    }

    if(md32_ocd_input_parse(buf, n, &cfg))
    {
        pr_debug("%s", md32_ocd_help_msg());
        return -EINVAL;
    }

    if(md32_ocd_execute_cmd(&cfg))
    {
        pr_err("ocd execute fail\n");
        return -EBUSY;
    }

    pr_debug("\nmd32_ocd_store end\n\n\n");


    return n;
}

DEVICE_ATTR(md32_ocd, 0644, md32_ocd_show, md32_ocd_store);

#if MD32_JTAG_GPIO_DVT
void md32_jtag_dvt_setup_gpio(u32 jtag_setup)
{
    switch(jtag_setup) {
    case 0:
        pr_debug("setup gpio108 - gpio112 to mode 5\n");
        mt_set_gpio_mode(GPIO108, 5);
        mt_set_gpio_mode(GPIO109, 5);
        mt_set_gpio_mode(GPIO110, 5);
        mt_set_gpio_mode(GPIO111, 5);
        mt_set_gpio_mode(GPIO112, 5);
        break;
    case 1:
        pr_debug("setup gpio005 - gpio009 to mode 5\n");
        mt_set_gpio_mode(GPIO5, 5);
        mt_set_gpio_mode(GPIO6, 5); mt_set_gpio_pull_enable(GPIO6, 1);
        mt_set_gpio_mode(GPIO7, 5);
        mt_set_gpio_mode(GPIO8, 5); mt_set_gpio_dir(GPIO8, 1);
        mt_set_gpio_mode(GPIO9, 5);
        break;
    case 2:
        pr_debug("setup gpio005 - gpio009 to mode 5\n");
        mt_set_gpio_mode(GPIO5, 5);
        mt_set_gpio_mode(GPIO6, 5); mt_set_gpio_pull_enable(GPIO6, 1);
        mt_set_gpio_mode(GPIO7, 5);
        mt_set_gpio_mode(GPIO8, 5);
        mt_set_gpio_mode(GPIO9, 5);

        mt_set_gpio_dir(GPIO5, GPIO_DIR_IN);
        mt_set_gpio_dir(GPIO6, GPIO_DIR_IN);
        mt_set_gpio_dir(GPIO7, GPIO_DIR_IN);
        mt_set_gpio_dir(GPIO8, GPIO_DIR_IN);
        mt_set_gpio_dir(GPIO8, GPIO_DIR_IN);
        break;

    }
}

static ssize_t md32_jtag_gpio_dvt_show(struct device *kobj, struct device_attribute *attr, char *buf)
{
    char *ptr = buf;
    u32 mode;
    u32 i, start, end, gpio;
    /* AP_MD32_TMS , MUX at
       GPIO5      , Aux Func.5
       GPIO108  , Aux Func.5

       AP_MD32_TCK , MUX at
       GPIO6      , Aux Func.5
       GPIO109  , Aux Func.5

       AP_MD32_TDI , MUX at
       GPIO7      , Aux Func.5
       GPIO110  , Aux Func.5

       AP_MD32_TDO , MUX at
       GPIO8      , Aux Func.5
       GPIO111  , Aux Func.5

       AP_MD32_TRSTN , MUX at
       GPIO9      , Aux Func.5
       GPIO112  , Aux Func.5
    */
    start = 5;
    end = 9;
    for(i = start; i <= end; i++)
    {
        gpio = i + GPIO0;
        mode = mt_get_gpio_mode(gpio);
        ptr += sprintf(ptr, "gpio%03d mode=%d, dir=%d, pull_en=%d, ies=%d, inv=%d, out=%d in=%d\n",
                       gpio,
                       mode,
                       mt_get_gpio_dir(gpio),
                       mt_get_gpio_pull_enable(gpio),
                       mt_get_gpio_ies(gpio),
                       mt_get_gpio_inversion(gpio),
                       mt_get_gpio_out(gpio),
                       mt_get_gpio_in(gpio));
    }

    ptr += sprintf(ptr, "\n");
    if(mode == 5)
    {
        ptr += sprintf(ptr, "%s md32 jtag\n", "ap");
    }
    else
    {
        ptr += sprintf(ptr, "not md32 jtag\n");
    }
    ptr += sprintf(ptr, "\n\n");


    start = 108;
    end = 112;
    for(i = start; i <= end; i++)
    {
        gpio = i + GPIO0;
        mode = mt_get_gpio_mode(gpio);
        ptr += sprintf(ptr, "gpio%03d mode=%d, dir=%d, pull_en=%d, ies=%d, inv=%d, out=%d in=%d\n",
                       gpio,
                       mode,
                       mt_get_gpio_dir(gpio),
                       mt_get_gpio_pull_enable(gpio),
                       mt_get_gpio_ies(gpio),
                       mt_get_gpio_inversion(gpio),
                       mt_get_gpio_out(gpio),
                       mt_get_gpio_in(gpio));
    }

    ptr += sprintf(ptr, "\n");
    if(mode == 5)
    {
        ptr += sprintf(ptr, "%s md32 jtag\n", "ap");
    }
    else
    {
        ptr += sprintf(ptr, "not md32 jtag\n");
    }
    ptr += sprintf(ptr, "\n\n");

    return (ptr - buf);
}


static ssize_t md32_jtag_gpio_dvt_store(struct device *kobj, struct device_attribute *attr, const char *buf, size_t n)
{
    int setup;
    u32 res = sscanf(buf, "%d", &setup);
    if(res != 1)
    {
        pr_err("%s: expect 1 numbers\n", __func__);
        return -EINVAL;
    }

    md32_jtag_dvt_setup_gpio(setup);
    return n;
}

DEVICE_ATTR(md32_jtag_gpio_dvt, 0644, md32_jtag_gpio_dvt_show, md32_jtag_gpio_dvt_store);
#endif //MD32_JTAG_GPIO_DVT



#if MD32_JTAG_GPIO_DVT

static const char *jtag_mode_str[] = {
    /* 0x0 */ "io",
    /* 0x1 */ "ap",
    /* 0x2 */ "mfg",
    /* 0x3 */ "tdd",
    /* 0x4 */ "lte",
    /* 0x5 */ "md32",
    /* 0x6 */ "dfd",
};

void md32_jtag_setup_gpio(u32 jtag_mode)
{
    if(jtag_mode < JTAG_GPIO_MODE_TOTAL)
    {
        pr_debug("change the AP JTAG pins to %s(%d)\n", jtag_mode_str[jtag_mode], jtag_mode);
        mt_set_gpio_mode(GPIO108,jtag_mode);
        mt_set_gpio_mode(GPIO109,jtag_mode);
        mt_set_gpio_mode(GPIO110,jtag_mode);
        mt_set_gpio_mode(GPIO111,jtag_mode);
        mt_set_gpio_mode(GPIO112,jtag_mode);
    }
}

static ssize_t md32_jtag_switch_show(struct device *kobj, struct device_attribute *attr, char *buf)
{
    char *ptr = buf;
    u32 mode;
    mode = mt_get_gpio_mode(GPIO108);

    ptr += sprintf(ptr, "current ap jtag mode: ");

    switch(mode){
    case JTAG_GPIO_MODE_AP:
    case JTAG_GPIO_MODE_MFG:
    case JTAG_GPIO_MODE_TDD:
    case JTAG_GPIO_MODE_LTE:
    case JTAG_GPIO_MODE_MD32:
    case JTAG_GPIO_MODE_DFD:
        ptr += sprintf(ptr, "%s", jtag_mode_str[mode]);
        break;

    default:
        ptr += sprintf(ptr, "unknow (%d)", mode);
        break;
    }
    ptr += sprintf(ptr, "\n");

    /* show usage */
    ptr += sprintf(ptr,
                   "jtag switch usage:\n"
                   "echo [mode] > md32_jtag_switch\n"
                   "[mode]\n");

    for(mode = 0; mode < JTAG_GPIO_MODE_TOTAL; mode++)
    {
        ptr += sprintf(ptr, "%d : %s\n", mode, jtag_mode_str[mode]);
    }

    return (ptr - buf);
}

static ssize_t md32_jtag_switch_store(struct device *kobj, struct device_attribute *attr, const char *buf, size_t n)
{
    int mode;
    u32 res = sscanf(buf, "%d", &mode);
    if(res != 1)
    {
        pr_err("%s: expect 1 numbers\n", __func__);
        return -EINVAL;
    }

    switch(mode){
    case JTAG_GPIO_MODE_AP:
    case JTAG_GPIO_MODE_MFG:
    case JTAG_GPIO_MODE_TDD:
    case JTAG_GPIO_MODE_LTE:
    case JTAG_GPIO_MODE_MD32:
    case JTAG_GPIO_MODE_DFD:
        pr_debug("jtag switch set to %s(%d)\n", jtag_mode_str[mode], mode);
        md32_jtag_setup_gpio(mode);
        break;
    default:
        pr_err("unknow mode\n");
    }

    return n;
}

DEVICE_ATTR(md32_jtag_switch, 0644, md32_jtag_switch_show, md32_jtag_switch_store);
#endif //MD32_JTAG_GPIO_DVT


void md32_ocd_init(void)
{
    spin_lock_init(&md32_ocd_cfg.spinlock);
}
