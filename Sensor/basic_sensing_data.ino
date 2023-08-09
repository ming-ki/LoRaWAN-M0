#include <Arduino.h>   // required before wiring_private.h
#include "wiring_private.h" // pinPeripheral() function
#include <PMS.h>
#include "DFRobot_OzoneSensor.h"
#define ContinueMode 0
#define PollingMode 1
#define COLLECT_NUMBER   20
#define Ozone_IICAddress OZONE_ADDRESS_3
//-------------------------------------------------------------------------
//function
String value_convert(String value);
//-------------------------------------------------------------------------
//Add Uart with sercom 0,1,2,5 (Modify variable.h for sercom2
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
//-------------------------------------------------------------------------
//dust
PMS pms(Serial1);
PMS::DATA data;
DFRobot_OzoneSensor Ozone;
//-------------------------------------------------------------------------
//variable
char buffer[30];
char CO_data[50];
char NO2_data[50];
char SO2_data[50];
int dust = 0;
//-------------------------------------------------------------------------
void setup(){
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
  Ozone.setModes(MEASURE_MODE_PASSIVE);
  pms.passiveMode();
}
void loop(){
  pms.requestRead();
  if(pms.readUntil(data)){
    dust = data.PM_AE_UG_2_5;
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
  while (Serial2.available()){
     CO_data[i] = Serial2.read();
     i++;
  }
  i = 0;
  while (Serial3.available()){
    NO2_data[i] = Serial3.read();
    i++;
  }
  i = 0;
  while (Serial4.available()){
    SO2_data[i] = Serial4.read();
    i++;
  }
  int16_t ozoneConcentration = Ozone.readOzoneData(COLLECT_NUMBER);
  delay(100);
  String str_co_data = "";
  String str_no2_data = "";
  String str_so2_data = "";
  for(int j = 12; j < 18; j++){
     str_co_data += CO_data[j];
     str_no2_data += NO2_data[j];
     str_so2_data += SO2_data[j];
  }
  Serial.println(str_so2_data);
//-------------------------------------------------------------------------
//dust data, ppb data extraction
  String dustData, coPPB, no2PPB, so2PPB, allData;
  dustData = String(dust);
  coPPB = value_convert(str_co_data);
  no2PPB = value_convert(str_no2_data);
  so2PPB = value_convert(str_so2_data);
  //allData = dustData + ' ' + coPPB + ' ' + no2PPB + ' ' + so2PPB;
//-------------------------------------------------------------------------
  //Serial.println(allData);
  Serial.print("dust : ");
  Serial.println(dust);
  Serial.print("co : ");
  Serial.println(coPPB);
  Serial.print("no2 : ");
  Serial.println(no2PPB);
  Serial.print("so2 : ");
  Serial.println(so2PPB);
  String ozone = String(ozoneConcentration);
  Serial.println(ozone);
  delay(1000);
}
//-------------------------------------------------------------------------
//function definition
String value_convert(String value){
  int start = value.indexOf(',') + 1; // 시작 인덱스
  int end = value.indexOf(',', start); // 끝 인덱스
  // 쉼표와 쉼표 사이의 값
  String con_value = value.substring(start, end);
  //int convalue = con_value.toInt();
  return con_value;
}
