//
// Created by ludger on 21.01.18.
//

#include "database.h"
#include "databaseCommands.h"
#include <syslog.h>
#include <stdlib.h>
#include <memory.h>
#include <stdio.h>
#include <assert.h>

static const char *databaseFileName = "flightLogs.sqlite";
static sqlite3 *db = NULL;
static int flightId = -1;

static sqlite3_stmt *flightModeStatement, *gpsVelocityStatement, *airVelocityStatement, *positionStatement, *altitudeStatement,  *attitudeStatement, *staticPressureStatement, *pitotPressureStatement, *gyroRawStatement, *magRawStatement, *accelRawStatement, *batteryDataStatement, *outputStatementSetStatement, *currentCommandStatement, *homeBasesStatement;

//Static method definitions
static int selectFlightIdCallback(void *NotUsed, int argc, char **argv, char **azColName);
static int initPreparedStatements(void);

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

int writeMessageToDatabase(DroneMessage *msg) {
    int resetCount = 1532;
    //timestamp is always set, slp doPressorCompensation maybe.
    {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(flightModeStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(flightModeStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);

        //Bind the values
        retVal = sqlite3_bind_int64(flightModeStatement, 1, msg->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(flightModeStatement, 2, (int)msg->current_mode);
        assert(retVal == SQLITE_OK);
        
        if(msg->has_sea_level_pressure) {
            retVal = sqlite3_bind_double(flightModeStatement, 17, msg->sea_level_pressure);
            assert(retVal == SQLITE_OK);
        } else {
            retVal = sqlite3_bind_null(flightModeStatement, 17);
            assert(retVal == SQLITE_OK);
        }

        if(msg->has_do_pressure_compensation) {
            retVal = sqlite3_bind_int(flightModeStatement, 7, msg->do_pressure_compensation);
            assert(retVal == SQLITE_OK);
        } else {
            retVal = sqlite3_bind_null(flightModeStatement, 7);
            assert(retVal == SQLITE_OK);
        }

        retVal = sqlite3_step(flightModeStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            return -1;
        }

        retVal = sqlite3_reset(flightModeStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            return -1;
        }

        retVal = sqlite3_clear_bindings(flightModeStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            return -1;
        }
    }
    return 0;
}

int initDatabase(void) {
    setlogmask(LOG_UPTO (LOG_DEBUG));
    openlog("flightManager-serial", LOG_PERROR | LOG_PID | LOG_CONS, LOG_USER);

    int retVal = sqlite3_open(databaseFileName, &db);
    assert(db != NULL && retVal == SQLITE_OK);

    char* error = NULL;
    retVal = sqlite3_exec(db, connectionPragmaCommand, NULL, NULL, &error);
    if (retVal != SQLITE_OK || error != NULL) {
        syslog(LOG_ERR, "Setting connection properties failed: %s, %s", error, sqlite3_errmsg(db));
        sqlite3_free(error);
        return -1;
    }

    error = NULL;
    retVal = sqlite3_exec(db, createDatabaseCommand, NULL, NULL, &error);
    if (retVal != SQLITE_OK || error != NULL) {
        syslog(LOG_ERR, "Creating sqlite tables failed: %s, %s", error, sqlite3_errmsg(db));
        sqlite3_free(error);
        return -1;
    }

    initPreparedStatements();

    return 0;
}

void deinitDatabase(void) {
    int retVal = sqlite3_close_v2(db);
    assert(retVal == SQLITE_OK);

    retVal = sqlite3_finalize(flightModeStatement);
    assert(retVal == SQLITE_OK);

    closelog();
}

//Static methods
static int selectFlightIdCallback(void *NotUsed, int argc, char **argv, char **azColName) {
    assert(argc == 1);
    flightId = atoi(argv[0]);
    assert(flightId >= 1);
    return 0;
}

static int initPreparedStatements(void) {
    int retVal = sqlite3_prepare_v2(db, flightModeCommand, strlen(flightModeCommand), &flightModeStatement, NULL);
    if(retVal != SQLITE_OK || flightModeStatement == NULL) {
        syslog(LOG_ERR, "Setting statement up failed: %s", sqlite3_errmsg(db));
        return -1;
    }

    return 0;
}