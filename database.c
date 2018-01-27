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
#include <math.h>

#define _unused(x) ((void)(x))

static const char *databaseFileName = "flightLogs.sqlite";
static sqlite3 *db = NULL;
static int flightId = -1;

static sqlite3_stmt *flightModeStatement, *gpsVelocityStatement, *airVelocityStatement, *positionStatement, *altitudeStatement,  *attitudeStatement, *staticPressureStatement, *pitotPressureStatement, *gyroRawStatement, *magRawStatement, *accelRawStatement, *batteryDataStatement, *outputCommandSetStatement, *currentCommandStatement, *homeBasesStatement, *beginTransactionStatement, *endTransactionStatement;

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
    if(sql == NULL) {
        syslog(LOG_ERR, "Malloc failed in %s at %i", __FILE__, __LINE__);
        return -2;
    }
    sprintf(sql, endFlightCommandUnformatted, flightId);

    char *error = NULL;
    int retVal = sqlite3_exec(db, sql, NULL, NULL, &error);
    if (retVal != SQLITE_OK || error != NULL) {
        syslog(LOG_ERR, "Ending flight failed: %s, %s", error, sqlite3_errmsg(db));
        sqlite3_free(error);
        free(sql);
        return -1;
    }

    free(sql);
    return 0;
}

int writeMessageToDatabase(DroneMessage *msg, unsigned resetCount) {
    int functionReturn = 0;

    {//Start the transaction
        int retVal = sqlite3_step(beginTransactionStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            return -1;
        }

        retVal = sqlite3_reset(beginTransactionStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            return -1;
        }
    }

    //timestamp is always set, slp doPressorCompensation maybe.
    {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(flightModeStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(flightModeStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);

        //Bind the timestamp and mode
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
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(flightModeStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(flightModeStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->current_groundspeed != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(gpsVelocityStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(gpsVelocityStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int64(gpsVelocityStatement, 30, msg->current_groundspeed->timestamp);
        assert(retVal == SQLITE_OK);
        //Convert speed from cm/s to km/h
        retVal = sqlite3_bind_double(gpsVelocityStatement, 31, msg->current_groundspeed->speed/27.7777);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(gpsVelocityStatement, 32, msg->current_groundspeed->course_over_ground/64.0);
        assert(retVal == SQLITE_OK);

        retVal = sqlite3_step(gpsVelocityStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(gpsVelocityStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(gpsVelocityStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->current_airspeed != 0) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(airVelocityStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(airVelocityStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        assert(msg->current_airspeed->has_timestamp);
        retVal = sqlite3_bind_int64(airVelocityStatement, 40, msg->current_airspeed->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(airVelocityStatement, 41, msg->current_airspeed->speed/27.7777);

        retVal = sqlite3_step(airVelocityStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(airVelocityStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(airVelocityStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->current_position != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(positionStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(positionStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        assert(msg->current_position->has_timestamp);
        retVal = sqlite3_bind_int64(positionStatement, 20, msg->current_position->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(positionStatement, 22, msg->current_position->latitude);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(positionStatement, 23, msg->current_position->longitude);
        assert(retVal == SQLITE_OK);
        assert(msg->current_position->has_gps_altitude);
        retVal = sqlite3_bind_double(positionStatement, 25, msg->current_position->gps_altitude/100.0); //cm to m
        assert(retVal == SQLITE_OK);
        assert(msg->current_position->has_real_time);
        //Creating the real time is a bit intricate
        double fractions = fmod(msg->current_position->real_time, 1);
        unsigned seconds = floor(fmod(msg->current_position->real_time, 100));
        unsigned minutes = (floor(fmod(msg->current_position->real_time, 10000)) - seconds) / 100;
        unsigned hours = (floor(msg->current_position->real_time) - seconds - (minutes * 100)) / 10000;
        char* timeString = malloc(13);
        sprintf(timeString, "%02u:%02u:%02u.%03.0f", hours, minutes, seconds, floor(fractions*1000));
        retVal = sqlite3_bind_text(positionStatement, 21, timeString, -1, SQLITE_TRANSIENT);
        free(timeString);
        assert(retVal == SQLITE_OK);
        assert(msg->current_position->has_number_of_satellites);
        retVal = sqlite3_bind_int(positionStatement, 26, msg->current_position->number_of_satellites);

        retVal = sqlite3_step(positionStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(positionStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(positionStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->current_altitude != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(altitudeStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(altitudeStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        assert(msg->current_altitude->has_timestamp);
        retVal = sqlite3_bind_int64(altitudeStatement, 50, msg->current_altitude->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(altitudeStatement, 51, msg->current_altitude->altitude/100.0);
        assert(retVal == SQLITE_OK);
        assert(msg->current_altitude->has_rate_of_climb);
        retVal = sqlite3_bind_double(altitudeStatement, 52, msg->current_altitude->rate_of_climb/100.0);

        retVal = sqlite3_step(altitudeStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(altitudeStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(altitudeStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->current_attitude != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(attitudeStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(attitudeStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        assert(msg->current_attitude->has_timestamp);
        retVal = sqlite3_bind_int64(attitudeStatement, 60, msg->current_attitude->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(attitudeStatement, 61, msg->current_attitude->course_magnetic/64.0);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(attitudeStatement, 62, msg->current_attitude->pitch/64.0);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(attitudeStatement, 63, msg->current_attitude->roll/64.0);

        retVal = sqlite3_step(attitudeStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(attitudeStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(attitudeStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->static_pressure != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(staticPressureStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(staticPressureStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int64(staticPressureStatement, 70, msg->static_pressure->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(staticPressureStatement, 71, msg->static_pressure->pressure/25600.0);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(staticPressureStatement, 72, msg->static_pressure->temperature/100.0);

        retVal = sqlite3_step(staticPressureStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(staticPressureStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(staticPressureStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->pitot_pressure != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(pitotPressureStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(pitotPressureStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int64(pitotPressureStatement, 70, msg->pitot_pressure->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(pitotPressureStatement, 71, msg->pitot_pressure->pressure/25600.0);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(pitotPressureStatement, 72, msg->pitot_pressure->temperature/100.0);

        retVal = sqlite3_step(pitotPressureStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(pitotPressureStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(pitotPressureStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->gyro_raw != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(gyroRawStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(gyroRawStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int64(gyroRawStatement, 80, msg->gyro_raw->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(gyroRawStatement, 81, msg->gyro_raw->x);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(gyroRawStatement, 82, msg->gyro_raw->y);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(gyroRawStatement, 83, msg->gyro_raw->z);
        assert(retVal == SQLITE_OK);

        retVal = sqlite3_step(gyroRawStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(gyroRawStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(gyroRawStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->mag_raw != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(magRawStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(magRawStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int64(magRawStatement, 80, msg->mag_raw->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(magRawStatement, 81, msg->mag_raw->x);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(magRawStatement, 82, msg->mag_raw->y);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(magRawStatement, 83, msg->mag_raw->z);
        assert(retVal == SQLITE_OK);

        retVal = sqlite3_step(magRawStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(magRawStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(magRawStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->accel_raw != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(accelRawStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(accelRawStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int64(accelRawStatement, 80, msg->accel_raw->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(accelRawStatement, 81, msg->accel_raw->x);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(accelRawStatement, 82, msg->accel_raw->y);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(accelRawStatement, 83, msg->accel_raw->z);
        assert(retVal == SQLITE_OK);

        retVal = sqlite3_step(accelRawStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(accelRawStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(accelRawStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->current_battery_data != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(batteryDataStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(batteryDataStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        assert(msg->current_battery_data->has_timestamp);
        retVal = sqlite3_bind_int64(batteryDataStatement, 90, msg->current_battery_data->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(batteryDataStatement, 91, msg->current_battery_data->voltage/1000.0);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(batteryDataStatement, 92, msg->current_battery_data->current/1000.0);

        retVal = sqlite3_step(batteryDataStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(batteryDataStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(batteryDataStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->output_command_set != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(outputCommandSetStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(outputCommandSetStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        assert(msg->output_command_set->has_timestamp);
        retVal = sqlite3_bind_int64(outputCommandSetStatement, 100, msg->output_command_set->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(outputCommandSetStatement, 101, msg->output_command_set->yaw);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(outputCommandSetStatement, 102, msg->output_command_set->pitch);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(outputCommandSetStatement, 103, msg->output_command_set->thrust);
        if(msg->output_command_set->has_roll) {
            retVal = sqlite3_bind_int(outputCommandSetStatement, 104, msg->output_command_set->roll);
            assert(retVal == SQLITE_OK);
        } else {
            retVal = sqlite3_bind_null(outputCommandSetStatement, 104);
            assert(retVal == SQLITE_OK);
        }

        retVal = sqlite3_step(outputCommandSetStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(outputCommandSetStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(outputCommandSetStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->current_command != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(currentCommandStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(currentCommandStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        assert(msg->current_command->has_timestamp);
        retVal = sqlite3_bind_int64(currentCommandStatement, 120, msg->current_command->timestamp);
        assert(retVal == SQLITE_OK);

        switch (msg->current_command->vertical_command_case) {
            case DRONE_MESSAGE__COMMAND_UPDATE__VERTICAL_COMMAND_PITCH_ANGLE:
                retVal = sqlite3_bind_double(currentCommandStatement, 110, msg->current_command->pitch_angle/64.0);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_null(currentCommandStatement, 111);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_null(currentCommandStatement, 112);
                assert(retVal == SQLITE_OK);
                break;

            case DRONE_MESSAGE__COMMAND_UPDATE__VERTICAL_COMMAND_ALTITUDE:
                retVal = sqlite3_bind_null(currentCommandStatement, 110);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_double(currentCommandStatement, 111, msg->current_command->altitude/100.0);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_null(currentCommandStatement, 112);
                assert(retVal == SQLITE_OK);
                break;

            case DRONE_MESSAGE__COMMAND_UPDATE__VERTICAL_COMMAND_RATE_OF_CLIMB:
                retVal = sqlite3_bind_null(currentCommandStatement, 110);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_null(currentCommandStatement, 111);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_double(currentCommandStatement, 112, msg->current_command->rate_of_climb/100.0);
                break;

            case DRONE_MESSAGE__COMMAND_UPDATE__VERTICAL_COMMAND__NOT_SET:
            default:
                syslog(LOG_ERR, "Fail in %s at %i", __FILE__, __LINE__);
                break;
        }

        switch (msg->current_command->horizontal_command_case) {
            case DRONE_MESSAGE__COMMAND_UPDATE__HORIZONTAL_COMMAND_HEADING:
                retVal = sqlite3_bind_double(currentCommandStatement, 113, msg->current_command->heading/64.0);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_null(currentCommandStatement, 114);
                assert(retVal == SQLITE_OK);
                break;

            case DRONE_MESSAGE__COMMAND_UPDATE__HORIZONTAL_COMMAND_RATE_OF_TURN:
                retVal = sqlite3_bind_null(currentCommandStatement, 113);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_int(currentCommandStatement, 114, msg->current_command->rate_of_turn);
                assert(retVal == SQLITE_OK);
                break;

            case DRONE_MESSAGE__COMMAND_UPDATE__HORIZONTAL_COMMAND__NOT_SET:
            default:
                syslog(LOG_ERR, "Fail in %s at %i", __FILE__, __LINE__);
                break;
        }

        switch (msg->current_command->speed_command_case) {
            case DRONE_MESSAGE__COMMAND_UPDATE__SPEED_COMMAND_SPEED:
                retVal = sqlite3_bind_null(currentCommandStatement, 115);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_double(currentCommandStatement, 116, msg->current_command->speed/27.7777);
                assert(retVal == SQLITE_OK);
                break;

            case DRONE_MESSAGE__COMMAND_UPDATE__SPEED_COMMAND_THROTTLE:
                retVal = sqlite3_bind_int(currentCommandStatement, 115, msg->current_command->throttle);
                assert(retVal == SQLITE_OK);
                retVal = sqlite3_bind_null(currentCommandStatement, 116);
                assert(retVal == SQLITE_OK);
                break;

            case DRONE_MESSAGE__COMMAND_UPDATE__SPEED_COMMAND__NOT_SET:
            default:
                syslog(LOG_ERR, "Fail in %s at %i", __FILE__, __LINE__);
                break;
        }

        retVal = sqlite3_step(currentCommandStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(currentCommandStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(currentCommandStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    if(msg->home_base != NULL) {
        //Bind the values that are constant
        int retVal = sqlite3_bind_int(homeBasesStatement, 999, flightId);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_int(homeBasesStatement, 998, resetCount);
        assert(retVal == SQLITE_OK);
        assert(msg->home_base->has_timestamp);
        retVal = sqlite3_bind_int64(homeBasesStatement, 200, msg->home_base->timestamp);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(homeBasesStatement, 201, msg->home_base->latitude);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(homeBasesStatement, 202, msg->home_base->longitude);
        assert(retVal == SQLITE_OK);
        retVal = sqlite3_bind_double(homeBasesStatement, 203, msg->home_base->altitude/100.0);
        if(msg->home_base->has_orbit_radius) {
            retVal = sqlite3_bind_double(homeBasesStatement, 204, msg->home_base->orbit_radius/100.0);
            assert(retVal == SQLITE_OK);
        } else {
            retVal = sqlite3_bind_null(homeBasesStatement, 204);
            assert(retVal == SQLITE_OK);
        }
        if(msg->home_base->has_orbit_until_target_altitude) {
            retVal = sqlite3_bind_int(homeBasesStatement, 205, msg->home_base->orbit_until_target_altitude);
            assert(retVal == SQLITE_OK);
        } else {
            retVal = sqlite3_bind_null(homeBasesStatement, 205);
            assert(retVal == SQLITE_OK);
        }
        if(msg->home_base->has_orbit_clockwise) {
            retVal = sqlite3_bind_int(homeBasesStatement, 206, msg->home_base->has_orbit_clockwise);
            assert(retVal == SQLITE_OK);
        } else {
            retVal = sqlite3_bind_null(homeBasesStatement, 206);
        }

        retVal = sqlite3_step(homeBasesStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(homeBasesStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_clear_bindings(homeBasesStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

    }

    {//end the transaction
        int retVal = sqlite3_step(endTransactionStatement);
        if (retVal != SQLITE_OK && retVal != SQLITE_DONE) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }

        retVal = sqlite3_reset(endTransactionStatement);
        if (retVal != SQLITE_OK) {
            syslog(LOG_ERR, "Fail in %s at %i: %s", __FILE__, __LINE__, sqlite3_errmsg(db));
            functionReturn -= 1;
        }
    }

    return functionReturn;
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
    sqlite3_stmt* insertStatements[] = {flightModeStatement, gpsVelocityStatement, airVelocityStatement, positionStatement, altitudeStatement, attitudeStatement, staticPressureStatement, pitotPressureStatement, gyroRawStatement, magRawStatement, accelRawStatement, batteryDataStatement, outputCommandSetStatement, currentCommandStatement, homeBasesStatement, beginTransactionStatement, endTransactionStatement};

    for (unsigned i = 0; i < sizeof(insertStatements)/sizeof(insertStatements[0]); i++) {
        int retVal = sqlite3_finalize(insertStatements[i]);
        assert(retVal == SQLITE_OK);
        _unused(retVal);
    }

    int retVal = sqlite3_close_v2(db);
    assert(retVal == SQLITE_OK);
    _unused(retVal);

    closelog();
}

//Static methods
static int selectFlightIdCallback(void *NotUsed __attribute__((unused)), int argc, char **argv, char **azColName __attribute__((unused))) {
    assert(argc == 1);
    _unused(argc);
    flightId = atoi(argv[0]);
    assert(flightId >= 1);
    return 0;
}

static int initPreparedStatements(void) {
    const char* insertCommands[] = {flightModeCommand, gpsVelocityCommand, airVelocityCommand, positionCommand, altitudeCommand, attitudeCommand, staticPressureCommand, pitotPressureCommand, gyroRawCommand, magRawCommand, accelRawCommand, batteryDataCommand, outputCommandSetCommand, currentCommandCommand, homeBasesCommand, beginTransactionCommand, endTransactionCommand};

    sqlite3_stmt** insertStatements[] = {&flightModeStatement, &gpsVelocityStatement, &airVelocityStatement, &positionStatement, &altitudeStatement, &attitudeStatement, &staticPressureStatement, &pitotPressureStatement, &gyroRawStatement, &magRawStatement, &accelRawStatement, &batteryDataStatement, &outputCommandSetStatement, &currentCommandStatement, &homeBasesStatement, &beginTransactionStatement, &endTransactionStatement};

    assert(sizeof(insertStatements) == sizeof(insertCommands));

    for (unsigned i = 0; i < sizeof(insertCommands)/sizeof(insertCommands[0]); i++) {
        int retVal = sqlite3_prepare_v2(db, insertCommands[i], (int)strlen(insertCommands[i]), insertStatements[i], NULL);
        if(retVal != SQLITE_OK || insertStatements[i] == NULL) {
            syslog(LOG_ERR, "Setting statement up failed: %s", sqlite3_errmsg(db));
            return -1;
        }
    }


    return 0;
}
