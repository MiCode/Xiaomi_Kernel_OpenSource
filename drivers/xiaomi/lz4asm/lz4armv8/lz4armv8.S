/*
 * lz4armv8.S
 * LZ4 decompression optimization based on arm64 NEON instruction
 */

#include <linux/linkage.h>
#include <asm/assembler.h>
#include <linux/version.h>
/**
 * _lz4_decompress_asm: The fast LZ4 decompression, lz4 decompression algothrim asm
 * routine,support Huawei EROFS filesystem striving for maximum decompression speed.
 * Entry point _lz4_decompress_asm.
 * @para:
 * x0 = current destination address ptr
 * x1 = destination start position
 * x2 = destination end position
 * x3 = current source address ptr
 * x4 = source end position
 * x5 = flag for DIP
 * @ret:
 * 0 on success, -1 on failure
 *
 * x9: match_length
 * x10: literal_legth
 * x11: copy start ptr
 * x12: copy end ptr
 */


#define match_length		x9
#define literal_length		x10
#define copy_from_ptr		x11    /* copy source ptr*/
#define copy_to_ptr		x12   /* copy destination ptr*/
#define w_tmp			w13   /* temp var */
#define tmp			x13
#define w_offset		w14
#define offset			x14
#define permtable_addr		x15
#define cplen_table_addr	x19
#define save_dst		x20
#define save_src		x21
#define offset_src_ptr		x22
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#ifdef CONFIG_EROFS_FS_PLAT_PARACTRL
#define w_tmp_match_length	w6
#define tmp_match_length	x6
#else
#define w_tmp_match_length	w23
#define tmp_match_length	x23
#endif
#else
#define w_tmp_match_length	w23
#define tmp_match_length	x23
#endif

/* x3 >= x4 src overflow */
.macro check_src_overflow
	cmp	x3, x4
	b.hs	Done
.endm

.macro check_src_overflow1
	cmp	x3, x4
	b.hs	Done1
.endm
/* x0 >= x2 dst overflow */
.macro check_dst_overflow
	cmp	x0, x2
	b.hs	Done
.endm

.macro check_dst_overflow1
	cmp	x0, x2
	b.hs	Done1
.endm

.altmacro
.macro lz4_decompress_asm_generic	doprfm=1
	stp     x29, x30, [sp, #-16]!
	mov     x29, sp
	stp     x19, x20, [sp, #-16]!
	stp     x21, x22, [sp, #-16]!
	stp     x23, x24, [sp, #-16]!
	stp	x3, x0, [sp, #-16]!		/* push src and dst in stack */
	ldr	x3, [x3]				/* x3 = *src_ptr */
	ldr	x0, [x0]				/* x0 = *dst_ptr */
	adr_l	permtable_addr, Permtable
	adr_l	cplen_table_addr, Copylength_table

1:
	/*
	 * Lz4_decompress_begin:
	 * save current dst and src ,ensure when return from asm routine
	 * current both of "dst" and "src" save good position.
	 */
	mov	save_dst, x0
	mov	save_src, x3

	check_dst_overflow
	check_src_overflow

.if \doprfm
	add tmp, x0, #512
	cmp x2, tmp
	b.ls 2f
	prfm pstl2strm,[x0,#512]
.endif

	/* Decode Token Byte: */
2:
	/* Decode_token:*/
	ldrb	w_tmp, [x3], #1		/* read Token Byte */
	lsr	literal_length, tmp, #4	/* get literal_length */
	and	tmp_match_length, tmp, #0xf	/* get match_length */
	add	match_length, tmp_match_length, #4	/* match_length >=4 */

	/*
	 * literal_length <= 14 : no more literal length byte,fllowing zero
	 * or more bytes are liteal bytes.
	 */
	cmp	literal_length, #14
	b.ls	6f

	/*
	 * literal_length == 15 : more literal length bytes after TokenByte.
	 * continue decoding more literal length bytes.
	 */
3:
	/* Get_literal_length: */
	check_src_overflow
	ldrb	w_tmp, [x3], #1
	add	literal_length, literal_length, tmp
	cmp	tmp, #255
	b.eq	3b

/* literal copy */
4:
	/* Copy_long_literal_hs_15: */
	mov	copy_from_ptr, x3
	mov	copy_to_ptr, x0
	add	x3, x3, literal_length
	add	x0, x0, literal_length
	check_dst_overflow
	check_src_overflow
5:
	/* Copy_long_literal_loop: */
	ldr	q0, [copy_from_ptr], #16
	str	q0, [copy_to_ptr], #16

	cmp	x0, copy_to_ptr
	b.ls	7f
	b	5b

6:
	/* Copy_literal_lt_15: */
	ldr q0, [x3]
	str q0, [x0]
	add	x3, x3, literal_length
	add	x0, x0, literal_length

	/* Decode offset and match_length */
	/* Decode_offset_matchlength: */
7:
	mov	offset_src_ptr, x3
	ldrh	w_offset, [x3], #2	/* 2Byte:offset bytes */
	cbz	offset, Failed			/* match_length == 0 is invalid */
	sub	copy_from_ptr, x0, offset
	cmp	copy_from_ptr, x1
	b.lo	Failed
	mov	copy_to_ptr, x0
	/*
	 * set x0 to the end of "match copy";
	 */
	add	x0, x0, match_length
	cmp	match_length, #19
	b.lo	9f
	/*
	 * continue decoding more match length bytes.
	 */
8:
	/* Get_long_matchlength: */
	check_src_overflow1
	ldrb	w_tmp, [x3], #1
	add	x0, x0, tmp
	add	match_length, match_length, tmp
	cmp	tmp, #255
	b.eq	8b

	/*
	 * here got the matchlength,start "match copy".
	 */
9:
	/* Copy_match_begin: */
	check_dst_overflow1
	cmp	offset , match_length
	b.hs	14f

10:
	/* Cond_offset_lt_matchlength: */
	cmp	offset , #32
	b.hs	14f

11:
	/* Copy_offset_lt_32: */
	ldr	q1, [copy_from_ptr]
	add	tmp, permtable_addr, offset, lsl #5
	ldp	q2, q3, [tmp]
	tbl	v0.16b, {v1.16b}, v2.16b
	tbl	v1.16b, {v1.16b}, v3.16b
	cmp     offset , #16
	b.lo    12f
	ldp     q0, q1, [copy_from_ptr]
12:
	/* Copy_match_perm: */
	ldrb	w_tmp, [cplen_table_addr, offset]
	stp	q0, q1, [copy_to_ptr]
	add	copy_to_ptr, copy_to_ptr, tmp
	cmp	x0, copy_to_ptr
	b.ls	1b
13:
	/* Copy_offset_lt_32_loop: */
	stp	q0, q1, [copy_to_ptr]
	add	copy_to_ptr, copy_to_ptr, tmp
	stp	q0, q1, [copy_to_ptr]
	add	copy_to_ptr, copy_to_ptr, tmp
	cmp	x0, copy_to_ptr
	b.hi	13b
	b	1b

/* offset >= match */
14:
	/* Cond_offset_ge_matchlength: */
	ldr	q0, [copy_from_ptr], #16
	str	q0, [copy_to_ptr], #16

	cmp	x0, copy_to_ptr
	b.ls	1b
15:
	/* Copy_offset_ge_match_loop: */
	ldp	q0, q1, [copy_from_ptr], #32
	stp	q0, q1, [copy_to_ptr], #32

	cmp	x0, copy_to_ptr
	b.hi	15b
	b	1b
.endm

.text
.p2align 4

SYM_FUNC_START(_lz4_decompress_asm)
	lz4_decompress_asm_generic
SYM_FUNC_END(_lz4_decompress_asm)

SYM_INNER_LABEL(Failed, SYM_L_LOCAL)
	mov	tmp, #-1
	b	Exit_here

Done1:
	cbz	x5, Done
	sub	save_src, offset_src_ptr, #1
	strb	w_tmp_match_length, [save_src]
	add	save_dst,save_dst,literal_length
Done:
	mov	tmp, #0

Exit_here:
	ldp	x3, x0, [sp], #16
	str	save_src, [x3]
	str	save_dst, [x0]
	ldp     x23, x24, [sp], #16
	ldp     x21, x22, [sp], #16
	ldp     x19, x20, [sp], #16
	mov	x0, tmp
	ldp     x29, x30, [sp], #16
	ret     x30

/*
 * In case of offset <= 31 < matchlength ,expand the pattern and store in
 * repeating pattern size(RPS),store the RPS in Copylength_table.
 * case 1): 1 <= offset <= 15
 * expand the pattern according to the Permtable and store their repeating pattern in q0 q1;
 * RPS = 32 - (32 % offset) offset <= 31
 * case 2): offset >= 16
 * read the pattern and store in q0 q1.
 * RPS = offset.
 */
.pushsection	".rodata", "a"
.p2align 8
Permtable:
.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  //offset = 0
.byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  //offset = 1
.byte 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1  //offset = 2
.byte 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1  //offset = 3
.byte 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3  //offset = 4
.byte 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1, 2, 3, 4, 0, 1  //offset = 5
.byte 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1, 2, 3, 4, 5, 0, 1  //offset = 6
.byte 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3, 4, 5, 6, 0, 1, 2, 3  //offset = 7
.byte 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7, 0, 1, 2, 3, 4, 5, 6, 7  //offset = 8
.byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 2, 3, 4  //offset = 9
.byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1  //offset = 10
.byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9  //offset = 11
.byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11, 0, 1, 2, 3, 4, 5, 6, 7  //offset = 12
.byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12, 0, 1, 2, 3, 4, 5  //offset = 13
.byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13, 0, 1, 2, 3  //offset = 14
.byte 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14, 0, 1  //offset = 15

.p2align 8
Copylength_table:
.byte 32,32,32,30,32,30,30,28,32,27,30,22,24,26,28,30  // 0  .. 15
.byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31  // 16 .. 31
.popsection

.text
.p2align 4
SYM_FUNC_START(_lz4_decompress_asm_noprfm)
	lz4_decompress_asm_generic	0
SYM_FUNC_END(_lz4_decompress_asm_noprfm)
