== tac ==
    a = 1071
    b = 462
L0:
    $t0 = b != 0
    ifz $t0 goto L1
    r = a % b
    a = b
    b = r
    goto L0
L1:
    print a
--- stderr ---
--- exit: 0 ---
