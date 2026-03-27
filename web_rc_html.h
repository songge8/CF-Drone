// web_rc_html.h
// Web RC HTML页面定义 - 基础版（无气压定高）

#if WEB_RC_ENABLED

const char webRCIndexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,maximum-scale=1.0,user-scalable=no">
<title>无人机遥控器</title>
<style>
/*======== 响应式变量 ========*/
:root{
  --js-size:clamp(130px,min(36vw,38vh),240px);
  --knob-size:calc(var(--js-size)*0.25);
  --pad:clamp(6px,1.5vmin,15px);
  --gap:clamp(6px,1.5vmin,15px);
}
/*======== 通用样式 ========*/
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent;touch-action:none;user-select:none}
body{font-family:Arial,"Microsoft YaHei",sans-serif;background:linear-gradient(135deg,#0f2027 0%,#203a43 50%,#2c5364 100%);color:#fff;overflow:hidden;height:100vh;height:100dvh;width:100vw}
.container{width:100%;height:100%;display:flex;flex-direction:column;padding:var(--pad);gap:var(--gap);max-width:1200px;margin:0 auto;overflow:hidden}

/*======== 顶部状态栏 ========*/
.header {
  text-align: center;
  padding: 6px 10px;
  background: rgba(0,0,0,.5);
  border-radius: 12px;
  border: 2px solid rgba(76,201,240,.4);
  box-shadow: 0 4px 16px rgba(0,0,0,.4);
  backdrop-filter: blur(10px)
}

.header h1 {
  font-size: 1.1rem;
  color: #4cc9f0;
  text-shadow: 0 0 10px rgba(76,201,240,.8);
  margin-bottom: 4px;
}

.status-bar {
  display: flex;
  justify-content: center;
  align-items: center;
  gap: clamp(4px,1.2vmin,10px);
  flex-wrap: nowrap;
  overflow: hidden;
  margin-top: 4px;
}

.status-item {
  display: flex;
  align-items: center;
  gap: 4px;
  padding: 2px 5px;
  background: rgba(0,0,0,.3);
  border-radius: 6px;
  border: 1px solid rgba(255,255,255,.1);
  font-size: clamp(0.58rem,1.8vmin,0.7rem);
  white-space: nowrap;
  flex-shrink: 1;
  min-width: 0;
}

.status-dot {
  width: 8px;
  height: 8px;
  border-radius: 50%;
  display: inline-block;
}

.status-dot.connected { background: #0f8; box-shadow: 0 0 8px #0f8; }
.status-dot.disconnected { background: #f33; box-shadow: 0 0 8px #f33; }
.status-dot.warning { background: #ff9; box-shadow: 0 0 8px #ff9; }

/*======== 内容区 ========*/
.content{display:flex;flex:1;gap:var(--gap);overflow:hidden;min-height:0}
.joystick-container{flex:1;display:flex;flex-direction:column;justify-content:center;align-items:center;background:rgba(0,0,0,.4);border-radius:20px;padding:clamp(8px,1.5vmin,15px);border:2px solid rgba(67,97,238,.5);box-shadow:inset 0 0 30px rgba(0,0,0,.5)}
.joystick-title{font-size:clamp(0.75rem,2.2vmin,1.1rem);color:#72efdd;text-shadow:0 0 10px rgba(114,239,221,.6)}
.joystick-wrapper{width:100%;height:var(--js-size);display:flex;justify-content:center;align-items:center;position:relative;margin-top:clamp(4px,2vh,20px);}
.joystick{width:var(--js-size);height:var(--js-size);background:radial-gradient(circle at 30% 30%,rgba(255,255,255,.15),rgba(0,0,0,.3));border-radius:50%;position:relative;border:3px solid #4361ee;box-shadow:inset 0 0 25px rgba(67,97,238,.5),0 8px 25px rgba(0,0,0,.5);overflow:hidden}
.joystick::before{content:'';position:absolute;top:50%;left:50%;width:2px;height:100%;background:linear-gradient(to bottom,transparent,rgba(255,255,255,.2),transparent);transform:translate(-50%,-50%)}
.joystick::after{content:'';position:absolute;top:50%;left:50%;width:100%;height:2px;background:linear-gradient(to right,transparent,rgba(255,255,255,.2),transparent);transform:translate(-50%,-50%)}
.joystick-knob{width:var(--knob-size);height:var(--knob-size);background:radial-gradient(circle at 30% 30%,#fff,#b8c6ff);border-radius:50%;position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);border:2px solid #fff;box-shadow:0 4px 15px rgba(0,0,0,.5),inset 0 0 10px rgba(255,255,255,.7);cursor:move;transition:transform .1s ease-out;z-index:10}
.coordinates{margin-top:clamp(4px,1vh,12px);text-align:center;font-family:'Courier New',monospace;font-size:clamp(0.65rem,1.8vmin,0.9rem);color:#a9def9;display:flex;flex-direction:column;gap:3px}

/*======== 按钮区 ========*/
.buttons-container{flex:.8;display:flex;flex-direction:column;gap:10px;padding:12px;background:rgba(0,0,0,.4);border-radius:20px;border:2px solid rgba(247,37,133,.4);box-shadow:inset 0 0 20px rgba(0,0,0,.5)}
.buttons-grid{display:grid;grid-template-columns:repeat(3,1fr);grid-template-rows:repeat(3,1fr);gap:8px;flex:1}
.button{background:linear-gradient(145deg,#3a0ca3,#4361ee);border:none;border-radius:10px;color:#fff;font-size:.85rem;font-weight:bold;display:flex;flex-direction:column;justify-content:center;align-items:center;text-align:center;padding:10px 5px;cursor:pointer;transition:all .15s cubic-bezier(.4,0,.2,1);box-shadow:0 3px 10px rgba(0,0,0,.3),inset 0 1px 0 rgba(255,255,255,.2);position:relative}
.button:hover{background:linear-gradient(145deg,#4361ee,#3a0ca3);transform:translateY(-1px)}
.button.active{background:linear-gradient(145deg,#f72585,#b5179e);box-shadow:0 0 15px rgba(247,37,133,.6),inset 0 1px 0 rgba(255,255,255,.3);transform:scale(.95)}
.button:active{transform:scale(.92)}
.button-icon{font-size:1.1rem;margin-bottom:4px}

/*======== 参数面板 ========*/
.params-toggle-bar{display:flex;justify-content:center;flex-shrink:0}
.params-toggle-btn{background:rgba(67,97,238,0.35);border:1px solid rgba(67,97,238,0.8);border-radius:8px;color:#a9def9;padding:5px 28px;font-size:0.78rem;cursor:pointer;transition:all .2s;touch-action:auto}
.params-toggle-btn.open{background:rgba(247,37,133,0.25);border-color:rgba(247,37,133,0.7);color:#faa}
.params-panel{max-height:0;transition:max-height 0.35s ease;flex-shrink:0;overflow:hidden}
.params-panel.open{max-height:min(280px,45vh);overflow-y:auto;-webkit-overflow-scrolling:touch}
.params-inner{background:rgba(0,0,0,.5);border-radius:12px;border:1px solid rgba(67,97,238,.3);padding:10px 14px}
.params-grid{display:grid;grid-template-columns:1fr 1fr;gap:6px 20px}

/*======== 控制滑块 ========*/
.slider-group{display:flex;flex-direction:column;gap:3px}
.slider-label{color:#a9def9;font-size:.72rem;display:flex;justify-content:space-between}
.slider-label span{color:#4cc9f0;font-weight:bold}
input[type=range]{touch-action:pan-x}
.slider{width:100%;height:6px;-webkit-appearance:none;background:linear-gradient(to right,#3a0ca3,#4361ee);border-radius:3px;outline:none}
.slider::-webkit-slider-thumb{-webkit-appearance:none;width:16px;height:16px;border-radius:50%;background:#fff;cursor:pointer;border:2px solid #4361ee;box-shadow:0 0 8px rgba(67,97,238,.8)}

/*======== 协议选择器 =========*/
.protocol-selector{display:flex;gap:8px;margin-top:8px;justify-content:center}
.protocol-btn{background:rgba(67,97,238,0.3);border:1px solid rgba(67,97,238,0.6);border-radius:6px;color:#a9def9;padding:3px 10px;font-size:0.68rem;cursor:pointer;transition:all .2s;touch-action:auto}
.protocol-btn.active{background:rgba(67,97,238,0.8);color:#fff;border-color:#4cc9f0;box-shadow:0 0 8px rgba(76,201,240,0.5)}

/*======== 动画 ========*/
@keyframes pulse{0%{box-shadow:0 0 0 0 rgba(67,97,238,.7)}70%{box-shadow:0 0 0 12px rgba(67,97,238,0)}100%{box-shadow:0 0 0 0 rgba(67,97,238,0)}}
.joystick.active{animation:pulse 1.5s infinite}
@keyframes fadeIn{from{opacity:0;transform:translateY(20px)}to{opacity:1;transform:translateY(0)}}
.container>*{animation:fadeIn .5s ease-out}

/*======== 竖屏自适应 ========*/
@media (orientation:portrait){
  :root{--js-size:clamp(130px,40vw,200px);--knob-size:calc(var(--js-size)*0.25)}
  .content{
    display:grid;
    grid-template-columns:1fr 1fr;
    grid-template-rows:auto auto;
    height:auto;
    flex:none;
    overflow:visible;
  }
  .buttons-container{grid-column:1/3;grid-row:1}
  .content>.joystick-container:first-child{grid-column:1;grid-row:2}
  .content>.joystick-container:last-child{grid-column:2;grid-row:2}
  .params-grid{grid-template-columns:1fr}
  .header h1{font-size:clamp(0.85rem,3.5vw,1.1rem)}
  .status-bar{gap:6px}
  .status-item{font-size:clamp(0.6rem,2.5vw,0.7rem);padding:2px 5px}
}

/*======== 小屏横屏自适应（高度≤420px）========*/
@media (max-height:420px) and (orientation:landscape){
  :root{--js-size:clamp(110px,min(33vw,34vh),190px);--knob-size:calc(var(--js-size)*0.25)}
  .joystick-title{font-size:0.72rem}
  .coordinates{font-size:0.62rem}
  .header h1{font-size:0.82rem}
  .header{padding:3px 8px}
  .header h1{margin-bottom:2px}
  .status-bar{margin-top:2px;gap:4px}
  .status-item{font-size:0.58rem;padding:2px 4px}
  .button{font-size:0.68rem;padding:5px 3px}
  .button-icon{font-size:0.88rem;margin-bottom:2px}
  .params-grid{grid-template-columns:repeat(3,1fr)}
}
</style>
</head>
<body>
<div class="container">
  <!-- 顶部状态栏 -->
  <div class="header">
    <h1>无人机 WEB RC 遥控器</h1>
    <div class="status-bar">
      <div class="status-item"><span class="status-dot" id="status-dot"></span><span id="connection-text">连接中...</span></div>
      <div class="status-item"><span>⏱️</span><span id="latency">-</span></div>
      <div class="status-item"><span>📦</span><span id="packet-loss">0%</span></div>
      <div class="status-item"><span>🎮</span><span id="flight-mode">STAB</span></div>
      <div class="status-item"><span>⚡</span><span id="battery">-</span></div>
    </div>
  </div>

  <div class="content">
    <!-- 左摇杆 -->
    <div class="joystick-container">
      <div class="joystick-title">左摇杆 (油门/偏航)</div>
      <div class="coordinates">
        <div>油门: <span id="left-y">0</span></div>
        <div>偏航: <span id="left-x">0</span></div>
      </div>
      <div class="joystick-wrapper">
        <div class="joystick" id="joystick-left"><div class="joystick-knob" id="knob-left"></div></div>
      </div>
    </div>

    <!-- 按钮区 -->
    <div class="buttons-container">
      <div class="buttons-grid" id="buttons-container"></div>
    </div>

    <!-- 右摇杆 -->
    <div class="joystick-container">
      <div class="joystick-title">右摇杆 (俯仰/横滚)</div>
      <div class="coordinates">
        <div>俯仰: <span id="right-y">0</span></div>
        <div>横滚: <span id="right-x">0</span></div>
      </div>
      <div class="joystick-wrapper">
        <div class="joystick" id="joystick-right"><div class="joystick-knob" id="knob-right"></div></div>
      </div>
    </div>
  </div>

  <!-- 参数折叠面板 -->
  <div class="params-panel" id="params-panel">
    <div class="params-inner">
      <div class="params-grid">
        <div class="slider-group">
          <div class="slider-label">油门功率限制 <span id="throttle-sens-value">100</span>%</div>
          <input type="range" class="slider" id="throttle-sensitivity" min="60" max="100" value="100">
        </div>
        <div class="slider-group">
          <div class="slider-label">摇杆死区 <span id="deadzone-value">3</span>%</div>
          <input type="range" class="slider" id="deadzone-slider" min="0" max="10" value="3">
        </div>
        <div class="slider-group">
          <div class="slider-label">姿态灵敏度 <span id="stick-sens-value">85</span>%</div>
          <input type="range" class="slider" id="stick-sensitivity" min="30" max="100" value="85">
        </div>
        <div class="slider-group">
          <div class="slider-label">中心柔和度 <span id="expo-value">25</span>%</div>
          <input type="range" class="slider" id="expo-slider" min="0" max="60" value="25">
        </div>
        <div class="slider-group">
          <div class="slider-label">最大倾斜角 <span id="tilt-value">30</span>°</div>
          <input type="range" class="slider" id="tilt-slider" min="10" max="45" value="30">
        </div>
        <div class="slider-group">
          <div class="slider-label">操控平滑度 <span id="smooth-value">40</span>%</div>
          <input type="range" class="slider" id="smooth-slider" min="10" max="80" value="40">
        </div>
      </div>
      <div class="protocol-selector">
        <button class="protocol-btn active" onclick="setProtocol('json')">JSON</button>
        <button class="protocol-btn" onclick="setProtocol('binary')">二进制</button>
        <button class="protocol-btn" onclick="setProtocol('delta')">增量</button>
      </div>
    </div>
  </div>
</div>

<script>
/*======================== 全局变量 ========================*/
let lastAnimationTime = 0;
let connectionOk = false;
let packetStats = { sent: 0, lost: 0 };
let latencyHistory = new Array(10).fill(0);
let latencyIndex = 0;
let lastSendTime = 0;
const SEND_INTERVAL = 100; // P1优化: ~10Hz (100ms 高延迟模式，原60Hz)

const touches = new Map();
let leftStick = {x:0, y:0, rawX:0, rawY:0};
let rightStick = {x:0, y:0, rawX:0, rawY:0};

let lastSentValues = { throttle: 0, roll: 0, pitch: 0, yaw: 0 };
let currentValues = { throttle: 0, roll: 0, pitch: 0, yaw: 0 };
const MIN_CHANGE_THRESHOLD = 0.5;

let currentProtocol = 'json';
let useBinaryProtocol = false;
let useDeltaProtocol = false;

let deadzone = 3;
let expo = 25;
let stickSensitivity = 85;
let throttleSensitivity = 80;
let smoothing = 40;
let tiltMax = 30;

let buttonStates = new Array(16).fill(false);
let lastButtonStates = new Array(16).fill(false);

/*======================== 按钮配置（3×3 九宫格）========================*/
// 排列：第一行:解锁/上锁/急停  第二行:起飞/降落/蜂鸣  第三行:灯光/摇杆校准/调参
const buttonConfigs = [
  {icon:"🔓",label:"解锁",   color:"#00ff88",desc:"解锁电机"},
  {icon:"🔒",label:"上锁",   color:"#ff3333",desc:"锁定电机"},
  {icon:"🛑",label:"急停",   color:"#ff0055",desc:"紧急停止电机"},
  {icon:"🚀",label:"起飞",   color:"#ff9900",desc:"自动起飞"},
  {icon:"🛬",label:"降落",   color:"#ff6600",desc:"自动降落"},
  {icon:"🔊",label:"蜂鸣",   color:"#ff9900",desc:"蜂鸣器定位"},
  {icon:"�",label:"模式",   color:"#00cfff",desc:"ACRO↔STAB模式切换"},
  {icon:"🎯",label:"摇杆校准",color:"#7209b7",desc:"重置摇杆零点"},
  {icon:"⚙️",label:"调参",   color:"#4cc9f0",desc:"展开参数设置"}
];

/*======================== 初始化 ========================*/
function init() {
  initButtons();
  initControls();
  initNetwork();
  initPointerEvents();
  loadSettings();
  requestAnimationFrame(animationLoop);
}

function initButtons() {
  const container = document.getElementById('buttons-container');
  container.innerHTML = '';
  buttonConfigs.forEach((cfg, idx) => {
    const btn = document.createElement('button');
    btn.className = 'button';
    btn.id = `btn-${idx}`;
    btn.title = cfg.desc;
    btn.innerHTML = `<div class="button-icon">${cfg.icon}</div><div>${cfg.label}</div>`;
    btn.style.background = `linear-gradient(145deg, ${cfg.color}44, ${cfg.color}88)`;
    btn.style.border = `2px solid ${cfg.color}`;
    
    btn.addEventListener('pointerdown', e => {
      e.preventDefault();
      handleButton(idx);
      if (navigator.vibrate) navigator.vibrate(20);
    });
    
    container.appendChild(btn);
  });
}

function initControls() {
  const sliders = [
    'deadzone-slider', 'expo-slider', 'stick-sensitivity',
    'throttle-sensitivity', 'smooth-slider', 'tilt-slider'
  ];
  
  sliders.forEach(id => {
    const slider = document.getElementById(id);
    if (slider) {
      slider.addEventListener('input', updateControlParams);
    }
  });
}

function initNetwork() {
  updateConnectionStatus(true);
  setInterval(updateNetworkStatus, 1000);
}

function initPointerEvents() {
  const joysticks = [
    { id: 'joystick-left', side: 'left' },
    { id: 'joystick-right', side: 'right' }
  ];
  
  joysticks.forEach(js => {
    const element = document.getElementById(js.id);
    
    element.addEventListener('pointerdown', e => {
      e.preventDefault();
      handlePointerStart(e, js.side);
      element.setPointerCapture(e.pointerId);
    });
    
    element.addEventListener('pointermove', e => {
      e.preventDefault();
      handlePointerMove(e, js.side);
    });
    
    element.addEventListener('pointerup', e => {
      e.preventDefault();
      handlePointerEnd(e, js.side);
    });
    
    element.addEventListener('pointercancel', e => {
      e.preventDefault();
      handlePointerEnd(e, js.side);
    });
    
    element.addEventListener('touchstart', e => e.preventDefault());
    element.addEventListener('touchmove', e => e.preventDefault());
  });
}

/*======================== 动画循环 ========================*/
function animationLoop(timestamp) {
  const deltaTime = timestamp - lastAnimationTime;
  
  if (deltaTime >= SEND_INTERVAL) {
    processJoystickInput();
    checkAndSendChanges();
    updateDisplayAll();
    lastAnimationTime = timestamp;
  }
  
  requestAnimationFrame(animationLoop);
}

/*======================== 控制参数更新 ========================*/
function updateControlParams() {
  deadzone = parseInt(document.getElementById('deadzone-slider').value);
  expo = parseInt(document.getElementById('expo-slider').value);
  stickSensitivity = parseInt(document.getElementById('stick-sensitivity').value);
  throttleSensitivity = parseInt(document.getElementById('throttle-sensitivity').value);
  smoothing = parseInt(document.getElementById('smooth-slider').value);
  tiltMax = parseInt(document.getElementById('tilt-slider').value);
  
  document.getElementById('deadzone-value').textContent = deadzone;
  document.getElementById('expo-value').textContent = expo;
  document.getElementById('stick-sens-value').textContent = stickSensitivity;
  document.getElementById('throttle-sens-value').textContent = throttleSensitivity;
  document.getElementById('smooth-value').textContent = smoothing;
  document.getElementById('tilt-value').textContent = tiltMax;
  
  sendParamsUpdate();
  saveSettings();
}

/*======================== 摇杆曲线处理 ========================*/
function applyCurve(value) {
  const absVal = Math.abs(value);
  if (absVal < deadzone / 100) return 0;
  
  const normalized = (absVal - deadzone / 100) / (1 - deadzone / 100);
  const sign = value >= 0 ? 1 : -1;
  const expoFactor = expo / 100;
  
  // 仅应用指数曲线，不在前端做灵敏度缩放（后端通过 webRCStickScale 单次应用）
  let curved = normalized * (1 - expoFactor) + Math.pow(normalized, 3) * expoFactor;
  
  return curved * sign;
}

function applyThrottleCurve(value) {
  // 线性直通：直接发送原始油门值(0~100)，功率限制在后端应用
  return value;
}

/*======================== 摇杆数据处理 ========================*/
function processJoystickInput() {
  let throttleRaw = (leftStick.rawY + 100) / 2;
  currentValues.throttle = applyThrottleCurve(throttleRaw);
  currentValues.yaw = applyCurve(leftStick.rawX);
  currentValues.pitch = applyCurve(rightStick.rawY);
  currentValues.roll = applyCurve(rightStick.rawX);
  
  leftStick.x = applyCurve(leftStick.rawX);
  leftStick.y = throttleRaw - 50;
  rightStick.x = applyCurve(rightStick.rawX);
  rightStick.y = applyCurve(rightStick.rawY);
}

function hasSignificantChange(newValues) {
  return (
    Math.abs(newValues.throttle - lastSentValues.throttle) > MIN_CHANGE_THRESHOLD ||
    Math.abs(newValues.roll - lastSentValues.roll) > MIN_CHANGE_THRESHOLD ||
    Math.abs(newValues.pitch - lastSentValues.pitch) > MIN_CHANGE_THRESHOLD ||
    Math.abs(newValues.yaw - lastSentValues.yaw) > MIN_CHANGE_THRESHOLD
  );
}

function checkAndSendChanges() {
  if (hasSignificantChange(currentValues)) {
    sendJoystickData();
  }
  
  for (let i = 0; i < buttonStates.length; i++) {
    if (buttonStates[i] !== lastButtonStates[i]) {
      sendButtonData(i, buttonStates[i]);
      lastButtonStates[i] = buttonStates[i];
    }
  }
}

/*======================== 数据发送函数 ========================*/
function sendJoystickData() {
  const data = {
    t: 1,
    th: Math.round(currentValues.throttle),
    r: Math.round(currentValues.roll),
    p: Math.round(currentValues.pitch),
    y: Math.round(currentValues.yaw),
    ts: performance.now(),
    dz: deadzone,
    ex: expo,
    ss: stickSensitivity
  };
  
  sendToESP('/web_rc', data);
  lastSentValues = {...currentValues};
  packetStats.sent++;
}

function sendButtonData(buttonIndex, state) {
  const data = {
    t: 2,
    b: buttonIndex,
    s: state ? 1 : 0,
    ts: performance.now()
  };
  
  // P1优化: 关键按鈕（解锁/上锁/急停）重传3次提高可靠性
  const criticalButtons = [0, 1, 2]; // 解锁、上锁、急停
  const retries = criticalButtons.includes(buttonIndex) ? 3 : 1;
  
  for (let i = 0; i < retries; i++) {
    setTimeout(() => {
      sendToESP('/web_rc', data);
    }, i * 50);  // 间隔50ms发送，避免冲突
  }
  
  if (navigator.vibrate) {
    navigator.vibrate(20);
  }
}

function sendParamsUpdate() {
  const data = {
    t: 5,
    ths: throttleSensitivity,
    sss: stickSensitivity,
    ys: stickSensitivity * 0.8,
    ts: performance.now()
  };
  
  sendToESP('/web_rc', data);
}

/*======================== 显示更新 ========================*/
function updateDisplayAll() {
  document.getElementById('left-x').textContent = Math.round(leftStick.x);
  document.getElementById('left-y').textContent = Math.round(currentValues.throttle) + '%';
  document.getElementById('right-x').textContent = Math.round(rightStick.x);
  document.getElementById('right-y').textContent = Math.round(rightStick.y);
}

/*======================== Pointer Events处理 ========================*/
function handlePointerStart(e, side) {
  e.preventDefault();
  touches.set(e.pointerId, side);
  document.getElementById(`joystick-${side}`).classList.add('active');
  updateJoystickPosition(side, e.clientX, e.clientY);
}

function handlePointerMove(e, side) {
  e.preventDefault();
  if (touches.get(e.pointerId) === side) {
    updateJoystickPosition(side, e.clientX, e.clientY);
  }
}

function handlePointerEnd(e, side) {
  e.preventDefault();
  if (touches.get(e.pointerId) === side) {
    touches.delete(e.pointerId);
    const joystick = document.getElementById(`joystick-${side}`);
    const knob = document.getElementById(`knob-${side}`);
    
    joystick.classList.remove('active');
    knob.style.transition = 'transform 0.2s ease-out';
    knob.style.transform = 'translate(-50%, -50%)';
    
    setTimeout(() => {
      knob.style.transition = '';
    }, 200);
    
    if (side === 'left') {
      leftStick = {x:0, y:0, rawX:0, rawY:0};
    } else {
      rightStick = {x:0, y:0, rawX:0, rawY:0};
    }
    
    processJoystickInput();
    sendJoystickData();
  }
}

function updateJoystickPosition(side, clientX, clientY) {
  const joystick = document.getElementById(`joystick-${side}`);
  const knob = document.getElementById(`knob-${side}`);
  const rect = joystick.getBoundingClientRect();
  
  const centerX = rect.width / 2;
  const centerY = rect.height / 2;
  const radius = centerX - 30;
  
  let deltaX = (clientX - rect.left) - centerX;
  let deltaY = (clientY - rect.top) - centerY;
  
  const distance = Math.sqrt(deltaX * deltaX + deltaY * deltaY);
  if (distance > radius) {
    deltaX = (deltaX / distance) * radius;
    deltaY = (deltaY / distance) * radius;
  }
  
  knob.style.transform = `translate(calc(-50% + ${deltaX}px), calc(-50% + ${deltaY}px))`;
  
  const normX = (deltaX / radius) * 100;
  const normY = -(deltaY / radius) * 100;
  
  if (side === 'left') {
    leftStick.rawX = normX;
    leftStick.rawY = normY;
  } else {
    rightStick.rawX = normX;
    rightStick.rawY = normY;
  }
}

/*======================== 网络处理 ========================*/
function sendToESP(url, data) {
  const startTime = performance.now();
  
  fetch(url, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(data)
  })
  .then(response => {
    if (response.ok) {
      const latency = performance.now() - startTime;
      updateLatency(latency);
      return response.json();
    }
    throw new Error('Network response was not ok');
  })
  .then(responseData => {
    updateConnectionStatus(true);
    // 更新飞行模式和解锁状态显示
    if (responseData.m !== undefined) {
      const modeNames = ['RAW', 'ACRO', 'STAB', 'AUTO'];
      const modeName = modeNames[responseData.m] || 'STAB';
      document.getElementById('flight-mode').textContent = modeName;
    }
  })
  .catch(error => {
    packetStats.lost++;
    updateConnectionStatus(false);
  });
}

function updateLatency(latency) {
  latencyHistory[latencyIndex] = latency;
  latencyIndex = (latencyIndex + 1) % latencyHistory.length;
  
  const avgLatency = latencyHistory.reduce((a, b) => a + b) / latencyHistory.length;
  document.getElementById('latency').textContent = Math.round(avgLatency) + 'ms';
  
  const latencyDot = document.querySelector('.status-dot');
  if (avgLatency > 200) {
    latencyDot.className = 'status-dot warning';
  } else if (avgLatency > 100) {
    latencyDot.className = 'status-dot warning';
  } else {
    latencyDot.className = 'status-dot connected';
  }
}

function updateNetworkStatus() {
  const lossRate = packetStats.sent > 0 ? 
    (packetStats.lost / packetStats.sent * 100).toFixed(1) : '0';
  document.getElementById('packet-loss').textContent = lossRate + '%';
  
  fetch('/web_rc/status')
    .then(r => r.json())
    .then(data => {
      document.getElementById('battery').textContent = data.battery + '%';
    })
    .catch(() => {
      document.getElementById('battery').textContent = '-';
    });
}

function updateConnectionStatus(connected) {
  connectionOk = connected;
  const dot = document.getElementById('status-dot');
  const text = document.getElementById('connection-text');
  
  if (connected) {
    dot.className = 'status-dot connected';
    text.textContent = '已连接';
    text.style.color = '#0f8';
  } else {
    dot.className = 'status-dot disconnected';
    text.textContent = '连接断开';
    text.style.color = '#f33';
  }
}

/*======================== 协议设置 ========================*/
function setProtocol(protocol) {
  currentProtocol = protocol;
  useBinaryProtocol = (protocol === 'binary');
  useDeltaProtocol = (protocol === 'delta');

  const btns = document.querySelectorAll('.protocol-btn');
  btns.forEach(btn => btn.classList.remove('active'));
  const pidx = ['json','binary','delta'].indexOf(protocol);
  if (pidx >= 0 && btns[pidx]) btns[pidx].classList.add('active');

  localStorage.setItem('flix_rc_protocol', protocol);
}

/*======================== 按钮处理 ========================*/
function handleButton(idx) {
  // 摇杆校准：直接执行，不切换状态
  if (idx === 7) {
    calibrateCenter();
    return;
  }
  // 调参按钮：展开/收起参数面板
  if (idx === 8) {
    toggleParams();
    return;
  }

  // 起飞/降落/蜂鸣：功能暂未实现，弹出提示
  if (idx === 3 || idx === 4 || idx === 5) {
    const names = ['起飞', '降落', '蜂鸣定位'];
    showToast('🚧 ' + names[idx - 3] + ' 功能暂未实现');
    return;
  }

  // 模式切换（ACRO↔STAB）
  if (idx === 6) {
    const currentMode = document.getElementById('flight-mode').textContent;
    const targetBit = (currentMode === 'STAB') ? 7 : 6;  // STAB时切到ACRO(bit7), 其他时切到STAB(bit6)
    sendButtonData(targetBit, 1);
    setTimeout(() => sendButtonData(targetBit, 0), 100);
    if (navigator.vibrate) navigator.vibrate(30);
    return;
  }

  // 解锁：检查油门是否过高
  if (idx === 0 && !buttonStates[0]) {
    if (currentValues.throttle > 30) {
      showToast('⚠️ 油门过高，无法解锁！请将油门降至30%以下');
      return;
    }
  }

  buttonStates[idx] = !buttonStates[idx];
  const btn = document.getElementById(`btn-${idx}`);
  
  if (buttonStates[idx]) {
    btn.classList.add('active');
    btn.style.transform = 'scale(0.95)';
  } else {
    btn.classList.remove('active');
    btn.style.transform = '';
  }
  
  sendButtonData(idx, buttonStates[idx]);
  
  if (navigator.vibrate) {
    navigator.vibrate(20);
  }
}

/*======================== Toast提示 ========================*/
function showToast(msg) {
  let toast = document.getElementById('toast-msg');
  if (!toast) {
    toast = document.createElement('div');
    toast.id = 'toast-msg';
    toast.style.cssText = 'position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);' +
      'background:rgba(0,0,0,0.85);color:#fff;padding:12px 20px;border-radius:10px;' +
      'font-size:14px;z-index:9999;pointer-events:none;text-align:center;max-width:80vw;' +
      'border:1px solid rgba(255,255,255,0.2);transition:opacity 0.3s;';
    document.body.appendChild(toast);
  }
  toast.textContent = msg;
  toast.style.opacity = '1';
  clearTimeout(toast._timer);
  toast._timer = setTimeout(() => { toast.style.opacity = '0'; }, 2000);
}

/*======================== 校准功能 ========================*/
function calibrateCenter() {
  if (confirm('请确保摇杆在中心位置，然后点击确定进行校准')) {
    leftStick = {x:0, y:0, rawX:0, rawY:0};
    rightStick = {x:0, y:0, rawX:0, rawY:0};
    currentValues = { throttle: 0, roll: 0, pitch: 0, yaw: 0 };
    lastSentValues = { throttle: 0, roll: 0, pitch: 0, yaw: 0 };
    
    sendToESP('/web_rc', {
      t: 3,
      ts: performance.now()
    });
    
    alert('校准完成！');
  }
}

/*======================== 设置保存/加载 ========================*/
function saveSettings() {
  const settings = {
    deadzone: deadzone,
    expo: expo,
    stickSensitivity: stickSensitivity,
    throttleSensitivity: throttleSensitivity,
    smoothing: smoothing,
    tiltMax: tiltMax,
    protocol: currentProtocol
  };
  
  localStorage.setItem('flix_rc_settings', JSON.stringify(settings));
}

function loadSettings() {
  const saved = localStorage.getItem('flix_rc_settings');
  if (saved) {
    try {
      const settings = JSON.parse(saved);
      deadzone = settings.deadzone || 3;
      expo = settings.expo || 25;
      stickSensitivity = settings.stickSensitivity || 85;
      throttleSensitivity = settings.throttleSensitivity || 80;
      smoothing = settings.smoothing || 40;
      tiltMax = settings.tiltMax || 30;
      currentProtocol = settings.protocol || 'json';
      
      document.getElementById('deadzone-slider').value = deadzone;
      document.getElementById('expo-slider').value = expo;
      document.getElementById('stick-sensitivity').value = stickSensitivity;
      document.getElementById('throttle-sensitivity').value = throttleSensitivity;
      document.getElementById('smooth-slider').value = smoothing;
      document.getElementById('tilt-slider').value = tiltMax;
      
      setProtocol(currentProtocol);
      updateControlParams();
    } catch (e) {
      console.error('Failed to load settings:', e);
    }
  }
}

/*======================== 参数面板折叠 ========================*/
function toggleParams() {
  const panel = document.getElementById('params-panel');
  const isOpen = panel.classList.toggle('open');
  // 同步调参按钮的 active 样式
  const btn = document.getElementById('btn-8');
  if (btn) {
    if (isOpen) { btn.classList.add('active'); btn.style.transform = 'scale(0.95)'; }
    else         { btn.classList.remove('active'); btn.style.transform = ''; }
  }
}

/*======================== 事件监听器 ========================*/
document.addEventListener('DOMContentLoaded', init);
document.addEventListener('contextmenu', e => e.preventDefault());

setInterval(() => {
  if (connectionOk) {
    sendToESP('/web_rc/heartbeat', {
      t: 4,
      ts: performance.now()
    });
  }
}, 5000);  // P0修复: 3s→5s，配合后端10s超时阈值
</script>
</body>
</html>
)rawliteral";

#endif
