import os
import sys
import logging
import paho.mqtt.client as mqtt
import json
import requests
import csv

USER = "m0-lora@ttn"
PASSWORD = "NNSXS.GEH4GZKFOS5DGJJT5L4VO2U5MSJNLMKKUIKNWHA.JWK4IKQRYGFXV6UVYSRVWPS3DBRW2QWUZYAJ3UTPGA3JEVW6QPMQ"
PUBLIC_TLS_ADDRESS = "eu1.cloud.thethings.network"
PUBLIC_TLS_ADDRESS_PORT = 8883
QOS = 0

# MQTT 클라이언트 생성
client = mqtt.Client()

# TTN 서버 연결 정보 설정
client.username_pw_set(USER, PASSWORD)
client.tls_set()

# 연결 콜백 함수 설정
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("\nMQTT 브로커에 연결되었습니다. 결과 코드: " + str(rc))
    else:
        print("\n연결 실패. 결과 코드: " + str(rc))

client.on_connect = on_connect

# combine_data 함수 정의
def combine_data(data_values):
    value_6_to_9 = []
    for i in range(5, 9):
        if data_values[i] == 'NaN' or data_values[i] == 'NULL':
            value_6_to_9.append('NaN')  # Use 'NaN' directly if encountered
        else:
            # Convert to float and divide by 1000
            value_6_to_9.append(str(float(data_values[i]) / 1000))
    return ','.join(data_values[:5] + value_6_to_9 + data_values[9:])


# CSV 파일에 데이터 저장 함수 정의
def save_to_csv(data):
    with open('experiment_1.csv', 'a', newline='') as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(data.split(','))

# 모비우스에 데이터 전송 함수 (REST API 사용)
def publish_to_mobius_rest_api(combined_values):
    mobius_url = 'http://114.71.220.59:7579/Mobius/gpm/experiment_1'
    headers = {
        'Accept': 'application/json',
        'X-M2M-RI': '12345',
        'X-M2M-Origin': 'SqNVGGACjuA',
        'Content-Type': 'application/vnd.onem2m-res+json; ty=4'
    }
    data = "{}".format(combined_values)
    con_01 = data.replace("\n","")
    print(con_01)
    
    send_data = "{\n    \"m2m:cin\": {\n        \"con\": \"" + con_01 + "\"\n    }\n}"
    # response = requests.post(mobius_url,headers=headers,json=send_data)
    response = requests.request("POST", mobius_url, headers=headers, data=send_data)
    if response.status_code == 201:
        print("Data sent to Mobius successfully!")
    else:
        print("Failed to send data to Mobius.")

# 메시지 수신 콜백 함수 설정
def on_message(client, userdata, message):
    print("\n메시지 수신 - 토픽: " + message.topic + " / QoS = " + str(message.qos))
    parsed_json = json.loads(message.payload)
    if "uplink_message" in parsed_json:
        uplink_message = parsed_json["uplink_message"]
        decoded_payload = uplink_message.get("decoded_payload", {}).get("message", None)
        if decoded_payload is not None:
            data_values = decoded_payload.split(',')
            # Check for '& humi' and replace with 'NaN'
            data_values = ['NaN' if '& humi' in value else value for value in data_values]
            data_values = ['NULL' if value.strip() == '' else value.strip() for value in data_values]
            combined_values = combine_data(data_values)
            save_to_csv(combined_values)  # 데이터를 CSV 파일에 저장
            publish_to_mobius_rest_api(combined_values)

client.on_message = on_message

# TTN 서버에 연결
client.connect(PUBLIC_TLS_ADDRESS, PUBLIC_TLS_ADDRESS_PORT, 60)

# 모든 토픽(#)을 구독하여 메시지 수신 대기
client.subscribe("#", QOS)

try:
    # 메시지 수신을 위한 루프 실행
    client.loop_forever()
except KeyboardInterrupt:
    # 사용자가 Ctrl+C를 누를 때 연결 종료
    print("\n연결을 종료합니다.")
    client.disconnect()
