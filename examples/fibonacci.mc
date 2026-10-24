int a = 0;
int b = 1;
int i = 0;

while (i < 15) {
    print(a);
    int next = a + b;
    a = b;
    b = next;
    i = i + 1;
}
