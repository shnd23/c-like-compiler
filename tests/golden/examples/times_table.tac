== tac ==
    row = 1
L0:
    $t0 = row <= 5
    ifz $t0 goto L1
    col = 1
    line_total = 0
L2:
    $t1 = col <= 5
    ifz $t1 goto L3
    $t2 = row * col
    line_total = line_total + $t2
    col = col + 1
    goto L2
L3:
    print line_total
    row = row + 1
    goto L0
L1:
--- stderr ---
--- exit: 0 ---
