#include <EEPROM.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

//-----------------------------------------------------
// 定义 GP0 引脚为 OUTPUT_EN输出继电器
//-----------------------------------------------------
const uint8_t OUTPUT_EN = 0;
// --------------------------------------------------
// CS3310 模拟 SPI 引脚定义 
// --------------------------------------------------
const uint8_t VOL_MUTE = 2; 
const uint8_t VOL_CLK = 3; 
const uint8_t VOL_SDA = 4; 
const uint8_t VOL_CS  = 5; 
//----------------------------------------------------
//to DSP 控制输入选择引脚
//----------------------------------------------------
const uint8_t DSP_S0 = 16;  //接DSP MP12 音源选择底位
const uint8_t DSP_S1 = 20;  //接DSP MP13 音源选择高位
//-----------------------------------------------------
// 定义 MAX7219 LED matrix引脚
//-----------------------------------------------------
const uint8_t DATA_PIN = 19;
const uint8_t CLK_PIN = 18;
const uint8_t CS_PIN = 17;
//-----------------------------------------------------
// 按键引脚定义
//-----------------------------------------------------
const uint8_t PIN_VOL_INC = 22;
const uint8_t PIN_SRC_SEL = 26;
const uint8_t PIN_SRC_MUTE = 27;
const uint8_t PIN_VOL_DEC = 28;

//-----------------------------------------------------
// 模拟红外遥控相关
//-----------------------------------------------------
#define DECODE_NEC
#include <IRremote.hpp> 
#define IR_RECEIVE_PIN 21  // 红外接收器接GP21
//键码
const uint16_t IR_CMD_VOL_INC   = 0x18;
const uint16_t IR_CMD_VOL_DEC   = 0x52;
const uint16_t IR_CMD_SRC_SEL   = 0x5A;
const uint16_t IR_CMD_SRC_MUTE  = 0x8;
const uint16_t IR_CMD_SRC_SEL01  = 0x45;
const uint16_t IR_CMD_SRC_SEL02  = 0x46;
const uint16_t IR_CMD_SRC_SEL03  = 0x44;


//-----------------------------------------------------
// 模拟EEPROM存储地址定义
//-----------------------------------------------------
#define ADDR_VOL 0
#define ADDR_SRC 1

//-----------------------------------------------------
// --- 按键处理结构体和常量 ---
//-----------------------------------------------------
const uint16_t DEBOUNCE_MS = 50;   // 消抖时间(ms)
const uint16_t LONG_PRESS_START_MS = 500; // 判定长按的初始时间(ms)
const uint16_t LONG_PRESS_RATE_MS = 100;  // 长按后连续触发的间隔(ms)

struct Button {
  const uint8_t pin;
  bool lastState;
  unsigned long pressedStartTime;
  unsigned long lastActionTime;
};
// 初始化四个按键实例
Button btnVolInc = {PIN_VOL_INC, HIGH, 0, 0};
Button btnSrcSel = {PIN_SRC_SEL, HIGH, 0, 0};
Button btnSrcMute = {PIN_SRC_MUTE, HIGH, 0, 0};
Button btnVolDec = {PIN_VOL_DEC, HIGH, 0, 0};

//-----------------------------------------------------
// 定义 MAX7219 的数量
//-----------------------------------------------------
#define HARDWARE_TYPE MD_MAX72XX::DR1CR0RR0_HW
#define MAX_DEVICES 4
// 创建 MD_MAX72XX 对象
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

//-----------------------------------------------------
//变量和常数
//-----------------------------------------------------
volatile uint8_t Ch_Vol = 0x80;   //音量值
volatile uint8_t Input_Source = 0x00;   //输入信号源
volatile bool Mute_EN = false;    //静音标志位

volatile bool PIN_SS_PRESSED = false;  // 音源切换按键按下标志位
volatile unsigned long LASTTIME_PSS_PRESSED ;  //音源切换按键按下时间
const int INTERVAL_PSS = 30000;        // 音源切换按键按下延迟常量，延迟30000ms = 30s 切换回音量显示

volatile bool DATA_CHANGED = false;  // 1=有vol or source 数据改变
volatile unsigned long LASTTIME_DATA_CHANGED;  // Vol or source数据改变时间
const int INTERVAL_DATA_SAVE = 30000;        // 延迟常量,延迟30000ms=30S存储一次vol and source到flash模拟的eeprom，用以减少flash磨损

volatile bool LED_BLINK = false;  //LED闪烁标志
volatile unsigned long LASTTIME_LED_BLINK;  //LED变灯时间
const int INTERVAL_LED_BLINK = 1000;  //闪烁时间常量ms

volatile unsigned long LASTTIME_IR_VOL;  //遥控音量增减时间
const int INTERVAL_IR_VOL = 100;  //遥控音量增减时间间隔

//-----------------------------------------------------
//子函数
//-----------------------------------------------------

//显示音量的函数
void Show_Volume(uint8_t Ch_Vol) {  
  // 计算 dB 值
  int db_value = (Ch_Vol - 0xC0) / 2;

  // 创建 dB 字符串
  char db_string[6];  // 包括符号和 'dB' 共 5 个字符 + 终止符
  if (db_value >= 0) {
    snprintf(db_string, sizeof(db_string), "+%02d", db_value);
  } else {
    snprintf(db_string, sizeof(db_string), "%+03d", db_value);  // 负数时加上负号
  }
  db_string[3] = 'd';  // 替换字符位置为 'd'
  db_string[4] = 'B';  // 替换字符位置为 'B'

  mx.setChar(29, db_string[0]);  // 显示第一个字符
  mx.setChar(23, db_string[1]);  // 显示第二个字符
  mx.setChar(17, db_string[2]);  // 显示第三个字符
  mx.setChar(11, db_string[3]);  // 显示第四个字符 ('d')
  mx.setChar(6, db_string[4]);   // 显示第五个字符 ('B')
}

// 显示 "MUTE" 字符的函数
void Show_MUTE() {
    mx.setChar(27, 'M');  // 显示 'M'
    mx.setChar(21, 'U');  // 显示 'U'
    mx.setChar(15, 'T'); // 显示 'T'
    mx.setChar(9, 'E'); // 显示 'E'
}

// 显示输入源的函数
void Show_Source(uint8_t Input_Source) {

  switch (Input_Source) {
    case 0x00:
      mx.setChar(31, '<');
      mx.setChar(27, 'R');
      mx.setChar(21, 'a');
      mx.setChar(16, 's');
      mx.setChar(11, 'P');
      mx.setChar(5, 'i');
      mx.setChar(2, '>');
      break;
      
    case 0x01:
      mx.setChar(31, '<');
      mx.setChar(27, 'C');
      mx.setChar(21, 'O');
      mx.setChar(15, 'A');
      mx.setChar(9, 'X');
      mx.setChar(2, '>');
      break;
      
    case 0x02:
      mx.setChar(31, '<');
      mx.setChar(24, 'U');
      mx.setChar(18, 'S');
      mx.setChar(12, 'B');
      mx.setChar(2, '>'); 
      break;
    }
 }

//开机显示UPiDSP 0206
void Show_Welcom(){  
    mx.setChar(31, 'U');  
    mx.setChar(25, 'P');  
    mx.setChar(19, 'i'); 
    mx.setChar(17, 'D');
    mx.setChar(11, 'S'); 
    mx.setChar(5, 'P');
    delay(1500);
    mx.clear();
    mx.setChar(26, '0'); 
    mx.setChar(20, '2'); 
    mx.setChar(14, '0'); 
    mx.setChar(8, '6'); 
    delay(1500);
}

// 根据 Input_Source 的值设置 DSP_S0 和 DSP_S1 引脚的状态的函数，实现输入选择
void DSP_SOURCE_SWITCH(uint8_t Input_Source) {
  if (Input_Source == 0x00) {
    digitalWrite(DSP_S0,LOW);   // 设置 DSP_S0 为低电平
    digitalWrite(DSP_S1,LOW);  // 设置 DSP_S1 为高电平
  }
  else if (Input_Source == 0x01) {
    digitalWrite(DSP_S0,HIGH);  // 设置 DSP_S0 为高电平
    digitalWrite(DSP_S1,LOW);   // 设置 DSP_S1 为低电平
  }
  else if (Input_Source == 0x02) {
    digitalWrite(DSP_S0,LOW);   // 设置 DSP_S0 为低电平
    digitalWrite(DSP_S1,HIGH);  // 设置 DSP_S1 为高电平
  }
}

// --------------------------------------------------
// 通用按键状态机处理函数
// 此函数处理消抖、短按和长按逻辑
// --------------------------------------------------
void updateButtonState(Button &b, void (*singleClickAction)(), void (*longPressAction)()) {
  // 读取当前引脚状态 (低电平表示按下)
  bool currentState = digitalRead(b.pin);
  unsigned long currentTime = millis();

  if (currentState != b.lastState) {
    // 状态改变，记录按下时间作为消抖起点
    if (currentState == LOW) {
      b.pressedStartTime = currentTime;
      b.lastActionTime = currentTime; // Reset action timer on press start
    }
    // 更新上一个状态
    b.lastState = currentState;
  }

  // 检查是否按下 (LOW) 并且已经过了消抖时间
  if (currentState == LOW) {
    // 检查是否是长按判断后的连续触发时间点
    bool isLongPressRepeat = (currentTime - b.lastActionTime >= LONG_PRESS_RATE_MS) &&
                             (currentTime - b.pressedStartTime >= LONG_PRESS_START_MS);

    if (isLongPressRepeat) {
      if (longPressAction != nullptr) {
        longPressAction();
      }
      b.lastActionTime = currentTime; // 更新上次执行动作的时间
    }
  } else {
    // 检查是否释放 (HIGH) 并且是短按结束
    // 如果按下时间短于长按阈值，且大于消抖时间，则判定为一次短按
    if ((currentTime - b.pressedStartTime > DEBOUNCE_MS) &&
        (currentTime - b.pressedStartTime < LONG_PRESS_START_MS)) {
        // 当按键释放时才执行短按动作，避免多次触发
        // 注意：这里的逻辑只会在松开时触发一次
        if (singleClickAction != nullptr) {
            singleClickAction();
        }
        b.pressedStartTime = 0; // 重置按下时间
    }
  }
}

// --------------------------------------------------
// 各个按键对应的动作函数
// --------------------------------------------------

// --- 音量加 ---
void actionVolInc() {
  // 范围 0x00 ~ 0xFE, 步进 +2
  if (Ch_Vol <= 0xFC) {
    Ch_Vol += 2;
  } else {
    Ch_Vol = 0xFE; // 确保不超过上限
  }
  digitalWrite(VOL_MUTE,HIGH);
  Mute_EN = false;
  mx.clear();
  Show_Volume(Ch_Vol);
  SET_Volume(Ch_Vol);

  PIN_SS_PRESSED = false;  // 重置按钮按下状态

  DATA_CHANGED = true; 
  LASTTIME_DATA_CHANGED = millis();

}

// --- 音量减 ---
void actionVolDec() {
  // 范围 0x00 ~ 0xFE, 步进 -2
  if (Ch_Vol >= 0x02) {
    Ch_Vol -= 2;
  } else {
    Ch_Vol = 0x00; // 确保不低于下限
  }
  digitalWrite(VOL_MUTE,HIGH);
  Mute_EN = false;
  mx.clear();
  Show_Volume(Ch_Vol);
  SET_Volume(Ch_Vol);
  PIN_SS_PRESSED = false;  // 重置按钮按下状态
  DATA_CHANGED = true;  
  LASTTIME_DATA_CHANGED = millis();

}

// --- 音源选择 (无长按) ---
void actionSrcSel() {
  // 范围 0x00 ~ 0x02 循环
  Input_Source++;
  if (Input_Source > 0x02) {
    Input_Source = 0x00;
  }

  mx.clear();
  Show_Source(Input_Source);
  
  DSP_SOURCE_SWITCH(Input_Source);
  
  digitalWrite(VOL_MUTE,HIGH);
  Mute_EN = false;
  
  PIN_SS_PRESSED = true;  
  DATA_CHANGED = true;  
  LASTTIME_PSS_PRESSED = millis();
  LASTTIME_DATA_CHANGED = millis();
  
}

// --- 音源XX ---
void actionSrcSelXX(uint8_t Input_Source) {
  
  mx.clear();
  Show_Source(Input_Source);
  
  DSP_SOURCE_SWITCH(Input_Source);
  
  digitalWrite(VOL_MUTE,HIGH);
  Mute_EN = false;
  
  PIN_SS_PRESSED = true;  
  DATA_CHANGED = true;  
  LASTTIME_PSS_PRESSED = millis();
  LASTTIME_DATA_CHANGED = millis();
  
}

// --- 静音开关 (无长按) ---
void actionSrcMute() {
  // 布尔值取反
  Mute_EN = !Mute_EN;
  PIN_SS_PRESSED = false;  // 重置按钮按下状态
  if (Mute_EN){
    digitalWrite(VOL_MUTE,LOW);
    mx.clear();
    Show_MUTE();
  } else {
      digitalWrite(VOL_MUTE,HIGH);
      mx.clear();
      Show_Volume(Ch_Vol);
  }
}
// --------------------------------------------------
// 初始化 CS3310 控制引脚
// --------------------------------------------------
void setup_CS3310_GPIO() {
  pinMode(VOL_MUTE, OUTPUT);
  pinMode(VOL_CLK, OUTPUT);
  pinMode(VOL_SDA, OUTPUT);
  pinMode(VOL_CS,  OUTPUT);

  digitalWrite(VOL_MUTE, LOW);
  digitalWrite(VOL_CLK, LOW);
  digitalWrite(VOL_SDA, LOW);
  digitalWrite(VOL_CS, HIGH); 
}

// --------------------------------------------------
// 模拟 SPI 发送一个字节数据 (MSB first)
// --------------------------------------------------
void spi_write_byte(uint8_t data) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(VOL_SDA, (data >> i) & 0x01);
    delayMicroseconds(1); // 增加延迟以满足时序要求

    digitalWrite(VOL_CLK, HIGH);
    delayMicroseconds(1); // 增加延迟以满足时序要求

    digitalWrite(VOL_CLK, LOW);
    delayMicroseconds(1); // 增加延迟以满足时序要求
  }
}

// --------------------------------------------------
// 设置音量函数：发送全局变量 Ch_Vol 到 3 片 CS3310
// --------------------------------------------------
void SET_Volume(uint8_t volume) {

  digitalWrite(VOL_CS, LOW);
  delayMicroseconds(1);

  spi_write_byte(volume); // 发送给第 3 片/远端芯片
  spi_write_byte(volume); 
  spi_write_byte(volume); // 发送给第 2 片芯片
  spi_write_byte(volume); 
  spi_write_byte(volume); // 发送给第 1 片/近端芯片
  spi_write_byte(volume); 
  
  digitalWrite(VOL_CS, HIGH);
 }

//--------------------------------------------------
//主程序初始化
//--------------------------------------------------
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(PIN_VOL_INC, INPUT_PULLUP);    // Vol_Inc 按键
  pinMode(PIN_VOL_DEC, INPUT_PULLUP);    // Vol_Dec 按键
  pinMode(PIN_SRC_SEL, INPUT_PULLUP);    // Src_Sel 按键
  pinMode(PIN_SRC_MUTE, INPUT_PULLUP);   // Src_MUTE 按键
  pinMode(OUTPUT_EN,OUTPUT);
  pinMode(DSP_S0, OUTPUT);
  pinMode(DSP_S1, OUTPUT);
  
  setup_CS3310_GPIO();
  
  //Serial.begin(9600);
  IrReceiver.begin(IR_RECEIVE_PIN);

  mx.begin();
  // Clear the display before showing any text
  mx.clear();
  mx.control(MD_MAX72XX::INTENSITY, 0);
  
  // 初始化 NVM 库并加载数据
  EEPROM.begin(64); 
  // 从 Flash 中读取之前保存的值
  Ch_Vol = EEPROM.read(ADDR_VOL);
  Input_Source = EEPROM.read(ADDR_SRC);
  
  if (Ch_Vol > 0xFE) Ch_Vol = 0x80;
  if (Input_Source > 0x02) Input_Source = 0x00;
  DSP_SOURCE_SWITCH(Input_Source);
  
  Show_Welcom();
  
  mx.clear();
  Show_Source(Input_Source); 
  
  mx.clear();
  Show_Volume(Ch_Vol);  // 初始显示音量
  SET_Volume(Ch_Vol);

  digitalWrite(VOL_MUTE, HIGH);
  digitalWrite(OUTPUT_EN, HIGH);
}

// --------------------------------------------------
// Loop 函数
// --------------------------------------------------
void loop() {
  // 持续更新四个按键的状态机
  
  // 音量加：支持短按和长按连续触发
  updateButtonState(btnVolInc, actionVolInc, actionVolInc); 

  // 音量减：支持短按和长按连续触发
  updateButtonState(btnVolDec, actionVolDec, actionVolDec);

  // 音源选择：只支持短按 (长按动作为空指针)
  updateButtonState(btnSrcSel, actionSrcSel, nullptr); 

  // 静音：只支持短按 (长按动作为空指针)
  updateButtonState(btnSrcMute, actionSrcMute, nullptr);

  //红外遥控接收
  if (IrReceiver.decode()) {

    //Serial.print("KeyValue: 0x");
    //Serial.println(IrReceiver.decodedIRData.command, HEX);

    bool isRepeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
    uint16_t IR_CMD = IrReceiver.decodedIRData.command;

    //MUTE键和SourceSel键不响应IR重复按键
    if (!isRepeat){
      switch(IR_CMD){
        
        case IR_CMD_SRC_MUTE:
             //Serial.println("IR MUTE PRESSED");
             actionSrcMute();
             break;
             
        case IR_CMD_SRC_SEL:
             //Serial.println("IR Soure Select PRESSED");
             actionSrcSel();
             break;

        case IR_CMD_SRC_SEL01:
             Input_Source = 0x00;
             actionSrcSelXX(Input_Source);
             break;          
             
        case IR_CMD_SRC_SEL02:
             Input_Source = 0x01;
             actionSrcSelXX(Input_Source);
             break;   
             
        case IR_CMD_SRC_SEL03:
             Input_Source = 0x02;
             actionSrcSelXX(Input_Source);
             break;
        }
      }
      
    switch(IR_CMD){
        case IR_CMD_VOL_INC:
          //Serial.println("IR VOL + PRESSED");
          if ( millis() - LASTTIME_IR_VOL >= INTERVAL_IR_VOL){
            LASTTIME_IR_VOL = millis();
            actionVolInc();
            }
          break;
          
        case IR_CMD_VOL_DEC:
          //Serial.println("IR VOL - PRESSED");
          if ( millis() - LASTTIME_IR_VOL >= INTERVAL_IR_VOL){
            LASTTIME_IR_VOL = millis();
            actionVolDec();
          }
          break;
      }
    
    IrReceiver.resume(); // 继续接收下一信号
  }


  //音源选择按钮按下时间到后，切换显示音量
  if (PIN_SS_PRESSED && (millis() - LASTTIME_PSS_PRESSED >= INTERVAL_PSS)) {
    
      PIN_SS_PRESSED = false;  // 重置按钮按下状态
      mx.clear();
      Show_Volume(Ch_Vol);
      //Serial.println("Turned to show volume.");
    }
    
  //存储volume 和 source 到 flash 
  if (DATA_CHANGED && (millis() - LASTTIME_DATA_CHANGED >= INTERVAL_DATA_SAVE)) {
    
      DATA_CHANGED = false;  // 重置数据改变状态
      
      EEPROM.write(ADDR_VOL,Ch_Vol);   //最多30s存一次，减少flash磨损
      EEPROM.write(ADDR_SRC,Input_Source);
      EEPROM.commit();
      //Serial.println("Data saved.");
    }

  
  //LED Blink  
  if ( millis() - LASTTIME_LED_BLINK >= INTERVAL_LED_BLINK ){
    LASTTIME_LED_BLINK = millis();
    LED_BLINK = !LED_BLINK;
    digitalWrite(LED_BUILTIN,LED_BLINK);
    }

}
