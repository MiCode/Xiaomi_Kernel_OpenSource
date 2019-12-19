extern int show_io_level[];

enum io_level_index {
	syscall,
	blk_common,
	blk_driver,
	blk_submit_bio,
	jbd2,
	elv,
	detail,
	log_switch,
	level_max,
};

#define IO_SYSCALL_LEVEL show_io_level[syscall]
#define IO_BLK_LEVEL show_io_level[blk_common]
#define IO_BLK_DRIVER_LEVEL show_io_level[blk_driver]
#define IO_BLK_SUBMIT_BIO_LEVEL show_io_level[blk_submit_bio]
#define IO_JBD2_LEVEL show_io_level[jbd2]
#define IO_ELV_LEVEL show_io_level[elv]
#define IO_SHOW_DETAIL show_io_level[detail]
#define IO_SHOW_LOG show_io_level[log_switch]
