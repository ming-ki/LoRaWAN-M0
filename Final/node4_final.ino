#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>
#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function
#include <PMS.h>
#include <RTCZero.h>
#include <TimeLib.h>
#include "DFRobot_OzoneSensor.h"
#include "DHT.h"

#define DHTPIN 5        // SDA 핀의 설정
#define DHTTYPE DHT22   // DHT22 (AM2302) 센서종류 설정
#define ContinueMode 0
#define PollingMode 1
#define TX_INTERVAL 300
#define COLLECT_NUMBER   20
#define Ozone_IICAddress OZONE_ADDRESS_3
#define BATTERY_PIN A7
//function
String value_convert(String value);
//co
Uart Serial2(&sercom5, A5, 6, SERCOM_RX_PAD_0, UART_TX_PAD_2);
void SERCOM5_Handler()
{
    Serial2.IrqHandler();
}
//no2
Uart Serial3(&sercom1, 12, 10, SERCOM_RX_PAD_3, UART_TX_PAD_2);
void SERCOM1_Handler()
{
    Serial3.IrqHandler();
}
//so2
Uart Serial4(&sercom0, A4, A3, SERCOM_RX_PAD_1, UART_TX_PAD_0);
void SERCOM0_Handler()
{
    Serial4.IrqHandler();
}
//dust
PMS pms(Serial1);
PMS::DATA data;
DFRobot_OzoneSensor Ozone;
DHT dht(DHTPIN, DHTTYPE);
//variable
char buffer[30];
char CO_data[50];
char NO2_data[50];
char SO2_data[50];
int pm1 = 0;
int pm25 = 0;
int pm10 = 0;
RTCZero rtc;
bool timeRequested = false;
char time_str[20];
uint8_t send_year,send_month,send_day,send_hours, send_minutes, send_seconds;
// This EUI must be in little-endian format, see the comments in your code
static const u1_t PROGMEM APPEUI[8] = { 0x7D, 0x5A, 0x38, 0x9B, 0x5A, 0x61, 0xFC, 0x92 };
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
// This should also be in little endian format
static const u1_t PROGMEM DEVEUI[8] = { 0x54, 0x00, 0x06, 0xD0, 0x7E, 0xD5, 0xB3, 0x70 };
void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
// This key should be in big endian format
static const u1_t PROGMEM APPKEY[16] = { 0x83, 0xD5, 0x70, 0x29, 0x46, 0x4B, 0x6B, 0x9A, 0xD4, 0xCB, 0xBF, 0xA9, 0x0B, 0xBA, 0xDD, 0xAD };
void os_getDevKey (u1_t* buf) {  memcpy_P(buf, APPKEY, 16); }
static osjob_t sendjob;
// Pin mapping
const lmic_pinmap lmic_pins = {
    .nss = 8,
    .rxtx = LMIC_UNUSED_PIN,
    .rst = LMIC_UNUSED_PIN,
    .dio = {3, 11, LMIC_UNUSED_PIN},
};
void onEvent (ev_t ev) {
    switch(ev) {
        case EV_SCAN_TIMEOUT:
            Serial.println(F("EV_SCAN_TIMEOUT"));
            break;
        case EV_BEACON_FOUND:
            Serial.println(F("EV_BEACON_FOUND"));
            break;
        case EV_BEACON_MISSED:
            Serial.println(F("EV_BEACON_MISSED"));
            break;
        case EV_BEACON_TRACKED:
            Serial.println(F("EV_BEACON_TRACKED"));
            break;
        case EV_JOINING:
            Serial.println(F("EV_JOINING"));
            break;
        case EV_JOINED:
            Serial.println(F("EV_JOINED"));
            // Disable link check validation (automatically enabled
            // during join, but not supported by TTN at this time).
            LMIC_setLinkCheckMode(0);
            break;
        case EV_RFU1:
            Serial.println(F("EV_RFU1"));
            break;
        case EV_JOIN_FAILED:
            Serial.println(F("EV_JOIN_FAILED"));
            break;
        case EV_REJOIN_FAILED:
            Serial.println(F("EV_REJOIN_FAILED"));
            break;
            break;
        case EV_TXCOMPLETE:
            Serial.println(F("EV_TXCOMPLETE (includes waiting for RX windows)"));
            if (LMIC.txrxFlags & TXRX_ACK)
              Serial.println(F("Received ack"));
            if (LMIC.dataLen) {
              Serial.println(F("Received "));
              Serial.println(LMIC.dataLen);
              Serial.println(F(" bytes of payload"));
            }
            break;
        case EV_LOST_TSYNC:
            Serial.println(F("EV_LOST_TSYNC"));
            break;
        case EV_RESET:
            Serial.println(F("EV_RESET"));
            break;
        case EV_RXCOMPLETE:
            // data received in ping slot
            Serial.println(F("EV_RXCOMPLETE"));
            break;
        case EV_LINK_DEAD:
            Serial.println(F("EV_LINK_DEAD"));
            break;
        case EV_LINK_ALIVE:
            Serial.println(F("EV_LINK_ALIVE"));
            break;
         default:
//            Serial.println(F("Unknown event"));
            break;
    }
}
String readBatteryVoltage() {
    double measuredvbat = analogRead(BATTERY_PIN);
    measuredvbat *= 2;    // 배터리 전압이 분배기를 통해 절반으로 줄어듦
    measuredvbat *= 3.3;  // 아날로그 참조 전압
    measuredvbat /= 1024; // 10비트 ADC 해상도
    return String(measuredvbat, 2);  // 소수점 두 자리까지의 전압 반환
}
uint32_t userUTCTime; // Seconds since the UTC epoch
// Utility function for digital clock display: prints preceding colon and
// leading 0
void printDigits(int digits) {
    Serial.print(':');
    if (digits < 10) Serial.print('0');
    Serial.print(digits);
}
void alarmMatch()
{
  os_setCallback(&sendjob, do_send);
}
void setAlarmFor5MinutesFromNow()
{
  byte nextHour = rtc.getHours();
  byte nextMinute = rtc.getMinutes();
  byte nextSecond = 0;
  // 5분 단위로 반올림
  byte remainder = nextMinute % 5;
  nextMinute = nextMinute - remainder + 5;
  // 분이 60을 초과하는 경우 시간을 조정
  if (nextMinute >= 60) {
    nextMinute -= 60;
    nextHour++;
    if (nextHour >= 24) {
      nextHour = 0;
    }
  }
  rtc.setAlarmTime(nextHour, nextMinute, nextSecond);
  rtc.enableAlarm(rtc.MATCH_HHMMSS);
}
void user_request_network_time_callback(void *pVoidUserUTCTime, int flagSuccess) {
    uint32_t *pUserUTCTime = (uint32_t *) pVoidUserUTCTime;
    lmic_time_reference_t lmicTimeReference;
    if (flagSuccess != 1) {
        Serial.println(F("USER CALLBACK: Not a success"));
        return;
    }
    flagSuccess = LMIC_getNetworkTimeReference(&lmicTimeReference);
    if (flagSuccess != 1) {
        Serial.println(F("USER CALLBACK: LMIC_getNetworkTimeReference didn't succeed"));
        return;
    }
    *pUserUTCTime = lmicTimeReference.tNetwork + 315964800;
    *pUserUTCTime += 32400;
    ostime_t ticksNow = os_getTime();
    // Time when the request was sent, in ticks
    ostime_t ticksRequestSent = lmicTimeReference.tLocal;
    uint32_t requestDelaySec = osticks2ms(ticksNow - ticksRequestSent) / 1000;
    *pUserUTCTime += requestDelaySec;
    // Update the system time with the time read from the network
    setTime(*pUserUTCTime);
    int twoDigitYear = year() % 100;
    rtc.begin();
    rtc.setTime(hour(), minute(), second());
    rtc.setDate(day(), month(), twoDigitYear);
    setAlarmFor5MinutesFromNow();  // Initialize the alarm for 5 minutes from the current time
    rtc.attachInterrupt(alarmMatch);
}
void do_send(osjob_t* j) {
    char mydata[100];
    setAlarmFor5MinutesFromNow();
    // Check if there is not a current TX/RX job running
    if (LMIC.opmode & OP_TXRXPEND) {
        Serial.println(F("OP_TXRXPEND, not sending"));
    } else {
        send_year = rtc.getYear();
        send_month = rtc.getMonth();
        send_day = rtc.getDay();
        send_hours = rtc.getHours();
        send_minutes = rtc.getMinutes();
        send_seconds = rtc.getSeconds();
        snprintf(time_str, sizeof(time_str), "%d%02d%02d%02d%02d%02d", send_year,send_month,send_day,send_hours, send_minutes, send_seconds);
        pms.requestRead();
        if (pms.readUntil(data)) {
            pm1 = data.PM_AE_UG_1_0;
            pm25 = data.PM_AE_UG_2_5;
            pm10 = data.PM_AE_UG_10_0;
        }
        #if PollingMode
          Serial2.write('\r');
          Serial3.write('\r');
          Serial4.write('\r');
          delay(1000);
        #else
          delay(100);
        #endif
        int i = 0;
        while (Serial2.available()) {
            CO_data[i] = Serial2.read();
            i++;
        }
        i = 0;
        while (Serial3.available()) {
            NO2_data[i] = Serial3.read();
            i++;
        }
        i = 0;
        while (Serial4.available()) {
            SO2_data[i] = Serial4.read();
            i++;
        }
        int16_t ozoneConcentration = Ozone.readOzoneData(COLLECT_NUMBER);
        float h = dht.readHumidity();
        float t = dht.readTemperature();
        delay(100);
        String str_co_data = "";
        String str_no2_data = "";
        String str_so2_data = "";
        for (int loopIdx = 12; loopIdx < 18; loopIdx++) {
            str_co_data += CO_data[loopIdx];
            str_no2_data += NO2_data[loopIdx];
            str_so2_data += SO2_data[loopIdx];
        }
        String dust1,dust25,dust10, coPPB, no2PPB, so2PPB, allData;
        dust1 = String(pm1);
        dust25 = String(pm25);
        dust10 = String(pm10);
        coPPB = value_convert(str_co_data);
        no2PPB = value_convert(str_no2_data);
        so2PPB = value_convert(str_so2_data);
        String ozone = String(ozoneConcentration);
        String humidity = String(h);
        String temperature = String(t);
        // Serial.print("dust : ");
        // Serial.print(dust1);
        // Serial.print(dust25);
        // Serial.println(dust10);
        // Serial.print("co : ");
        // Serial.println(coPPB);
        // Serial.print("no2 : ");
        // Serial.println(no2PPB);
        // Serial.print("so2 : ");
        // Serial.println(so2PPB);
        // Serial.println(ozone);
        String batteryVoltage = readBatteryVoltage();
    // 데이터 문자열에 배터리 전압 추가
        snprintf(mydata, sizeof(mydata), "4,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s", time_str, dust1.c_str(), dust25.c_str(), dust10.c_str(), coPPB.c_str(), no2PPB.c_str(), so2PPB.c_str(), ozone.c_str(), humidity.c_str(),temperature.c_str(),batteryVoltage.c_str());
        LMIC_setTxData2(1, (uint8_t*)mydata, strlen(mydata), 0);
        Serial.println(LMIC.freq);
    }
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
    dht.begin();
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
