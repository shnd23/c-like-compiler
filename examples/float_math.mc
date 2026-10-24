float guess = 1.0;
int round = 0;

while (round < 6) {
    guess = (guess + 2 / guess) / 2;
    round = round + 1;
}

print(guess);

int total = 17;
int count = 4;
float average = total / count;
print(average);
average = total / 4.0;
print(average);
print(-average);
print(1.0 / 3.0);
