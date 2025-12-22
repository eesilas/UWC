import pygame
import socketio
import time

# SocketIO客戶端連接至RPi 5伺服器
sio = socketio.Client(reconnection=True, reconnection_attempts=5, reconnection_delay=1)  # 添加重連邏輯
SERVER_URL = 'http://192.168.68.59:5000'  # 更新為您的RPi 5 IP和端口

@sio.event(namespace='/')
def connect():
    print('Connected to server')

@sio.event(namespace='/')/
def disconnect():
    print('Disconnected from server')

# 連接伺服器
try:
    sio.connect(SERVER_URL, namespaces=['/'])
except Exception as e:
    print(f"Connection failed: {e}")
    exit(1)

# 初始化pygame和手柄
pygame.init()
pygame.joystick.init()
if pygame.joystick.get_count() == 0:
    print("未檢測到遊戲控制器")
    exit(1)
joystick = pygame.joystick.Joystick(0)
joystick.init()

INTERVAL = 0.05  # 發送間隔（秒）

while True:
    pygame.event.pump()
    surge = -joystick.get_axis(1)  # 左搖桿Y (上 -1 → +1 surge, 下 +1 → -1 surge)
    sway = joystick.get_axis(0)    # 左搖桿X (左 -1 → -1 sway, 右 +1 → +1 sway)
    heave = -joystick.get_axis(3)  # 右搖桿Y (上 -1 → +1 heave, 下 +1 → -1 heave)
    yaw = joystick.get_axis(2)     # 右搖桿X (左 -1 → yaw左, 右 +1 → yaw右)
    
    # 獲取所有按鈕狀態
    buttons = [joystick.get_button(i) for i in range(joystick.get_numbuttons())]
    
    # 發送輸入至伺服器，包括按鈕
    try:
        sio.emit('joystick_input', {'surge': surge, 'sway': sway, 'heave': heave, 'yaw': yaw, 'buttons': buttons}, namespace='/')
        print(f"Sent input: surge={surge:.2f}, sway={sway:.2f}, heave={heave:.2f}, yaw={yaw:.2f}, A_button={buttons[0]}")  # 添加調試打印
    except Exception as e:
        print(f"Emit failed: {e}")
    
    time.sleep(INTERVAL)
