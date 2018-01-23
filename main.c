
#include "database.h"
#include <stdio.h>
#include <time.h>

int main() {
    initDatabase();

    unsigned ticks = 1000;
    clock_t start = clock(), diff;
    for (int i = 0; i < ticks; i++) {
        printf("%i\n", i);
        startNewFlight();
        endFlight();
    }

    diff = clock() - start;

    double msec = diff * 1000.0 / CLOCKS_PER_SEC;
    printf("Time taken %.2f ms per tick\n", msec / ticks);

    deinitDatabase();

    //initSerial();
}