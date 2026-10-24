int counter = 2;
float result = 0.0;

while (counter) {
    if (counter) {
        result = result + 1.5;
    } else {
        result = result - 0.5;
    }
    counter = counter - 1;
}

print(result);
