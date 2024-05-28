#ifndef BMS_STATE_H
#define BMS_STATE_H

#include <stdbool.h>

#define MAX_CELLS 20
#define MAX_BMS_UNITS 10


// Data Structs for storing adjacent BMS data
// Struct to represent a single cell
typedef struct {
    float voltage;
} Cell;

// Struct to represent temperature probes
typedef struct {
    float temp1;
    float temp2;
} TemperatureProbes;

// Struct to represent a single BMS unit
typedef struct {
    int batteryID;  // Unique identifier for the battery
    Cell cells[MAX_CELLS];
    float batteryVoltage;
    float batteryCurrent;
    float batterySOC;
    TemperatureProbes temperatureProbes;
    bool isChargeEnabled;
    bool isDischargeEnabled;
    bool isBalancing;
    bool overTemp;
    bool underTemp;
    bool overCurrent;
    bool overVoltage;
    bool underVoltage;
    bool polling;

} BMS;

typedef struct
{
    int batteryID;
    float ChargeLimit;
    float DischageLitmit;
    float Capacity;
    int lastTimeSeen;

}BMS_Limits;

// Struct to hold the state of the BMS system
typedef struct {
    BMS localBMS;
    BMS otherBMS[MAX_BMS_UNITS];
    int numOtherBMS;
} BMSState;

#endif /* BMS_STATE_H */

//
#ifndef BMS_SHARED_MSGS
#define BMS_SHARED_MSGS

// Define message types
#define MSG_BATTERY_INFO 0x01
#define MSG_CELL_VOLTAGES 0x02
#define MSG_TEMPERATURE_PROBES 0x03
#define MSG_STATUS_FLAGS 0x04

// Cell Voltages Message
typedef struct {
    uint8_t messageType;    // Message type (0x02)
    uint8_t index;           // Index of the first cell voltage
    float cellVoltages[2];  // Array of cell voltages
} CellVoltagesMessage;

// Battery Voltage and Current Message
typedef struct {
    uint8_t messageType;    // Message type (0x01)
    uint8_t packID;         // Pack ID
    float batteryVoltage;   // Battery voltage
    float batteryCurrent;   // Battery current
    float batterySOC;       // Battery state of charge
} BatteryInfoMessage;

// Temperature Probes Message
typedef struct {
    uint8_t messageType;    // Message type (0x03)
    uint8_t packID;         // Pack ID
    float tempProbe1;       // Temperature probe 1
    float tempProbe2;       // Temperature probe 2
} TemperatureProbesMessage;

// Status Flags Message
// Status Flags Message
typedef struct {
    uint8_t messageType;    // Message type (0x04)
    uint8_t packID;         // Pack ID
    bool isChargeEnabled;   // Charge enabled flag
    bool isDischargeEnabled;// Discharge enabled flag
    bool isBalancing;       // Balancing flag
    bool overTemp;          // Over temperature flag
    bool underTemp;         // Under temperature flag
} StatusFlagsMessage;

// BMS Limits Message
typedef struct {
    uint8_t messageType;    // Message type (0x05)
    uint8_t packID;         // Pack ID
    float chargeLimit;      // Charge limit
    float dischargeLimit;   // Discharge limit
    float capacity;         // Capacity
    int lastTimeSeen;       // Last time seen
} BMSLimitsMessage;

// Broadcast Complete Message
typedef struct {
    uint8_t messageType;    // Message type (0x05)
    uint8_t packID;         // Pack ID
    uint16_t timeout;       // Timeout time till next broadcast (in milliseconds)
} BroadcastCompleteMessage;

#endif