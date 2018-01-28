
#include <mqueue.h>
#include "database.h"
#include "serialPort.h"

int main() {
    const char *navigationQueueSendName = "/navQueue-fromFlightController";
    const char *navigationQueueRecvName = "/navQueue-toFlightController";
    const char *databaseWriterQueueName = "/dbQueue";
    mq_unlink(navigationQueueRecvName);
    mq_unlink(navigationQueueSendName);
    mq_unlink(databaseWriterQueueName);

    initDatabase();
    startNewFlight();

    initSerial();

    endFlight();
    deinitDatabase();
}
