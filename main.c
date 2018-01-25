
#include "database.h"
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

int main() {
    initDatabase();
    startNewFlight();

    DroneMessage msg = DRONE_MESSAGE__INIT;
    msg.has_do_pressure_compensation = true;
    msg.do_pressure_compensation = true;
    msg.has_sea_level_pressure = true;

    unsigned ticks = 1000;
    clock_t start = clock(), diff;
    for (int i = 0; i < ticks; i++) {
        msg.sea_level_pressure = i;
        msg.timestamp = (unsigned)i;
        writeMessageToDatabase(&msg);
        printf("%i\n", i);
    }

    diff = clock() - start;

    double msec = diff * 1000.0 / CLOCKS_PER_SEC;
    printf("Time taken %.2f ms per tick\n", msec / ticks);

    endFlight();
    deinitDatabase();

    //initSerial();
}