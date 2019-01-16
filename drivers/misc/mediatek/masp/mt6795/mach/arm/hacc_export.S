	.arch armv7-a
	.fpu softvfp
	.eabi_attribute 20, 1
	.eabi_attribute 21, 1
	.eabi_attribute 23, 3
	.eabi_attribute 24, 1
	.eabi_attribute 25, 1
	.eabi_attribute 26, 2
	.eabi_attribute 30, 2
	.eabi_attribute 18, 4
	.file	"hacc_export.c"
	.text
.Ltext0:
	.align	2
	.type	sp_hacc_internal.isra.0, %function
sp_hacc_internal.isra.0:
.LFB204:
	.file 1 "/alps/kernel/mediatek/platform/mt6582/kernel/drivers/masp/mach/hacc_export.c"
	.loc 1 83 0
	@ args = 8, pretend = 0, frame = 8
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL0:
	mov	ip, sp
.LCFI0:
	stmfd	sp!, {r4, r5, r6, r7, fp, ip, lr, pc}
.LCFI1:
	sub	fp, ip, #4
.LCFI2:
	sub	sp, sp, #8
.LCFI3:
.LVL1:
	.loc 1 90 0
	cmp	r3, #1
	.loc 1 83 0
	mov	r5, r3
	mov	r4, r0
	mov	r6, r1
	ldrb	r7, [fp, #8]	@ zero_extendqisi2
	.loc 1 90 0
	beq	.L17
.LVL2:
.L2:
	.loc 1 105 0
	cmp	r2, #1
	beq	.L6
	bcs	.L18
	.loc 1 111 0
	adds	r0, r7, #0
	ldr	r1, .L21
	movne	r0, #1
.L15:
	.loc 1 122 0
	bl	HACC_V3_Init
	.loc 1 124 0
	mov	r0, r4
	mov	r1, r6
	mov	r2, r4
	bl	HACC_V3_Run
	.loc 1 126 0
	bl	HACC_V3_Terminate
	.loc 1 159 0
	cmp	r5, #1
	beq	.L19
.L9:
	.loc 1 177 0
	mov	r0, r4
	sub	sp, fp, #28
	ldmfd	sp, {r4, r5, r6, r7, fp, sp, pc}
.L18:
	.loc 1 105 0
	cmp	r2, #2
	.loc 1 152 0
	movwne	r2, #24588
	.loc 1 105 0
	beq	.L20
.L4:
.LVL3:
	.loc 1 167 0
	cmp	r5, #1
	beq	.L3
.LVL4:
.L10:
	.loc 1 172 0 discriminator 1
	ldr	r1, .L21+4
	ldr	r0, .L21+8
	bl	printk
	.loc 1 174 0 discriminator 1
#APP
@ 174 "/alps/kernel/mediatek/platform/mt6582/kernel/drivers/masp/mach/hacc_export.c" 1
	.word 0xe7f001f2
@ 0 "" 2
.L20:
	.loc 1 133 0
	ldr	r3, .L21+12
	ldrb	r3, [r3, #0]	@ zero_extendqisi2
	cmp	r3, #0
	beq	.L13
	.loc 1 140 0
	mov	r0, r2
	mov	r1, #32
	bl	hacc_set_key
.LVL5:
	.loc 1 142 0
	subs	r2, r0, #0
	bne	.L4
	.loc 1 145 0
	mov	r2, r4
	ldr	r0, [fp, #4]
.LVL6:
	mov	r1, r4
	bic	r3, r6, #127
	bl	hacc_do_aes
.LVL7:
	.loc 1 147 0
	subs	r2, r0, #0
	bne	.L4
	.loc 1 159 0
	cmp	r5, #1
	bne	.L9
.LVL8:
.L19:
	.loc 1 161 0
	bl	osal_hacc_unlock
	b	.L9
.L6:
	.loc 1 122 0
	adds	r0, r7, #0
	ldr	r1, .L21+16
	movne	r0, #1
	b	.L15
.LVL9:
.L17:
	.loc 1 93 0
	str	r2, [fp, #-32]
	bl	osal_hacc_lock
.LVL10:
	.loc 1 95 0
	ldr	r2, [fp, #-32]
	cmp	r0, #0
	beq	.L2
	.loc 1 97 0
	movw	r2, #24586
.LVL11:
.L3:
	.loc 1 169 0
	str	r2, [fp, #-32]
	bl	osal_hacc_unlock
	ldr	r2, [fp, #-32]
	b	.L10
.L13:
	.loc 1 135 0
	movw	r2, #24583
	b	.L4
.L22:
	.align	2
.L21:
	.word	.LANCHOR0
	.word	.LC1
	.word	.LC0
	.word	.LANCHOR1
	.word	.LANCHOR0+32
.LFE204:
	.size	sp_hacc_internal.isra.0, .-sp_hacc_internal.isra.0
	.align	2
	.global	masp_hal_sp_hacc_enc
	.type	masp_hal_sp_hacc_enc, %function
masp_hal_sp_hacc_enc:
.LFB200:
	.loc 1 183 0
	@ args = 4, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL12:
	mov	ip, sp
.LCFI4:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI5:
	sub	fp, ip, #4
.LCFI6:
	sub	sp, sp, #8
.LCFI7:
	.loc 1 184 0
	mov	r2, r3
.LVL13:
	mov	r3, #1
.LVL14:
	str	r3, [sp, #0]
	str	r3, [sp, #4]
	ldrb	r3, [fp, #4]	@ zero_extendqisi2
	bl	sp_hacc_internal.isra.0
.LVL15:
	.loc 1 185 0
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.LFE200:
	.size	masp_hal_sp_hacc_enc, .-masp_hal_sp_hacc_enc
	.align	2
	.global	masp_hal_sp_hacc_dec
	.type	masp_hal_sp_hacc_dec, %function
masp_hal_sp_hacc_dec:
.LFB201:
	.loc 1 192 0
	@ args = 4, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL16:
	mov	ip, sp
.LCFI8:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI9:
	sub	fp, ip, #4
.LCFI10:
	sub	sp, sp, #8
.LCFI11:
	.loc 1 193 0
	mov	r2, r3
.LVL17:
	mov	r3, #0
.LVL18:
	str	r3, [sp, #0]
	str	r3, [sp, #4]
	ldrb	r3, [fp, #4]	@ zero_extendqisi2
	bl	sp_hacc_internal.isra.0
.LVL19:
	.loc 1 194 0
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.LFE201:
	.size	masp_hal_sp_hacc_dec, .-masp_hal_sp_hacc_dec
	.align	2
	.global	masp_hal_sp_hacc_blk_sz
	.type	masp_hal_sp_hacc_blk_sz, %function
masp_hal_sp_hacc_blk_sz:
.LFB202:
	.loc 1 200 0
	@ args = 0, pretend = 0, frame = 0
	@ frame_needed = 1, uses_anonymous_args = 0
	mov	ip, sp
.LCFI12:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI13:
	sub	fp, ip, #4
.LCFI14:
	.loc 1 202 0
	mov	r0, #16
	ldmfd	sp, {fp, sp, pc}
.LFE202:
	.size	masp_hal_sp_hacc_blk_sz, .-masp_hal_sp_hacc_blk_sz
	.align	2
	.global	masp_hal_sp_hacc_init
	.type	masp_hal_sp_hacc_init, %function
masp_hal_sp_hacc_init:
.LFB203:
	.loc 1 208 0
	@ args = 0, pretend = 0, frame = 40
	@ frame_needed = 1, uses_anonymous_args = 0
.LVL20:
	mov	ip, sp
.LCFI15:
	stmfd	sp!, {fp, ip, lr, pc}
.LCFI16:
	sub	fp, ip, #4
.LCFI17:
	sub	sp, sp, #48
.LCFI18:
.LVL21:
	.loc 1 212 0
	cmp	r1, #16
	.loc 1 214 0
	movwne	r0, #24580
.LVL22:
	.loc 1 212 0
	beq	.L31
.LVL23:
.L27:
	.loc 1 227 0
	sub	sp, fp, #12
	ldmfd	sp, {fp, sp, pc}
.LVL24:
.L31:
	.loc 1 217 0
	sub	r1, fp, #12
.LVL25:
	.loc 1 218 0
	mov	r3, #0
	.loc 1 217 0
	mov	r2, #32
	str	r2, [fp, #-48]
.LVL26:
.L28:
	.loc 1 207 0
	sub	ip, fp, #48
	.loc 1 220 0
	ldrb	r2, [r0, r3]	@ zero_extendqisi2
	.loc 1 207 0
	add	lr, ip, r3
	.loc 1 218 0 discriminator 2
	add	r3, r3, #1
.LVL27:
	.loc 1 218 0 is_stmt 0 discriminator 1
	cmp	r3, #16
	.loc 1 221 0 is_stmt 1
	add	ip, r2, #1
	.loc 1 220 0
	strb	r2, [lr, #4]
	.loc 1 221 0
	strb	ip, [r1, #-1]!
	.loc 1 218 0 discriminator 1
	bne	.L28
	.loc 1 224 0 discriminator 1
	ldrb	ip, [fp, #-41]	@ zero_extendqisi2
	ldrb	r1, [fp, #-44]	@ zero_extendqisi2
	ldrb	r2, [fp, #-43]	@ zero_extendqisi2
	ldrb	r3, [fp, #-42]	@ zero_extendqisi2
.LVL28:
	ldr	r0, .L32
	str	ip, [sp, #0]
	bl	printk
	.loc 1 226 0
	sub	r0, fp, #48
	bl	hacc_init
	b	.L27
.L33:
	.align	2
.L32:
	.word	.LC2
.LFE203:
	.size	masp_hal_sp_hacc_init, .-masp_hal_sp_hacc_init
	.global	bHACC_SWKeyInit
	.global	bHACC_HWWrapKeyInit
	.section	.rodata
	.align	2
.LANCHOR0 = . + 0
	.type	g_HACC_CFG_1, %object
	.size	g_HACC_CFG_1, 32
g_HACC_CFG_1:
	.word	-1630272512
	.word	15238305
	.word	-470776899
	.word	793669002
	.word	-8155556
	.word	-381640477
	.word	-1924272954
	.word	1174145417
	.type	g_HACC_CFG_2, %object
	.size	g_HACC_CFG_2, 32
g_HACC_CFG_2:
	.word	-1437324070
	.word	1431445780
	.word	-470776899
	.word	1431445780
	.word	-1437324070
	.word	-1437324070
	.word	1431445780
	.word	-1437324070
	.section	.rodata.str1.4,"aMS",%progbits,1
	.align	2
.LC0:
	.ascii	"[%s] HACC Fail (0x%x)\012\000"
	.space	1
.LC1:
	.ascii	"ASF\000"
.LC2:
	.ascii	"0x%x,0x%x,0x%x,0x%x\012\000"
	.bss
.LANCHOR1 = . + 0
	.type	bHACC_HWWrapKeyInit, %object
	.size	bHACC_HWWrapKeyInit, 1
bHACC_HWWrapKeyInit:
	.space	1
	.type	bHACC_SWKeyInit, %object
	.size	bHACC_SWKeyInit, 1
bHACC_SWKeyInit:
	.space	1
	.section	.debug_frame,"",%progbits
.Lframe0:
	.4byte	.LECIE0-.LSCIE0
.LSCIE0:
	.4byte	0xffffffff
	.byte	0x1
	.ascii	"\000"
	.uleb128 0x1
	.sleb128 -4
	.byte	0xe
	.byte	0xc
	.uleb128 0xd
	.uleb128 0
	.align	2
.LECIE0:
.LSFDE0:
	.4byte	.LEFDE0-.LASFDE0
.LASFDE0:
	.4byte	.Lframe0
	.4byte	.LFB204
	.4byte	.LFE204-.LFB204
	.byte	0x4
	.4byte	.LCFI0-.LFB204
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI1-.LCFI0
	.byte	0x8e
	.uleb128 0x2
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8b
	.uleb128 0x4
	.byte	0x87
	.uleb128 0x5
	.byte	0x86
	.uleb128 0x6
	.byte	0x85
	.uleb128 0x7
	.byte	0x84
	.uleb128 0x8
	.byte	0x4
	.4byte	.LCFI2-.LCFI1
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE0:
.LSFDE2:
	.4byte	.LEFDE2-.LASFDE2
.LASFDE2:
	.4byte	.Lframe0
	.4byte	.LFB200
	.4byte	.LFE200-.LFB200
	.byte	0x4
	.4byte	.LCFI4-.LFB200
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI5-.LCFI4
	.byte	0x8e
	.uleb128 0x2
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8b
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI6-.LCFI5
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE2:
.LSFDE4:
	.4byte	.LEFDE4-.LASFDE4
.LASFDE4:
	.4byte	.Lframe0
	.4byte	.LFB201
	.4byte	.LFE201-.LFB201
	.byte	0x4
	.4byte	.LCFI8-.LFB201
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI9-.LCFI8
	.byte	0x8e
	.uleb128 0x2
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8b
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI10-.LCFI9
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE4:
.LSFDE6:
	.4byte	.LEFDE6-.LASFDE6
.LASFDE6:
	.4byte	.Lframe0
	.4byte	.LFB202
	.4byte	.LFE202-.LFB202
	.byte	0x4
	.4byte	.LCFI12-.LFB202
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI13-.LCFI12
	.byte	0x8e
	.uleb128 0x2
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8b
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI14-.LCFI13
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE6:
.LSFDE8:
	.4byte	.LEFDE8-.LASFDE8
.LASFDE8:
	.4byte	.Lframe0
	.4byte	.LFB203
	.4byte	.LFE203-.LFB203
	.byte	0x4
	.4byte	.LCFI15-.LFB203
	.byte	0xd
	.uleb128 0xc
	.byte	0x4
	.4byte	.LCFI16-.LCFI15
	.byte	0x8e
	.uleb128 0x2
	.byte	0x8d
	.uleb128 0x3
	.byte	0x8b
	.uleb128 0x4
	.byte	0x4
	.4byte	.LCFI17-.LCFI16
	.byte	0xc
	.uleb128 0xb
	.uleb128 0x4
	.align	2
.LEFDE8:
	.text
.Letext0:
	.file 2 "/alps/kernel/../mediatek/platform/mt6582/kernel/core/include/mach/mt_typedefs.h"
	.file 3 "/alps/kernel/../mediatek/kernel/include/mach/mt_sec_hal.h"
	.file 4 "/alps/kernel/mediatek/platform/mt6582/kernel/drivers/masp/asf/asf_inc/sec_typedef.h"
	.file 5 "/alps/kernel/mediatek/platform/mt6582/kernel/drivers/masp/mach/hacc_mach.h"
	.file 6 "/alps/kernel/arch/arm/include/asm/hwcap.h"
	.file 7 "/alps/kernel/include/linux/printk.h"
	.file 8 "/alps/kernel/include/linux/kernel.h"
	.section	.debug_info,"",%progbits
.Ldebug_info0:
	.4byte	0x433
	.2byte	0x2
	.4byte	.Ldebug_abbrev0
	.byte	0x4
	.uleb128 0x1
	.4byte	.LASF46
	.byte	0x1
	.4byte	.LASF47
	.4byte	.LASF48
	.4byte	.Ltext0
	.4byte	.Letext0
	.4byte	.Ldebug_line0
	.4byte	.Ldebug_pubnames0
	.4byte	.Ldebug_pubtypes0
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF0
	.uleb128 0x2
	.byte	0x4
	.byte	0x7
	.4byte	.LASF1
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF2
	.uleb128 0x2
	.byte	0x1
	.byte	0x6
	.4byte	.LASF3
	.uleb128 0x2
	.byte	0x1
	.byte	0x8
	.4byte	.LASF4
	.uleb128 0x2
	.byte	0x2
	.byte	0x5
	.4byte	.LASF5
	.uleb128 0x2
	.byte	0x2
	.byte	0x7
	.4byte	.LASF6
	.uleb128 0x3
	.byte	0x4
	.byte	0x5
	.ascii	"int\000"
	.uleb128 0x2
	.byte	0x8
	.byte	0x5
	.4byte	.LASF7
	.uleb128 0x2
	.byte	0x8
	.byte	0x7
	.4byte	.LASF8
	.uleb128 0x2
	.byte	0x4
	.byte	0x5
	.4byte	.LASF9
	.uleb128 0x2
	.byte	0x1
	.byte	0x2
	.4byte	.LASF10
	.uleb128 0x4
	.byte	0x4
	.4byte	0x49
	.uleb128 0x5
	.ascii	"U8\000"
	.byte	0x2
	.byte	0x4d
	.4byte	0x49
	.uleb128 0x5
	.ascii	"U32\000"
	.byte	0x2
	.byte	0x51
	.4byte	0x34
	.uleb128 0x2
	.byte	0x4
	.byte	0x4
	.4byte	.LASF11
	.uleb128 0x6
	.4byte	.LASF12
	.byte	0x2
	.byte	0x8b
	.4byte	0x49
	.uleb128 0x7
	.byte	0x4
	.byte	0x3
	.byte	0x12
	.4byte	0xc9
	.uleb128 0x8
	.4byte	.LASF13
	.sleb128 0
	.uleb128 0x8
	.4byte	.LASF14
	.sleb128 1
	.uleb128 0x8
	.4byte	.LASF15
	.sleb128 2
	.byte	0
	.uleb128 0x6
	.4byte	.LASF16
	.byte	0x3
	.byte	0x16
	.4byte	0xae
	.uleb128 0x6
	.4byte	.LASF17
	.byte	0x4
	.byte	0x32
	.4byte	0x34
	.uleb128 0x6
	.4byte	.LASF18
	.byte	0x4
	.byte	0x33
	.4byte	0x49
	.uleb128 0x7
	.byte	0x4
	.byte	0x5
	.byte	0x9c
	.4byte	0xff
	.uleb128 0x8
	.4byte	.LASF19
	.sleb128 0
	.uleb128 0x8
	.4byte	.LASF20
	.sleb128 1
	.byte	0
	.uleb128 0x6
	.4byte	.LASF21
	.byte	0x5
	.byte	0x9f
	.4byte	0xea
	.uleb128 0x7
	.byte	0x4
	.byte	0x5
	.byte	0xa2
	.4byte	0x125
	.uleb128 0x8
	.4byte	.LASF22
	.sleb128 16
	.uleb128 0x8
	.4byte	.LASF23
	.sleb128 24
	.uleb128 0x8
	.4byte	.LASF24
	.sleb128 32
	.byte	0
	.uleb128 0x7
	.byte	0x4
	.byte	0x5
	.byte	0xa9
	.4byte	0x140
	.uleb128 0x8
	.4byte	.LASF25
	.sleb128 0
	.uleb128 0x8
	.4byte	.LASF26
	.sleb128 1
	.uleb128 0x8
	.4byte	.LASF27
	.sleb128 2
	.byte	0
	.uleb128 0x9
	.byte	0x24
	.byte	0x5
	.byte	0xb3
	.4byte	0x165
	.uleb128 0xa
	.4byte	.LASF28
	.byte	0x5
	.byte	0xb4
	.4byte	0x91
	.byte	0x2
	.byte	0x23
	.uleb128 0
	.uleb128 0xa
	.4byte	.LASF29
	.byte	0x5
	.byte	0xb5
	.4byte	0x165
	.byte	0x2
	.byte	0x23
	.uleb128 0x4
	.byte	0
	.uleb128 0xb
	.4byte	0x87
	.4byte	0x175
	.uleb128 0xc
	.4byte	0x34
	.byte	0x1f
	.byte	0
	.uleb128 0x6
	.4byte	.LASF30
	.byte	0x5
	.byte	0xb6
	.4byte	0x140
	.uleb128 0xd
	.4byte	.LASF49
	.byte	0x1
	.byte	0x53
	.byte	0x1
	.4byte	0x1f1
	.byte	0x1
	.4byte	0x1f1
	.uleb128 0xe
	.ascii	"buf\000"
	.byte	0x1
	.byte	0x53
	.4byte	0x1f1
	.uleb128 0xf
	.4byte	.LASF28
	.byte	0x1
	.byte	0x53
	.4byte	0xd4
	.uleb128 0xe
	.ascii	"bAC\000"
	.byte	0x1
	.byte	0x53
	.4byte	0xa3
	.uleb128 0xf
	.4byte	.LASF31
	.byte	0x1
	.byte	0x53
	.4byte	0xc9
	.uleb128 0xf
	.4byte	.LASF32
	.byte	0x1
	.byte	0x53
	.4byte	0xa3
	.uleb128 0xf
	.4byte	.LASF33
	.byte	0x1
	.byte	0x53
	.4byte	0xff
	.uleb128 0xe
	.ascii	"bEn\000"
	.byte	0x1
	.byte	0x53
	.4byte	0xa3
	.uleb128 0x10
	.ascii	"err\000"
	.byte	0x1
	.byte	0x55
	.4byte	0xd4
	.uleb128 0x11
	.4byte	.LASF50
	.byte	0x1
	.byte	0xa6
	.byte	0
	.uleb128 0x4
	.byte	0x4
	.4byte	0xdf
	.uleb128 0x12
	.4byte	0x180
	.4byte	.LFB204
	.4byte	.LFE204
	.4byte	.LLST0
	.4byte	0x253
	.uleb128 0x13
	.4byte	0x191
	.4byte	.LLST1
	.uleb128 0x13
	.4byte	0x19c
	.4byte	.LLST2
	.uleb128 0x13
	.4byte	0x1b2
	.4byte	.LLST3
	.uleb128 0x13
	.4byte	0x1bd
	.4byte	.LLST4
	.uleb128 0x14
	.4byte	0x1c8
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.uleb128 0x14
	.4byte	0x1d3
	.byte	0x2
	.byte	0x91
	.sleb128 4
	.uleb128 0x15
	.4byte	0x1de
	.4byte	.LLST5
	.uleb128 0x16
	.4byte	0x1e9
	.4byte	.L4
	.byte	0
	.uleb128 0x17
	.byte	0x1
	.4byte	.LASF34
	.byte	0x1
	.byte	0xb6
	.byte	0x1
	.4byte	0x81
	.4byte	.LFB200
	.4byte	.LFE200
	.4byte	.LLST6
	.4byte	0x2bb
	.uleb128 0x18
	.ascii	"buf\000"
	.byte	0x1
	.byte	0xb6
	.4byte	0x81
	.4byte	.LLST7
	.uleb128 0x19
	.4byte	.LASF28
	.byte	0x1
	.byte	0xb6
	.4byte	0x34
	.4byte	.LLST8
	.uleb128 0x18
	.ascii	"bAC\000"
	.byte	0x1
	.byte	0xb6
	.4byte	0x49
	.4byte	.LLST9
	.uleb128 0x19
	.4byte	.LASF31
	.byte	0x1
	.byte	0xb6
	.4byte	0xc9
	.4byte	.LLST10
	.uleb128 0x1a
	.4byte	.LASF32
	.byte	0x1
	.byte	0xb6
	.4byte	0x49
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.byte	0
	.uleb128 0x17
	.byte	0x1
	.4byte	.LASF35
	.byte	0x1
	.byte	0xbf
	.byte	0x1
	.4byte	0x81
	.4byte	.LFB201
	.4byte	.LFE201
	.4byte	.LLST11
	.4byte	0x323
	.uleb128 0x18
	.ascii	"buf\000"
	.byte	0x1
	.byte	0xbf
	.4byte	0x81
	.4byte	.LLST12
	.uleb128 0x19
	.4byte	.LASF28
	.byte	0x1
	.byte	0xbf
	.4byte	0x34
	.4byte	.LLST13
	.uleb128 0x18
	.ascii	"bAC\000"
	.byte	0x1
	.byte	0xbf
	.4byte	0x49
	.4byte	.LLST14
	.uleb128 0x19
	.4byte	.LASF31
	.byte	0x1
	.byte	0xbf
	.4byte	0xc9
	.4byte	.LLST15
	.uleb128 0x1a
	.4byte	.LASF32
	.byte	0x1
	.byte	0xbf
	.4byte	0x49
	.byte	0x2
	.byte	0x91
	.sleb128 0
	.byte	0
	.uleb128 0x1b
	.byte	0x1
	.4byte	.LASF51
	.byte	0x1
	.byte	0xc7
	.byte	0x1
	.4byte	0x34
	.4byte	.LFB202
	.4byte	.LFE202
	.4byte	.LLST16
	.uleb128 0x17
	.byte	0x1
	.4byte	.LASF36
	.byte	0x1
	.byte	0xcf
	.byte	0x1
	.4byte	0x34
	.4byte	.LFB203
	.4byte	.LFE203
	.4byte	.LLST17
	.4byte	0x393
	.uleb128 0x19
	.4byte	.LASF37
	.byte	0x1
	.byte	0xcf
	.4byte	0x81
	.4byte	.LLST18
	.uleb128 0x19
	.4byte	.LASF28
	.byte	0x1
	.byte	0xcf
	.4byte	0x34
	.4byte	.LLST19
	.uleb128 0x1c
	.4byte	.LASF38
	.byte	0x1
	.byte	0xd1
	.4byte	0x175
	.byte	0x2
	.byte	0x91
	.sleb128 -52
	.uleb128 0x1d
	.ascii	"i\000"
	.byte	0x1
	.byte	0xd2
	.4byte	0xd4
	.4byte	.LLST20
	.byte	0
	.uleb128 0xb
	.4byte	0xd4
	.4byte	0x3a3
	.uleb128 0xc
	.4byte	0x34
	.byte	0x7
	.byte	0
	.uleb128 0x1c
	.4byte	.LASF39
	.byte	0x1
	.byte	0x46
	.4byte	0x3b4
	.byte	0x5
	.byte	0x3
	.4byte	g_HACC_CFG_1
	.uleb128 0x1e
	.4byte	0x393
	.uleb128 0x1c
	.4byte	.LASF40
	.byte	0x1
	.byte	0x4b
	.4byte	0x3ca
	.byte	0x5
	.byte	0x3
	.4byte	g_HACC_CFG_2
	.uleb128 0x1e
	.4byte	0x393
	.uleb128 0x1f
	.4byte	.LASF41
	.byte	0x6
	.byte	0xc
	.4byte	0x34
	.byte	0x1
	.byte	0x1
	.uleb128 0xb
	.4byte	0x5e
	.4byte	0x3e7
	.uleb128 0x20
	.byte	0
	.uleb128 0x1f
	.4byte	.LASF42
	.byte	0x7
	.byte	0x22
	.4byte	0x3dc
	.byte	0x1
	.byte	0x1
	.uleb128 0xb
	.4byte	0x3b
	.4byte	0x3ff
	.uleb128 0x20
	.byte	0
	.uleb128 0x21
	.4byte	.LASF43
	.byte	0x8
	.2byte	0x1af
	.4byte	0x40d
	.byte	0x1
	.byte	0x1
	.uleb128 0x1e
	.4byte	0x3f4
	.uleb128 0x22
	.4byte	.LASF44
	.byte	0x1
	.byte	0x40
	.4byte	0xa3
	.byte	0x1
	.byte	0x5
	.byte	0x3
	.4byte	bHACC_HWWrapKeyInit
	.uleb128 0x22
	.4byte	.LASF45
	.byte	0x1
	.byte	0x41
	.4byte	0xa3
	.byte	0x1
	.byte	0x5
	.byte	0x3
	.4byte	bHACC_SWKeyInit
	.byte	0
	.section	.debug_abbrev,"",%progbits
.Ldebug_abbrev0:
	.uleb128 0x1
	.uleb128 0x11
	.byte	0x1
	.uleb128 0x25
	.uleb128 0xe
	.uleb128 0x13
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1b
	.uleb128 0xe
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x10
	.uleb128 0x6
	.uleb128 0x2134
	.uleb128 0x6
	.uleb128 0x2135
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x2
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0xe
	.byte	0
	.byte	0
	.uleb128 0x3
	.uleb128 0x24
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3e
	.uleb128 0xb
	.uleb128 0x3
	.uleb128 0x8
	.byte	0
	.byte	0
	.uleb128 0x4
	.uleb128 0xf
	.byte	0
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x5
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x6
	.uleb128 0x16
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x7
	.uleb128 0x4
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x8
	.uleb128 0x28
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x1c
	.uleb128 0xd
	.byte	0
	.byte	0
	.uleb128 0x9
	.uleb128 0x13
	.byte	0x1
	.uleb128 0xb
	.uleb128 0xb
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xa
	.uleb128 0xd
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x38
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0xb
	.uleb128 0x1
	.byte	0x1
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xc
	.uleb128 0x21
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2f
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0xd
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x20
	.uleb128 0xb
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xe
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0xf
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x10
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x11
	.uleb128 0xa
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.byte	0
	.byte	0
	.uleb128 0x12
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x13
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x14
	.uleb128 0x5
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x15
	.uleb128 0x34
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x16
	.uleb128 0xa
	.byte	0
	.uleb128 0x31
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.byte	0
	.byte	0
	.uleb128 0x17
	.uleb128 0x2e
	.byte	0x1
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.uleb128 0x1
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x18
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x19
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x1a
	.uleb128 0x5
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x1b
	.uleb128 0x2e
	.byte	0
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x27
	.uleb128 0xc
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x11
	.uleb128 0x1
	.uleb128 0x12
	.uleb128 0x1
	.uleb128 0x40
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x1c
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.uleb128 0x1d
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0x8
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x2
	.uleb128 0x6
	.byte	0
	.byte	0
	.uleb128 0x1e
	.uleb128 0x26
	.byte	0
	.uleb128 0x49
	.uleb128 0x13
	.byte	0
	.byte	0
	.uleb128 0x1f
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0
	.byte	0
	.uleb128 0x20
	.uleb128 0x21
	.byte	0
	.byte	0
	.byte	0
	.uleb128 0x21
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0x5
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x3c
	.uleb128 0xc
	.byte	0
	.byte	0
	.uleb128 0x22
	.uleb128 0x34
	.byte	0
	.uleb128 0x3
	.uleb128 0xe
	.uleb128 0x3a
	.uleb128 0xb
	.uleb128 0x3b
	.uleb128 0xb
	.uleb128 0x49
	.uleb128 0x13
	.uleb128 0x3f
	.uleb128 0xc
	.uleb128 0x2
	.uleb128 0xa
	.byte	0
	.byte	0
	.byte	0
	.section	.debug_loc,"",%progbits
.Ldebug_loc0:
.LLST0:
	.4byte	.LFB204-.Ltext0
	.4byte	.LCFI0-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI0-.Ltext0
	.4byte	.LCFI2-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI2-.Ltext0
	.4byte	.LFE204-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST1:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL2-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL2-.Ltext0
	.4byte	.LFE204-.Ltext0
	.2byte	0x1
	.byte	0x54
	.4byte	0
	.4byte	0
.LLST2:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL2-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL2-.Ltext0
	.4byte	.LFE204-.Ltext0
	.2byte	0x1
	.byte	0x56
	.4byte	0
	.4byte	0
.LLST3:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL2-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL9-.Ltext0
	.4byte	.LVL10-1-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	0
	.4byte	0
.LLST4:
	.4byte	.LVL0-.Ltext0
	.4byte	.LVL2-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	.LVL9-.Ltext0
	.4byte	.LVL10-1-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	0
	.4byte	0
.LLST5:
	.4byte	.LVL1-.Ltext0
	.4byte	.LVL2-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL3-.Ltext0
	.4byte	.LVL4-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	.LVL5-.Ltext0
	.4byte	.LVL6-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL7-.Ltext0
	.4byte	.LVL8-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	.LVL9-.Ltext0
	.4byte	.LVL10-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL10-.Ltext0
	.4byte	.LVL11-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	0
	.4byte	0
.LLST6:
	.4byte	.LFB200-.Ltext0
	.4byte	.LCFI4-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI4-.Ltext0
	.4byte	.LCFI6-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI6-.Ltext0
	.4byte	.LFE200-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST7:
	.4byte	.LVL12-.Ltext0
	.4byte	.LVL15-1-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	0
	.4byte	0
.LLST8:
	.4byte	.LVL12-.Ltext0
	.4byte	.LVL15-1-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	0
	.4byte	0
.LLST9:
	.4byte	.LVL12-.Ltext0
	.4byte	.LVL13-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	0
	.4byte	0
.LLST10:
	.4byte	.LVL12-.Ltext0
	.4byte	.LVL14-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	.LVL14-.Ltext0
	.4byte	.LVL15-1-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	0
	.4byte	0
.LLST11:
	.4byte	.LFB201-.Ltext0
	.4byte	.LCFI8-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI8-.Ltext0
	.4byte	.LCFI10-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI10-.Ltext0
	.4byte	.LFE201-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST12:
	.4byte	.LVL16-.Ltext0
	.4byte	.LVL19-1-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	0
	.4byte	0
.LLST13:
	.4byte	.LVL16-.Ltext0
	.4byte	.LVL19-1-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	0
	.4byte	0
.LLST14:
	.4byte	.LVL16-.Ltext0
	.4byte	.LVL17-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	0
	.4byte	0
.LLST15:
	.4byte	.LVL16-.Ltext0
	.4byte	.LVL18-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	.LVL18-.Ltext0
	.4byte	.LVL19-1-.Ltext0
	.2byte	0x1
	.byte	0x52
	.4byte	0
	.4byte	0
.LLST16:
	.4byte	.LFB202-.Ltext0
	.4byte	.LCFI12-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI12-.Ltext0
	.4byte	.LCFI14-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI14-.Ltext0
	.4byte	.LFE202-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST17:
	.4byte	.LFB203-.Ltext0
	.4byte	.LCFI15-.Ltext0
	.2byte	0x2
	.byte	0x7d
	.sleb128 0
	.4byte	.LCFI15-.Ltext0
	.4byte	.LCFI17-.Ltext0
	.2byte	0x2
	.byte	0x7c
	.sleb128 0
	.4byte	.LCFI17-.Ltext0
	.4byte	.LFE203-.Ltext0
	.2byte	0x2
	.byte	0x7b
	.sleb128 4
	.4byte	0
	.4byte	0
.LLST18:
	.4byte	.LVL20-.Ltext0
	.4byte	.LVL22-.Ltext0
	.2byte	0x1
	.byte	0x50
	.4byte	0
	.4byte	0
.LLST19:
	.4byte	.LVL20-.Ltext0
	.4byte	.LVL23-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	.LVL24-.Ltext0
	.4byte	.LVL25-.Ltext0
	.2byte	0x1
	.byte	0x51
	.4byte	0
	.4byte	0
.LLST20:
	.4byte	.LVL21-.Ltext0
	.4byte	.LVL23-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL24-.Ltext0
	.4byte	.LVL26-.Ltext0
	.2byte	0x2
	.byte	0x30
	.byte	0x9f
	.4byte	.LVL27-.Ltext0
	.4byte	.LVL28-.Ltext0
	.2byte	0x1
	.byte	0x53
	.4byte	0
	.4byte	0
	.section	.debug_pubnames,"",%progbits
.Ldebug_pubnames0:
	.4byte	0xe0
	.2byte	0x2
	.4byte	.Ldebug_info0
	.4byte	0x437
	.4byte	0x1f7
	.ascii	"sp_hacc_internal.isra.0\000"
	.4byte	0x253
	.ascii	"masp_hal_sp_hacc_enc\000"
	.4byte	0x2bb
	.ascii	"masp_hal_sp_hacc_dec\000"
	.4byte	0x323
	.ascii	"masp_hal_sp_hacc_blk_sz\000"
	.4byte	0x33c
	.ascii	"masp_hal_sp_hacc_init\000"
	.4byte	0x3a3
	.ascii	"g_HACC_CFG_1\000"
	.4byte	0x3b9
	.ascii	"g_HACC_CFG_2\000"
	.4byte	0x412
	.ascii	"bHACC_HWWrapKeyInit\000"
	.4byte	0x424
	.ascii	"bHACC_SWKeyInit\000"
	.4byte	0
	.section	.debug_pubtypes,"",%progbits
.Ldebug_pubtypes0:
	.4byte	0x133
	.2byte	0x2
	.4byte	.Ldebug_info0
	.4byte	0x437
	.4byte	0x2d
	.ascii	"long unsigned int\000"
	.4byte	0x34
	.ascii	"unsigned int\000"
	.4byte	0x3b
	.ascii	"char\000"
	.4byte	0x42
	.ascii	"signed char\000"
	.4byte	0x49
	.ascii	"unsigned char\000"
	.4byte	0x50
	.ascii	"short int\000"
	.4byte	0x57
	.ascii	"short unsigned int\000"
	.4byte	0x5e
	.ascii	"int\000"
	.4byte	0x65
	.ascii	"long long int\000"
	.4byte	0x6c
	.ascii	"long long unsigned int\000"
	.4byte	0x73
	.ascii	"long int\000"
	.4byte	0x7a
	.ascii	"_Bool\000"
	.4byte	0x87
	.ascii	"U8\000"
	.4byte	0x91
	.ascii	"U32\000"
	.4byte	0x9c
	.ascii	"float\000"
	.4byte	0xa3
	.ascii	"BOOL\000"
	.4byte	0xc9
	.ascii	"HACC_USER\000"
	.4byte	0xd4
	.ascii	"uint32\000"
	.4byte	0xdf
	.ascii	"uchar\000"
	.4byte	0xff
	.ascii	"AES_OPS\000"
	.4byte	0x175
	.ascii	"AES_KEY_SEED\000"
	.4byte	0
	.section	.debug_aranges,"",%progbits
	.4byte	0x1c
	.2byte	0x2
	.4byte	.Ldebug_info0
	.byte	0x4
	.byte	0
	.2byte	0
	.2byte	0
	.4byte	.Ltext0
	.4byte	.Letext0-.Ltext0
	.4byte	0
	.4byte	0
	.section	.debug_line,"",%progbits
.Ldebug_line0:
	.section	.debug_str,"MS",%progbits,1
.LASF42:
	.ascii	"console_printk\000"
.LASF45:
	.ascii	"bHACC_SWKeyInit\000"
.LASF33:
	.ascii	"aes_type\000"
.LASF12:
	.ascii	"BOOL\000"
.LASF6:
	.ascii	"short unsigned int\000"
.LASF32:
	.ascii	"bDoLock\000"
.LASF44:
	.ascii	"bHACC_HWWrapKeyInit\000"
.LASF40:
	.ascii	"g_HACC_CFG_2\000"
.LASF31:
	.ascii	"user\000"
.LASF13:
	.ascii	"HACC_USER1\000"
.LASF11:
	.ascii	"float\000"
.LASF8:
	.ascii	"long long unsigned int\000"
.LASF49:
	.ascii	"sp_hacc_internal\000"
.LASF4:
	.ascii	"unsigned char\000"
.LASF34:
	.ascii	"masp_hal_sp_hacc_enc\000"
.LASF0:
	.ascii	"long unsigned int\000"
.LASF22:
	.ascii	"AES_KEY_128\000"
.LASF46:
	.ascii	"GNU C 4.6.x-google 20120106 (prerelease)\000"
.LASF15:
	.ascii	"HACC_USER3\000"
.LASF21:
	.ascii	"AES_OPS\000"
.LASF18:
	.ascii	"uchar\000"
.LASF39:
	.ascii	"g_HACC_CFG_1\000"
.LASF41:
	.ascii	"elf_hwcap\000"
.LASF5:
	.ascii	"short int\000"
.LASF17:
	.ascii	"uint32\000"
.LASF36:
	.ascii	"masp_hal_sp_hacc_init\000"
.LASF1:
	.ascii	"unsigned int\000"
.LASF28:
	.ascii	"size\000"
.LASF2:
	.ascii	"char\000"
.LASF25:
	.ascii	"AES_SW_KEY\000"
.LASF37:
	.ascii	"sec_seed\000"
.LASF35:
	.ascii	"masp_hal_sp_hacc_dec\000"
.LASF16:
	.ascii	"HACC_USER\000"
.LASF19:
	.ascii	"AES_DEC\000"
.LASF47:
	.ascii	"ALPS_SW/T"
	.ascii	"RUNK/ALPS.JB2/alps/kernel/mediatek/platform/mt6582/"
	.ascii	"kernel/drivers/masp/mach/hacc_export.c\000"
.LASF7:
	.ascii	"long long int\000"
.LASF23:
	.ascii	"AES_KEY_192\000"
.LASF30:
	.ascii	"AES_KEY_SEED\000"
.LASF50:
	.ascii	"_err\000"
.LASF26:
	.ascii	"AES_HW_KEY\000"
.LASF20:
	.ascii	"AES_ENC\000"
.LASF27:
	.ascii	"AES_HW_WRAP_KEY\000"
.LASF43:
	.ascii	"hex_asc\000"
.LASF29:
	.ascii	"seed\000"
.LASF9:
	.ascii	"long int\000"
.LASF51:
	.ascii	"masp_hal_sp_hacc_blk_sz\000"
.LASF38:
	.ascii	"keyseed\000"
.LASF3:
	.ascii	"signed char\000"
.LASF48:
	.ascii	"ALPS_SW/T"
	.ascii	"RUNK/ALPS.JB2/alps/kernel/out\000"
.LASF14:
	.ascii	"HACC_USER2\000"
.LASF24:
	.ascii	"AES_KEY_256\000"
.LASF10:
	.ascii	"_Bool\000"
	.ident	"GCC: (GNU) 4.6.x-google 20120106 (prerelease)"
	.section	.note.GNU-stack,"",%progbits
