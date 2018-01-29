//
// Created by ludger on 21.01.18.
//

#ifndef FLIGHTMANAGER_DATABASE_H
#define FLIGHTMANAGER_DATABASE_H

#include "sqlite/sqlite3.h"
#include "protobuf/communicationProtocol.pb-c.h"

int startNewFlight();

int endFlight(void);

int writeMessageToDatabase(DroneMessage *msg, unsigned resetCount);

int initDatabase(void);

void deinitDatabase(void);

#endif //FLIGHTMANAGER_DATABASE_H
