# sub.py - Auto Mode ver1 (原版不變) + ver2 (IMU + Ultrasonic 聯動清洗4面牆)
import eventlet
eventlet.monkey_patch()

import serial
from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import threading
import time
import json
import os
import re
import math

app = Flask(__name__)
socketio = SocketIO(app, async_mode='eventlet', cors_allowed_origins="*")

SERIAL_PORT = '/dev/Combined'
BAUD_RATE = 9600

try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1.0)
    time.sleep(2)
    ser.flushInput()
    ser.flushOutput()
    print("串口已連接")
except Exception as e:
    print("串口錯誤:", e)
    exit(1)

lock = threading.Lock()

# ====================== 狀態變數 ======================
current_pwms = [1500] * 8
target_pwms = [1500] * 8

switch_states = [0, 0]
sensor_distances = [None, None, None]  # S1左, S2底, S3右
accel = [0.0, 0.0, 0.0]
quat_i = 0.0
angles = [0.0, 0.0, 0.0]  # yaw, pitch, roll

relay_states = [False, False, False]
neo_state = False

last_motor_cmd = {"1": "STOP", "2": "STOP", "3": "STOP", "4": "STOP"}
last_buttons = [0] * 16

# Auto Mode 變數 (原版 ver1)
auto_mode_active = False
auto_mode_thread = None
auto_mode_version = 1  # 1=原版, 2=新版
CONFIRM_TIME = 1.0  # 開關確認時間（秒）
STOP_TIME = 0.4  # 停止時間（秒）
PUSH_TIME = 3.0  # 初始推動時間（秒）
DOWN_TIME = 1.7  # 向下潛時間（秒）
PWM_OFFSET = 400  # PWM偏移倍數（可調）

# ver2 專用變數
total_depth = 0.0
dive_depth_step = 0.0
dive_count = 0
wall_count = 0
wall_threshold = 0.025  # 牆距離閾值 (25mm)

CONFIG_FILE = '/home/pi/config.json'
def load_tam():
    if os.path.exists(CONFIG_FILE):
        try:
            with open(CONFIG_FILE, 'r') as f:
                data = json.load(f)
                if isinstance(data, list) and len(data) == 8 and all(len(row)==8 for row in data):
                    return data
        except: pass
    return [[1.0 if i==j else 0.0 for j in range(8)] for i in range(8)]
TAM = load_tam()
def save_tam(t):
    try:
        with open(CONFIG_FILE, 'w') as f:
            json.dump(t, f)
    except: pass

def compute_target_pwm(inputs):
    pwms = []
    for i in range(8):
        thrust = sum(TAM[i][j] * (max(inputs[j//2],0) if j%2==0 else abs(min(inputs[j//2],0)) if j<6 else (abs(min(inputs[3],0)) if j==6 else max(inputs[3],0)))
                     for j in range(8))
        thrust = max(min(thrust, 1.0), -1.0)
        pwm = 1500 + int(thrust * PWM_OFFSET)
        pwms.append(max(1100, min(1900, pwm)))
    return pwms

def stop_thrusters():
    global target_pwms
    target_pwms = [1500] * 8

def send_motor(cmd, m):
    if cmd == last_motor_cmd.get(str(m)): return
    with lock:
        ser.write((cmd + '\n').encode())
        ser.flush()
    time.sleep(0.15)
    last_motor_cmd[str(m)] = cmd

def send_relay(relay, on):
    cmd = f"R{relay} {'ON' if on else 'OFF'}"
    with lock:
        ser.write((cmd + '\n').encode())
        ser.flush()
    time.sleep(0.15)
    relay_states[relay-1] = on

def send_neo(on):
    cmd = f"N {'ON' if on else 'OFF'}"
    with lock:
        ser.write((cmd + '\n').encode())
        ser.flush()
    time.sleep(0.15)
    global neo_state
    neo_state = on

# ====================== Auto Mode ver1 (您的原版程式碼，完全不變) ======================
def auto_mode_sequence():
    global auto_mode_active
    print("Auto Mode ver1 啟動")
    stop_thrusters()
    time.sleep(0.5)
    target_pwms = compute_target_pwm([1.0, 0.0, 0.0, 0.0])  # 初始推動
    time.sleep(PUSH_TIME)
    stop_thrusters()
    time.sleep(STOP_TIME)
    while auto_mode_active:
        # 向左直到開關1觸發
        target_pwms = compute_target_pwm([0.0, -1.0, 0.0, 0.0])
        while auto_mode_active and switch_states[0] == 0:
            time.sleep(0.05)
        stop_thrusters()
        time.sleep(STOP_TIME)

        # 向右直到開關2觸發
        target_pwms = compute_target_pwm([0.0, 1.0, 0.0, 0.0])
        while auto_mode_active and switch_states[1] == 0:
            time.sleep(0.05)
        stop_thrusters()
        time.sleep(STOP_TIME)

        # 向下潛
        target_pwms = compute_target_pwm([0.0, 0.0, -1.0, 0.0])
        time.sleep(DOWN_TIME)
        stop_thrusters()
        time.sleep(STOP_TIME)

    stop_thrusters()
    print("Auto Mode ver1 結束")

# ====================== Auto Mode ver2 (新版) ======================
def auto_mode_v2():
    global auto_mode_active, total_depth, dive_depth_step, dive_count, wall_count

    print("Auto Mode ver2 啟動 - 清洗四面牆")
    stop_thrusters()
    time.sleep(0.5)

    wall_count = 1
    total_depth = 0.0
    dive_depth_step = 0.0
    dive_count = 0

    while auto_mode_active and wall_count <= 4:
        print(f"清洗第 {wall_count} 面牆")

        # 首次碰右牆測總深度
        if dive_count == 0:
            target_pwms = compute_target_pwm([0.8, 0.0, 0.0, 0.0])  # surge = 0.8
            time.sleep(3.0)
            stop_thrusters()
            time.sleep(STOP_TIME)

            # 左移到左牆
            target_pwms = compute_target_pwm([0.0, -0.8, 0.0, 0.0])  # sway = -0.8
            while auto_mode_active and switch_states[0] == 0 and (sensor_distances[0] is None or sensor_distances[0] > wall_threshold):
                time.sleep(0.05)
            stop_thrusters()
            time.sleep(STOP_TIME)

            # 右移測底深度
            target_pwms = compute_target_pwm([0.0, 0.8, 0.0, 0.0])  # sway = 0.8
            while auto_mode_active and switch_states[1] == 0 and (sensor_distances[2] is None or sensor_distances[2] > wall_threshold):
                time.sleep(0.05)
            stop_thrusters()
            time.sleep(STOP_TIME)
            total_depth = sensor_distances[1] if sensor_distances[1] is not None else 0.5
            dive_depth_step = total_depth / 7.0
            print(f"總深度: {total_depth}m, 每次下潛: {dive_depth_step}m")

        # 清洗一面牆 (7 次下潛)
        for dive in range(7):
            if not auto_mode_active:
                break

            # 左移到左牆
            target_pwms = compute_target_pwm([0.0, -0.8, 0.0, 0.0])
            while auto_mode_active and switch_states[0] == 0 and (sensor_distances[0] is None or sensor_distances[0] > wall_threshold):
                time.sleep(0.05)
            stop_thrusters()
            time.sleep(STOP_TIME)

            # 右移到右牆
            target_pwms = compute_target_pwm([0.0, 0.8, 0.0, 0.0])
            while auto_mode_active and switch_states[1] == 0 and (sensor_distances[2] is None or sensor_distances[2] > wall_threshold):
                time.sleep(0.05)
            stop_thrusters()
            time.sleep(STOP_TIME)

            # 檢查深度，補潛
            current_depth = sensor_distances[1] if sensor_distances[1] is not None else 0.0
            target_depth = (dive + 1) * dive_depth_step
            if current_depth < target_depth - 0.05:
                target_pwms = compute_target_pwm([0.0, 0.0, -0.5, 0.0])  # heave = -0.5
                time.sleep((target_depth - current_depth) / 0.5)
                stop_thrusters()
                time.sleep(STOP_TIME)

            dive_count += 1
            print(f"第 {dive+1} 次下潛完成，深度: {current_depth}m")

        if not auto_mode_active:
            break

        # 上浮到水面
        target_pwms = compute_target_pwm([0.0, 0.0, 1.0, 0.0])  # 上浮
        time.sleep(total_depth / 1.0)
        stop_thrusters()
        time.sleep(STOP_TIME)

        # 轉移下一牆: 後退 3秒 + IMU 監控
        start_time = time.time()
        start_accel_x = accel[0] if accel[0] else 9.5
        start_accel_z = accel[2] if accel[2] else 0.5
        target_pwms = compute_target_pwm([-0.8, 0.0, 0.0, 0.0])  # 後退 surge = -0.8
        while auto_mode_active and time.time() - start_time < 3.0:
            current_x = accel[0] if accel[0] else 0
            current_z = accel[2] if accel[2] else 0
            if 5 <= current_x <= 8 and 2 <= current_z <= 6:
                print("後退確認")
            time.sleep(0.1)
        stop_thrusters()
        time.sleep(STOP_TIME)

        # 右旋轉 5秒 (yaw 變化 90°)
        start_yaw = angles[0] if angles[0] else 0
        target_pwms = compute_target_pwm([0.0, 0.0, 0.0, 0.8])  # yaw = 0.8
        start_time = time.time()
        while auto_mode_active and time.time() - start_time < 5.0:
            current_yaw = angles[0] if angles[0] else 0
            if abs(current_yaw - start_yaw - 90) < 10:
                print("旋轉確認")
            time.sleep(0.1)
        stop_thrusters()
        time.sleep(STOP_TIME)

        wall_count += 1
        dive_count = 0

    stop_thrusters()
    print("Auto Mode ver2 完成四面牆清洗")

# ====================== 背景執行緒 ======================
def background_thread():
    global switch_states, sensor_distances, accel, quat_i, angles, current_pwms

    while True:
        # 推進器漸變
        changed = False
        for i in range(8):
            diff = target_pwms[i] - current_pwms[i]
            if abs(diff) > 8:
                current_pwms[i] += 8 if diff > 0 else -8
                changed = True
        if changed:
            for i, pwm in enumerate(current_pwms, 1):
                with lock:
                    ser.write(f"{i}:{pwm}\n".encode())
                    ser.flush()
            socketio.emit('update_pwms', current_pwms)

        # 讀取數據
        with lock:
            while ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    if not line: continue

                    def get(key):
                        m = re.search(f'{key}:([+-]?[0-9]*\\.?[0-9]+)', line)
                        return float(m.group(1)) if m else None

                    # 開關
                    sw1 = get('开关1')
                    sw2 = get('开关2')
                    if sw1 is not None and sw2 is not None:
                        new_sw = [int(sw1), int(sw2)]
                        if new_sw != switch_states:
                            switch_states = new_sw
                            socketio.emit('update_switches', switch_states)

                    # 超聲波
                    s1, s2, s3 = get('S1'), get('S2'), get('S3')
                    if None not in (s1, s2, s3):
                        new_dist = [s1, s2, s3]
                        if new_dist != sensor_distances:
                            sensor_distances = new_dist
                            socketio.emit('update_sensors', sensor_distances)

                    # IMU 加速度計
                    ax = get('AccelX'); ay = get('AccelY'); az = get('AccelZ')
                    if None not in (ax, ay, az):
                        new_accel = [ax, ay, az]
                        if new_accel != accel:
                            accel = new_accel
                            socketio.emit('update_accel', accel)

                    # IMU 四元數 i + 角度
                    qi = get('QuatX')
                    yaw = get('Yaw'); pitch = get('Pitch'); roll = get('Roll')
                    if qi is not None and None not in (yaw, pitch, roll):
                        if qi != quat_i or [yaw, pitch, roll] != angles:
                            quat_i = qi
                            angles = [yaw, pitch, roll]
                            socketio.emit('update_quat_angles', {'quat_i': quat_i, 'angles': angles})

                except: pass

        time.sleep(0.01)

threading.Thread(target=background_thread, daemon=True).start()

# ====================== 路由 ======================
@app.route('/', methods=['GET', 'POST'])
def index():
    global TAM
    if request.method == 'POST':
        try:
            TAM = [[float(request.form.get(f'tam_{i}_{j}', 0.0)) for j in range(8)] for i in range(8)]
            save_tam(TAM)
        except: pass
    return render_template('combined_index.html',
                           pwms=current_pwms,
                           tam=TAM,
                           sensors=sensor_distances)

@app.route('/control', methods=['POST'])
def control():
    m = request.form['motor']
    d = request.form['direction']
    s = request.form['speed']
    cmd = f"M{m} STOP" if d == "STOP" else f"M{m} {d} {s}"
    send_motor(cmd, m)
    return jsonify(status="ok")

@app.route('/relay', methods=['POST'])
def relay():
    relay = int(request.form['relay'])
    on = request.form['state'] == 'ON'
    send_relay(relay, on)
    return jsonify(status="ok")

@app.route('/neo', methods=['POST'])
def neo():
    on = request.form['state'] == 'ON'
    send_neo(on)
    return jsonify(status="ok")

@socketio.on('connect')
def on_connect():
    emit('update_pwms', current_pwms)
    emit('update_switches', switch_states)
    emit('update_sensors', sensor_distances)
    emit('update_accel', accel)
    emit('update_quat_angles', {'quat_i': quat_i, 'angles': angles})

@socketio.on('joystick_input')
def handle_joystick(data):
    global target_pwms, auto_mode_active, auto_mode_thread, auto_mode_version, last_buttons

    buttons = data.get('buttons', [])
    if len(buttons) > 1:
        # A Button (index 0) - ver1
        if buttons[0] == 1 and last_buttons[0] == 0:
            auto_mode_version = 1
            auto_mode_active = not auto_mode_active
            if auto_mode_active:
                auto_mode_thread = threading.Thread(target=auto_mode_sequence, daemon=True)
                auto_mode_thread.start()
            else:
                print("Auto Mode ver1 停止")
        # B Button (index 1) - ver2
        if buttons[1] == 1 and last_buttons[1] == 0:
            auto_mode_version = 2
            auto_mode_active = not auto_mode_active
            if auto_mode_active:
                auto_mode_thread = threading.Thread(target=auto_mode_v2, daemon=True)
                auto_mode_thread.start()
            else:
                print("Auto Mode ver2 停止")

    last_buttons = buttons[:]

    if auto_mode_active:
        return

    # 手動搖桿
    surge = data.get('surge', 0)
    sway = data.get('sway', 0)
    heave = data.get('heave', 0)
    yaw = data.get('yaw', 0)
    if all(abs(x) < 0.05 for x in [surge, sway, heave, yaw]):
        target_pwms = [1500] * 8
    else:
        target_pwms = compute_target_pwm([surge, sway, heave, yaw])

if __name__ == '__main__':
    print("伺服器啟動，請訪問 http://<樹莓派IP>:5000")
    socketio.run(app, host='0.0.0.0', port=5000, debug=False)
