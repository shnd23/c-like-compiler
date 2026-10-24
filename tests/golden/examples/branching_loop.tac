== tac ==
    counter = 2
    result = 0.0
L0:
    ifz counter goto L1
    ifz counter goto L2
    result = result + 1.5
    goto L3
L2:
    result = result - 0.5
L3:
    counter = counter - 1
    goto L0
L1:
    print result
--- stderr ---
--- exit: 0 ---
