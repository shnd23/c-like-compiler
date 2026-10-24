== tac ==
    n = 1
    product = 1
L0:
    $t0 = n <= 20
    ifz $t0 goto L1
    product = product * n
    $t4 = n == 5
    ifnz $t4 goto L4
    $t5 = n == 10
    ifnz $t5 goto L4
    $t3 = 0
    goto L5
L4:
    $t3 = 1
L5:
    ifnz $t3 goto L2
    $t6 = n == 20
    ifnz $t6 goto L2
    $t2 = 0
    goto L3
L2:
    $t2 = 1
L3:
    ifz $t2 goto L6
    print product
L6:
    n = n + 1
    goto L0
L1:
--- stderr ---
--- exit: 0 ---
