
#include "database.h"
#include "serialPort.h"
#include <stdio.h>
#include <time.h>
#include <stdbool.h>
#include <mqueue.h>

int main() {
    for(int i = 0; i < 5; i++) {
        printf("%i\n", i);
        initDatabase();
        startNewFlight();

        initSerial();

        endFlight();
        deinitDatabase();

        mq_unlink(databaseWriterQueueName);
    }
}
