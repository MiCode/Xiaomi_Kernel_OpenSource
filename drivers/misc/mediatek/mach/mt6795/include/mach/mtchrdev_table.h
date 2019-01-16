#ifdef MAKE_MTCHRDEV_ENUM
enum{
    DNY_CHRDEV,
#endif
    MTCHRDEV_REG(  1, mem)
    MTCHRDEV_REG(  2, pty)
    MTCHRDEV_REG(  3, ttyp)
    MTCHRDEV_REG(  4, dev_vc)
    MTCHRDEV_REG(  5, dev_tty)
    MTCHRDEV_REG(  7, vcs)
    MTCHRDEV_REG( 10, misc)
    MTCHRDEV_REG( 13, input)
    MTCHRDEV_REG( 14, sound)
    MTCHRDEV_REG( 29, fb)
    MTCHRDEV_REG( 90, mtd)
    MTCHRDEV_REG( 116, alsa)
    MTCHRDEV_REG( 128, ptm)
    MTCHRDEV_REG( 136, pts)
    MTCHRDEV_REG( 151, ampc)
    MTCHRDEV_REG( 153, wmt_WIFI)
    MTCHRDEV_REG( 160, VCodec )
    MTCHRDEV_REG( 167, emd_cfifo_drv)
    MTCHRDEV_REG( 169, ccci_MD2)
    MTCHRDEV_REG( 180, usb)
    MTCHRDEV_REG( 182, sec)
    MTCHRDEV_REG( 184, ccci_MD1)
    MTCHRDEV_REG( 188, M4U)
    MTCHRDEV_REG( 189, usb_device)
    MTCHRDEV_REG( 190, mtk_stp_wmp)
    MTCHRDEV_REG( 191, mtk_stp_GPS)
    MTCHRDEV_REG( 192, mtk_stp_BT)
    MTCHRDEV_REG( 193, fm)
    MTCHRDEV_REG( 196, devmap)
    MTCHRDEV_REG( 204, ttyMT)
#ifdef MAKE_MTCHRDEV_ENUM
    MTCHRDEV_COUNT 
};
#undef MAKE_MTCHRDEV_ENUM
#endif


