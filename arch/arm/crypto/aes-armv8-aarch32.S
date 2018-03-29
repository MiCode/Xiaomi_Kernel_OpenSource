/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define __ARM_ARCH__ __LINUX_ARM_ARCH__

#include <linux/linkage.h>

#if __ARM_ARCH__>=7

.fpu crypto-neon-fp-armv8

.text

# AES assembly implementation for ARMv8 AArch32
#   - AES_encrypt_ce
#   - AES_decrypt_ce
#   - private_AES_set_encrypt_key_ce
#   - private_AES_set_decrypt_key_ce

# void AES_encrypt_ce(const unsigned char *in, unsigned char *out,
#      const AES_KEY *key) {
.align  5
ENTRY(AES_encrypt_ce)
    vld1.8      {d24-d25}, [r2]!
    vld1.8      {d0-d1}, [r0]
    ldr         r3, [r2, #240-16]
    vld1.8      {d26-d27}, [r2]!
    aese.8      q0, q12
    aesmc.8     q0, q0
    sub         r3, r3, #4
    vld1.8      {d28-d29}, [r2]!
.LPrivateEncLoop:
    subs        r3, r3, #2
    aese.8      q0, q13
    aesmc.8     q0, q0
    vld1.8      {d26-d27}, [r2]!
    aese.8      q0, q14
    aesmc.8     q0, q0
    vld1.8      {d28-d29}, [r2]!
    bpl         .LPrivateEncLoop
    aese.8      q0, q13
    veor.8      q0, q0, q14
    vst1.8      {d0-d1}, [r1]
    bx          lr
ENDPROC(AES_encrypt_ce)

# void AES_decrypt_ce(const unsigned char *in, unsigned char *out,
#      const AES_KEY *key) {
.align  5
ENTRY(AES_decrypt_ce)
    vld1.8      {d24-d25}, [r2]!
    vld1.8      {d0-d1}, [r0]
    ldr         r3, [r2, #240-16]
    vld1.8      {d26-d27}, [r2]!
    aesd.8      q0, q12
    aesimc.8    q0, q0
    sub         r3, r3, #4
    vld1.8      {d28-d29}, [r2]!
.LPrivateDecLoop:
    subs        r3, r3, #2
    aesd.8      q0, q13
    aesimc.8    q0, q0
    vld1.8      {d26-d27}, [r2]!
    aesd.8      q0, q14
    aesimc.8    q0, q0
    vld1.8      {d28-d29}, [r2]!
    bpl         .LPrivateDecLoop
    aesd.8      q0, q13
    veor.8      q0, q0, q14
    vst1.8      {d0-d1}, [r1]
    bx lr
ENDPROC(AES_decrypt_ce)


.align 5
private_rcon:
.long   0x00000001,0x00000001,0x00000001,0x00000001
.long   0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d
.long   0x0000001b,0x0000001b,0x0000001b,0x0000001b


# int private_AES_set_encrypt_key_ce(const unsigned char *userKey, const int bits,
#      AES_KEY *key) {
.align  5
ENTRY(private_AES_set_encrypt_key_ce)
    adr       r3,  private_rcon
    cmp       r1,  #192
    veor      q0,  q0,  q0
    vld1.8    {q8},[r0]!
    mov       r1,  #8
    vld1.32   {q1,q2},[r3]!
    beq       .LPrivateExpandEnc192
    bgt       .LPrivateExpandEnc256
.LPrivateExpandEnc128:
.LPrivateLoopEnc128:
    vst1.32   {q8},[r2]!
    vtbl.8    d6,  {q8},d4
    vtbl.8    d7,  {q8},d5
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    subs      r1,  r1,  #1
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    bne       .LPrivateLoopEnc128
    vld1.32   {q1},[r3]
    vst1.32   {q8},[r2]!
    vtbl.8    d6,  {q8},d4
    vtbl.8    d7,  {q8},d5
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    vst1.32   {q8},[r2]!
    vtbl.8    d6,  {q8},d4
    vtbl.8    d7,  {q8},d5
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    veor      q3,  q3,  q1
    veor      q8,  q3,  q15
    vst1.32   {q8},[r2]
    add       r2,  r2,  #0x50
    mov       r12, #10
    str       r12, [r2]
    eor       r0,  r0,  r0
    bx        lr

.align  4
.LPrivateExpandEnc192:
    vld1.8    {d18},[r0]
    vmov.i8   q3,  #8
    vst1.32   {q8},[r2]!
    vsub.i8   q2,  q2,  q3
    vst1.32   {d18},[r2]!
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vtbl.8    d6,  {q9},d4
    vtbl.8    d7,  {q9},d5
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    mov       r1,  #3
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    vdup.32   q15, d31[1]
    veor      q15, q15, q9
    vext.8    q14, q0,  q9,  #12
    veor      q15, q15, q14
    vst1.32   {q8},[r2]!
    veor      q9,  q3,  q15
    vst1.32   {d18},[r2]!
.LPrivateLoopEnc192:
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vtbl.8    d6,  {q9},d4
    vtbl.8    d7,  {q9},d5
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    subs      r1,  r1,  #1
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q10, q3,  q15
    vdup.32   q15, d31[1]
    veor      q15, q15, q9
    vext.8    q14, q0,  q9,  #12
    veor      q15, q15, q14
    vst1.32   {q10},[r2]!
    veor      q11, q3,  q15
    vst1.32   {d22},[r2]!
    vext.8    q14, q0,  q10, #12
    veor      q15, q10, q14
    vtbl.8    d6,  {q11},d4
    vtbl.8    d7,  {q11},d5
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    vdup.32   q15, d31[1]
    veor      q15, q15, q11
    vext.8    q14, q0,  q11, #12
    veor      q15, q15, q14
    vst1.32   {q8},[r2]!
    veor      q9,  q3,  q15
    vst1.32   {d18},[r2]!
    bne       .LPrivateLoopEnc192
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vtbl.8    d6,  {q9},d4
    vtbl.8    d7,  {q9},d5
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    veor      q3,  q3,  q1
    veor      q8,  q3,  q15
    vst1.32   {q8},[r2]!
    mov       r12, #12
    add       r2,  r2,  #0x20
    str       r12, [r2]
    eor       r0,  r0,  r0
    bx        lr

.align  4
.LPrivateExpandEnc256:
    vld1.8    {q9},[r0]
    mov       r1,  #6
.LPrivateLoopEnc256:
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vst1.32   {q8},[r2]!
    vtbl.8    d6,  {q9},d4
    vst1.32   {q9},[r2]!
    vtbl.8    d7,  {q9},d5
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    subs      r1,  r1,  #1
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    vext.8    q14, q0,  q9,  #12
    veor      q15, q9,  q14
    vdup.32   q3,  d17[1]
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    veor      q9,  q3,  q15
    bne       .LPrivateLoopEnc256
    vst1.32   {q8},[r2]!
    vtbl.8    d6,  {q9},d4
    vst1.32   {q9},[r2]!
    vtbl.8    d7,  {q9},d5
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    veor      q3,  q3,  q1
    veor      q8,  q3,  q15
    vst1.32   {q8},[r2]!
    mov       r12, #14
    str       r12, [r2]
    eor       r0,  r0,  r0
    bx        lr
ENDPROC(private_AES_set_encrypt_key_ce)


# int private_AES_set_decrypt_key_ce(const unsigned char *userKey, const int bits,
#       AES_KEY *key) {
.align  5
ENTRY(private_AES_set_decrypt_key_ce)
    adr       r3,  private_rcon
    cmp       r1,  #192
    veor      q0,  q0,  q0
    vld1.8    {q8},[r0]!
    add       r2,  r2,  #160
    vld1.32   {q1,q2},[r3]!
    beq       .LPrivateExpandDec192
    bgt       .LPrivateExpandDec256
.LPrivateExpandDec128:
    vst1.32   {q8},[r2]
    vtbl.8    d6,  {q8},d4
    vtbl.8    d7,  {q8},d5
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    sub       r2,  r2,  #16
    aese.8    q3,  q0
    mov       r1,  #7
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    aesimc.8  q12, q8
.LPrivateLoopDec128:
    vst1.32   {q12},[r2]
    vtbl.8    d6,  {q8},d4
    vtbl.8    d7,  {q8},d5
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    sub       r2,  r2,  #16
    aese.8    q3,  q0
    subs      r1,  r1,  #1
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    aesimc.8  q12, q8
    bne       .LPrivateLoopDec128
    vld1.32   {q1},[r3]
    vst1.32   {q12},[r2]
    vtbl.8    d6,  {q8},d4
    vtbl.8    d7,  {q8},d5
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    sub       r2,  r2,  #16
    aese.8    q3,  q0
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    aesimc.8  q12, q8
    vst1.32   {q12},[r2]
    vtbl.8    d6,  {q8},d4
    vtbl.8    d7,  {q8},d5
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    sub       r2,  r2,  #16
    aese.8    q3,  q0
    veor      q3,  q3,  q1
    veor      q8,  q3,  q15
    vst1.32   {q8},[r2]
    add       r2,  r2,  #240
    mov       r12, #10
    str       r12, [r2]
    eor       r0,  r0,  r0
    bx        lr

.align  4
.LPrivateExpandDec192:
    vld1.8    {d18},[r0]
    add       r2,  r2,  #32
    vmov.i8   q3,  #8
    mov       r1,  #3
    vst1.32   {q8},[r2]
    sub       r2,  r2,  #16
    vsub.i8   q2,  q2,  q3
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vtbl.8    d6,  {q9},d4
    vtbl.8    d7,  {q9},d5
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    vmov      d26, d18
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    vdup.32   q15, d31[1]
    vmov      d27, d16
    veor      q15, q15, q9
    aesimc.8  q13, q13
    vmov      d24, d17
    vext.8    q14, q0,  q9,  #12
    veor      q15, q15, q14
    vst1.32   {q13},[r2]
    sub       r2,  r2,  #16
    veor      q9,  q3,  q15
    vmov      d25, d18
.LPrivateLoopDec192:
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    aesimc.8  q12, q12
    vtbl.8    d6,  {q9},d4
    vtbl.8    d7,  {q9},d5
    vst1.32   {q12},[r2]
    sub       r2,  r2,  #16
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    subs      r1,  r1,  #1
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q10, q3,  q15
    vdup.32   q15, d31[1]
    veor      q15, q15, q9
    aesimc.8  q12, q10
    vext.8    q14, q0,  q9,  #12
    veor      q15, q15, q14
    vst1.32   {q12},[r2]
    sub       r2,  r2,  #16
    veor      q11, q3,  q15
    vext.8    q14, q0,  q10, #12
    veor      q15, q10, q14
    vtbl.8    d6,  {q11},d4
    vtbl.8    d7,  {q11},d5
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    vmov      d26, d22
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    vdup.32   q15, d31[1]
    vmov      d27, d16
    veor      q15, q15, q11
    aesimc.8  q13, q13
    vmov      d24, d17
    vext.8    q14, q0,  q11, #12
    veor      q15, q15, q14
    vst1.32   {q13},[r2]
    sub       r2,  r2,  #16
    veor      q9,  q3,  q15
    vmov      d25, d18
    bne       .LPrivateLoopDec192
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    aesimc.8  q12, q12
    vtbl.8    d6,  {q9},d4
    vtbl.8    d7,  {q9},d5
    vst1.32   {q12},[r2]
    sub       r2,  r2,  #16
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    veor      q3,  q3,  q1
    veor      q8,  q3,  q15
    vst1.32   {q8},[r2]
    add       r2,  r2,  #240
    mov       r12, #12
    str       r12, [r2]
    eor       r0,  r0,  r0
    bx        lr

.align  4
.LPrivateExpandDec256:
    vld1.8    {q9},[r0]
    add       r2,  r2,  #80
    vmov      q12, q8
    mov       r1,  #14
    str       r1,  [r2]
    sub       r2,  r2,  #16
    mov       r1,  #6
.LPrivateLoopDec256:
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    aesimc.8  q13, q9
    vst1.32   {q12},[r2]
    sub       r2,  r2,  #16
    vtbl.8    d6,  {q9},d4
    vst1.32   {q13},[r2]
    sub       r2,  r2,  #16
    vtbl.8    d7,  {q9},d5
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    subs      r1,  r1,  #1
    veor      q3,  q3,  q1
    vshl.u8   q1,  q1,  #1
    veor      q8,  q3,  q15
    vext.8    q14, q0,  q9,  #12
    veor      q15, q9,  q14
    aesimc.8  q12, q8
    vdup.32   q3,  d17[1]
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    veor      q9,  q3,  q15
    bne       .LPrivateLoopDec256
    aesimc.8  q13, q9
    vst1.32   {q12},[r2]
    sub       r2,  r2,  #16
    vtbl.8    d6,  {q9},d4
    vst1.32   {q13},[r2]
    sub       r2,  r2,  #16
    vtbl.8    d7,  {q9},d5
    vext.8    q14, q0,  q8,  #12
    veor      q15, q8,  q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    vext.8    q14, q0,  q14, #12
    veor      q15, q15, q14
    aese.8    q3,  q0
    veor      q3,  q3,  q1
    veor      q8,  q3,  q15
    vst1.32   {q8},[r2]
    eor       r0,  r0,  r0
    bx        lr
ENDPROC(private_AES_set_decrypt_key_ce)

.align  2

#endif
