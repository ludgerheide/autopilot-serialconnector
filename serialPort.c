//
// Created by ludger on 14.01.18.
//

#include <fcntl.h>           /* For O_* constants */
#include <mqueue.h>

#ifndef SIMULATE_SERIAL_PORT
#include <libserialport.h>
#else

#include <stdio.h>

#endif

#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include <unistd.h>
#include "c11threads/c11threads.h"
#include "serialPort.h"
#include "database.h"
#include <syslog.h>
#include <sys/param.h>
#include <stdio.h>

#define _unused(x) ((void)(x))

struct msgInfo {
    DroneMessage *msg;
    unsigned resetCount;
};

//Constants
static const char startMarker[5] = {'s', 't', 'a', 'r', 't'};
static const char *serialPortName = "/dev/ttyAMA0";
static const unsigned baudRate = 115200;
static const size_t bufferSize = 4096;
const char *navigationQueueSendName = "/navQueue-fromFlightController";
const char *navigationQueueRecvName = "/navQueue-toFlightController";
static const struct mq_attr navQueueAttributes = {.mq_maxmsg=1, .mq_msgsize = 8192};
const char *databaseWriterQueueName = "/dbQueue";
static const struct mq_attr databaseQueueAttributes = {.mq_maxmsg=10, .mq_msgsize = 8192};

//Threading and buffer variables
static unsigned char *buffer;
static size_t bufferIndex;
static mtx_t bufferMutex;
static cnd_t bufferCondition;
static thrd_t threads[4];
_Atomic bool parserThreadShouldExit;
_Atomic bool loggerThreadShouldExit;
_Atomic bool writerThreadShouldExit;
_Atomic bool readerThreadShouldExit;

//Static method definitions
static struct sp_port *setupSerialPort();

static int serialListenerThread(void *p);

static int serialWriterThread(void *p);

static int parserThread(void *p);

static int loggerThread(void *p);

static unsigned char calculateChecksum(const unsigned char *buf, unsigned len);

static void quitHandler();

void initSerial() {

    setlogmask(LOG_UPTO (LOG_DEBUG));
    openlog("flightManager-serial", LOG_PERROR | LOG_PID | LOG_CONS, LOG_USER);

#ifndef SIMULATE_SERIAL_PORT
    struct sp_port *port = setupSerialPort();
    assert(port != NULL);
#else
    struct sp_port *port = NULL;
#endif

    buffer = malloc(bufferSize);
    assert(buffer != NULL);
    bufferIndex = 0;

    int mutexInitialized = mtx_init(&bufferMutex, mtx_plain);
    assert(mutexInitialized == thrd_success);
    _unused(mutexInitialized);

    int conditionInitialized = cnd_init(&bufferCondition);
    assert(conditionInitialized == thrd_success);
    _unused(conditionInitialized);

    parserThreadShouldExit = false;

    loggerThreadShouldExit = false;

    //Here, start the main thread
    thrd_create(&threads[0], parserThread, NULL);
    thrd_create(&threads[2], loggerThread, NULL);
    thrd_create(&threads[3], serialWriterThread, port);
    thrd_create(&threads[1], serialListenerThread, port);

    thrd_detach(threads[0]);

    signal(SIGINT, quitHandler);

    thrd_join(threads[1], NULL);

    //Signal the parser thread to exit and wake it up
    parserThreadShouldExit = true;
    cnd_signal(&bufferCondition);

    loggerThreadShouldExit = true;
    thrd_join(threads[2], NULL);

    writerThreadShouldExit = true;
    thrd_join(threads[3], NULL);

    cnd_destroy(&bufferCondition);

    mtx_destroy(&bufferMutex);

    free(buffer);

#ifndef SIMULATE_SERIAL_PORT
    enum sp_return closedSuccess = sp_close(port);
    assert(closedSuccess == SP_OK);
    _unused(closedSuccess);
#endif
    closelog();
}

void addSerialSubscriber() {

}

void removeSerialSubscriber() {

}

#ifndef SIMULATE_SERIAL_PORT
static struct sp_port *setupSerialPort() {
    struct sp_port *port;

    enum sp_return foundPort = sp_get_port_by_name(serialPortName, &port);
    assert(foundPort == SP_OK);
    _unused(foundPort);

    enum sp_return openedPort = sp_open(port, SP_MODE_READ_WRITE);
    assert(openedPort == SP_OK);
    _unused(openedPort);

    enum sp_return setFlowControl = sp_set_flowcontrol(port, SP_FLOWCONTROL_NONE);
    assert(setFlowControl == SP_OK);
    _unused(setFlowControl);

    enum sp_return setBaudRate = sp_set_baudrate(port, baudRate);
    assert(setBaudRate == SP_OK);
    _unused(setBaudRate);

    return port;
}
#endif

static int serialListenerThread(void *p) {
    struct sp_port *port = p;
    int port_fd, result;
    fd_set readset;
#ifndef SIMULATE_SERIAL_PORT
    enum sp_return gotPortHandle = sp_get_port_handle(port, &port_fd);
    if (gotPortHandle != SP_OK) {
        return -1;
    }
#else
    port_fd = open("/home/ludger/Schreibtisch/received.raw", O_RDONLY);
    if (port_fd < 0) {
        perror("Error opening file");
    }
    assert(port_fd > 0);
#endif

    while (1) {
#ifdef SIMULATE_SERIAL_PORT
        //Sleep 4ms to simualte the msg delay
        unsigned wait_time_ms = 40;
        usleep(wait_time_ms * 1000);
#endif

        do {
            //Wait at least 100 character times to save the cpu
            unsigned wait_time_us = 100 * (1000000 / (baudRate / 8));
            usleep(wait_time_us);

            FD_ZERO(&readset);
            FD_SET(port_fd, &readset);
            result = select(port_fd + 1, &readset, NULL, NULL, NULL);
        } while (result == -1 && errno == EINTR && readerThreadShouldExit == false);

        if (readerThreadShouldExit) {
            return 0;
        }

        if (result > 0) {
            if (FD_ISSET(port_fd, &readset)) {
                /* The socket_fd has data available to be read */
                mtx_lock(&bufferMutex);
                ssize_t available_space = bufferSize - bufferIndex;
                assert(available_space >= 0);

                if (available_space > 0) {
                    //We still have space available to read
                    int bytesRead = read(port_fd, &buffer[bufferIndex], available_space);
                    if (bytesRead <= 0) {
                        syslog(LOG_ERR, "Error reading, code %i!\n", bytesRead);
                        return -1;
#ifdef SIMULATE_SERIAL_PORT
                        close(port_fd);
#endif
                    } else {
                        bufferIndex += bytesRead;
                        if (bufferIndex >= 0.5 * bufferSize) {
                            syslog(LOG_WARNING, "Incoming buffer %.1f%% full!", (100.0 * bufferIndex) / bufferSize);
                        }
                    }
                }
                cnd_signal(&bufferCondition);
                mtx_unlock(&bufferMutex);
            }
        } else if (result < 0) {
            /* An error ocurred, just print it to stdout */
            syslog(LOG_ERR, "Error on select(): %s\n", strerror(errno));
        }
    }
}

static int serialWriterThread(void *p) {
    struct sp_port *port = p;

    //Connect to the message queue
    mqd_t navQueue = mq_open(navigationQueueRecvName, O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR,
                             &navQueueAttributes);
    if (navQueue == -1) {
        syslog(LOG_ERR, "Opening database receive queue failed: %s", strerror(errno));
        return -1;
    }
    struct mq_attr attr;
    mq_getattr(navQueue, &attr);
    size_t msgBufSize = MAX((size_t) attr.mq_msgsize, bufferSize);
    char *msgBuf = malloc(msgBufSize);
    assert(msgBuf != NULL);

    while (true) {
        struct timespec timeout;
        timespec_get(&timeout, CLOCK_MONOTONIC);
        timeout.tv_sec += 1;
        int retVal = mq_timedreceive(navQueue, msgBuf, msgBufSize, NULL, &timeout);
        if (retVal > 0) {
            if (retVal > UINT8_MAX) {
                syslog(LOG_WARNING, "Writer thread: Message too long!");
                continue;
            }
            uint8_t payloadLength = (uint8_t) retVal;
            //Make sure it's a valid drone message
            DroneMessage *decodedMsg = drone_message__unpack(NULL, payloadLength, (uint8_t *) msgBuf);
            if (decodedMsg == NULL) {
                syslog(LOG_WARNING, "Writer thread: Invalid message received, will not write it out over serial");
                continue;
            } else {
                drone_message__free_unpacked(decodedMsg, NULL);
            }

            //Create the buffer for the message to be sent
            unsigned char *sendBuf = malloc(payloadLength + sizeof(startMarker) + 2);
            memcpy(sendBuf, startMarker, sizeof(startMarker));
            sendBuf[sizeof(startMarker)] = payloadLength;
            memcpy(&sendBuf[sizeof(startMarker) + 1], msgBuf, payloadLength);
            uint8_t checksum = calculateChecksum(sendBuf, payloadLength);
            sendBuf[payloadLength + sizeof(startMarker) + 1] = checksum;

            retVal = sp_blocking_write(port, sendBuf, payloadLength + sizeof(startMarker) + 2, 100);
            free(sendBuf);
            if (retVal < 0) {
                syslog(LOG_WARNING, "Writer thread: Writing message to serial port failed");
                continue;
            }


        } else {
            if (errno != ETIMEDOUT) {
                struct mq_attr attr;
                mq_getattr(navQueue, &attr);
                syslog(LOG_WARNING, "Receiving message failed (%lu messages in queue): %s", attr.mq_curmsgs,
                       strerror(errno));
            }

            if (writerThreadShouldExit) {
                struct mq_attr attr;
                mq_getattr(navQueue, &attr);
                if (attr.mq_curmsgs > 0 && errno != ETIMEDOUT) {
                    syslog(LOG_WARNING, "Closing, still %lu messages in queue.", attr.mq_curmsgs);
                } else {
                    mq_close(navQueue);
                    free(msgBuf);
                    return 0;
                }
            }
        }
    }
}

enum parserStatus {
    WAITING_FOR_START,
    RECEIVING_DATA,
};

static int parserThread(void *p) {
    _unused(p);
    unsigned dataLength = 0;
    unsigned noPosCount = 0;
    unsigned char *msgBuf = NULL;
    bool newMessageAvailable = false;
    enum parserStatus status = WAITING_FOR_START;

    //Connect to the message queues
    mqd_t databaseQueue = mq_open(databaseWriterQueueName, O_CREAT | O_WRONLY | O_NONBLOCK, 0666,
                                  &databaseQueueAttributes);
    if (databaseQueue == -1) {
        syslog(LOG_ERR, "Opening database send queue failed: %s", strerror(errno));
        return -1;
    }
    mqd_t navQueue = mq_open(navigationQueueSendName, O_CREAT | O_WRONLY | O_NONBLOCK, 0666, &navQueueAttributes);
    if (navQueue == -1) {
        syslog(LOG_ERR, "Opening database send queue failed: %s", strerror(errno));
        return -1;
    }

    while (true) {
        mtx_lock(&bufferMutex);
        while ((status == WAITING_FOR_START && bufferIndex < sizeof(startMarker) + 2) ||
               (status == RECEIVING_DATA && bufferIndex < dataLength + 1)) {
            struct timespec timeout;
            timespec_get(&timeout, CLOCK_MONOTONIC);
            timeout.tv_sec += 1;
            cnd_timedwait(&bufferCondition, &bufferMutex, &timeout);

            if (parserThreadShouldExit) {
                mq_close(databaseQueue);
                return 0;
            }
        }


        //Parse the newly received data
        while (status == WAITING_FOR_START && bufferIndex >= sizeof(startMarker) + 2) {
            //Check if the first five bytes match the start marker
            bool synchronized = true;
            for (unsigned i = 0; i < sizeof(startMarker); i++) {
                if (buffer[i] != startMarker[i]) {
                    synchronized = false;
                    break;
                }
            }

            if (synchronized) {
                dataLength = (buffer[5] << 8) + (buffer[6]);
                status = RECEIVING_DATA;

                //Now move the already-received payload data (if any) to the front and set the index back to 0
                if (bufferIndex > sizeof(startMarker) + 2) {
                    size_t sizeToCopy = bufferIndex - (sizeof(startMarker) + 2);
                    memmove(buffer, &buffer[sizeof(startMarker) + 2], sizeToCopy);
                }

                //And set the index back to by 7
                bufferIndex -= sizeof(startMarker) + 2;
            } else {
                //Move the content of the ring buffer back one byte and set the index back by 1
                memmove(buffer, &buffer[1], bufferIndex - 1);
                bufferIndex -= 1;
            }
        }

        if (status == RECEIVING_DATA && bufferIndex >= dataLength + 1) {
            unsigned char receivedChecksum = buffer[dataLength];
            unsigned char calculatedChecksum = calculateChecksum(buffer, dataLength);

            if (calculatedChecksum == receivedChecksum) {
                msgBuf = malloc(dataLength);
                assert(msgBuf != NULL);
                memcpy(msgBuf, buffer, dataLength);
                newMessageAvailable = true;
            } else {
                syslog(LOG_INFO, "Incoming message checksum failed");
            }

            //Now move the already-received next message (if any) to the front and set the index back
            if (bufferIndex > dataLength + 1) {
                size_t sizeToCopy = bufferIndex - (dataLength + 1);
                memmove(buffer, &buffer[dataLength + 1], sizeToCopy);
                bufferIndex -= dataLength + 1;
            }
            status = WAITING_FOR_START;
        }
        mtx_unlock(&bufferMutex);

        if (newMessageAvailable) {

            //Now, just decode the message
            DroneMessage *decodedMessage = drone_message__unpack(NULL, dataLength, msgBuf);
            newMessageAvailable = false;
            if (decodedMessage == NULL) {
                syslog(LOG_INFO, "Error decoding protobuf message");
            } else {
                int retVal = mq_send(databaseQueue, (void *) msgBuf, dataLength, 0);
                if (retVal != 0) {
                    struct mq_attr attr;
                    mq_getattr(databaseQueue, &attr);
                    syslog(LOG_WARNING, "Sending message to database queue failed with %lu messages in queue: %s",
                           attr.mq_curmsgs, strerror(errno));
                }
                if (decodedMessage->current_position != NULL) {
                    noPosCount = 0;
                    retVal = mq_send(navQueue, (void *) msgBuf, dataLength, 0);
                    if (retVal != 0) {
                        struct mq_attr attr;
                        mq_getattr(navQueue, &attr);
                        if (errno != EAGAIN) {
                            syslog(LOG_WARNING, "Sending message to nav queue failed with %lu messages in queue: %s",
                                   attr.mq_curmsgs, strerror(errno));
                        }
                    }
                } else {
                    noPosCount++;
                    if (noPosCount > 20) {
                        syslog(LOG_WARNING, "No position received within the last %d messages", noPosCount);
                    }
                }
                free(msgBuf);
                drone_message__free_unpacked(decodedMessage, NULL);
            }
        }
    }
}

static int loggerThread(void *p) {
    _unused(p);
    //Connect to the message queue
    mqd_t databaseQueue = mq_open(databaseWriterQueueName, O_CREAT | O_RDONLY, S_IWUSR | S_IRUSR,
                                  &databaseQueueAttributes);
    if (databaseQueue == -1) {
        syslog(LOG_ERR, "Opening database receive queue failed: %s", strerror(errno));
        return -1;
    }
    struct mq_attr attr;
    mq_getattr(databaseQueue, &attr);
    size_t msgBufSize = MAX((size_t) attr.mq_msgsize, bufferSize);
    char *msgBuf = malloc(msgBufSize);
    assert(msgBuf != NULL);
    unsigned dataLength = 0;
    unsigned resetCount = 1;
    uint64_t lastTimestamp = 0;

    while (true) {
        struct timespec timeout;
        timespec_get(&timeout, CLOCK_MONOTONIC);
        timeout.tv_sec += 1;
        int retVal = mq_timedreceive(databaseQueue, msgBuf, msgBufSize, NULL, &timeout);
        if (retVal > 0) {
            dataLength = retVal;
            DroneMessage *decodedMessage = drone_message__unpack(NULL, dataLength, (uint8_t *) msgBuf);
            if (decodedMessage == NULL) {
                syslog(LOG_INFO, "Error decoding protobuf message");
            } else {
                if (decodedMessage->timestamp < lastTimestamp) {
                    syslog(LOG_WARNING, "Flight Controller Reset occured!");
                    resetCount++;
                }
                lastTimestamp = decodedMessage->timestamp;
                writeMessageToDatabase(decodedMessage, resetCount);
                drone_message__free_unpacked(decodedMessage, NULL);
            }
        } else {
            struct mq_attr attr;
            mq_getattr(databaseQueue, &attr);
            if (errno != ETIMEDOUT) {
                syslog(LOG_ERR, "Receiving message failed (%lu messages in queue): %s", attr.mq_curmsgs,
                       strerror(errno));
            }

            if (loggerThreadShouldExit) {
                struct mq_attr attr;
                mq_getattr(databaseQueue, &attr);
                if (attr.mq_curmsgs > 0 && errno != ETIMEDOUT) {
                    syslog(LOG_WARNING, "Closing, still %lu messages in queue.", attr.mq_curmsgs);
                } else {
                    mq_close(databaseQueue);
                    free(msgBuf);
                    return 0;
                }
            }
        }
    }
}

static unsigned char calculateChecksum(const unsigned char *buf, unsigned len) {
    unsigned char calculatedChecksum = 0;
    for (unsigned i = 0; i < len; i++) {
        calculatedChecksum += buf[i];
    }
    return calculatedChecksum;
}

static void quitHandler() {
    //Closing the port will force a read error, making sure we close down nicely...
    printf("Ctrl-C pressed, signaling threads to exit...");
    readerThreadShouldExit = true;
    writerThreadShouldExit = true;
    parserThreadShouldExit = true;
    loggerThreadShouldExit = true;
}