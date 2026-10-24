== tac ==
    n = 27
    steps = 0
L0:
    $t0 = n != 1
    ifz $t0 goto L1
    $t1 = n % 2
    $t2 = $t1 == 0
    ifz $t2 goto L2
    n = n / 2
    goto L3
L2:
    $t4 = 3 * n
    n = $t4 + 1
L3:
    steps = steps + 1
    goto L0
L1:
    print steps
--- stderr ---
--- exit: 0 ---
