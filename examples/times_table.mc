int row = 1;

while (row <= 5) {
    int col = 1;
    int line_total = 0;
    while (col <= 5) {
        line_total = line_total + row * col;
        col = col + 1;
    }
    print(line_total);
    row = row + 1;
}
