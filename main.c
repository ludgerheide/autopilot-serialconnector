
#include "database.h"
#include "serialPort.h"

int main() {
    initDatabase();
    startNewFlight();

    initSerial();

    endFlight();
    deinitDatabase();
}
