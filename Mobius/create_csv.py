import csv
import requests
url = "http://114.71.220.59:7579/Mobius/gpm/Data/la"
headers = {
    'Accept': 'application/json',
    'X-M2M-RI': '12345',
    'X-M2M-Origin': 'S-xwJrLz1ot'
}
r = requests.get(url, headers=headers)
try:
    r.raise_for_status()
    jr = r.json()
    con_data = jr['m2m:cin']['con']
    # Splitting the con_data into individual values
    values = con_data.split(',')
    # Creating a CSV file and writing the parsed data
    with open('Data.csv', 'w', newline='') as csvfile:
        csv_writer = csv.writer(csvfile)
        csv_writer.writerow(['ID', 'Time', 'PM1', 'PM2.5', 'PM10', 'CO', 'NO2', 'SO2','O3'])
        csv_writer.writerow(values)
    print("CSV file created successfully.")
except Exception as exc:
    print('There was a problem: %s' % (exc))
