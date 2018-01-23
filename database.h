//
// Created by ludger on 21.01.18.
//

#ifndef FLIGHTMANAGER_DATABASE_H
#define FLIGHTMANAGER_DATABASE_H

#include <sqlite3.h>
#include "protobuf/communicationProtocol.pb-c.h"

int startNewFlight();

int endFlight(void);

void writeMessageToDatabase(DroneMessage *msg);

int initDatabase(void);

void deinitDatabase(void);

#endif //FLIGHTMANAGER_DATABASE_H
