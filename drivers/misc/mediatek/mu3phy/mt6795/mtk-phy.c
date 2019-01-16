#define U3_PHY_LIB
#include <linux/mu3phy/mtk-phy.h>
#undef U3_PHY_LIB

#ifdef CONFIG_C60802_SUPPORT
#include <linux/mu3phy/mtk-phy-c60802.h>
#endif
#ifdef CONFIG_D60802_SUPPORT
#include <linux/mu3phy/mtk-phy-d60802.h>
#endif
#ifdef CONFIG_E60802_SUPPORT
#include <linux/mu3phy/mtk-phy-e60802.h>
#endif
#ifdef CONFIG_PROJECT_PHY
#include <linux/mu3phy/mtk-phy-asic.h>
#endif

#ifdef CONFIG_C60802_SUPPORT
static const struct u3phy_operator c60802_operators = {
	.init = phy_init_c60802,
	.change_pipe_phase = phy_change_pipe_phase_c60802,
	.eyescan_init = eyescan_init_c60802,
	.eyescan = phy_eyescan_c60802,
	.u2_connect = u2_connect_c60802,
	.u2_disconnect = u2_disconnect_c60802,
	.u2_save_current_entry = u2_save_cur_en_c60802,
	.u2_save_current_recovery = u2_save_cur_re_c60802,
	.u2_slew_rate_calibration = u2_slew_rate_calibration_c60802,
};
#endif
#ifdef CONFIG_D60802_SUPPORT
static const struct u3phy_operator d60802_operators = {
	.init = phy_init_d60802,
	.change_pipe_phase = phy_change_pipe_phase_d60802,
	.eyescan_init = eyescan_init_d60802,
	.eyescan = phy_eyescan_d60802,
	.u2_connect = u2_connect_d60802,
	.u2_disconnect = u2_disconnect_d60802,
	//.u2_save_current_entry = u2_save_cur_en_d60802,
	//.u2_save_current_recovery = u2_save_cur_re_d60802,
	.u2_slew_rate_calibration = u2_slew_rate_calibration_d60802,
};
#endif
#ifdef CONFIG_E60802_SUPPORT
static const struct u3phy_operator e60802_operators = {
	.init = phy_init_e60802,
	.change_pipe_phase = phy_change_pipe_phase_e60802,
	.eyescan_init = eyescan_init_e60802,
	.eyescan = phy_eyescan_e60802,
	.u2_connect = u2_connect_e60802,
	.u2_disconnect = u2_disconnect_e60802,
	//.u2_save_current_entry = u2_save_cur_en_e60802,
	//.u2_save_current_recovery = u2_save_cur_re_e60802,
	.u2_slew_rate_calibration = u2_slew_rate_calibration_e60802,
};
#endif

#ifdef CONFIG_PROJECT_PHY
static struct u3phy_operator project_operators = {
	.init = phy_init_soc,
	.u2_slew_rate_calibration = u2_slew_rate_calibration,
};
#endif


PHY_INT32 u3phy_init(){
#ifndef CONFIG_PROJECT_PHY
	PHY_INT32 u3phy_version;
#endif

	if (u3phy != NULL)
		return PHY_TRUE;

	u3phy = kmalloc(sizeof(struct u3phy_info), GFP_NOIO);
#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
	u3phy->phyd_version_addr = 0x2000e4;
#else
	u3phy->phyd_version_addr = U3_PHYD_B2_BASE + 0xe4;
#endif
	u3phy_ops = NULL;

#ifdef CONFIG_PROJECT_PHY
	u3phy->u2phy_regs_e = (struct u2phy_reg_e *)U2_PHY_BASE;
	u3phy->u3phyd_regs_e = (struct u3phyd_reg_e *)U3_PHYD_BASE;
	u3phy->u3phyd_bank2_regs_e = (struct u3phyd_bank2_reg_e *)U3_PHYD_B2_BASE;
	u3phy->u3phya_regs_e = (struct u3phya_reg_e *)U3_PHYA_BASE;
	u3phy->u3phya_da_regs_e = (struct u3phya_da_reg_e *)U3_PHYA_DA_BASE;
	u3phy->sifslv_chip_regs_e = (struct sifslv_chip_reg_e *)SIFSLV_CHIP_BASE;
	u3phy->spllc_regs_e = (struct spllc_reg_e *)SIFSLV_SPLLC_BASE;
	u3phy->sifslv_fm_regs_e = (struct sifslv_fm_feg_e *)SIFSLV_FM_FEG_BASE;
	u3phy_ops = (struct u3phy_operator *)&project_operators;
#else

	//parse phy version
	u3phy_version = U3PhyReadReg32(u3phy->phyd_version_addr);
	printk(KERN_ERR "phy version: %x\n", u3phy_version);
	u3phy->phy_version = u3phy_version;

	if (u3phy_version == 0xc60802a) {
	#ifdef CONFIG_C60802_SUPPORT
	#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
		u3phy->u2phy_regs_c = (struct u2phy_reg_c *)0x0;
		u3phy->u3phyd_regs_c = (struct u3phyd_reg_c *)0x100000;
		u3phy->u3phyd_bank2_regs_c = (struct u3phyd_bank2_reg_c *)0x200000;
		u3phy->u3phya_regs_c = (struct u3phya_reg_c *)0x300000;
		u3phy->u3phya_da_regs_c = (struct u3phya_da_reg_c *)0x400000;
		u3phy->sifslv_chip_regs_c = (struct sifslv_chip_reg_c *)0x500000;
		u3phy->sifslv_fm_regs_c = (struct sifslv_fm_feg_c *)0xf00000;
	#else
		u3phy->u2phy_regs_c = (struct u2phy_reg_c *)U2_PHY_BASE;
		u3phy->u3phyd_regs_c = (struct u3phyd_reg_c *)U3_PHYD_BASE;
		u3phy->u3phyd_bank2_regs_c = (struct u3phyd_bank2_reg_c *)U3_PHYD_B2_BASE;
		u3phy->u3phya_regs_c = (struct u3phya_reg_c *)U3_PHYA_BASE;
		u3phy->u3phya_da_regs_c = (struct u3phya_da_reg_c *)U3_PHYA_DA_BASE;
		u3phy->sifslv_chip_regs_c = (struct sifslv_chip_reg_c *)SIFSLV_CHIP_BASE;
		u3phy->sifslv_fm_regs_c = (struct sifslv_fm_feg_c *)SIFSLV_FM_FEG_BASE;
	#endif
		u3phy_ops = (struct u3phy_operator *)&c60802_operators;
	#endif
	}
	else if (u3phy_version == 0xd60802a) {
	#ifdef CONFIG_D60802_SUPPORT
	#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
		u3phy->u2phy_regs_d = (struct u2phy_reg_d *) 0x0;
		u3phy->u3phyd_regs_d = (struct u3phyd_reg_d *)0x100000;
		u3phy->u3phyd_bank2_regs_d = (struct u3phyd_bank2_reg_d *)0x200000;
		u3phy->u3phya_regs_d = (struct u3phya_reg_d *)0x300000;
		u3phy->u3phya_da_regs_d = (struct u3phya_da_reg_d *)0x400000;
		u3phy->sifslv_chip_regs_d = (struct sifslv_chip_reg_d *)0x500000;
		u3phy->sifslv_fm_regs_d = (struct sifslv_fm_feg_d *)0xf00000;
	#else
		u3phy->u2phy_regs_d = (struct u2phy_reg_d *)U2_PHY_BASE;
		u3phy->u3phyd_regs_d = (struct u3phyd_reg_d *)U3_PHYD_BASE;
		u3phy->u3phyd_bank2_regs_d = (struct u3phyd_bank2_reg_d *)U3_PHYD_B2_BASE;
		u3phy->u3phya_regs_d = (struct u3phya_reg_d *)U3_PHYA_BASE;
		u3phy->u3phya_da_regs_d = (struct u3phya_da_reg_d *)U3_PHYA_DA_BASE;
		u3phy->sifslv_chip_regs_d = (struct sifslv_chip_reg_d *)SIFSLV_CHIP_BASE;
		u3phy->sifslv_fm_regs_d = (struct sifslv_fm_feg_d *)SIFSLV_FM_FEG_BASE;
	#endif
		u3phy_ops = (struct u3phy_operator *)&d60802_operators;
	#endif
	}
	else if (u3phy_version == 0xe60802a) {
	#ifdef CONFIG_E60802_SUPPORT
	#ifdef CONFIG_U3_PHY_GPIO_SUPPORT
		u3phy->u2phy_regs_e = (struct u2phy_reg_e *)0x0;
		u3phy->u3phyd_regs_e = (struct u3phyd_reg_e *)0x100000;
		u3phy->u3phyd_bank2_regs_e = (struct u3phyd_bank2_reg_e *)0x200000;
		u3phy->u3phya_regs_e = (struct u3phya_reg_e *)0x300000;
		u3phy->u3phya_da_regs_e = (struct u3phya_da_reg_e *)0x400000;
		u3phy->sifslv_chip_regs_e = (struct sifslv_chip_reg_e *)0x500000;
		u3phy->spllc_regs_e = (struct spllc_reg_e *)0x600000;
		u3phy->sifslv_fm_regs_e = (struct sifslv_fm_feg_e *)0xf00000;
	#else
		u3phy->u2phy_regs_e = (struct u2phy_reg_e *)U2_PHY_BASE;
		u3phy->u3phyd_regs_e = (struct u3phyd_reg_e *)U3_PHYD_BASE;
		u3phy->u3phyd_bank2_regs_e = (struct u3phyd_bank2_reg_e *)U3_PHYD_B2_BASE;
		u3phy->u3phya_regs_e = (struct u3phya_reg_e *)U3_PHYA_BASE;
		u3phy->u3phya_da_regs_e = (struct u3phya_da_reg_e *)U3_PHYA_DA_BASE;
		u3phy->sifslv_chip_regs_e = (struct sifslv_chip_reg_e *)SIFSLV_CHIP_BASE;
		u3phy->sifslv_fm_regs_e = (struct sifslv_fm_feg_e *)SIFSLV_FM_FEG_BASE;
	#endif
		u3phy_ops = (struct u3phy_operator *)&e60802_operators;
	#endif
	}
	else {
		printk(KERN_ERR "No match phy version\n");
		return PHY_FALSE;
	}

#endif

	if (!u3phy_ops)
		return PHY_FALSE;
	else
		return PHY_TRUE;
}

PHY_INT32 U3PhyWriteField8(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value){
	PHY_INT8 cur_value;
	PHY_INT8 new_value;

	cur_value = U3PhyReadReg8((u3phy_addr_t)addr);
	new_value = (cur_value & (~mask))| ((value << offset) & mask);

	mb();

	U3PhyWriteReg8((u3phy_addr_t)addr, new_value);

	mb();

	return PHY_TRUE;
}

PHY_INT32 U3PhyWriteField32(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask, PHY_INT32 value){
	PHY_INT32 cur_value;
	PHY_INT32 new_value;

	cur_value = U3PhyReadReg32((u3phy_addr_t)addr);
	new_value = (cur_value & (~mask)) | ((value << offset) & mask);

	mb();

	U3PhyWriteReg32((u3phy_addr_t)addr, new_value);

	mb();

	return PHY_TRUE;
}

PHY_INT32 U3PhyReadField8(phys_addr_t addr,PHY_INT32 offset,PHY_INT32 mask){

	return ((U3PhyReadReg8((u3phy_addr_t)addr) & mask) >> offset);
}

PHY_INT32 U3PhyReadField32(phys_addr_t addr, PHY_INT32 offset, PHY_INT32 mask){

	return ((U3PhyReadReg32((u3phy_addr_t)addr) & mask) >> offset);
}

void phy_hsrx_set(void){
    switch(u3phy->phy_version){
    #ifdef CONFIG_D60802_SUPPORT
    case 0xd60802a:
        U3PhyWriteField32(((phys_addr_t)&u3phy->u2phy_regs_d->usbphyacr6)
        ,D60802_RG_USB20_HSRX_MMODE_SELE_OFST,D60802_RG_USB20_HSRX_MMODE_SELE,0x2);

        printk("%s: WRITE HSRX_MMODE_SELE(%d)\n", __func__,
            U3PhyReadField32(((phys_addr_t)&u3phy->u2phy_regs_d->usbphyacr6)
            ,D60802_RG_USB20_HSRX_MMODE_SELE_OFST,D60802_RG_USB20_HSRX_MMODE_SELE));
        break;
    #endif
    }
}

void phy_hsrx_reset(void){
    switch(u3phy->phy_version){
    #ifdef CONFIG_D60802_SUPPORT
    case 0xd60802a:
        U3PhyWriteField32(((phys_addr_t)&u3phy->u2phy_regs_d->usbphyacr6)
        ,D60802_RG_USB20_HSRX_MMODE_SELE_OFST,D60802_RG_USB20_HSRX_MMODE_SELE,0x0);

        printk("%s: WRITE HSRX_MMODE_SELE(%d)\n", __func__,
            U3PhyReadField32(((phys_addr_t)&u3phy->u2phy_regs_d->usbphyacr6)
            ,D60802_RG_USB20_HSRX_MMODE_SELE_OFST,D60802_RG_USB20_HSRX_MMODE_SELE));
        break;
    #endif
    }
}

