#include <ESP32Servo.h>

#define ESC_PIN 16 // ESC 信號引腳

Servo esc;
int stopPulse = 1500; // 停止點脈寬（可微調）
int currentPulse = 1500; // 當前脈寬

void setup() {
  // 初始化串口通信
  Serial.begin(115200);
  Serial.println("ESP32 Thruster Control Initialized");

  // 附加 ESC，設置 PWM 範圍為 1000-2000µs
  esc.attach(ESC_PIN, 1000, 2000);

  // 初始化 ESC（設置為停止）
  esc.writeMicroseconds(stopPulse);
  delay(2000); // 等待 ESC 初始化

  Serial.println("輸入速度（-100 到 100）或 'c' 校準停止點。負值表示後退，0 表示停止。");
}

void smoothTransition(int targetPulse) {
  // 逐漸調整 PWM 以減少振動
  while (currentPulse != targetPulse) {
    if (currentPulse < targetPulse) {
      currentPulse += 10;
      if (currentPulse > targetPulse) currentPulse = targetPulse;
    } else {
      currentPulse -= 10;
      if (currentPulse < targetPulse) currentPulse = targetPulse;
    }
    esc.writeMicroseconds(currentPulse);
    delay(20); // 每次調整間隔 20ms
  }
}

void loop() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim(); // 去除多餘空格

    if (input.equalsIgnoreCase("c")) {
      // 進入停止點校準模式
      Serial.println("輸入停止點脈寬（1480-1520）：");
      while (!Serial.available()) {}
      String calibInput = Serial.readStringUntil('\n');
      stopPulse = calibInput.toInt();
      stopPulse = constrain(stopPulse, 1480, 1520); // 限制範圍
      currentPulse = stopPulse;
      esc.writeMicroseconds(stopPulse);
      Serial.print("停止點設置為：");
      Serial.println(stopPulse);
    } else {
      // 處理速度輸入
      int speed = input.toInt();
      speed = constrain(speed, -100, 100); // 限制速度範圍

      // 計算目標 PWM 值
      int targetPulse;
      if (speed == 0) {
        targetPulse = stopPulse; // 使用校準的停止點
      } else {
        targetPulse = map(abs(speed), 0, 100, stopPulse, speed >= 0 ? 2000 : 1000);
      }

      // 平滑過渡到目標 PWM
      smoothTransition(targetPulse);

      // 打印狀態
      Serial.print("速度: ");
      Serial.print(speed);
      Serial.print("%, PWM: ");
      Serial.println(targetPulse);
    }
  }
}
