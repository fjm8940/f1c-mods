/* F1C vehicle/driver struct — stride 0x48C0, 67 named fields.
 * Source: docs/26 (F1CUI runtime-validated) + docs/13. Field offsets are
 * community-proven via F1CUI's live memory reader/writer.
 *
 * Array accessor: base pointer lives at 0x005F0DEC (DriverDataArrayBase).
 *   Driver *d = f1c_driver(i);   // i = vehicle index (0..VehicleCount-1)
 *
 * WRITABLE LEVERS proven by F1CUI writer methods (safe to poke = inputs):
 *   tirePuncture_FL/FR/RL/RR  (setFakePuncture)
 *   EngineMaxRPM              (setLimitToAIEngines — cap AI revs)
 *   fuelConsumptionFactor@0xD84 (setLimitToAISpeed — F1CUI caps AI speed here)
 *   nextChosenTires_x@0x139C  (suppressUselessPits)
 *   pitStopsSpeed@0x12A8      (recalcPitStopsSpeed)
 * Everything else: preflight-probe before poking (input vs recomputed output).
 */
#ifndef F1C_DRIVER_H
#define F1C_DRIVER_H
#include <stdint.h>

typedef struct Driver {
    char _gap_0000[180];
    int32_t numberOfLaps;  // 0x00B4 int
    char _gap_00b8[140];
    int32_t numberOfLaps_x;  // 0x0144 int
    char _gap_0148[64];
    int32_t nextChosenTires;  // 0x0188 int
    char _gap_018c[24];
    float elapsedTime;  // 0x01A4 float
    char _gap_01a8[32];
    int32_t callToBox;  // 0x01C8 int
    char _gap_01cc[16];
    float maxSpeed;  // 0x01DC float
    float timeOfNextExit;  // 0x01E0 float
    char _gap_01e4[32];
    int32_t driverStatus;  // 0x0204 int
    char _gap_0208[188];
    float trackPartFromPitExit;  // 0x02C4 float
    char _gap_02c8[12];
    float actualSpeed;  // 0x02D4 float
    char _gap_02d8[824];
    int32_t noOfTrackLimits;  // 0x0610 int
    char _gap_0614[24];
    int32_t continuousNonRace;  // 0x062C ?
    char _gap_0630[44];
    float totalTimeWithoutStartDelay;  // 0x065C float
    char _gap_0660[16];
    float bestI2;  // 0x0670 float
    float bestI1;  // 0x0674 float
    float bestLap;  // 0x0678 float
    float totalTimeOnFinishLine;  // 0x067C float
    int32_t lapOfRun;  // 0x0680 int
    char _gap_0684[32];
    int32_t noOfPenaltiesToProcess;  // 0x06A4 int
    char _gap_06a8[9];
    int8_t DNF_DSQ_flag;  // 0x06B1 byte
    char _gap_06b2[2];
    int32_t position;  // 0x06B4 int
    float timeOfQualySession;  // 0x06B8 float
    int32_t plannedPositionAtStart;  // 0x06BC int
    float lastI2;  // 0x06C0 float
    float lastI1;  // 0x06C4 float
    float lastLap;  // 0x06C8 float
    float trackPartExact;  // 0x06CC float
    char _gap_06d0[8];
    int32_t lapNumber;  // 0x06D8 int
    char _gap_06dc[716];
    int32_t carConditions;  // 0x09A8 int
    char _gap_09ac[876];
    int32_t gearNumber;  // 0x0D18 int
    char _gap_0d1c[84];
    float EngineMaxRPM;  // 0x0D70 float
    float EngineRPM;  // 0x0D74 float
    char _gap_0d78[12];
    float fuelConsumptionFactor;  // 0x0D84 float
    char _gap_0d88[8];
    float fuelLevel;  // 0x0D90 float
    char _gap_0d94[4];
    int32_t tireChoice;  // 0x0D98 int
    char _gap_0d9c[84];
    float tirePuncture_FL;  // 0x0DF0 float
    char _gap_0df4[244];
    float tirePuncture_FR;  // 0x0EE8 float
    char _gap_0eec[244];
    float tirePuncture_RL;  // 0x0FE0 float
    char _gap_0fe4[244];
    float tirePuncture_RR;  // 0x10D8 float
    char _gap_10dc[180];
    float BrakeFactor;  // 0x1190 float
    char _gap_1194[4];
    float ThrottleFactor;  // 0x1198 float
    char _gap_119c[192];
    float elapsedTime_x;  // 0x125C float
    char _gap_1260[20];
    float pitTime_0;  // 0x1274 float
    char _gap_1278[24];
    float nextStintFuel;  // 0x1290 float
    float pitTime_1;  // 0x1294 float
    char _gap_1298[8];
    float lastPitTime;  // 0x12A0 float
    float pitTime_2;  // 0x12A4 float
    float pitStopsSpeed;  // 0x12A8 float
    char _gap_12ac[24];
    int32_t pittingStatus;  // 0x12C4 int
    char _gap_12c8[4];
    float theoricNumberOfLapsWithOneLitre;  // 0x12CC float
    char _gap_12d0[28];
    int32_t unlockChoosableTires;  // 0x12EC int
    char _gap_12f0[172];
    int32_t nextChosenTires_x;  // 0x139C int
    char _gap_13a0[60];
    float nextStintFuel_x;  // 0x13DC float
    char _gap_13e0[56];
    int32_t penalties_0;  // 0x1418 int
    int32_t penalties_1;  // 0x141C int
    int32_t penalties_2;  // 0x1420 int
    char _gap_1424[8252];
    int32_t numberOfHitsInRace;  // 0x3460 int
    char _gap_3464[584];
    int32_t rpmLimiter;  // 0x36AC int
    char _gap_36b0[180];
    int32_t driverMemoryAddress;  // 0x3764 long
    char _gap_3768[1944];
    float coordinatesInTrack;  // 0x3F00 float
    char _gap_3f04[4];
    float coordinatesInTrack_x;  // 0x3F08 float
    char _gap_3f0c[188];
    float coordinatesInTrack_x_x;  // 0x3FC8 float
    char _gap_3fcc[4];
    float coordinatesInTrack_x_x_x;  // 0x3FD0 float
    char _gap_3fd4[40];
    float directionInTrack;  // 0x3FFC float
    char _gap_4000[4];
    float directionInTrack_x;  // 0x4004 float
    char _gap_4008[1608];
    int32_t isHumanInControl;  // 0x4650 int
    char _gap_4654[192];
    int32_t postProcessInput;  // 0x4714 ?
    char _tail[424];
} Driver;

/* array base is a POINTER stored at 0x005F0DEC */
#define F1C_DRIVER_BASE_PTR (*(char **)0x005F0DEC)
static inline Driver *f1c_driver(int i) {
    return (Driver *)(F1C_DRIVER_BASE_PTR + (i) * 0x48C0);
}
#endif /* F1C_DRIVER_H */
