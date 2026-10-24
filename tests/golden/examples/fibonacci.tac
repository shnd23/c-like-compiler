== tac ==
    a = 0
    b = 1
    i = 0
L0:
    $t0 = i < 15
    ifz $t0 goto L1
    print a
    next = a + b
    a = b
    b = next
    i = i + 1
    goto L0
L1:
--- stderr ---
--- exit: 0 ---
