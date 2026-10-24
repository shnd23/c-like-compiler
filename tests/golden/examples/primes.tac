== tac ==
    n = 2
L0:
    $t0 = n < 50
    ifz $t0 goto L1
    divisor = 2
    is_prime = 1
L2:
    $t2 = divisor * divisor
    $t3 = $t2 <= n
    ifz $t3 goto L4
    ifz is_prime goto L4
    $t1 = 1
    goto L5
L4:
    $t1 = 0
L5:
    ifz $t1 goto L3
    $t4 = n % divisor
    $t5 = $t4 == 0
    ifz $t5 goto L6
    is_prime = 0
L6:
    divisor = divisor + 1
    goto L2
L3:
    ifz is_prime goto L7
    print n
L7:
    n = n + 1
    goto L0
L1:
--- stderr ---
--- exit: 0 ---
