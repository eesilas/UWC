const int relayPin = 7; // 继电器控制引脚 (D7)

void setup() {
  Serial.begin(115200); // 初始化串口，波特率115200
  pinMode(relayPin, OUTPUT); // 设置继电器引脚为输出
  digitalWrite(relayPin, LOW); // 初始状态：继电器断开（电机停止）
  Serial.println("请输入 '1' 启动电机，'0' 停止电机");
}

void loop() {
  if (Serial.available() > 0) { // 检查是否有串口输入
    char input = Serial.read(); // 读取输入字符
    if (input == '0') { // 输入 '1' 启动电机
      digitalWrite(relayPin, HIGH); // 闭合继电器（低电平触发）
      Serial.println("电机停止");
    } else if (input == '1') { // 输入 '0' 停止电机
      digitalWrite(relayPin, LOW); // 断开继电器
      Serial.println("电机启动");
    }
    // 清除串口缓冲区，防止重复读取
    while (Serial.available() > 0) {
      Serial.read();
    }
  }
}
