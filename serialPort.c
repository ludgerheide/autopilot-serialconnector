//
// Created by ludger on 14.01.18.
//

#include <limits.h>
#ifndef __APPLE__
#include <libserialport.h>
#else
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#endif
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include <unistd.h>
#include "c11threads/c11threads.h"
#include "serialPort.h"
#include "database.h"
#include "protobuf/communicationProtocol.pb-c.h"
#include <syslog.h>

#define _unused(x) ((void)(x))

struct msgInfo {
    DroneMessage *msg;
    unsigned resetCount;
};

//Constants
static const unsigned timeout = 1000;
static const char startMarker[5] = {'s', 't', 'a', 'r', 't'};
static const char *serialPortName = "/dev/ttyAMA0";
static const unsigned baudRate = 115200;
static const size_t bufferSize = 4096;

//Threading and buffer variables
static unsigned char *buffer;
static size_t bufferIndex;
static mtx_t bufferMutex, databaseMutex;
static cnd_t bufferCondition;
static thrd_t threads[2];
_Atomic int waitingWrites = 0;

//Static method definitions
static struct sp_port *setupSerialPort();

static int serialListenerThread(void *p);

static int parserThread(void *p) __attribute__ ((noreturn));

static int loggerThread(void *p);

static unsigned char calculateChecksum(const unsigned char *buf, unsigned len);

void initSerial() {
    setlogmask(LOG_UPTO (LOG_DEBUG));
    openlog("flightManager-serial", LOG_PERROR | LOG_PID | LOG_CONS, LOG_USER);

#ifndef __APPLE__
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

    mutexInitialized = mtx_init(&databaseMutex, mtx_plain);
    assert(mutexInitialized == thrd_success);
    _unused(mutexInitialized);

    int conditionInitialized = cnd_init(&bufferCondition);
    assert(conditionInitialized == thrd_success);
    _unused(conditionInitialized);

    //Here, start the main thread
    thrd_create(&threads[0], parserThread, NULL);
    thrd_create(&threads[1], serialListenerThread, port);

    thrd_detach(threads[0]);
    thrd_join(threads[1], NULL);

    cnd_destroy(&bufferCondition);

    mtx_destroy(&databaseMutex);

    mtx_destroy(&bufferMutex);

    free(buffer);

#ifndef __APPLE__
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

#ifndef __APPLE__
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
    struct sp_port *restrict port = p;
    int port_fd, result;
    fd_set readset;
#ifndef __APPLE__
    enum sp_return gotPortHandle = sp_get_port_handle(port, &port_fd);
    if (gotPortHandle != SP_OK) {
        return -1;
    }
#else
    port_fd = open("/Users/ludger/Desktop/recived3.raw", O_RDONLY);
    if(port_fd < 0) {
        perror("Error opening file");
    }
    assert(port_fd>0);
#endif

    while (1) {
        do {
            //Wait at least 100 character times to save the cpu
            unsigned wait_time_us = 100 * (1000000 / (baudRate / 8));
            usleep(wait_time_us);

            FD_ZERO(&readset);
            FD_SET(port_fd, &readset);
            result = select(port_fd + 1, &readset, NULL, NULL, NULL);
        } while (result == -1 && errno == EINTR);

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
                        while (waitingWrites > 0) {
                            usleep(1000);
                        }
                        return -1;
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

enum parserStatus {
    WAITING_FOR_START,
    RECEIVING_DATA,
};

static int parserThread(void *p) {
    _unused(p);
    unsigned char dataLength = 0;
    unsigned char *msgBuf = NULL;
    bool newMessageAvailable = false;
    enum parserStatus status = WAITING_FOR_START;

    //Flight management stuff for the database
    uint64_t lastTimestamp = 0;
    unsigned int resetCount = 1;

    while (true) {
        mtx_lock(&bufferMutex);
        while ((status == WAITING_FOR_START && bufferIndex < sizeof(startMarker) + 1) ||
               (status == RECEIVING_DATA && bufferIndex < (unsigned) dataLength + 1)) {
            cnd_wait(&bufferCondition, &bufferMutex);
        }

        //Parse the newly received data
        while (status == WAITING_FOR_START && bufferIndex >= sizeof(startMarker) + 1) {
            //Check if the first five bytes match the start marker
            bool synchronized = true;
            for (unsigned i = 0; i < sizeof(startMarker); i++) {
                if (buffer[i] != startMarker[i]) {
                    synchronized = false;
                    break;
                }
            }

            if (synchronized) {
                dataLength = buffer[5];
                status = RECEIVING_DATA;

                //Now move the already-received payload data (if any) to the front and set the index back to 0
                if (bufferIndex > sizeof(startMarker) + 1) {
                    size_t sizeToCopy = bufferIndex - (sizeof(startMarker) + 1);
                    memmove(buffer, &buffer[sizeof(startMarker) + 1], sizeToCopy);
                }

                //And set the index back to by 6
                bufferIndex -= sizeof(startMarker) + 1;
            } else {
                //Move the content of the ring buffer back one byte and set the index back by 1
                memmove(buffer, &buffer[1], bufferIndex - 1);
                bufferIndex -= 1;
            }
        }

        if (status == RECEIVING_DATA && bufferIndex >= (unsigned) dataLength + 1) {
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
            if (bufferIndex > (unsigned) dataLength + 1) {
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
            free(msgBuf);
            newMessageAvailable = false;
            if (decodedMessage == NULL) {
                syslog(LOG_INFO, "Error decoding protobuf message");
            } else {
                //Check if we had a reset, if yes increment resetCount
                if(decodedMessage->timestamp < lastTimestamp) {
                    resetCount++;
                }
                lastTimestamp = decodedMessage->timestamp;

                //Start the logger
                struct msgInfo* info = malloc(sizeof(struct msgInfo));
                info->resetCount = resetCount;
                info->msg = decodedMessage;

                thrd_t myLoggerThread;
                thrd_create(&myLoggerThread, loggerThread, info);
                thrd_detach(myLoggerThread);
            }
        }
    }
}

static int loggerThread(void *p) {
    waitingWrites++;
    if(waitingWrites > 10) {
        syslog(LOG_WARNING, "%d messages waiting to be written to database", waitingWrites);
    }

    //Get the info passed to us and free it
    struct msgInfo *restrict msgInfo = p;
    unsigned int myResetCount = msgInfo->resetCount;
    DroneMessage *msg = msgInfo->msg;
    free(msgInfo);

    //We use a message mutex so no unsafe acesses are done to the prepared statements. The alternative would be to make them thread-local
    mtx_lock(&databaseMutex);
    writeMessageToDatabase(msg, myResetCount);
    mtx_unlock(&databaseMutex);

    waitingWrites--;
    drone_message__free_unpacked(msg, NULL);
    return 0;
}

static unsigned char calculateChecksum(const unsigned char *buf, unsigned len) {
    unsigned char calculatedChecksum = 0;
    for (unsigned i = 0; i < len; i++) {
        calculatedChecksum += buf[i];
    }
    return calculatedChecksum;
}
