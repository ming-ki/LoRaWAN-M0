import os
import sys
import logging
import paho.mqtt.client as mqtt
import json
import requests  # 추가한 부분
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
client.on_connect = lambda client, userdata, flags, rc: print("\nMQTT 브로커에 연결되었습니다. 결과 코드: " + str(rc))
# combine_data 함수 정의
def combine_data(data_values):
    value_6_to_9 = [str(int(data_values[i]) / 1000) if data_values[i] != 'NULL' else 'NULL' for i in range(5, 9)]
    return ','.join(data_values[:5] + value_6_to_9)
# 모비우스에 데이터 전송 함수 (REST API 사용)
def publish_to_mobius_rest_api(combined_values):
    mobius_url = 'http://114.71.220.59:7579/Mobius/gpm/Data'
    headers = {
        'Accept': 'application/json',
        'X-M2M-RI': '12345',
        'X-M2M-Origin': 'S-xwJrLz1ot',
        'Content-Type': 'application/vnd.onem2m-res+json; ty=4'
    }
    data = "{}".format(combined_values)
    # print(type(data))
    payload_data = {
        "m2m:cin": {
            "con": data
        }
    }
    response = requests.post(mobius_url, headers=headers, json=payload_data)
    # response = requests.post(mobius_url, json=payload, headers=headers)
    if response.status_code == 201:
        print("Data sent to Mobius successfully!")
    else:
        print("Failed to send data to Mobius.")
# 메시지 수신 콜백 함수 설정
def on_message(client, userdata, message):
    print("\n메시지 수신 - 토픽: " + message.topic + " / QoS = " + str(message.qos))
    # 메시지를 JSON 형식으로 파싱하여 출력
    parsed_json = json.loads(message.payload)
    # print(parsed_json)
    if "uplink_message" in parsed_json:
        uplink_message = parsed_json["uplink_message"]
        decoded_payload = uplink_message.get("decoded_payload", {}).get("message", None)
        if decoded_payload is not None:
            # print("Decoded Payload:", decoded_payload)
            data_values = decoded_payload.split(',')
            # Replace empty values with 'NULL'
            data_values = ['NULL' if value.strip() == '' else value.strip() for value in data_values]
            combined_values = combine_data(data_values)
            # print("Combined Values:", combined_values)
            # 모비우스에 데이터 전송
            publish_to_mobius_rest_api(combined_values)
# 메시지 수신 콜백 함수 설정
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
