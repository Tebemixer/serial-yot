#include <Wire.h>
#include <MGS_FR403.h> // библиотека для огня
#include "SparkFun_SGP30_Arduino_Library.h" // библиотека для газа
#include <Adafruit_MCP4725.h> // библиотека для Генератора звука
#include <ESP32Servo.h> // библиотека для сервопривода
#include <PCA9634.h> // библиотека для MGL-RGB3

#define WIND 17

MGS_FR403 Fire; // создаём объект огня
SGP30 mySensor; // создаём объект газа
Adafruit_MCP4725 buzzer; // создаем объект генератора звука
Servo myservo; // создаем экземпляр класса сервопривода

// MGL-RGB3: 0x1C при трех надетых джамперах JP0/JP1/JP2
PCA9634 leds(0x1C);

bool flag = false;
float fire = 0;
float gas = 0;
int j = 30000;

// Константы для I2C шилда
#define I2C_HUB_ADDR 0x70
#define EN_MASK 0x08
#define DEF_CHANNEL 0x00
#define MAX_CHANNEL 0x08

/*
  I2C порт 0x07 - выводы GP16 (SDA), GP17 (SCL)
  I2C порт 0x06 - выводы GP4 (SDA), GP13 (SCL)
  I2C порт 0x05 - выводы GP14 (SDA), GP15 (SCL)
  I2C порт 0x04 - выводы GP5 (SDA), GP23 (SCL)
  I2C порт 0x03 - выводы GP18 (SDA), GP19 (SCL)
*/

// Для MGL-RGB3 по документации:
// 0 и 6 – белые светодиоды
// 3, 2, 5 – красный, зеленый и синий соответственно
void setWhite() {
  setBusChannel(0x03);
  leds.write1(0, 255); // белый
  leds.write1(6, 255); // белый
  leds.write1(3, 0);   // R
  leds.write1(2, 0);   // G
  leds.write1(5, 0);   // B
  leds.write1(1, 0);   // UV
  leds.write1(4, 0);   // UV
}

void setBlue() {
  setBusChannel(0x03);
  leds.write1(0, 0);   // белый
  leds.write1(6, 0);   // белый
  leds.write1(3, 0);   // R
  leds.write1(2, 0);   // G
  leds.write1(5, 255); // B
  leds.write1(1, 0);   // UV
  leds.write1(4, 0);   // UV
}

void setup() {
  Serial.begin(115200);

  // инициализация сервопривода
  myservo.attach(18);
  myservo.write(0);

  // инициализация реле+вентилятора
  pinMode(WIND, OUTPUT);
  digitalWrite(WIND, LOW);

  // Инициализация I2C
  Wire.begin();
  Fire.begin(); // инициализация датчика огня

  // Инициализация генератора звука
  setBusChannel(0x06);
  buzzer.begin(0x61); // если с перемычкой, может быть 0x60

  // Инициализация датчика газа
  setBusChannel(0x05);
  if (mySensor.begin() == false) {
    Serial.println("No SGP30 Detected. Check connections.");
    while (1);
  }
  mySensor.initAirQuality();

  // Инициализация RGB MGL-RGB3
  setBusChannel(0x03);
  leds.begin();

  // Переводим каналы в PWM-режим
  for (int channel = 0; channel < leds.channelCount(); channel++) {
    leds.setLedDriverMode(channel, PCA9634_LEDOFF);
    leds.write1(channel, 0x00);
    leds.setLedDriverMode(channel, PCA9634_LEDPWM);
  }

  setWhite(); // обычный режим - белый
}

void loop() {
  // Считывание датчиков
  Fire.get_ir_and_vis();
  fire = Fire.ir_data;

  setBusChannel(0x05);
  mySensor.measureAirQuality();
  gas = mySensor.CO2;

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    if (input.equalsIgnoreCase("ПРИВЕТ") && flag == true) {
      flag = false;
      digitalWrite(WIND, LOW);
      myservo.write(0);
      setWhite();
      Serial.println("Флаг сброшен!");
    }
  }

  if (flag == false && (gas > 50 && fire > 50)) {
    flag = true;
    digitalWrite(WIND, HIGH);
    myservo.write(180);
  }

  // Синий свет при превышении CO2, иначе белый
  if (gas > 50) {
    setBlue();
  } else {
    setWhite();
  }

  if (flag == true) {
    Serial.print("ИК: ");
    Serial.println(String(fire, 1));
    Serial.print("CO2: ");
    Serial.println(mySensor.CO2);

    setBusChannel(0x06);
    for (int i = 0; i < (5 - j / 6000); i++) {
      buzzer.setVoltage(0, false);
      delay_nop(j);
      buzzer.setVoltage(4095, false);
      delay_nop(j);
    }

    j = j - 100;
    if (j <= 0) {
      j = 30000;
    }
  }
}

// Функция установки нужного выхода I2C
bool setBusChannel(uint8_t i2c_channel) {
  if (i2c_channel >= MAX_CHANNEL) {
    return false;
  } else {
    Wire.beginTransmission(I2C_HUB_ADDR);
    Wire.write(i2c_channel | EN_MASK); // для микросхемы PCA9547
    // Wire.write(0x01 << i2c_channel); // Для микросхемы PW548A
    Wire.endTransmission();
    return true;
  }
}

// Задержка пустыми командами
void delay_nop(int u) {
  for (int i = 0; i < u; i++) {
    _NOP();
  }
}