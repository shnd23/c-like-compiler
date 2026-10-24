int n = 2;

while (n < 50) {
    int divisor = 2;
    int is_prime = 1;
    while (divisor * divisor <= n && is_prime) {
        if (n % divisor == 0) {
            is_prime = 0;
        }
        divisor = divisor + 1;
    }
    if (is_prime) {
        print(n);
    }
    n = n + 1;
}
