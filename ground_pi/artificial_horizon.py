import serial
import struct
import threading
import time
from flask import Flask, jsonify, send_file

state = {'roll': 0, 'pitch': 0, 'alt': 0, 'temp': 0,
         'lat': 0, 'lon': 0, 'speed': 0, 'course': 0, 'count': 0}

last_time = 0


def to_degree(v):
    deg = v // 10000000
    minute = (v - deg * 10000000) / 100000
    return deg + minute / 60


def reader():
    global last_time
    port = serial.Serial('/dev/ttyACM0', 115200)
    while True:
        if port.read(1) != b'\xaa':
            continue
        if port.read(1) != b'\x55':
            continue
        data = port.read(21)
        payload = data[:20]
        x = 0
        for b in payload:
            x ^= b
        if x != data[20]:
            continue
        v = struct.unpack('<hhhhiihh', payload)
        state['roll']   = v[0] / 10
        state['pitch']  = v[1] / 10
        state['alt']    = v[2] / 10
        state['temp']   = v[3] / 10
        state['lat']    = to_degree(v[4])
        state['lon']    = to_degree(v[5])
        state['speed']  = v[6] / 10
        state['course'] = v[7] / 10
        state['count'] += 1
        last_time = time.time()


threading.Thread(target=reader, daemon=True).start()

app = Flask(__name__)


@app.route('/data')
def data():
    if last_time == 0:
        state['age'] = None
    else:
        state['age'] = time.time() - last_time
    r = jsonify(state)
    r.headers['Cache-Control'] = 'no-store'
    return r


@app.route('/')
def index():
    return send_file('index.html')


app.run(host='0.0.0.0', port=8000)
