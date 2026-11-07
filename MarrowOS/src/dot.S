.text
.globl marrow_dot
.type marrow_dot,@function
marrow_dot:
    xorps %xmm0, %xmm0
    test %rdx, %rdx
    jz 2f
1:  movss (%rdi), %xmm1
    mulss (%rsi), %xmm1
    addss %xmm1, %xmm0
    add $4, %rdi
    add $4, %rsi
    dec %rdx
    jnz 1b
2:  ret
.size marrow_dot, .-marrow_dot
.section .note.GNU-stack,"",@progbits
