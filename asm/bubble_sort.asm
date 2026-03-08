# bubble_sort.asm - sorts 8 integers in memory (array), expects data_base = 256
array: .word 8,5,3,9,1,7,2,4
n:     .word 8

addi x1, x0, 256    # base address of array
lw   x6, n(x0)      # load n

addi x2, x0, 0      # i = 0

outer:
  addi x3, x0, 0    # j = 0

inner:
  # load a[j] into x4
  add x7, x3, x3
  add x7, x7, x7
  add x7, x1, x7
  lw  x4, 0(x7)

  # load a[j+1] into x5
  addi x3, x3, 1
  add x7, x3, x3
  add x7, x7, x7
  add x7, x1, x7
  lw  x5, 0(x7)
  addi x3, x3, -1

  # if a[j+1] < a[j] then swap
  slt x7, x5, x4
  bne x7, x0, do_swap

  # advance j; if j < n-1 then continue inner
  addi x3, x3, 1
  addi x8, x6, -1     # calculate n - 1
  slt x8, x3, x8      # is j < n - 1?
  bne x8, x0, inner

  # finished one pass, i++
  addi x2, x2, 1
  addi x8, x6, -1     # calculate n - 1
  bne x2, x8, outer

  jal x0, done

do_swap:
  # compute addr of a[j]
  add x7, x3, x3
  add x7, x7, x7
  add x7, x1, x7
  sw  x4, 4(x7)     # a[j+1] = a[j]
  sw  x5, 0(x7)     # a[j]   = a[j+1]

  # advance j and check
  addi x3, x3, 1
  addi x8, x6, -1     # calculate n - 1
  slt x8, x3, x8      # is j < n - 1?
  bne x8, x0, inner

  # finished one pass, i++
  addi x2, x2, 1
  addi x8, x6, -1     # calculate n - 1
  bne x2, x8, outer

done:
