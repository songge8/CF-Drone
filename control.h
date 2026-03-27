// control.h
#ifndef CONTROL_H
#define CONTROL_H

// 飞行模式定义
enum FlightMode {
    MODE_MANUAL = 0,
    MODE_ACRO = 1,
    MODE_STAB = 2,
    MODE_AUTO = 3
};

// 控制状态结构
struct ControlState {
    int mode;
    bool armed;
    float thrustTarget;
    float rollTarget;
    float pitchTarget;
    float yawTarget;
};

// 函数声明
void control();
void interpretControls();
void interpretWebRC();
void combineInputs();
void controlAttitude();
void controlRates();
void controlTorque();

// 辅助函数
const char* getModeName(int mode);

#endif // CONTROL_H
