# -*- coding: utf-8 -*-
from flask import Flask, request, jsonify
import requests

app = Flask(__name__)

@app.route('/iot/v1/deviceData', methods=['POST'])
def receive_data():
    if request.is_json:
        data = request.get_json()
        payload_hex = data.get('Payload', '')
        payload_ascii = bytes.fromhex(payload_hex).decode('ascii')
        print("Received payload:", payload_ascii)
        if payload_ascii is not None:
            data_values = payload_ascii.split(',')
            data_values = ['NaN' if '& humi' in value else value for value in data_values]
            data_values = ['NaN' if 'y Senso' in value else value for value in data_values]
            data_values = ['NaN' if value.strip() == '' else value.strip() for value in data_values]
            combined_values = combine_data(data_values)
            # save_to_csv(combined_values)
            publish_to_mobius_rest_api(combined_values)
        return jsonify({"message": "Data received and processed", "Payload ASCII": payload_ascii}), 200
    else:
        return jsonify({"error": "Request must be JSON"}), 400

def combine_data(data_values):
    value_6_to_9 = []
    for i in range(5, 9):
        if data_values[i] == 'NaN' or data_values[i] == 'NULL':
            value_6_to_9.append('NaN')
    return ','.join(data_values[:5] + data_values[5:])

# def save_to_csv(data):
#     with open('experiment3.csv', 'a', newline='') as csvfile:
#         writer = csv.writer(csvfile)
#         writer.writerow(data.split(','))

def publish_to_mobius_rest_api(combined_values):
    mobius_url = 'http://114.71.220.59:7579/Mobius/gpm/experiment_3'
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
    response = requests.request("POST", mobius_url, headers=headers, data=send_data)
    if response.status_code == 201:
        print("Data sent to Mobius successfully!")
    else:
        print("Failed to send data to Mobius.")



if __name__ == '__main__':
    app.run(debug=True, host='0.0.0.0', port=7575)
