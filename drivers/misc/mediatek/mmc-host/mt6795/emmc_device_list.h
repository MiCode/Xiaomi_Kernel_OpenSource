typedef struct {
	/* MID + PNM in CID register */
	u8 m_id;		/* Manufacturer ID */
	char pro_name[8];	/* Product name */

	u8 r_smpl;
	u8 d_smpl;
	u8 cmd_rxdly;
	u8 rd_rxdly;
	u8 wr_rxdly;
} mmcdev_info, *pmmcdev_info;

static const mmcdev_info g_mmcTable[] = {
	/* hynix */
	{0x90, "HYNIX ", 0, 0, 0, 0, 0},

	/* end */
	{0x00, "xxxxxx", 0, 0, 0, 0, 0}
};
