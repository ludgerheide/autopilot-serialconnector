//
// Created by ludger on 21.01.18.
//

#include "database.h"
#include "databaseCommands.h"
#include <syslog.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>

static const char *databaseFileName = "flightLogs.sqlite";
static sqlite3 *db = NULL;
static int flightId = -1;

//Static method definitions
static int selectFlightIdCallback(void *NotUsed, int argc, char **argv, char **azColName);

int startNewFlight() {
    //If we already have a flight in progress, end the last flight
    if (flightId != -1) {
        //endFlight();
    }

    //Now, create a new flight
    char *error = NULL;
    int retVal = sqlite3_exec(db, startNewFlightCommand, NULL, NULL, &error);
    if (retVal != SQLITE_OK || error != NULL) {
        syslog(LOG_ERR, "Starting flight failed: %s, %s", error, sqlite3_errmsg(db));
        sqlite3_free(error);
        return -1;
    }

    //Get the flight Id
    const char *sql = "SELECT MAX(Id) from flights;";
    error = NULL;
    retVal = sqlite3_exec(db, sql, selectFlightIdCallback, NULL, &error);
    if (retVal != SQLITE_OK || error != NULL) {
        syslog(LOG_ERR, "Getting flight Id failed: %s, %s", error, sqlite3_errmsg(db));
        sqlite3_free(error);
        return -1;
    }

    return 0;
}

int endFlight(void) {
    const char *endFlightCommandUnformatted = "UPDATE flights SET end=datetime('now') WHERE Id=%i;";
    char *sql = malloc(strlen(endFlightCommandUnformatted) + 10);
    sprintf(sql, endFlightCommandUnformatted, flightId);

    char *error = NULL;
    int retVal = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (retVal != SQLITE_OK || error != NULL) {
        syslog(LOG_ERR, "Ending flight failed: %s, %s", error, sqlite3_errmsg(db));
        sqlite3_free(error);
        return -1;
    }

    return 0;
}

void writeMessageToDatabase(DroneMessage *msg) {

}

int initDatabase(void) {
    setlogmask(LOG_UPTO (LOG_DEBUG));
    openlog("flightManager-serial", LOG_PERROR | LOG_PID | LOG_CONS, LOG_USER);

    int retVal = sqlite3_open(databaseFileName, &db);
    assert(db != NULL && retVal == SQLITE_OK);

    char *error = NULL;
    retVal = sqlite3_exec(db, createDatabaseCommand, NULL, NULL, &error);
    if (retVal != SQLITE_OK || error != NULL) {
        syslog(LOG_ERR, "Creating sqlite tables failed: %s, %s", error, sqlite3_errmsg(db));
        sqlite3_free(error);
        return -1;
    }

    error = NULL;
    retVal = sqlite3_exec(db, connectionPragmaCommand, NULL, NULL, &error);
    if (retVal != SQLITE_OK || error != NULL) {
        syslog(LOG_ERR, "Setting connection properties failed: %s, %s", error, sqlite3_errmsg(db));
        sqlite3_free(error);
        return -1;
    }


    return 0;
}

void deinitDatabase(void) {
    int retVal = sqlite3_close_v2(db);
    assert(retVal == SQLITE_OK);

    closelog();
}

//Static methods
static int selectFlightIdCallback(void *NotUsed, int argc, char **argv, char **azColName) {
    assert(argc == 1);
    flightId = atoi(argv[0]);
    assert(flightId > 1);
    return 0;
}