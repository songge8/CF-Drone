// web_rc_global.h
// Web RC 全局声明 - 基础版（无气压定高）

#ifndef WEB_RC_GLOBAL_H
#define WEB_RC_GLOBAL_H

#if WEB_RC_ENABLED

// 全局变量声明（使用extern）
extern bool webRCEnabled;
extern bool useWebRC;
extern bool webRCUpdated;
extern float webRCRoll, webRCPitch, webRCYaw, webRCThrottle;
extern uint16_t webRCButtons;
extern unsigned long webRCLastUpdate;

// Web RC控制参数
extern float webRCThrottleScale;
extern float webRCStickScale;
extern float webRCYawScale;

// 油门参数
extern float throttleDeadzone;
extern float throttleExpo;

// 飞控状态（来自 control.ino，供 web_rc_core.ino 读取）
extern int mode;
extern bool armed;

// 函数声明
void setWebRCInput(float roll, float pitch, float yaw, float throttle, uint16_t buttons);
bool isWebRCEnabled();
bool isUsingWebRC();
void setupWebRC();
void readWebRC();
void processWebRC();

#endif // WEB_RC_ENABLED

#endif // WEB_RC_GLOBAL_H
