
typedef struct { // Battery Limits
  
  uint16_t ChargeVoltage;          // U16 // 
  int16_t MaxChargingCurrent;      // S16
  int16_t MaxDischargingCurrent;   // S16
  uint16_t DischargeVoltageLimit;  // U16
} SmaCanMessage0x0351 ;

typedef struct  // to be removed or merged with SMACanMessage 0x0351
  {
    // CVL - 0.1V scale
    uint16_t chargevoltagelimit;
    // CCL - 0.1A scale
    int16_t maxchargecurrent;
    // DCL - 0.1A scale
    int16_t maxdischargecurrent;
    // Not currently used by Victron
    // 0.1V scale
    uint16_t dischargevoltage;
  }data351;

typedef struct  { // Soc and State of health
  
  uint16_t StateOfCharge;         // U16
  uint16_t StateOfHealth;         // U16
  uint16_t StateOfChargeHighRes;  // U16
}SmaCanMessage0x0355;

typedef struct 
  {
    uint16_t stateofchargevalue;
    uint16_t stateofhealthvalue;
    uint16_t highresolutionsoc;
  } VictronCanMessage0x355;


typedef struct  { // Battery actual values
  
  int16_t BatteryVoltage;      // S16
  int16_t BatteryCurrent;      // S16
  int16_t BatteryTemperature;  // S16
}SmaCanMessage0x0356;

typedef struct  { // not pylon tech
  
  uint32_t AlarmBitmask;    // 32 Bits
  uint32_t WarningBitmask;  // 32 Bits
}SmaCanMessage0x035A;

typedef struct  // pylon tech specific
{
  
  uint8_t ErrorBits;   // Overvoltage err, Undervoltage err, overtemperature err, under temp err, over current discharge, charge current error
  uint8_t SysErrorBits; // only 1 bit used for system error 3rd bit or location[2]
  uint8_t warningBits; // High voltage warn, Low voltage warn, hight temperature warn, low temp warn, high current discharge, charge current warn
  uint8_t internalerror; // 1 bit used for internal error 3rd bit or location[2]
  uint8_t SysStateBits; // 8 bits
}PylontechErrorWarnMessage0x0359;

typedef struct  // This may get removed in favour of the pylon tech message. but i am unsure what the other bytes do
  {
    // Protection - Table 1
    uint8_t byte0; // Overvoltage err, Undervoltage err, overtemperature err, under temp err, over current discharge, charge current error
    // Protection - Table 2
    uint8_t byte1; // only 1 bit used for system error 3rd bit or location[2]
    // Warnings - Table
    uint8_t byte2; // High voltage warn, Low voltage warn, hight temperature warn, low temp warn, high current discharge, charge current warn
    // Warnings - Table 4
    uint8_t byte3; // 1 bit used for internal error 3rd bit or location[2]
    // Quantity of banks in parallel
    uint8_t byte4; // 8 bits
    uint8_t byte5;
    uint8_t byte6;
    // Online address of banks in parallel - Table 5
    uint8_t byte7;
  }data359;

typedef struct 
{
    uint8_t byte0;
}data35c; // these structs come from DIY BMS, a decision needs to be made as to whether or not they stay here or move to the top of the file with the others

typedef struct   
{
    int16_t voltage;
    int16_t current;
    int16_t temperature;
}data356; // these structs come from DIY BMS, a decision needs to be made as to whether or not they stay here or move to the top of the file with the others

  
typedef struct  { // matches pylontech
  
  char Model[8] ;
}SmaCanMessage0x035E;


typedef struct  { // is not a pylon tech message
  
    uint16_t BatteryModel;
    uint16_t Firmwareversion;
    uint16_t OnlinecapacityinAh;
  // uint16_t CellChemistry;
  // uint8_t HardwareVersion[2];
  // uint16_t NominalCapacity;
  // uint8_t SoftwareVersion[2];
}VictronCanMessage0x035F;

typedef struct  {
  
  uint16_t MinCellvoltage;  // v * 1000.0f
  uint16_t MaxCellvoltage;  // v * 1000.0f
  uint16_t MinTemperature;  // v * 273.15f
  uint16_t MaxTemperature;  // v * 273.15f
}Victron_message_0x373;

typedef struct 
{
    //uint8_t _ID; // 0x372
    uint16_t numberofmodulesok;
    uint16_t numberofmodulesblockingcharge;
    uint16_t numberofmodulesblockingdischarge;
    uint16_t numberofmodulesoffline;
} Victron_message_0x372;



typedef struct  {
  uint8_t _ID;
  int64_t Alive;
}NetworkAlivePacket;

typedef struct {
    uint8_t _ID;
    char text[8];
} SmaCanMessageCustom;

// Structs for each specific message

typedef struct {    
    uint8_t text[8]; // Placeholder for text data
} Victron_Message_0x374;

typedef struct {    
    uint8_t text[8]; // Placeholder for text data
} Victron_Message_0x375;

typedef struct {
    
    uint8_t text[8]; // Placeholder for text data
} Victron_Message_0x376;

typedef struct {
    
    uint8_t text[8]; // Placeholder for text data
} Victron_Message_0x377;

typedef struct 
    {
      char text[8];
    } candata;

NetworkAlivePacket alivePacket = {(uint8_t)0x0305, 1};