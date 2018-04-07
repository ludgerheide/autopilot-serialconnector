
#include <mqueue.h>
#include "database.h"
#include "serialPort.h"

int main() {
    //Remove the queues of they already exist with wrong parameters
    const char *navigationQueueSendName = "/navQueue-fromFlightController";
    const char *navigationQueueRecvName = "/navQueue-toFlightController";
    const char *databaseWriterQueueName = "/dbQueue";
    mq_unlink(navigationQueueRecvName);
    mq_unlink(navigationQueueSendName);
    mq_unlink(databaseWriterQueueName);

    initDatabase();
    startNewFlight();

    init();

    endFlight();
    deinitDatabase();
}
