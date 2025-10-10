import eventlet
eventlet.monkey_patch()

import serial
from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import threading
import time
import json
import os

app = Flask(__name__)
socketio = SocketIO(app)

# 串口配置（使用固定名称）
SERIAL_MOTOR = '/dev/MotorBrush'
SERIAL_THRUSTERS = '/dev/thrusters'
SERIAL_SWITCH_SOUND = '/dev/switch_sound'  # 新Metro M4串口
BAUD_RATE = 9600

try:
    ser_motor = serial.Serial(SERIAL_MOTOR, BAUD_RATE, timeout=1)
    ser_thrusters = serial.Serial(SERIAL_THRUSTERS, BAUD_RATE, timeout=1)
    ser_switch_sound = serial.Serial(SERIAL_SWITCH_SOUND, BAUD_RATE, timeout=1)
    time.sleep(2)  # 等待串口初始化
except serial.SerialException as e:
    print(f"串口错误: {e}")
    exit(1)

# 线程锁，用于同步串口写入
lock = threading.Lock()

# 当前 PWM 值数组（初始停止）
current_pwms = [1500] * 8

# 目标 PWM 值数组
target_pwms = [1500] * 8

# 渐变步长和变化间隔（0.5 秒完成 400 PWM 偏移，步长 8 PWM/循环）
STEP = 8  # PWM 变化步长
INTERVAL = 0.01  # 变化间隔（秒），50 循环 = 0.5 秒

# 小死区阈值（0.05 以过滤噪声）
DEADZONE = 0.05

# 输入平滑缓冲区（平均最近 5 次读取值）
INPUT_BUFFER_SIZE = 5
input_buffers = [[0.0] * INPUT_BUFFER_SIZE for _ in range(3)]  # 3 DOF: surge, sway, heave
buffer_indices = [0] * 3

# 开关状态数组（初始未触发）
switch_states = [0, 0]

# Auto Mode状态和参数
auto_mode_active = False
auto_mode_thread = None
CONFIRM_TIME = 1.0  # 开关确认时间（秒）
STOP_TIME = 5.0  # 停止时间（秒）
PUSH_TIME = 3.0  # 初始推动时间（秒）
UP_FLOAT_TIME = 0.3  # 上浮时间（秒，约20cm）
UP_FLOAT_COUNT = 5  # 上浮次数阈值（估算浮出水面，调整为实际）

# 半自动模式方向（None或'left', 'right', 'forward', 'down'）
semi_auto_direction = None

# 上次按钮状态（用于检测按下/释放）
last_buttons = [0] * 16  # 假设16个按钮

# TAM 配置文件路径
CONFIG_FILE = '/home/pi/config.json'  # 调整为您的路径

# 加载 TAM 配置（如果文件不存在，使用默认）
def load_tam():
    if os.path.exists(CONFIG_FILE):
        with open(CONFIG_FILE, 'r') as f:
            tam = json.load(f)
        if isinstance(tam, list) and all(isinstance(row, list) for row in tam):
            return tam
        else:
            print("TAM 配置类型错误，使用默认值")
    # 默认 TAM (基于您的自定义逻辑, 3 DOF: surge, sway, heave)
    default_tam = [
        [0.0, 1.0, -1.0],  # 推进器1 (R1)
        [0.0, -1.0, -1.0], # 推进器2 (L1)
        [1.0, 0.0, 0.0],   # 推进器3 (R3)
        [-1.0, 0.0, 0.0],  # 推进器4 (L3)
        [-1.0, 0.0, 0.0],  # 推进器5 (R2)
        [1.0, 0.0, 0.0],   # 推进器6 (L2)
        [0.0, -1.0, 1.0],  # 推进器7 (R4)
        [0.0, 1.0, 1.0]    # 推进器8 (L4)
    ]
    return default_tam

# 保存 TAM 配置到文件
def save_tam(tam):
    with open(CONFIG_FILE, 'w') as f:
        json.dump(tam, f)

# 初始加载 TAM
TAM = load_tam()

# 函数：平滑输入值（平均缓冲区）
def smooth_input(raw_value, dof_index):
    input_buffers[dof_index][buffer_indices[dof_index]] = raw_value
    buffer_indices[dof_index] = (buffer_indices[dof_index] + 1) % INPUT_BUFFER_SIZE
    return sum(input_buffers[dof_index]) / INPUT_BUFFER_SIZE

# 函数：计算目标 PWM
def compute_target_pwm(inputs):
    pwms = []
    for i in range(8):
        thrust = sum(TAM[i][j] * inputs[j] for j in range(3))  # 使用TAM计算
        thrust = max(min(thrust, 1), -1)
        pwm = 1500 + int(thrust * 400)
        pwms.append(pwm)
    return pwms

# 函数：渐变更新 PWM（固定步长 8 PWM，确保 0.5 秒完成）
def update_pwm_gradually():
    global current_pwms, target_pwms
    for i in range(8):
        if current_pwms[i] < target_pwms[i]:
            current_pwms[i] = min(current_pwms[i] + STEP, target_pwms[i])
        elif current_pwms[i] > target_pwms[i]:
            current_pwms[i] = max(current_pwms[i] - STEP, target_pwms[i])

# 更新推进器线程
def thruster_update_thread():
    while True:
        update_pwm_gradually()
        
        # 发送到串口
        for i, pwm in enumerate(current_pwms, start=1):
            command = f"{i}:{pwm}\n"
            with lock:
                ser_thrusters.write(command.encode())
        
        socketio.emit('update_pwms', current_pwms)
        
        time.sleep(INTERVAL)

# 开关状态读取线程
def switch_thread():
    global switch_states
    while True:
        with lock:
            line = ser_switch_sound.readline().decode('utf-8', errors='ignore').strip()  # 忽略无效字节
            if line:
                try:
                    parts = line.split(',')
                    state1_str = parts[0].split(':')[1].strip()
                    state2_str = parts[1].split(':')[1].strip()
                    switch_states = [int(state1_str), int(state2_str)]
                    print(f"Switch states: 1={switch_states[0]}, 2={switch_states[1]}")  # 调试日志
                    socketio.emit('update_switches', switch_states)
                except Exception as e:
                    print(f"开关数据解析错误: {e}")
        time.sleep(0.05)  # 读取间隔

# Auto Mode序列函数
def auto_mode_sequence():
    global target_pwms, auto_mode_active
    print("Auto Mode started")
    # 第一3秒：推动墙壁 + 向下移动 (surge +1, heave -1)
    inputs = [1.0, 0.0, -1.0]  # surge +1 (推墙), heave -1 (下)
    target_pwms = compute_target_pwm(inputs)
    time.sleep(PUSH_TIME)
    
    # 停止所有运动
    target_pwms = [1500] * 8
    time.sleep(1)  # 短暂缓冲
    
    up_count = 0  # 上浮计数
    while auto_mode_active and up_count < UP_FLOAT_COUNT:  # 直到停止或浮出
        # 推动墙壁 + 向左移动 (surge +1, sway -1)
        inputs = [1.0, -1.0, 0.0]
        target_pwms = compute_target_pwm(inputs)
        start_time = time.time()
        confirmed = False
        while auto_mode_active and not confirmed:
            if switch_states[0] == 1:  # 左开关触发
                if time.time() - start_time >= CONFIRM_TIME:
                    confirmed = True
                    print("Left switch confirmed")
            time.sleep(0.05)
        if not auto_mode_active:
            break
        target_pwms = [1500] * 8
        time.sleep(STOP_TIME)
        
        # 推动墙壁 + 向右移动 (surge +1, sway +1)
        inputs = [1.0, 1.0, 0.0]
        target_pwms = compute_target_pwm(inputs)
        start_time = time.time()
        confirmed = False
        while auto_mode_active and not confirmed:
            if switch_states[1] == 1:  # 右开关触发
                if time.time() - start_time >= CONFIRM_TIME:
                    confirmed = True
                    print("Right switch confirmed")
            time.sleep(0.05)
        if not auto_mode_active:
            break
        target_pwms = [1500] * 8
        time.sleep(STOP_TIME)
        
        # 上浮0.3秒 (heave +1)
        inputs = [0.0, 0.0, 1.0]
        target_pwms = compute_target_pwm(inputs)
        time.sleep(UP_FLOAT_TIME)
        target_pwms = [1500] * 8
        up_count += 1  # 计数上浮
        
    auto_mode_active = False
    target_pwms = [1500] * 8
    print("Auto Mode stopped")

# 启动线程
threading.Thread(target=thruster_update_thread, daemon=True).start()
threading.Thread(target=switch_thread, daemon=True).start()

@app.route('/', methods=['GET', 'POST'])
def index():
    global TAM
    if request.method == 'POST':
        # 更新 TAM 从表单
        new_tam = []
        for i in range(8):
            row = []
            for j in range(3):
                value = float(request.form.get(f'tam_{i}_{j}', 0.0))
                row.append(value)
            new_tam.append(row)
        TAM = new_tam
        save_tam(TAM)  # 保存到文件
    return render_template('combined_index.html', pwms=current_pwms, tam=TAM)

@app.route('/control', methods=['POST'])
def control():
    motor = request.form['motor']
    direction = request.form['direction']
    speed = request.form['speed']
    
    if direction == 'STOP':
        command = f"M{motor} STOP\n"
    else:
        command = f"M{motor} {direction} {speed}\n"
    
    with lock:
        ser_motor.write(command.encode())
        response = ser_motor.readline().decode('utf-8', errors='ignore').strip()  # 忽略无效字节
    return jsonify({
        'message': f"Command sent: {command.strip()}. Response: {response}",
        'status': 'success',
        'direction': direction,
        'speed': speed if direction != 'STOP' else '0'
    })

@app.route('/update_tam', methods=['POST'])
def update_tam():
    global TAM
    new_tam = []
    for i in range(8):
        row = []
        for j in range(3):
            value = float(request.form.get(f'tam_{i}_{j}', 0.0))
            row.append(value)
        new_tam.append(row)
    TAM = new_tam
    save_tam(TAM)
    return jsonify({'status': 'success'})

@socketio.on('connect')
def handle_connect():
    emit('update_pwms', current_pwms)
    emit('update_switches', switch_states)  # 初始发送开关状态

@socketio.on('joystick_input')
def handle_joystick_input(data):
    global target_pwms, auto_mode_active, auto_mode_thread, semi_auto_direction, last_buttons
    buttons = data.get('buttons', [])
    if len(buttons) > 7:  # 确保按钮列表足够长
        # A按钮检测（index 0）
        if buttons[0] == 1 and last_buttons[0] == 0:  # 按下边缘检测
            auto_mode_active = not auto_mode_active
            if auto_mode_active:
                auto_mode_thread = threading.Thread(target=auto_mode_sequence, daemon=True)
                auto_mode_thread.start()
                print("Auto Mode activated")
            else:
                print("Auto Mode deactivated")
        
        # 半自动模式检测（buttons 4-7，按下边缘检测）
        if buttons[4] == 1 and last_buttons[4] == 0:  # button 4 (左): 向前+左
            semi_auto_direction = 'left' if semi_auto_direction != 'left' else None
            print(f"Semi-auto left: {semi_auto_direction}")
        if buttons[5] == 1 and last_buttons[5] == 0:  # button 5 (右): 向前+右
            semi_auto_direction = 'right' if semi_auto_direction != 'right' else None
            print(f"Semi-auto right: {semi_auto_direction}")
        if buttons[6] == 1 and last_buttons[6] == 0:  # button 6 (向前): 向前
            semi_auto_direction = 'forward' if semi_auto_direction != 'forward' else None
            print(f"Semi-auto forward: {semi_auto_direction}")
        if buttons[7] == 1 and last_buttons[7] == 0:  # button 7 (向下): 向下
            semi_auto_direction = 'down' if semi_auto_direction != 'down' else None
            print(f"Semi-auto down: {semi_auto_direction}")
        
        last_buttons = buttons[:]  # 更新上次按钮状态
    
    if auto_mode_active:
        return  # 阻塞手动输入 during Auto Mode
    
    # 如果半自动模式激活，设置固定inputs
    if semi_auto_direction:
        if semi_auto_direction == 'left':
            inputs = [1.0, -1.0, 0.0]  # 向前+左
        elif semi_auto_direction == 'right':
            inputs = [1.0, 1.0, 0.0]  # 向前+右
        elif semi_auto_direction == 'forward':
            inputs = [1.0, 0.0, 0.0]  # 向前
        elif semi_auto_direction == 'down':
            inputs = [0.0, 0.0, -1.0]  # 向下
        target_pwms = compute_target_pwm(inputs)
        return
    
    # 手动模式
    raw_surge = data.get('surge', 0)
    raw_sway = data.get('sway', 0)
    raw_heave = data.get('heave', 0)
    
    # 调试打印原始轴值
    print(f"Raw axes - Surge: {raw_surge:.2f}, Sway: {raw_sway:.2f}, Heave: {raw_heave:.2f}")
    
    # 小死区过滤噪声，并平滑输入
    surge = smooth_input(0 if abs(raw_surge) < DEADZONE else raw_surge, 0)
    sway = smooth_input(0 if abs(raw_sway) < DEADZONE else raw_sway, 1)
    heave = smooth_input(0 if abs(raw_heave) < DEADZONE else raw_heave, 2)
    
    inputs = [surge, sway, heave]
    
    # 调试打印过滤后的输入
    print(f"Filtered inputs: {inputs}")
    
    target_pwms = compute_target_pwm(inputs)
    
    # 如果所有输入接近 0，强制目标 PWM 为 1500
    if all(abs(inp) < 0.05 for inp in inputs):  # 小阈值避免噪声
        target_pwms = [1500] * 8
        print("Inputs near zero, reset to 1500")  # 调试日志

if __name__ == '__main__':
    socketio.run(app, host='0.0.0.0', port=5000, debug=False)
