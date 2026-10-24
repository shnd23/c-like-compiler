int n = 1;
int product = 1;

while (n <= 20) {
    product = product * n;
    if (n == 5 || n == 10 || n == 20) {
        print(product);
    }
    n = n + 1;
}
