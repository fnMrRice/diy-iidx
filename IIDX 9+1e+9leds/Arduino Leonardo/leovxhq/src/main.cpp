#include <Joystick.h>
#include <Keyboard.h>
#include <Keypad.h>

#define USE_KEYPAD 1

// Gamepad的按键定义
enum GamePadKey_e : uint8_t {
    KeyA = 0,
    KeyB = 1,
    KeyX = 2,
    KeyY = 3,
    KeyLeftBumper = 4,
    KeyRightBumper = 5,
    KeyStart = 6,
    KeyOptions = 7,
    KeyLeftStick = 8,
    KeyRightStick = 9,
    CustomKey1,
    CustomKey2,
    CustomKey3,
    CustomKey4,
    CustomKey5,
    CustomKey6,
    CustomKey7,
    CustomKey8,
    kGamepadButtonCount,
};
static uint8_t Key_Fn = 0x80;
static uint8_t Key_Ununsed = 0x81;

// 在此查看引脚定义 https://docs.arduino.cc/hardware/leonardo
// 从其他地方得知 仅D0 D1 D2 D3支持中断
static uint8_t constexpr kEncoderPhaceA = PIN0;
static uint8_t constexpr kEncoderPhaceB = PIN1;
static int32_t constexpr kEncoderRpm = 600;  // 编码器参数

#if USE_KEYPAD
// 目前最大支持12个按钮
static byte constexpr kRowPinCount = 3;
static byte constexpr kColPinCount = 4;
// 这里不要用constexpr, keypad库需要non-const的数组
static byte kRowPins[kRowPinCount] = {A0, A1, A2};
static byte kColPins[kColPinCount] = {PIN4, PIN5, PIN6, PIN7};

// 控制模式, 键盘模式或者手柄模式
enum class ControlMode_e {
    KeyboardMode,
    GamepadMode,
};
static constexpr ControlMode_e controlMode = ControlMode_e::KeyboardMode;

static byte kGamepadKeyMap[kRowPinCount][kColPinCount] = {
    {KeyStart, CustomKey1, KeyOptions, Key_Ununsed},
    {CustomKey2, CustomKey3, CustomKey4, Key_Ununsed},
    {CustomKey5, CustomKey6, CustomKey7, CustomKey8},
};
static byte kKeyboardKeyMap[kRowPinCount][kColPinCount] = {
    {KEY_ESC, Key_Fn, KEY_RETURN, Key_Ununsed},
    {KEY_LEFT_ARROW, KEY_TAB, KEY_RIGHT_ARROW, Key_Ununsed},
    {'d', 'f', 'j', 'k'},
};

// Keypad
Keypad keypad = Keypad(makeKeymap(kGamepadKeyMap), kRowPins, kColPins, kRowPinCount, kColPinCount);
#else
static uint8_t constexpr kButtonCount = 10;
static uint8_t constexpr kGamepadKeys[kButtonCount] = {PIN2, PIN3, PIN4, PIN5, PIN6, PIN7, 8, 9, 10, 11};
static uint8_t constexpr kGamepadKeyMap[kButtonCount] = {KeyStart, CustomKey1, KeyOptions, CustomKey2, CustomKey3, CustomKey4, CustomKey5, CustomKey6, CustomKey7, CustomKey8};
static uint8_t gamepadLastState[kButtonCount] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH};
#endif

Joystick_ joystick(JOYSTICK_DEFAULT_REPORT_ID, JOYSTICK_TYPE_GAMEPAD,  // Joystick type
                   kGamepadButtonCount, 0,                             // Button Count, Hat Switch Count
                   true, false, false,                                 // X, Y, Z Axis
                   false, false, false,                                // Rx, Ry, Rz Axis
                   false, false, false, false, false                   // rudder, throttle, accelerator, brake, steering
);

static double constexpr kReportFrequency = 2;                          // 回报频率, 单位Hz
static int32_t constexpr kReportDelayUs = 1000000 / kReportFrequency;  // 回报延迟, 单位微秒

int32_t encL = 0;

void doEncL() {
    noInterrupts();
    auto const state = digitalRead(kEncoderPhaceB);
    // 根据文档描述, B相比A相多90°的相位差
    // 当编码器顺时针转动时, A相先输出, 然后B相再输出, 此时A相高电平, B相低电平
    // 当编码器逆时针转动时, B相先输出, 然后A相再输出, 此时B相高电平, A相低电平
    if (state == LOW) {
        encL++;
    } else {
        encL--;
    }
    interrupts();
}

void setup() {
    // 启用串口输出
    Serial.begin(9600);

#if USE_KEYPAD
#else
    for (auto &pin : kGamepadKeys) {
        pinMode(pin, INPUT_PULLUP);
    }
#endif
    switch (controlMode) {
    case ControlMode_e::KeyboardMode:
        keypad.begin(makeKeymap(kKeyboardKeyMap));
        Keyboard.begin();
        break;
    case ControlMode_e::GamepadMode:
        keypad.begin(makeKeymap(kGamepadKeyMap));
        // 初始化编码器所用引脚
        pinMode(kEncoderPhaceA, INPUT_PULLUP);
        pinMode(kEncoderPhaceB, INPUT_PULLUP);
        // Init interrupt
        attachInterrupt(digitalPinToInterrupt(kEncoderPhaceA), doEncL, RISING);
        // 初始化手柄模拟控制
        joystick.begin(false);
        joystick.setXAxisRange(0, kEncoderRpm);
        break;
    default:
        break;
    }
}

void loop() {
    // 用于延时
    uint64_t const start = micros();

#if USE_KEYPAD
    // 使用keypad进行按键输入, 可以用8个引脚输入16个按键
    if (keypad.getKeys()) {
        for (auto index = 0; index < LIST_MAX; index++) {
            auto const &key = keypad.key[index];
            if (key.stateChanged) {
                switch (key.kstate) {
                case PRESSED:
                    Serial.println("key pressed");
                    switch (controlMode) {
                    case ControlMode_e::KeyboardMode:
                        Keyboard.press(key.kchar);
                        break;
                    case ControlMode_e::GamepadMode:
                        joystick.pressButton(key.kchar);
                        break;
                    default:
                        break;
                    }
                    break;
                case HOLD:
                    Serial.println("key hold");
                    switch (controlMode) {
                    case ControlMode_e::KeyboardMode:
                        Keyboard.press(key.kchar);
                        break;
                    case ControlMode_e::GamepadMode:
                        joystick.pressButton(key.kchar);
                        break;
                    default:
                        break;
                    }
                    break;
                case RELEASED:
                    Serial.println("key released");
                    switch (controlMode) {
                    case ControlMode_e::KeyboardMode:
                        Keyboard.release(key.kchar);
                        break;
                    case ControlMode_e::GamepadMode:
                        joystick.releaseButton(key.kchar);
                        break;
                    default:
                        break;
                    }
                    break;
                case IDLE:
                    Serial.println("key idle");
                    switch (controlMode) {
                    case ControlMode_e::KeyboardMode:
                        Keyboard.release(key.kchar);
                        break;
                    case ControlMode_e::GamepadMode:
                        joystick.releaseButton(key.kchar);
                        break;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }
#else
    // 检测按键状态
    for (auto i = 0; i < kButtonCount; i++) {
        auto const state = digitalRead(kGamepadKeys[i]);
        if (state != gamepadLastState[i]) {
            gamepadLastState[i] = state;
            if (state == LOW) {
                joystick.pressButton(kGamepadKeyMap[i]);
            } else {
                joystick.releaseButton(kGamepadKeyMap[i]);
            }
        }
    }
#endif
    if (controlMode == ControlMode_e::GamepadMode) {
        // 设置编码器数据
        joystick.setXAxis(encL % kEncoderRpm);
        // 发送HID数据
        joystick.sendState();
    }

    // 根据回报率延时
    auto end = micros();
    auto delta = end - start;
    if (delta < kReportDelayUs) {
        delayMicroseconds(kReportDelayUs - delta);
    }
}
