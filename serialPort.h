//
// Created by ludger on 14.01.18.
//

#ifndef FLIGHTMANAGER_SERIALPORT_H
#define FLIGHTMANAGER_SERIALPORT_H


extern const char* navigationQueueName;

void init();

void addSerialSubscriber();

void removeSerialSubscriber();

#endif //FLIGHTMANAGER_SERIALPORT_H