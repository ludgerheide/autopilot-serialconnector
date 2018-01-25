
#include "database.h"
#include "serialPort.h"
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

int main() {
    for(int i = 0; i < 5; i++) {
    initDatabase();
    startNewFlight();

    initSerial();

    endFlight();
    deinitDatabase();
    }
}
