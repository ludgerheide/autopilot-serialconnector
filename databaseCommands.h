//
// Created by ludger on 21.01.18.
//

#ifndef FLIGHTMANAGER_DATABASECOMMANDS_H
#define FLIGHTMANAGER_DATABASECOMMANDS_H

const char *createDatabaseCommand = "CREATE TABLE IF NOT EXISTS flights\n"
        "(\n"
        "  Id    INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,\n"
        "  start DATETIME                          NOT NULL,\n"
        "  end   DATETIME\n"
        ");\n"
        "CREATE UNIQUE INDEX IF NOT EXISTS flights_Id_uindex\n"
        "  ON flights (Id);\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS flightMode\n"
        "(\n"
        "  FlightId               INTEGER          NOT NULL,\n"
        "  ResetCount             UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp              UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  FlightMode             UNSIGNED INTEGER NOT NULL,\n"
        "  SeaLevelPressure       FLOAT,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS GpsVelocity\n"
        "(\n"
        "  FlightId         INTEGER          NOT NULL,\n"
        "  ResetCount       UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp        UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  Speed            FLOAT            NOT NULL,\n"
        "  CourseOverGround FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS AirVelocity\n"
        "(\n"
        "  FlightId   INTEGER          NOT NULL,\n"
        "  ResetCount UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp  UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  Speed      FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS Position\n"
        "(\n"
        "  FlightId           INTEGER          NOT NULL,\n"
        "  ResetCount         UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp          UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  Latitude           FLOAT            NOT NULL,\n"
        "  Longitude          FLOAT            NOT NULL,\n"
        "  GpsAltitude        FLOAT            NOT NULL,\n"
        "  RealTime           DATETIME         NOT NULL,\n"
        "  NumberOfSatellites UNSIGNED INTEGER NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS Altitude\n"
        "(\n"
        "  FlightId    INTEGER          NOT NULL,\n"
        "  ResetCount  UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp   UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  Altitude    FLOAT            NOT NULL,\n"
        "  RateOfClimb FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS Attitude\n"
        "(\n"
        "  FlightId       INTEGER          NOT NULL,\n"
        "  ResetCount     UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp      UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  CourseMagnetic FLOAT            NOT NULL,\n"
        "  Pitch          FLOAT            NOT NULL,\n"
        "  Roll           FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS StaticPressure\n"
        "(\n"
        "  FlightId    INTEGER          NOT NULL,\n"
        "  ResetCount  UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp   UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  Pressure    FLOAT            NOT NULL,\n"
        "  Temperature FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS PitotPressure\n"
        "(\n"
        "  FlightId    INTEGER          NOT NULL,\n"
        "  ResetCount  UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp   UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  Pressure    FLOAT            NOT NULL,\n"
        "  Temperature FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS GyroRaw\n"
        "(\n"
        "  FlightId   INTEGER          NOT NULL,\n"
        "  ResetCount UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp  UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  x          FLOAT            NOT NULL,\n"
        "  y          FLOAT            NOT NULL,\n"
        "  z          FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS MagRaw\n"
        "(\n"
        "  FlightId   INTEGER          NOT NULL,\n"
        "  ResetCount UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp  UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  x          FLOAT            NOT NULL,\n"
        "  y          FLOAT            NOT NULL,\n"
        "  z          FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS AccelRaw\n"
        "(\n"
        "  FlightId   INTEGER          NOT NULL,\n"
        "  ResetCount UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp  UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  x          FLOAT            NOT NULL,\n"
        "  y          FLOAT            NOT NULL,\n"
        "  z          FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS BatteryData\n"
        "(\n"
        "  FlightId   INTEGER          NOT NULL,\n"
        "  ResetCount UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp  UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  Voltage    FLOAT            NOT NULL,\n"
        "  Current    FLOAT            NOT NULL,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS OutputCommandSet\n"
        "(\n"
        "  FlightId   INTEGER          NOT NULL,\n"
        "  ResetCount UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp  UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  Yaw        INTEGER          NOT NULL,\n"
        "  Pitch      INTEGER          NOT NULL,\n"
        "  Thrust     INTEGER          NOT NULL,\n"
        "  Roll       INTEGER,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS CurrentCommand\n"
        "(\n"
        "  FlightId    INTEGER          NOT NULL,\n"
        "  ResetCount  UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp   UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  PitchAngle  INTEGER,\n"
        "  Altitude    FLOAT,\n"
        "  RateOfClimb FLOAT,\n"
        "\n"
        "  Heading     FLOAT,\n"
        "  RateOfTurn  INTEGER,\n"
        "\n"
        "  Throttle    INTEGER,\n"
        "  Speed       FLOAT,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");\n"
        "\n"
        "CREATE TABLE IF NOT EXISTS HomeBases\n"
        "(\n"
        "  FlightId                 INTEGER          NOT NULL,\n"
        "  ResetCount               UNSIGNED INTEGER NOT NULL,\n"
        "  Timestamp                UNSIGNED INTEGER NOT NULL,\n"
        "\n"
        "  Latitude                 FLOAT            NOT NULL,\n"
        "  Longitude                FLOAT            NOT NULL,\n"
        "  Altitude                 FLOAT            NOT NULL,\n"
        "\n"
        "  OrbitRadius              FLOAT,\n"
        "  OrbitUntilTargetAltitude BOOLEAN,\n"
        "  OrbitClockwise           BOOLEAN,\n"
        "  PRIMARY KEY (FlightId, ResetCount, Timestamp)\n"
        ");";

const char *connectionPragmaCommand = "PRAGMA journal_mode = WAL;\n"
        "PRAGMA synchronous = OFF;";

const char *startNewFlightCommand = "INSERT INTO flights (start) VALUES (datetime('now'));";

const char *flightModeCommand = "INSERT INTO flightMode (FlightId, ResetCount, Timestamp, FlightMode, SeaLevelPressure) VALUES (?999, ?998, ?1, ?2, ?17);";

const char *gpsVelocityCommand = "INSERT INTO GpsVelocity (FlightId, ResetCount, Timestamp, Speed, CourseOverGround) VALUES (?999, ?998, ?30, ?31, ?32);";

const char *airVelocityCommand = "INSERT INTO AirVelocity (FlightId, ResetCount, Timestamp, Speed) VALUES (?999, ?998, ?40, ?41);";

const char *positionCommand = "INSERT INTO Position (FlightId, ResetCount, Timestamp, Latitude, Longitude, GpsAltitude, RealTime, NumberOfSatellites) VALUES (?999, ?998, ?20, ?22, ?23, ?25, ?21, ?26);";

const char *altitudeCommand = "INSERT INTO Altitude (FlightId, ResetCount, Timestamp, Altitude, RateOfClimb) VALUES (?999, ?998, ?50, ?51, ?52);";

const char *attitudeCommand = "INSERT INTO Attitude (FlightId, ResetCount, Timestamp, CourseMagnetic, Pitch, Roll) VALUES (?999, ?998, ?60, ?61, ?62, ?63);";

const char *staticPressureCommand = "INSERT INTO StaticPressure (FlightId, ResetCount, Timestamp, Pressure, Temperature) VALUES (?999, ?998, ?70, ?71, ?72);";

const char *pitotPressureCommand = "INSERT INTO PitotPressure (FlightId, ResetCount, Timestamp, Pressure, Temperature) VALUES (?999, ?998, ?70, ?71, ?72);";

const char *gyroRawCommand = "INSERT INTO GyroRaw (FlightId, ResetCount, Timestamp, x, y, z) VALUES (?999, ?998, ?80, ?81, ?82, ?83);";

const char *magRawCommand = "INSERT INTO MagRaw (FlightId, ResetCount, Timestamp, x, y, z) VALUES (?999, ?998, ?80, ?81, ?82, ?83);";

const char *accelRawCommand = "INSERT INTO AccelRaw (FlightId, ResetCount, Timestamp, x, y, z) VALUES (?999, ?998, ?80, ?81, ?82, ?83);";

const char *batteryDataCommand = "INSERT INTO BatteryData (FlightId, ResetCount, Timestamp, Voltage, Current) VALUES (?999, ?998, ?90, ?91, ?92);";

const char *outputCommandSetCommand = "INSERT INTO OutputCommandSet (FlightId, ResetCount, Timestamp, Yaw, Pitch, Thrust, Roll) VALUES (?999, ?998, ?100, ?101, ?102, ?103, ?104);";

const char *currentCommandCommand = "INSERT INTO CurrentCommand (FlightId, ResetCount, Timestamp, PitchAngle, Altitude, RateOfClimb, Heading, RateOfTurn, Throttle, Speed) VALUES (?999, ?998, ?120, ?110, ?111, ?112, ?113, ?114, ?115, ?116);";

const char *homeBasesCommand = "INSERT INTO HomeBases (FlightId, ResetCount, Timestamp, Latitude, Longitude, Altitude, OrbitRadius, OrbitUntilTargetAltitude, OrbitClockwise) VALUES (?999, ?998,?200, ?201, ?202, ?203, ?204, ?205, ?206);";

const char *beginTransactionCommand = "BEGIN TRANSACTION;";

const char *endTransactionCommand = "END TRANSACTION;";

#endif //FLIGHTMANAGER_DATABASECOMMANDS_H
