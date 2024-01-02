#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Arduino.h>
#include "wiring_private.h" // pinPeripheral() function
#include <PMS.h>
#include <RTCZero.h>
#include <TimeLib.h>
#include "DFRobot_OzoneSensor.h"
#include "DHTStable.h"

// Pin Definitions
#define DHT22_PIN       5
#define ContinueMode 0
#define PollingMode 1
#define TX_INTERVAL 300
#define COLLECT_NUMBER   20
#define Ozone_IICAddress OZONE_ADDRESS_3
#define BATTERY_PIN A7

// Sensor Objects
DHTStable DHT;
PMS pms(Serial1);
PMS::DATA data;
DFRobot_OzoneSensor Ozone;

// RTC Object
RTCZero rtc;

// Data Variables
char buffer[30];
char CO_data[50];
char NO2_data[50];
char SO2_data[50];
int pm1 = 0, pm25 = 0, pm10 = 0;
char time_str[20];
int16_t ozoneConcentration;
uint8_t send_year, send_month, send_day, send_hours, send_minutes, send_seconds;
uint32_t userUTCTime;

// LoRaWAN Credentials
static const u1_t PROGMEM APPEUI[8] = {0x25, 0xDE, 0xB2, 0x9A, 0xFF, 0xD9, 0xE0, 0x9E};
static const u1_t PROGMEM DEVEUI[8] = {0x52, 0x00, 0x06, 0xD0, 0x7E, 0xD5, 0xB3, 0x70};
static const u1_t PROGMEM APPKEY[16] = {0x92, 0x92, 0x4E, 0x98, 0xB0, 0x7B, 0xC6, 0x45, 0x7F, 0x71, 0x97, 0xBE, 0x76, 0xC6, 0x0B, 0x6F};
static osjob_t sendjob;

void os_getArtEui(u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
void os_getDevEui(u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
void os_getDevKey(u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

// LoRaWAN Pin Mapping
const lmic_pinmap lmic_pins = {
    .nss = 8,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = LMIC_UNUSED_PIN,
    .dio = {3, 11, LMIC_UNUSED_PIN},
};

// Forward Declarations for Custom Functions
String value_convert(String value);
void alarmMatch();
void readPMSData();
void readozoneData();

// Custom Serial Port and Handlers
Uart Serial2(&sercom5, A5, 6, SERCOM_RX_PAD_0, UART_TX_PAD_2);
void SERCOM5_Handler() { Serial2.IrqHandler(); }
Uart Serial3(&sercom1, 12, 10, SERCOM_RX_PAD_3, UART_TX_PAD_2);
void SERCOM1_Handler() { Serial3.IrqHandler(); }
Uart Serial4(&sercom0, A4, A3, SERCOM_RX_PAD_1, UART_TX_PAD_0);
void SERCOM0_Handler() { Serial4.IrqHandler(); }

void onEvent(ev_t ev) {
    if (ev == EV_TXCOMPLETE) {
        Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
        if (LMIC.txrxFlags & TXRX_ACK)
            Serial.println(F("Received ack"));
        if (LMIC.dataLen) {
            Serial.print(F("Received "));
            Serial.print(LMIC.dataLen);
            Serial.println(F(" bytes of payload"));
        }
    } else {
        Serial.println(F("Event: "));
        switch(ev) {
            case EV_SCAN_TIMEOUT: Serial.println(F("EV_SCAN_TIMEOUT")); break;
            case EV_BEACON_FOUND: Serial.println(F("EV_BEACON_FOUND")); break;
            case EV_LINK_DEAD: Serial.println(F("EV_LINK_DEAD")); break;
            case EV_LINK_ALIVE: Serial.println(F("EV_LINK_ALIVE")); break;
            default: Serial.println(F("Unknown event")); break;
        }
    }
}

String readBatteryVoltage() {
    double measuredvbat = analogRead(BATTERY_PIN);
    measuredvbat *= 2 * 3.3 / 1024; // 배터리 전압을 측정하고 계산
    return String(measuredvbat, 2); // 소수점 두 자리까지의 전압 반환
}

void printDigits(int digits) {
    Serial.print(':');
    if (digits < 10) Serial.print('0');
    Serial.print(digits);
}

void alarmMatch()
{
  os_setCallback(&sendjob, do_send);
}

void setAlarmFor5MinutesFromNow() {
    byte hours = rtc.getHours();
    byte minutes = rtc.getMinutes();
    // 5분 단위로 반올림
    minutes = (minutes / 5 + 1) * 5;
    if (minutes >= 60) {
        minutes -= 60;
        hours = (hours + 1) % 24;
    }
    rtc.setAlarmTime(hours, minutes, 0);
    rtc.enableAlarm(rtc.MATCH_HHMMSS);
}

void user_request_network_time_callback(void *pVoidUserUTCTime, int flagSuccess) {
    if (flagSuccess != 1) {
        Serial.println(F("USER CALLBACK: Network time request failed"));
        return;
    }

    uint32_t *pUserUTCTime = static_cast<uint32_t *>(pVoidUserUTCTime);
    lmic_time_reference_t lmicTimeReference;

    // Get network time reference from LMIC
    if (LMIC_getNetworkTimeReference(&lmicTimeReference) != 1) {
        Serial.println(F("USER CALLBACK: Failed to get network time reference"));
        return;
    }

    // Calculate the current UTC time based on network time reference
    *pUserUTCTime = lmicTimeReference.tNetwork + 315964800; // Unix time start offset
    *pUserUTCTime += 32400; // Timezone offset (example for GMT+9)

    // Adjust for the time elapsed since the time request was sent
    ostime_t ticksNow = os_getTime();
    uint32_t requestDelaySec = osticks2ms(ticksNow - lmicTimeReference.tLocal) / 1000;
    *pUserUTCTime += requestDelaySec;

    // Update the system time and RTC with the new time
    setTime(*pUserUTCTime);
    rtc.begin();
    rtc.setTime(hour(), minute(), second());
    rtc.setDate(day(), month(), year() % 100);

    // Initialize the alarm for 5 minutes from the current time
    setAlarmFor5MinutesFromNow();
    rtc.attachInterrupt(alarmMatch);
}

void readPMSData() {
    pms.requestRead();
    if (pms.readUntil(data)) {
        pm1 = data.PM_AE_UG_1_0;
        pm25 = data.PM_AE_UG_2_5;
        pm10 = data.PM_AE_UG_10_0;
    }
}

void readGasData() {
    readSerialData(Serial2, CO_data);
    readSerialData(Serial3, NO2_data);
    readSerialData(Serial4, SO2_data);
}

void readozoneData() {
    ozoneConcentration = Ozone.readOzoneData(COLLECT_NUMBER);
    delay(100);
}

void readDHTData() {
    DHT.reset();
    
}

void readSerialData(HardwareSerial& serial, char* data) {
    int i = 0;
    while (serial.available()) {
        data[i] = serial.read();
        i++;
    }
}

String getStringFromData(char* data, int start, int length) {
    String result = "";
    for (int i = start; i < start + length; i++) {
        result += data[i];
    }
    return result;
}

void do_send(osjob_t* j) {
    // Initialize data buffer and set an alarm for 5 minutes from now
    char mydata[100];
    setAlarmFor5MinutesFromNow();

    // Initialize variables
    pm1 = 0;
    pm25 = 0;
    pm10 = 0;
    memset(CO_data, 0, sizeof(CO_data));
    memset(NO2_data, 0, sizeof(NO2_data));
    memset(SO2_data, 0, sizeof(SO2_data));

    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
        return;
    } 

    // Retrieve current time
    char time_str[20];
    snprintf(time_str, sizeof(time_str), "%d%02d%02d%02d%02d%02d",
             rtc.getYear(), rtc.getMonth(), rtc.getDay(),
             rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());

    // Read sensor data
    readPMSData();
    readGasData();
    readozoneData();
    readDHTData();

    // Prepare data string
    String dust1 = String(pm1);
    String dust25 = String(pm25);
    String dust10 = String(pm10);
    String coPPB = value_convert(getStringFromData(CO_data, 12, 18));
    String no2PPB = value_convert(getStringFromData(NO2_data, 12, 18));
    String so2PPB = value_convert(getStringFromData(SO2_data, 12, 18));
    String ozone = String(ozoneConcentration);
    String humidity = String(DHT.getHumidity());
    String temperature = String(DHT.getTemperature());
    String batteryVoltage = readBatteryVoltage();

    // Construct data string
    snprintf(mydata, sizeof(mydata), "2,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
             time_str, dust1.c_str(), dust25.c_str(), dust10.c_str(), coPPB.c_str(),
             no2PPB.c_str(), so2PPB.c_str(), ozone.c_str(), humidity.c_str(),
             temperature.c_str(), batteryVoltage.c_str());

    // Send data
    LMIC_setTxData2(1, (uint8_t*)mydata, strlen(mydata), 0);
    Serial.println(LMIC.freq);
}

void setup() {
    Serial.begin(9600);
    Serial1.begin(9600);
    Serial2.begin(9600);
    Serial3.begin(9600);
    Serial4.begin(9600);
    //co
    pinPeripheral(6, PIO_SERCOM);
    pinPeripheral(A5, PIO_SERCOM_ALT);
    //no2
    pinPeripheral(10, PIO_SERCOM);
    pinPeripheral(12, PIO_SERCOM);
    //so2
    pinPeripheral(17ul, PIO_SERCOM_ALT);
    pinPeripheral(18ul, PIO_SERCOM_ALT);
    //pms
    pinPeripheral(1, PIO_SERCOM_ALT);
    pinPeripheral(0, PIO_SERCOM_ALT);
    while(!Ozone.begin(Ozone_IICAddress)) {
    Serial.println("I2c device number error !");
    delay(1000);
  }
    pms.passiveMode();
    Ozone.setModes(MEASURE_MODE_PASSIVE);
    os_init();
    LMIC_reset();
    LMIC_requestNetworkTime(user_request_network_time_callback, &userUTCTime);
    do_send(&sendjob);
}
void loop() {
    os_runloop_once();
}
//function definition
String value_convert(String value){
  int start = value.indexOf(',') + 1; // 시작 인덱스
  int end = value.indexOf(',', start); // 끝 인덱스
  // 쉼표와 쉼표 사이의 값
  String con_value = value.substring(start, end);
  //int convalue = con_value.toInt();
  return con_value;
}
