#define __ARM_ARCH__ __LINUX_ARM_ARCH__

#include <linux/linkage.h>

#if __ARM_ARCH__>=7

.text

# AES assembly implementation for ARMv8 AArch32
#   - aes_v8_cbc_encrypt
#   - aes_v8_cbc_decrypt

.align 5
rcon:
.long   0x00000001,0x00000001,0x00000001,0x00000001
.long   0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d,0x0c0f0e0d
.long   0x0000001b,0x0000001b,0x0000001b,0x0000001b

# void aes_v8_cbc_encrypt(u8 out[], u8 const in[], u8 const rk[],
#      int rounds, int blocks, u8 iv[], int first);
.align  5
ENTRY(aes_v8_cbc_encrypt)
    push      {r4, r5, r6}
    vpush     {q4, q5, q6, q7}
    ldr       r4, [sp, #76]     // blocks
    ldr       r5, [sp, #80]     // iv
    ldr       r6, [sp, #84]     // first

    cmp       r6, #0
    beq       .LcbcencDispatch
    vld1.8    {d2-d3}, [r5]
.LcbcencDispatch:
    cmp       r3, #12
    bhi       .Lcbcenc256
    beq       .Lcbcenc192

.Lcbcenc128:
    // Load round keys
    vld1.8    {d4-d5}, [r2]!
    vld1.8    {d6-d7}, [r2]!
    vld1.8    {d8-d9}, [r2]!
    vld1.8    {d10-d11}, [r2]!
    vld1.8    {d12-d13}, [r2]!
    vld1.8    {d14-d15}, [r2]!
    vld1.8    {d16-d17}, [r2]!
    vld1.8    {d18-d19}, [r2]!
    vld1.8    {d20-d21}, [r2]!
    vld1.8    {d22-d23}, [r2]!
    vld1.8    {d24-d25}, [r2]
.Lcbcenc128Loop:
    // load input 16 bytes, and eor with iv
    vld1.8    {d0-d1}, [r1]!
    veor.8    q0, q0, q1
    pld       [r1, #16]
    // aes kernel
    aese.8    q0, q2
    aesmc.8   q0, q0
    aese.8    q0, q3
    aesmc.8   q0, q0
    aese.8    q0, q4
    aesmc.8   q0, q0
    aese.8    q0, q5
    aesmc.8   q0, q0
    aese.8    q0, q6
    aesmc.8   q0, q0
    aese.8    q0, q7
    aesmc.8   q0, q0
    aese.8    q0, q8
    aesmc.8   q0, q0
    aese.8    q0, q9
    aesmc.8   q0, q0
    aese.8    q0, q10
    aesmc.8   q0, q0
    aese.8    q0, q11
    veor.8    q1, q0, q12
    // store output 16 bytes, and continue next round
    vst1.8    {d2-d3}, [r0]!
    subs      r4, r4, #1
    bne       .Lcbcenc128Loop
.Lcbcenc128Done:
    vpop      {q4, q5, q6, q7}
    pop       {r4, r5, r6}
    bx        lr

.Lcbcenc192:
    // Load round keys
    vld1.8    {d4-d5}, [r2]!
    vld1.8    {d6-d7}, [r2]!
    vld1.8    {d8-d9}, [r2]!
    vld1.8    {d10-d11}, [r2]!
    vld1.8    {d12-d13}, [r2]!
    vld1.8    {d14-d15}, [r2]!
    vld1.8    {d16-d17}, [r2]!
    vld1.8    {d18-d19}, [r2]!
    vld1.8    {d20-d21}, [r2]!
    vld1.8    {d22-d23}, [r2]!
    vld1.8    {d24-d25}, [r2]!
    vld1.8    {d26-d27}, [r2]!
    vld1.8    {d28-d29}, [r2]
.Lcbcenc192Loop:
    // load input 16 bytes, and eor with iv
    vld1.8    {d0-d1}, [r1]!
    veor.8    q0, q0, q1
    pld       [r1, #16]
    // aes kernel
    aese.8    q0, q2
    aesmc.8   q0, q0
    aese.8    q0, q3
    aesmc.8   q0, q0
    aese.8    q0, q4
    aesmc.8   q0, q0
    aese.8    q0, q5
    aesmc.8   q0, q0
    aese.8    q0, q6
    aesmc.8   q0, q0
    aese.8    q0, q7
    aesmc.8   q0, q0
    aese.8    q0, q8
    aesmc.8   q0, q0
    aese.8    q0, q9
    aesmc.8   q0, q0
    aese.8    q0, q10
    aesmc.8   q0, q0
    aese.8    q0, q11
    aesmc.8   q0, q0
    aese.8    q0, q12
    aesmc.8   q0, q0
    aese.8    q0, q13
    veor.8    q1, q0, q14
    // store output 16 bytes, and continue next round
    vst1.8    {d2-d3}, [r0]!
    subs      r4, r4, #1
    bne       .Lcbcenc192Loop
.Lcbcenc192Done:
    vpop      {q4, q5, q6, q7}
    pop       {r4, r5, r6}
    bx        lr

.Lcbcenc256:
    // Load round keys
    vld1.8    {d4-d5}, [r2]!
    vld1.8    {d6-d7}, [r2]!
    vld1.8    {d8-d9}, [r2]!
    vld1.8    {d10-d11}, [r2]!
    vld1.8    {d12-d13}, [r2]!
    vld1.8    {d14-d15}, [r2]!
    vld1.8    {d16-d17}, [r2]!
    vld1.8    {d18-d19}, [r2]!
    vld1.8    {d20-d21}, [r2]!
    vld1.8    {d22-d23}, [r2]!
    vld1.8    {d24-d25}, [r2]!
    vld1.8    {d26-d27}, [r2]!
    vld1.8    {d28-d29}, [r2]!
    mov       r3, r2
.Lcbcenc256Loop:
    // load input 16 bytes, and eor with iv
    vld1.8    {d0-d1}, [r1]!
    veor.8    q0, q0, q1
    pld       [r1, #16]
    // aes kernel
    aese.8    q0, q2
    aesmc.8   q0, q0
    aese.8    q0, q3
    aesmc.8   q0, q0
    vld1.8    {d30}, [r2]!
    aese.8    q0, q4
    aesmc.8   q0, q0
    aese.8    q0, q5
    aesmc.8   q0, q0
    vld1.8    {d31}, [r2]!
    aese.8    q0, q6
    aesmc.8   q0, q0
    aese.8    q0, q7
    aesmc.8   q0, q0
    aese.8    q0, q8
    aesmc.8   q0, q0
    aese.8    q0, q9
    aesmc.8   q0, q0
    aese.8    q0, q10
    aesmc.8   q0, q0
    aese.8    q0, q11
    aesmc.8   q0, q0
    aese.8    q0, q12
    aesmc.8   q0, q0
    aese.8    q0, q13
    aesmc.8   q0, q0
    aese.8    q0, q14
    aesmc.8   q0, q0
    aese.8    q0, q15
    vld1.8    {d30-d31}, [r2]
    veor.8    q1, q0, q15
    mov       r2, r3
    // store output 16 bytes, and continue next round
    vst1.8    {d2-d3}, [r0]!
    subs      r4, r4, #1
    bne       .Lcbcenc256Loop
.Lcbcenc256Done:
    vpop      {q4, q5, q6, q7}
    pop       {r4, r5, r6}
    bx        lr
ENDPROC(aes_v8_cbc_encrypt)


# void aes_v8_cbc_decrypt(u8 out[], u8 const in[], u8 const rk[],
#      int rounds, int blocks, u8 iv[], int first);
.align  5
ENTRY(aes_v8_cbc_decrypt)
    push      {r4, r5, r6}
    vpush     {q4, q5, q6, q7}
    ldr       r4, [sp, #76]     // blocks
    ldr       r5, [sp, #80]     // iv
    ldr       r6, [sp, #84]     // first

    cmp       r6, #0
    beq       .LcbcdecDispatch
    vld1.8    {d4-d5}, [r5]
.LcbcdecDispatch:
    cmp       r3, #12
    bhi       .Lcbcdec256
    beq       .Lcbcdec192

.Lcbcdec128:
    // Load round keys
    vld1.8    {d10-d11}, [r2]!
    vld1.8    {d12-d13}, [r2]!
    vld1.8    {d14-d15}, [r2]!
    vld1.8    {d16-d17}, [r2]!
    vld1.8    {d18-d19}, [r2]!
    vld1.8    {d20-d21}, [r2]!
    vld1.8    {d22-d23}, [r2]!
    vld1.8    {d24-d25}, [r2]!
    vld1.8    {d26-d27}, [r2]!
    vld1.8    {d28-d29}, [r2]!
    vld1.8    {d30-d31}, [r2]!
    // Sub by 2
    subs      r4, r4, #2
    bmi       .Lcbcdec128_1X
.Lcbcdec128_2XLoop:
    // Load input 32 bytes
    vld1.8    {d0-d3}, [r1]!
    pld       [r1, #32]
    vmov      q3, q0
    vmov      q4, q1
    // aes kernel
    aesd.8    q0, q5
    aesimc.8  q0, q0
    aesd.8    q1, q5
    aesimc.8  q1, q1
    aesd.8    q0, q6
    aesimc.8  q0, q0
    aesd.8    q1, q6
    aesimc.8  q1, q1
    aesd.8    q0, q7
    aesimc.8  q0, q0
    aesd.8    q1, q7
    aesimc.8  q1, q1
    aesd.8    q0, q8
    aesimc.8  q0, q0
    aesd.8    q1, q8
    aesimc.8  q1, q1
    aesd.8    q0, q9
    aesimc.8  q0, q0
    aesd.8    q1, q9
    aesimc.8  q1, q1
    aesd.8    q0, q10
    aesimc.8  q0, q0
    aesd.8    q1, q10
    aesimc.8  q1, q1
    aesd.8    q0, q11
    aesimc.8  q0, q0
    aesd.8    q1, q11
    aesimc.8  q1, q1
    aesd.8    q0, q12
    aesimc.8  q0, q0
    aesd.8    q1, q12
    aesimc.8  q1, q1
    aesd.8    q0, q13
    aesimc.8  q0, q0
    aesd.8    q1, q13
    aesimc.8  q1, q1
    aesd.8    q0, q14
    aesd.8    q1, q14
    veor.8    q0, q0, q15
    veor.8    q1, q1, q15
    veor.8    q0, q0, q2
    veor.8    q1, q1, q3
    vmov      q2, q4
    vst1.8    {d0-d1}, [r0]!
    vst1.8    {d2-d3}, [r0]!
    subs      r4, r4, #2
    bpl       .Lcbcdec128_2XLoop
.Lcbcdec128_1X:
    adds      r4, r4, #2
    bne       .Lcbcdec128_FinalRound
    vpop      {q4, q5, q6, q7}
    pop       {r4, r5, r6}
    bx        lr
.Lcbcdec128_FinalRound:
    // load input 16 bytes
    vld1.8    {d0-d1}, [r1]!
    // aes kernel
    aesd.8    q0, q5
    aesimc.8  q0, q0
    aesd.8    q0, q6
    aesimc.8  q0, q0
    aesd.8    q0, q7
    aesimc.8  q0, q0
    aesd.8    q0, q8
    aesimc.8  q0, q0
    aesd.8    q0, q9
    aesimc.8  q0, q0
    aesd.8    q0, q10
    aesimc.8  q0, q0
    aesd.8    q0, q11
    aesimc.8  q0, q0
    aesd.8    q0, q12
    aesimc.8  q0, q0
    aesd.8    q0, q13
    aesimc.8  q0, q0
    aesd.8    q0, q14
    veor.8    q0, q0, q15
    veor.8    q0, q0, q2
    vst1.8    {d0-d1}, [r0]!
    vpop      {q4, q5, q6, q7}
    pop       {r4, r5, r6}
    bx        lr

.Lcbcdec192:
    // Load round keys
    vld1.8    {d10-d11}, [r2]!
    vld1.8    {d12-d13}, [r2]!
    vld1.8    {d14-d15}, [r2]!
    vld1.8    {d16-d17}, [r2]!
    vld1.8    {d18-d19}, [r2]!
    vld1.8    {d20-d21}, [r2]!
    vld1.8    {d22-d23}, [r2]!
    vld1.8    {d24-d25}, [r2]!
    vld1.8    {d26-d27}, [r2]!
    vld1.8    {d28-d29}, [r2]!
    mov       r3, r2
    // Sub by 2
    subs      r4, r4, #2
    bmi       .Lcbcdec192_1X
.Lcbcdec192_2XLoop:
    // Load input 32 bytes
    vld1.8    {d0-d3}, [r1]!
    pld       [r1, #32]
    vmov      q3, q0
    vmov      q4, q1
    // aes kernel
    aesd.8    q0, q5
    aesimc.8  q0, q0
    aesd.8    q1, q5
    aesimc.8  q1, q1
    aesd.8    q0, q6
    aesimc.8  q0, q0
    aesd.8    q1, q6
    aesimc.8  q1, q1
    aesd.8    q0, q7
    aesimc.8  q0, q0
    aesd.8    q1, q7
    aesimc.8  q1, q1
    aesd.8    q0, q8
    aesimc.8  q0, q0
    aesd.8    q1, q8
    aesimc.8  q1, q1
    aesd.8    q0, q9
    aesimc.8  q0, q0
    aesd.8    q1, q9
    aesimc.8  q1, q1
    aesd.8    q0, q10
    aesimc.8  q0, q0
    aesd.8    q1, q10
    aesimc.8  q1, q1
    aesd.8    q0, q11
    aesimc.8  q0, q0
    aesd.8    q1, q11
    aesimc.8  q1, q1
    aesd.8    q0, q12
    aesimc.8  q0, q0
    aesd.8    q1, q12
    aesimc.8  q1, q1
    aesd.8    q0, q13
    aesimc.8  q0, q0
    aesd.8    q1, q13
    aesimc.8  q1, q1
    aesd.8    q0, q14
    aesimc.8  q0, q0
    aesd.8    q1, q14
    aesimc.8  q1, q1
    vld1.8    {d30-d31}, [r2]! 
    aesd.8    q0, q15
    aesimc.8  q0, q0
    aesd.8    q1, q15
    aesimc.8  q1, q1
    vld1.8    {d30-d31}, [r2]!
    aesd.8    q0, q15
    aesd.8    q1, q15
    vld1.8    {d30-d31}, [r2]!
    veor.8    q0, q0, q15
    veor.8    q1, q1, q15
    mov       r2, r3
    veor.8    q0, q0, q2
    veor.8    q1, q1, q3
    vmov      q2, q4
    vst1.8    {d0-d1}, [r0]!
    vst1.8    {d2-d3}, [r0]!
    subs      r4, r4, #2
    bpl       .Lcbcdec192_2XLoop
.Lcbcdec192_1X:
    adds      r4, r4, #2
    bne       .Lcbcdec192_FinalRound
    vpop      {q4, q5, q6, q7}
    pop       {r4, r5, r6}
    bx        lr
.Lcbcdec192_FinalRound:
    // load input 16 bytes
    vld1.8    {d0-d1}, [r1]!
    // aes kernel
    aesd.8    q0, q5
    aesimc.8  q0, q0
    vld1.8    {d30}, [r2]!
    aesd.8    q0, q6
    aesimc.8  q0, q0
    vld1.8    {d31}, [r2]!
    aesd.8    q0, q7
    aesimc.8  q0, q0
    aesd.8    q0, q8
    aesimc.8  q0, q0
    aesd.8    q0, q9
    aesimc.8  q0, q0
    aesd.8    q0, q10
    aesimc.8  q0, q0
    aesd.8    q0, q11
    aesimc.8  q0, q0
    aesd.8    q0, q12
    aesimc.8  q0, q0
    aesd.8    q0, q13
    aesimc.8  q0, q0
    aesd.8    q0, q14
    aesimc.8  q0, q0
    aesd.8    q0, q15
    aesimc.8  q0, q0
    vld1.8    {d30-d31}, [r2]!
    aesd.8    q0, q15
    vld1.8    {d30-d31}, [r2]!
    veor.8    q0, q0, q15
    veor.8    q0, q0, q2
    vst1.8    {d0-d1}, [r0]!
    vpop      {q4, q5, q6, q7}
    pop       {r4, r5, r6}
    bx        lr

.Lcbcdec256:
    // Load round keys
    vld1.8    {d10-d11}, [r2]!
    vld1.8    {d12-d13}, [r2]!
    vld1.8    {d14-d15}, [r2]!
    vld1.8    {d16-d17}, [r2]!
    vld1.8    {d18-d19}, [r2]!
    vld1.8    {d20-d21}, [r2]!
    vld1.8    {d22-d23}, [r2]!
    vld1.8    {d24-d25}, [r2]!
    vld1.8    {d26-d27}, [r2]!
    vld1.8    {d28-d29}, [r2]!
    mov       r3, r2
    // Sub by 2
    subs      r4, r4, #2
    bmi       .Lcbcdec256_1X
.Lcbcdec256_2XLoop:
    // Load input 32 bytes
    vld1.8    {d0-d3}, [r1]!
    pld       [r1, #32]
    vmov      q3, q0
    vmov      q4, q1
    // aes kernel
    aesd.8    q0, q5
    aesimc.8  q0, q0
    aesd.8    q1, q5
    aesimc.8  q1, q1
    aesd.8    q0, q6
    aesimc.8  q0, q0
    aesd.8    q1, q6
    aesimc.8  q1, q1
    aesd.8    q0, q7
    aesimc.8  q0, q0
    aesd.8    q1, q7
    aesimc.8  q1, q1
    aesd.8    q0, q8
    aesimc.8  q0, q0
    aesd.8    q1, q8
    aesimc.8  q1, q1
    aesd.8    q0, q9
    aesimc.8  q0, q0
    aesd.8    q1, q9
    aesimc.8  q1, q1
    aesd.8    q0, q10
    aesimc.8  q0, q0
    aesd.8    q1, q10
    aesimc.8  q1, q1
    aesd.8    q0, q11
    aesimc.8  q0, q0
    aesd.8    q1, q11
    aesimc.8  q1, q1
    aesd.8    q0, q12
    aesimc.8  q0, q0
    aesd.8    q1, q12
    aesimc.8  q1, q1
    aesd.8    q0, q13
    aesimc.8  q0, q0
    aesd.8    q1, q13
    aesimc.8  q1, q1
    aesd.8    q0, q14
    aesimc.8  q0, q0
    aesd.8    q1, q14
    aesimc.8  q1, q1
    vld1.8    {d30-d31}, [r2]!
    aesd.8    q0, q15
    aesimc.8  q0, q0
    aesd.8    q1, q15
    aesimc.8  q1, q1
    vld1.8    {d30-d31}, [r2]!
    aesd.8    q0, q15
    aesimc.8  q0, q0
    aesd.8    q1, q15
    aesimc.8  q1, q1
    vld1.8    {d30-d31}, [r2]!
    aesd.8    q0, q15
    aesimc.8  q0, q0
    aesd.8    q1, q15
    aesimc.8  q1, q1
    vld1.8    {d30-d31}, [r2]!
    aesd.8    q0, q15
    aesd.8    q1, q15
    vld1.8    {d30-d31}, [r2]!
    veor.8    q0, q0, q15
    veor.8    q1, q1, q15
    mov       r2, r3
    veor.8    q0, q0, q2
    veor.8    q1, q1, q3
    vmov      q2, q4
    vst1.8    {d0-d1}, [r0]!
    vst1.8    {d2-d3}, [r0]!
    subs      r4, r4, #2
    bpl       .Lcbcdec256_2XLoop
.Lcbcdec256_1X:
    adds      r4, r4, #2
    bne       .Lcbcdec256_FinalRound
    vpop      {q4, q5, q6, q7}
    pop       {r4, r5, r6}
    bx        lr
.Lcbcdec256_FinalRound:
    // load input 16 bytes
    vld1.8    {d0-d1}, [r1]!
    // aes kernel
    aesd.8    q0, q5
    aesimc.8  q0, q0
    vld1.8    {d30}, [r2]!
    aesd.8    q0, q6
    aesimc.8  q0, q0
    vld1.8    {d31}, [r2]!
    aesd.8    q0, q7
    aesimc.8  q0, q0
    aesd.8    q0, q8
    aesimc.8  q0, q0
    aesd.8    q0, q9
    aesimc.8  q0, q0
    aesd.8    q0, q10
    aesimc.8  q0, q0
    aesd.8    q0, q11
    aesimc.8  q0, q0
    aesd.8    q0, q12
    aesimc.8  q0, q0
    aesd.8    q0, q13
    aesimc.8  q0, q0
    aesd.8    q0, q14
    aesimc.8  q0, q0
    aesd.8    q0, q15
    aesimc.8  q0, q0
    vld1.8    {d30-d31}, [r2]!
    aesd.8    q0, q15
    aesimc.8  q0, q0
    vld1.8    {d30-d31}, [r2]!
    aesd.8    q0, q15
    aesimc.8  q0, q0
    vld1.8    {d30-d31}, [r2]!
    aesd.8    q0, q15
    vld1.8    {d30-d31}, [r2]!
    veor.8    q0, q0, q15
    veor.8    q0, q0, q2
    vst1.8    {d0-d1}, [r0]!
    vpop      {q4, q5, q6, q7}
    pop       {r4, r5, r6}
    bx        lr

ENDPROC(aes_v8_cbc_decrypt)

#endif
