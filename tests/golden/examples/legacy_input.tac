== tac ==
    counter = 5
    total = 0.0
    total = 10.5
L2:
    ifz counter goto L3
    $t2 = counter * 2
    $t3 = float($t2)
    total = total + $t3
    counter = counter - 1
    goto L2
L3:
    print total
--- stderr ---
--- exit: 0 ---
