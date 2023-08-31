import requests
url = "http://114.71.220.59:7579/Mobius/gpm/Data/la"
headers = {
    'Accept': 'application/json',
    'X-M2M-RI': '12345',
    'X-M2M-Origin': 'S-xwJrLz1ot'
}
r = requests.get(url,headers=headers)
try:
        r.raise_for_status()
        jr = r.json()
        print(jr['m2m:cin']['con'])
except Exception as exc:
    print('There was a problem : %s'%(exc))
