
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/twai.h"

#include "esp_log.h" // added to allow logging to the terminal by LN /2/1/24
#include "esp_task_wdt.h" // add to feed the WDT so we dont time out
#include "JKBMSWrapper.h"
#include "jkbmsinterface.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "VictronMessages.h"
#include "WiFiProvisioning.h"
#include "bms_state.h"
#include "mqttclient.h"
#define TAG "VictronCanIntegration"

twai_message_t rx_message; // Object to hold incoming messages
// esp_mqtt_client_handle_t client;

int batteryID = 4;
int cellDelta = 0;

int cellDeltaWarn = 400;
int cellDeltaError = 500;
int batHealth = 100;

//char topicprefix[] = "batteryStats/";
bool mqttConnected = false;

bool OverVoltAlarmRaised = false;
bool UnderVoltAlarmRaised = false;
bool OverCurrentAlarmRaised = false;
bool OverTempAlarmRaised = false;
bool LowTempAlarmRaised = false;

// define static thresholds
float VOLTAGE_THRESHOLD = 59.0;  // Example value for overvoltage threshold
float U_VOLTAGE_THRESHOLD = 46.0;  // Example value for overvoltage threshold
float CURRENT_THRESHOLD = 200.0;  // Example value for overcurrent threshold
float OVERTEMP_THRESHOLD = 60;
float Charge_Current_Target = 0;
float defaultAlarmClearModifier = 0.9; // when a value drops 10 below the threshold it should clear the alarm

float currentPackV = 0; // current pack voltage. 
#define Can_TX_GPIO_NUM             21
#define Can_RX_GPIO_NUM             22

static const char* hostname = "JKBMS";

BMSState bmsState;

char cfg_PackName[64];
char cfg_TopicName[64];

void SetBankAndModuleText(char *buffer, uint8_t cellid);

// pack values held here

int16_t packvoltage;
int16_t packcurrent;
int16_t packtemperature;

int8_t packStateOfCharge;
int8_t packStateOfHealth;
int8_t packStateOfChargeHR;
 
bool packOverVoltAlarmRaised = false;
bool packUnderVoltAlarmRaised = false;
bool packOverCurrentAlarmRaised = false;
bool packOverTempAlarmRaised = false;
bool packLowTempAlarmRaised = false;
bool packChargeEnabled = false;
bool packDischargeEnabled = false;

bool get_mqtt_config_loaded();
bool get_mqtt_configured();
bool get_wifiStatus();
bool shouldconnectMqtt = true;

twai_handle_t twai_bus_0;
twai_handle_t twai_bus_1;

// Function to update BMS info based on received CAN message
void updateBMSInfo(BMSState *bmsState, void *receivedMessage) {
    // Cast the received message to the appropriate struct type based on message type
    switch (((uint8_t *)receivedMessage)[0]) {
        case MSG_BATTERY_INFO: {
            BatteryInfoMessage *batteryInfoMessage = (BatteryInfoMessage *)receivedMessage;
            if (batteryInfoMessage->packID == bmsState->localBMS.batteryID) {
                // Update local BMS info
                bmsState->localBMS.batteryVoltage = batteryInfoMessage->batteryVoltage;
                bmsState->localBMS.batteryCurrent = batteryInfoMessage->batteryCurrent;
                bmsState->localBMS.batterySOC = batteryInfoMessage->batterySOC;
            } else {
                // Update other BMS info
                for (int i = 0; i < bmsState->numOtherBMS; ++i) {
                    if (bmsState->otherBMS[i].batteryID == batteryInfoMessage->packID) {
                        bmsState->otherBMS[i].batteryVoltage = batteryInfoMessage->batteryVoltage;
                        bmsState->otherBMS[i].batteryCurrent = batteryInfoMessage->batteryCurrent;
                        bmsState->otherBMS[i].batterySOC = batteryInfoMessage->batterySOC;
                        break;
                    }
                }
            }
            break;
        }
        // Similar cases for other message types (MSG_CELL_VOLTAGES, MSG_TEMPERATURE_PROBES, MSG_STATUS_FLAGS, MSG_BMS_LIMITS)...
        default:
            // Unknown message type, do nothing
            break;
    }
}

void copy_array(int source[], int destination[], int size) {
    for (int i = 0; i < size; i++) {
        destination[i] = source[i];
    }
}

char *format_json(int8_t packStateOfCharge, int8_t packStateOfHealth, int8_t packStateOfChargeHR ,float packvoltage, float packcurrent, int16_t packtemperature,
                  bool OverVoltAlarmRaised, bool UnderVoltAlarmRaised, bool OverCurrentAlarmRaised,
                  bool OverTempAlarmRaised, bool LowTempAlarmRaised, bool ChargeEnabled, bool DischargeEnabled) {
    // Buffer to store the formatted JSON object
    static char json_buffer[512]; // Adjust the buffer size as needed   

    // Format the JSON object
    snprintf(json_buffer, sizeof(json_buffer),
             "{"
             "\"State of Charge\": %i,"
             "\"State of Health\": %i,"
             "\"SOC Hi Res\": %i,"
             "\"packvoltage\": %.2f," // Use %f for float values
             "\"packcurrent\": %.2f," // Use %f for float values
             "\"packtemperature\": %d,"
             "\"OverVoltAlarmRaised\": %s,"
             "\"UnderVoltAlarmRaised\": %s,"
             "\"OverCurrentAlarmRaised\": %s,"
             "\"OverTempAlarmRaised\": %s,"
             "\"LowTempAlarmRaised\": %s,"
             "\"ChargeEnabled\": %s,"
             "\"DischargeEnabled\": %s"
             "}",
             packStateOfCharge, packStateOfHealth, packStateOfChargeHR,
             (packvoltage * 0.01), packcurrent, packtemperature,
             OverVoltAlarmRaised ? "true" : "false",
             UnderVoltAlarmRaised ? "true" : "false",
             OverCurrentAlarmRaised ? "true" : "false",
             OverTempAlarmRaised ? "true" : "false",
             LowTempAlarmRaised ? "true" : "false",
             ChargeEnabled ? "true" : "false",
             DischargeEnabled ? "true" : "false");
    
    return json_buffer;
}

void calculateHealth()
{
  if(cellDelta >= cellDeltaError)
  {
    batHealth = 0;
    return;
  }
  if(cellDelta < cellDeltaError && cellDelta > cellDeltaWarn)
  {
    batHealth = cellDeltaError -cellDeltaWarn;
    return;
  }
  batHealth = 100 - (cellDelta / cellDeltaWarn *100);


}

char* concat_str(const char* str1, const char* str2, const char* str3) {
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);
    size_t len3 = strlen(str3);

    // Allocate memory for the concatenated string
    char* result = (char*)malloc(len1 + len2 + len3 + 1); // +1 for the null terminator

    if (result == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        exit(EXIT_FAILURE);
    }

    // Copy the strings into the result buffer
    strcpy(result, str1);
    strcat(result, str2);
    strcat(result, str3);

    return result;
}

void SetFloatAsText(char *buffer, float floatValue) {
    // Use snprintf to convert float to text and store in buffer
    snprintf(buffer, 20, "%.2f", floatValue); // "%.2f" formats the float to two decimal places
}

void send_canbus_message(uint32_t id, uint8_t *data, size_t data_length) {
    //printf("Queuing Messages\n");
    for (int i = 0; i < 15; i++) {        
        twai_message_t message;
        // Populate the fields of 'message' structure
        message.identifier = id; // Set the CAN message ID
        message.data_length_code = data_length; // Set the DLC (Data Length Code)
        memcpy(message.data, data, data_length); // Copy the data to the message

        esp_err_t resp = twai_transmit(&message, pdMS_TO_TICKS(1000));

        if (resp == ESP_OK) {
            //printf("Message queued for transmission - code: %d\n", resp);
        } else {
            //printf("Failed to queue message for transmission. Error code: %d\n", resp);
        }
    }
}

void pylon_message_359()
{
    data359 data;
    memset(&data, 0, sizeof(data359));
    //data.byte0 = 0;  

// Set the bit for OverVoltage alarm
if (OverVoltAlarmRaised) {
  printf("\n Raising an over voltage alarm for no reason what so ever ****** \n");
    //data.byte0 |= 0b00000010;  // Set the second bit
    data.byte2 = 0b00000010; 
}

// Set the bit for UnderVoltage alarm
if (UnderVoltAlarmRaised) {
    //data.byte0 |= 0b00000100;  // Set the third bit
    data.byte2 |= 0b00000100;
}

// Set the bit for High Temperature alarm
if (OverTempAlarmRaised) {
    //data.byte0 |= 0b00001000;  // Set the fourth bit
    data.byte2 |= 0b00001000;
}

// Set the bit for Low Temperature alarm
if (LowTempAlarmRaised) {
    // data.byte0 |= 0b00010000;  // Set the fifth bit
    data.byte2 |= 0b00010000;
}

data.byte2 = 0;
// Other bytes (assuming these are set according to some other logic in your program)
 // this is high voltage
// data.byte2 |= 0b00000100;
// data.byte2 |= 0b00001000;
// data.byte2 |= 0b00010000;
data.byte3 = 0;
data.byte4 = 1;
data.byte5 = 0x50; // 'P'
data.byte6 = 0x4E; // 'N'
    //alivePacket 
    send_canbus_message(0x359, (uint8_t *)&data, sizeof(data359));
}

// 0x35C – C0 00 – Battery charge request flags //ylon_message_35c() pylon_message_35e()
void pylon_message_35c()
{

  data35c data;
  // bit 0/1/2/3 unused
  // bit 4 Force charge 2
  // bit 5 Force charge 1
  // bit 6 Discharge enable
  // bit 7 Charge enable
  data.byte0 = 0;


    data.byte0  |= 0b10000000;
    data.byte0  |= 0b01000000;


  send_canbus_message(0x35c, (uint8_t *)&data, sizeof(data35c));
}

// 0x35E – 50 59 4C 4F 4E 20 20 20 – Manufacturer name ("PYLON ")
void pylon_message_35e()
{
  // Send 8 byte "magic string" PYLON (with 3 trailing spaces)
  // const char pylon[] = "\x50\x59\x4c\x4f\x4e\x20\x20\x20";
  uint8_t pylon[] = { (uint8_t)"B", (uint8_t)"A", (uint8_t)"D", (uint8_t)"I", (uint8_t)"D", (uint8_t)"E", (uint8_t)"A", (uint8_t)"!"};
  send_canbus_message(0x35e, (uint8_t *)&pylon, sizeof(pylon) - 1);
}

void Set_Charge_Limits()
{


}


void publish_cell_voltages_to_mqtt() {
    if (mqttConnected) {
        char topic[50];
        char data_str[50];

        int cell_count = get_cell_count();
        for (int cellID = 1; cellID <= cell_count; cellID++) {
            // Get voltage for the current cell
            float cell_voltage = get_cell_voltage(cellID);

            // Construct MQTT topic
            snprintf(topic, sizeof(topic), "BMS/shelf%d/cells/cell%d", batteryID, cellID);

            // Construct data string
            snprintf(data_str, sizeof(data_str), "%.2f", cell_voltage);

            // Publish MQTT message
            mqtt_publish(topic, data_str, strlen(data_str));
        }
    }
}
// Battery voltage - 0x356 – 4e 13 02 03 04 05 – Voltage / Current / Temp
void pylon_message_356_2() // i created this second message type as i was seeing some unusual behavior. i am quite certain that there is nothing wrong with the first implementation
{
  data356 data;
  // If current shunt is installed, use the voltage from that as it should be more accurate
    packcurrent = get_pack_current() * 10;
    packvoltage = get_pack_voltage() * 100.0;
    data.voltage = packvoltage;
    data.current = packcurrent;
  // Temperature 0.1 C using external temperature sensor 
    data.temperature = (int16_t)25 * (int16_t)10;
  send_canbus_message(0x356, (uint8_t *)&data, sizeof(data356));

   if (mqttConnected) {
        char topic[50];
        char data_str[100];

        // Construct topic
        
        snprintf(topic, sizeof(topic), "BMS/shelf%d/pylonMessage/356_2", batteryID);

        // Construct data string
        snprintf(data_str, sizeof(data_str), "Voltage: %.2fV, Current: %.2fA, Temperature: %.1f°C",
                 (float)data.voltage / 100.0, (float)data.current / 10.0, (float)data.temperature / 10);

        // Publish MQTT message
        mqtt_publish(topic, data_str, strlen(data_str));
    }
}

void pylon_message_356()
{
  printf("Sending 356 packet, Voltage Current and Temperature");
  data356 data;
    currentPackV = 0;
    float currentA =0;
    currentPackV = get_pack_voltage() ;  
    
    currentA = get_pack_current();
    float BMSTemp = get_temps(2);
    
    // for troubleshooting
    printf("Intermeddiate Voltage: %f\n",currentPackV);
    printf("Intermeddiate Amps: %f\n",currentA);
    printf("Intermeddiate BMSTemp: %f\n",BMSTemp );
    // end of  troubleshooting   

    packvoltage = currentPackV * 100 ;//48 * 100.0;
    packcurrent = currentA  * 10;// * 10;  
    packtemperature = BMSTemp * 0.001;//(int16_t)25 * (int16_t)10; // assuming 25c 

    data.voltage = packvoltage;
    data.current = packcurrent;  
    data.temperature = packtemperature;//(int16_t)25 * (int16_t)10; // assuming 25c 
    send_canbus_message(0x356, (uint8_t *)&data, sizeof(data356));
    Set_Charge_Limits();
    
    if (mqttConnected) {
        char topic[50];
        char data_str[100];

        // Construct topic
        
        snprintf(topic, sizeof(topic), "BMS/shelf%d/pylonMessage/356", batteryID);

        // Construct data string
        snprintf(data_str, sizeof(data_str), "Voltage: %.2fV, Current: %.2fA, Temperature: %.2f°C",
                 currentPackV, currentA, BMSTemp);

        // Publish MQTT message
        mqtt_publish(topic, data_str, strlen(data_str));
    }
}

void victron_message_373(float lowestcellval, float highestcellval, float lowestcelltempval, float highestcelltempval )
{
//float lowestcellval, float highestcellval, float lowestcelltempval, float highestcelltempval 
  Victron_message_0x373 data;
  
  data.MinTemperature = 273 + lowestcelltempval; // rules.lowestExternalTemp; current lowest temp
  data.MaxTemperature = 273 + highestcelltempval ; // rules.highestExternalTemp; current highest Temp
  data.MaxCellvoltage = highestcellval * 1000;//rules.highestCellVoltage; current higest cell
  data.MinCellvoltage = lowestcellval  * 1000;//rules.lowestCellVoltage; current lowest cell

  send_canbus_message(0x373, (uint8_t *)&data, sizeof(Victron_message_0x373));
      if (mqttConnected) {
        char topic[50];
        char data_str[100];
        // Construct topic        
        snprintf(topic, sizeof(topic), "BMS/shelf%d/victronMessage/373", batteryID);
        // Construct data string
        snprintf(data_str, sizeof(data_str), "MinTemperature: %iK, MaxTemperature: %iK, MaxCellVoltage: %imV, MinCellVoltage: %imV",
                 data.MinTemperature, data.MaxTemperature, data.MaxCellvoltage, data.MinCellvoltage);
        // Publish MQTT message
        mqtt_publish(topic, data_str, strlen(data_str));
    }
}

void victron_message_372(int ModulesOK, int ModulesBlockingC, int ModulesBlockingD, int OfflineModules)
{
  Victron_message_0x372 data;

  data.numberofmodulesok = ModulesOK ;//TotalNumberOfCells() - rules.invalidModuleCount; // these are overall units
  data.numberofmodulesblockingcharge = ModulesBlockingC; // units currently charging
  data.numberofmodulesblockingdischarge = ModulesBlockingD; // units currently blocking charging
  data.numberofmodulesoffline = OfflineModules;//rules.invalidModuleCount; // modules that are shut off - or shelves
  
  send_canbus_message(0x372, (uint8_t *)&data, sizeof(Victron_message_0x372));

    // Publish to MQTT if connected
    if (mqttConnected) {
        char topic[50];
        char data_str[150];

        // Construct topic
        // snprintf(topic, sizeof(topic), "victronMessage/372");
        snprintf(topic, sizeof(topic), "BMS/shelf%d/victronMessage/372", batteryID);

        // Construct data string
        snprintf(data_str, sizeof(data_str), "NumberOfModulesOK: %d, NumberOfModulesBlockingCharge: %d, NumberOfModulesBlockingDischarge: %d, NumberOfModulesOffline: %d",
                 ModulesOK, ModulesBlockingC, ModulesBlockingD, OfflineModules);

        // Publish MQTT message
        mqtt_publish(topic, data_str, strlen(data_str));
    }
}

void victron_message_351(int numactiveErrors, uint16_t CVL, int16_t MCC, int16_t MDC, uint16_t DV)
{

  data351 data;
  // do some maths here to figure out the charge shiz and maybe set alarms is we need to
  // Defaults (do nothing)
  // Don't use zero for voltage - this indicates to Victron an over voltage situation, and Victron gear attempts to dump
  // the whole battery contents!  (feedback from end users)
  data.chargevoltagelimit = CVL /100 ;
  data.maxchargecurrent = MCC;
  // Discharge settings....
  data.maxdischargecurrent = 0; // disabled discharge
  data.dischargevoltage = DV;//mysettings.dischargevolt;
  data.maxdischargecurrent = MDC;//mysettings.dischargecurrent;

  send_canbus_message(0x351, (uint8_t *)&data, sizeof(data351));
  // Publish to MQTT if connected
    if (mqttConnected) {
        char topic[50];
        char data_str[150];

        // Construct topic        
        snprintf(topic, sizeof(topic), "BMS/shelf%d/victronMessage/351", batteryID);

        // Construct data string
        snprintf(data_str, sizeof(data_str), "ChargeVoltageLimit: %iV, MaxChargeCurrent: %d, MaxDischargeCurrent: %d, DischargeVoltage: %d, MaxDischargeCurrent: %d",
                 data.chargevoltagelimit, data.maxchargecurrent, data.maxdischargecurrent, data.dischargevoltage, data.maxdischargecurrent);

        // Publish MQTT message
        mqtt_publish(topic, data_str, strlen(data_str));
    }
}

// S.o.C value
void victron_message_355()
{
    VictronCanMessage0x355 data;
    // 0 SOC value un16 1 %
    packStateOfCharge =  (int8_t)get_state_of_Charge();
    data.stateofchargevalue = packStateOfCharge; //;rules.StateOfChargeWithRulesApplied(&mysettings, currentMonitor.stateofcharge);
    // 2 SOH value un16 1 %
    packStateOfHealth = batHealth;
    data.stateofhealthvalue = packStateOfHealth; // calculate this based on delta difference in the cells 
    packStateOfChargeHR = get_state_of_Charge();
    data.highresolutionsoc = packStateOfChargeHR;
    
    send_canbus_message(0x355, (uint8_t *)&data, sizeof(VictronCanMessage0x355));
    
    // Publish to MQTT if connected
    if (mqttConnected) {
        char topic[50];
        char data_str[100];
        // Construct topic including shelf number
        snprintf(topic, sizeof(topic), "BMS/shelf%d/victronMessage/355", batteryID);
        // Construct data string
        snprintf(data_str, sizeof(data_str), "StateOfCharge: %d%%, StateOfHealth: %d%%, HighResolutionStateOfCharge: %d%%",
                 data.stateofchargevalue, data.stateofhealthvalue, data.highresolutionsoc);
        // Publish MQTT message
        mqtt_publish(topic, data_str, strlen(data_str));
    }
}
// i would like to break these up in to individual messages for keeping this clean
// but also at some point multiple variables for multiple BMS's will need to be shared amongst each other.
// the message creation will have to take all the BMS's in to account
void victron_message_374_375_376_377(int lowestcellnum, float lowestcellval, int highestcellnum, float highestcellval, int lowestcelltempid, float lowestcelltempval, int highestcelltempid, float highestcelltempval )
{
  struct candata
  {
    char text[8];
  };

  candata data;
    highestcellnum = get_cell_H_n();
    highestcellval = get_cell_H_v();
    lowestcellnum = get_cell_L_n();
    lowestcellval = get_cell_L_v();

    SetFloatAsText(data.text, lowestcellval );//;2.8);;
    printf(data.text);
    SetBankAndModuleText(data.text, lowestcellnum + ((batteryID * 16) -1 ));////rules.address_LowestCellVoltage);
    send_canbus_message(0x374, (uint8_t *)&data, sizeof(candata));

    SetFloatAsText(data.text, highestcellval );//;2.8);;  
    SetBankAndModuleText(data.text, highestcellnum + ((batteryID * 16) -1));// rules.address_HighestCellVoltage);  
    send_canbus_message(0x375, (uint8_t *)&data, sizeof(candata));

    SetFloatAsText(data.text, lowestcelltempval );//;2.8);;  
    SetBankAndModuleText(data.text, lowestcelltempid  ); //rules.address_lowestExternalTemp);  
    send_canbus_message(0x376, (uint8_t *)&data, sizeof(candata));

    SetFloatAsText(data.text, highestcelltempval );//;2.8);;
    SetBankAndModuleText(data.text, highestcelltempid);//rules.address_highestExternalTemp);    
    send_canbus_message(0x377, (uint8_t *)&data, sizeof(candata));

    victron_message_373(lowestcellval, highestcellval, lowestcelltempval, highestcelltempval );
    // mqtt send 0x374
    if (mqttConnected) {
        char topic[50];
        char data_str[50];
        // Construct MQTT topic for message 0x374
        snprintf(topic, sizeof(topic), "BMS/shelf%d/victronMessage/374", batteryID);
        // Construct data string for message 0x374
        snprintf(data_str, sizeof(data_str), "LowestCellNumber: %d, LowestCellVoltage: %.2fV", lowestcellnum, lowestcellval);
        // Publish MQTT message for message 0x374
        mqtt_publish(topic, data_str, strlen(data_str));
    }
    // Publish to MQTT for message 0x375 if connected
    if (mqttConnected) {
        char topic[50];
        char data_str[50];

        // Construct MQTT topic for message 0x375
        snprintf(topic, sizeof(topic), "BMS/shelf%d/victronMessage/375", batteryID);

        // Construct data string for message 0x375
        snprintf(data_str, sizeof(data_str), "HighestCellNumber: %d, HighestCellVoltage: %.2fV", highestcellnum, highestcellval);

        // Publish MQTT message for message 0x375
        mqtt_publish(topic, data_str, strlen(data_str));
    }
    // Publish to MQTT for message 0x376 if connected
    if (mqttConnected) {
        char topic[50];
        char data_str[50];

        // Construct MQTT topic for message 0x376
        snprintf(topic, sizeof(topic), "BMS/shelf%d/victronMessage/376", batteryID);

        // Construct data string for message 0x376
        snprintf(data_str, sizeof(data_str), "LowestCellTempID: %d, LowestCellTempValue: %.2f°C", lowestcelltempid, lowestcelltempval);

        // Publish MQTT message for message 0x376
        mqtt_publish(topic, data_str, strlen(data_str));
    }
    if (mqttConnected) {
        char topic[50];
        char data_str[100];

        // Construct MQTT topic for message 0x377
        snprintf(topic, sizeof(topic), "BMS/shelf%d/victronMessage/377", batteryID);

        // Construct data string for message 0x377
        snprintf(data_str, sizeof(data_str), "HighestCellTempID: %d, HighestCellTempValue: %.2f°C", highestcelltempid, highestcelltempval);

        // Publish MQTT message for message 0x377
        mqtt_publish(topic, data_str, strlen(data_str));
    }
  //}
}


// Transmit the hostname via two CAN Messages (from diy bms)
void victron_message_370_371()
{
  char buffer[16+1];
  memset( buffer, 0, sizeof(buffer) );
  strncpy(buffer, hostname,sizeof(buffer)); // static const char* hostname = "JKBMS";

  send_canbus_message(0x370, (uint8_t *)&buffer[0], 8); //send_canbus_message(0x370, (const uint8_t *)&buffer[0], 8);
  send_canbus_message(0x371, (uint8_t *)&buffer[8], 8); //send_canbus_message(0x371, (const uint8_t *)&buffer[8], 8); 
}

void victron_message_35f(int BatModel, int OnlineCapacity, int firmwareMajor, int firmwareMinor)
{
  VictronCanMessage0x035F data;

  data.BatteryModel = BatModel;
  // Need to swap bytes for this to make sense.
  data.Firmwareversion = ((uint16_t)firmwareMajor << 8) | firmwareMinor; // figure out what goes in here

  data.OnlinecapacityinAh = OnlineCapacity ;// this value will need to be a total of all batteries

  send_canbus_message(0x35f, (uint8_t *)&data, sizeof(VictronCanMessage0x035F));
}


void send_victron_update(){
        // This sends the product name
        vTaskDelay(pdMS_TO_TICKS(100));
        pylon_message_35e(); // send pylon
        
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // sends battery model, online capcity and  firmware major, firmware minor version 
        //victron_message_35f(0, 200, 1, 2);
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // sends host name 
        victron_message_370_371();
        vTaskDelay(pdMS_TO_TICKS(100));

        //int lowestcellnum, float lowestcellval, int highestcellnum, float highestcellval, int lowestcelltempid, float lowestcelltempval, int highestcelltempid, float highestcelltempval 
        victron_message_374_375_376_377(1,2.8,2,3.6,3,5.5,4,35); 
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // state of charge, state of health, highres state of charge
        victron_message_355();  
        
        vTaskDelay(pdMS_TO_TICKS(100));

        // Sends Charge Voltage, Max Charge Current, Max Discharge Current.
        victron_message_351(0,58, 200, 200, 48);        
        vTaskDelay(pdMS_TO_TICKS(100));

        // void victron_message_372(int ModulesOK, int ModulesBlockingC, int ModulesBlockingD, int OfflineModules)
        
        victron_message_35f(1,get_total_cap() , 1, 1); // is this total available ?

        //victron_message_372(3, 0, 0, 1); //  is populated with example data, but data is not required. to be removed        
        //void victron_message_373(float lowestcellval, float highestcellval, float lowestcelltempval, float highestcelltempval )        
        //pylon_message_35c();
        
        // vTaskDelay(pdMS_TO_TICKS(50)); // adding delays for troubleshooting
        
        // This sets errors and alarms. // This message seems to be critical
        pylon_message_359();
        
        // vTaskDelay(pdMS_TO_TICKS(400)); // Delay for 400 milliseconds for troubleshooting
        
        //esp_task_wdt_reset();
        
        pylon_message_356_2();
        vTaskDelay(pdMS_TO_TICKS(100));
        publish_cell_voltages_to_mqtt();
        calculateHealth();
        }

void set_defaultValues(){
// not yet implemented
}
void SetBankAndModuleText(char *buffer, uint8_t cellid) {
    uint8_t bank = cellid / 16;
    uint8_t module = cellid - (bank * 16); // Corrected the calculation
    memset(buffer, 0, 8);
    snprintf(buffer, 9, "b%d m%d", bank, module);
}

void loop_task(void *pvParameters) {
  // Not implemented yet, but this could be the cause of the inverter cutting out
  unsigned long lastCANMessageTime = 0;
  unsigned long sendPacketTime = 1000;
  NetworkAlivePacket alivePacket = {(uint8_t)0x0305, 1};
    while (1) {
        unsigned long currentTime = esp_timer_get_time();
        unsigned long timePassed = currentTime - lastCANMessageTime;
        // Some of these methods take in data as arguments, this was for development only. Remediation is required here 
        if(timePassed > sendPacketTime)
        {
          printf("Time Elapsed.");
          // send a keep alive message regardless
          send_canbus_message(0x359, (uint8_t *)&alivePacket , sizeof(alivePacket));
          send_victron_update();
          lastCANMessageTime = esp_timer_get_time();

          // const char *topic = "my_topic";
          // const char *message = "Hello, MQTT!";
          // mqtt_publish(topic, message, strlen(message));
        }        
        
    }
}

 void jkbmsTask(void *pvParameters) { // start the JKBMS task, this is a CPP interface. To be honest i dont think we need the wrapper....
    JKBMSWrapper* jkInstance = JKBMS_Create();
    JKBMS_Start(jkInstance);        
    JKBMS_Destroy(jkInstance);    
    vTaskDelete(NULL);
}


void app_main(void)
{
start_wifi_provisioning_server();
set_defaultValues();  
   //Initialize configuration structures using macro initializers
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(Can_TX_GPIO_NUM, Can_RX_GPIO_NUM, TWAI_MODE_NO_ACK);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    //Install CAN driver
    ////twai_driver_install_v2
    if (twai_driver_install_v2(&g_config, &t_config, &f_config, &twai_bus_0) == ESP_OK) {
        printf("Driver installed\n");
    } else {
        printf("Failed to install driver\n");
        return;
    }

     if (twai_start() == ESP_OK) {
        printf("Driver started\n");
    } else {
        printf("Failed to start driver\n");
        return;
    }

    // Initialize the WiFi manager and other components
    //initialize_wifi_manager(); This is legacy from the original implementation.and needs to be replaced with the new implementation

    // Create even loop tasks for various componenets (CAN then JKBMS interface. Add more for MQTT, Influx etc)
    // This is the can send loop task, it sends the victron messages based on what is captured by the JKBMS task
    xTaskCreate(loop_task, "LoopTask", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 1, NULL);
    
    // this is the JKBMS task, it will read from the JKBMS serial port
    xTaskCreate(&jkbmsTask, "JKBMS Task", 4096, NULL, 5, NULL);   
 
    while(1){
      // main program loop, i was planning to work on the data sharing component and failover happening in this loop. 
       ESP_ERROR_CHECK(twai_receive(&rx_message, portMAX_DELAY));
       //esp_task_wdt_reset();
      //  if(mqtt_configured && !mqtt_connected)
      //  {
      //   mqtt_app_start();
      //  }
      if(get_wifiStatus() && shouldconnectMqtt)
        {
          //ESP_LOGE(TAG, "Wifi Is Connected, Mqtt Connection Requested");
          if(get_mqtt_config_loaded() && get_mqtt_configured())
          {
            ESP_LOGE(TAG, "Mqtt config is loaded, Mqtt is configured");
            mqttConfig* mconfig;
            mconfig = getPointerToConfig();
            //ESP_LOGE(TAG, "Mqtt host = %s", mconfig->mqtt_host ); // this is causing an exception            
            mqtt_app_start();
            shouldconnectMqtt = false;
            mqttConnected = true;
            
          }
        }
      
      
       if(1) //isWifiConnected()) // if(false == true )//
       {
        //  if(mqttConnected == false){
        //    esp_err_t err = init_and_publish_mqtt_data();
        //    if (err != ESP_OK) {
        //        ESP_LOGE(mqttTAG, "Failed to initialize and publish MQTT data (%d)", err);
        //     }
        //      else
        //      {
        //        //mqttConnected = true;
        //      }
        //  }else
        //  {
        //   publishStats();          
        //  }
        }
        else
        {
        //ESP_LOGE(mqttTAG, "Wifi is not connected");
        }
    }
}
