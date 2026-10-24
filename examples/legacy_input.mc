int counter = 5;
float total = 0.0;

if (counter) {
    total = total + 10.5;
} else {
    total = total - 1.0;
}

while (counter) {
    total = total + counter * 2;
    counter = counter - 1;
}

print(total);
