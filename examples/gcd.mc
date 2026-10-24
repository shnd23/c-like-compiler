int a = 1071;
int b = 462;

while (b != 0) {
    int r = a % b;
    a = b;
    b = r;
}

print(a);
