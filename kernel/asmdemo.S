.section .text
.global sum_to
# sum_to(n)
# expects argument in a0
# returns result in a0
sum_to:

  mv t0, a0          # t0 <- a0
  li a0, 0           # a0 <- 0
loop:
  add a0, a0, t0     # a0 <- a0 + t0
  addi t0, t0, -1    # t0 <- t0 - 1
  bnez t0, loop      # if t0 != 0: pc <- loop
  ret



.global sum_then_double

sum_then_double:
  addi sp, sp, -16
  sd ra, 0(sp)

  call sum_to
  li t0, 2
  mul a0, a0, t0

  ld ra, 0(sp)
  addi sp, sp, 16

  ret