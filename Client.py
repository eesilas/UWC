import pygame
import socketio
import time

# SocketIO客户端连接到RPi 5服务器
sio = socketio.Client(reconnection=True, reconnection_attempts=5, reconnection_delay=1)  # 添加重连逻辑
SERVER_URL = 'http://192.168.0.191:5000'  # 更新为您的RPi 5 IP和端口

@sio.event(namespace='/')
def connect():
    print('Connected to server')

@sio.event(namespace='/')
def disconnect():
    print('Disconnected from server')

# 连接服务器
try:
    sio.connect(SERVER_URL, namespaces=['/'])
except Exception as e:
    print(f"Connection failed: {e}")
    exit(1)

# 初始化pygame和手柄
pygame.init()
pygame.joystick.init()
if pygame.joystick.get_count() == 0:
    print("未检测到游戏控制器")
    exit(1)
joystick = pygame.joystick.Joystick(0)
joystick.init()

INTERVAL = 0.05  # 发送间隔（秒）

while True:
    pygame.event.pump()
    surge = -joystick.get_axis(1)  # 左摇杆Y (上 -1 → +1 surge, 下 +1 → -1 surge)
    sway = joystick.get_axis(0)    # 左摇杆X (左 -1 → -1 sway, 右 +1 → +1 sway)
    heave = -joystick.get_axis(3)  # 右摇杆Y (上 -1 → +1 heave, 下 +1 → -1 heave)
    
    # 获取所有按钮状态
    buttons = [joystick.get_button(i) for i in range(joystick.get_numbuttons())]
    
    # 发送输入到服务器，包括按钮
    try:
        sio.emit('joystick_input', {'surge': surge, 'sway': sway, 'heave': heave, 'buttons': buttons}, namespace='/')
        print(f"Sent input: surge={surge:.2f}, sway={sway:.2f}, heave={heave:.2f}, buttons={buttons}")  # 添加调试打印
    except Exception as e:
        print(f"Emit failed: {e}")
    
    time.sleep(INTERVAL)
