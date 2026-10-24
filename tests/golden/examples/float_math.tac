== tac ==
    guess = 1.0
    round = 0
L0:
    $t0 = round < 6
    ifz $t0 goto L1
    $t2 = 2.0 / guess
    $t3 = guess + $t2
    guess = $t3 / 2.0
    round = round + 1
    goto L0
L1:
    print guess
    total = 17
    count = 4
    average = 4.0
    print 4.0
    average = 4.25
    print 4.25
    print -4.25
    print 0.3333333333
--- stderr ---
--- exit: 0 ---
