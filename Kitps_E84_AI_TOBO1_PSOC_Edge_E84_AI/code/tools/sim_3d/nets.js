/*
 * 全部板间连线。依据 code/WIRING.md 和各固件的引脚定义。
 * 改接线时先改 WIRING.md，再改这里；tools/check_sim_wiring.py 会检查两边是否一致。
 *
 * from/to 的格式是“部件.引脚”。BB.G、BB.V 表示面包板负轨、舵机电源正轨，每次用到时自动分配一个孔。
 */
window.SIM_NETS = (function () {
  var C = { tx: '#2f7fe0', rx: '#25a55f', gnd: '#23262b', v33: '#e0453a', v18: '#f0a030', v5: '#d8212a', trig: '#f08020', echo: '#e8c020', sig: '#ef8a1e', srv: '#d82a2a', brown: '#6d3f22', usb: '#b9bdc2' };
  var n = [];
  function add(group, title, from, to, color, note) { n.push({ id: 'n' + n.length, group: group, title: title, from: from, to: to, color: color, note: note || '' }); }

  // H7 ↔ 84E（必须经过 1.8 V ↔ 3.3 V 电平转换）
  add('uart84', 'H7→84E 命令（3.3 V 侧）', 'H7.PB6', 'LS.HV1', C.tx, 'USART1 TX，115200 8N1');
  add('uart84', 'H7→84E 命令（1.8 V 侧）', 'LS.LV1', 'E.J5_2', C.tx, 'J5 第 2 针 = P9_3 = SCB1 RX');
  add('uart84', '84E→H7 结果（1.8 V 侧）', 'E.J5_3', 'LS.LV2', C.rx, 'J5 第 3 针 = P9_2 = SCB1 TX');
  add('uart84', '84E→H7 结果（3.3 V 侧）', 'LS.HV2', 'H7.PB7', C.rx, 'USART1 RX');
  add('uart84', '电平转换 3.3 V 供电', 'H7.3V3A', 'LS.HV', C.v33, '来自 H7 板 3.3 V');
  add('uart84', '电平转换 1.8 V 基准', 'E.J5_1', 'LS.LV', C.v18, 'J5 第 1 针 = VDD_1V8');
  add('uart84', '电平转换共地', 'LS.GND', 'BB.G', C.gnd);
  add('uart84', '84E 共地', 'E.J5_6', 'BB.G', C.gnd, 'J5 第 6 针 = GND');

  // H7 ↔ ESP32
  add('uartA', 'H7→ESP32-A 命令', 'H7.PD5', 'A.G2', C.tx, 'USART2 TX → ESP32 UART1 RX');
  add('uartA', 'ESP32-A→H7 结果', 'A.G1', 'H7.PD6', C.rx, 'ESP32 UART1 TX → USART2 RX');
  add('uartA', 'ESP32-A 共地', 'A.GND', 'BB.G', C.gnd);
  add('uartA', 'ESP32-A 角色脚', 'A.G14', 'A.3V3', C.v33, 'GPIO14 接 3.3 V 表示 A 板');
  add('uartB', 'H7→ESP32-B 命令', 'H7.PD8', 'B.G2', C.tx, 'USART3 TX → ESP32 UART1 RX');
  add('uartB', 'ESP32-B→H7 结果', 'B.G1', 'H7.PD9', C.rx, 'ESP32 UART1 TX → USART3 RX');
  add('uartB', 'ESP32-B 共地', 'B.GND', 'BB.G', C.gnd);
  add('uartB', 'ESP32-B 角色脚', 'B.G14', 'BB.G', C.gnd, 'GPIO14 接 GND 表示 B 板（也可悬空）');

  // 雷达
  add('radar', '雷达触发', 'H7.PC0', 'R.TRIG', C.trig, 'GPIO 推挽输出');
  add('radar', '雷达回波（分压前，5 V）', 'R.ECHO', 'BB.DIV_IN', C.echo, '经 2.2 kΩ 到分压点');
  add('radar', '雷达回波（分压后）', 'BB.DIV_OUT', 'H7.PC6', C.echo, 'TIM3_CH1 输入捕获，分压点约 3.0 V');
  add('radar', '雷达供电', 'H7.5VA', 'R.VCC', C.v5, '5 V');
  add('radar', '雷达共地', 'R.GND', 'BB.G', C.gnd);

  // 调试串口
  add('debug', '调试串口 H7 发送', 'H7.PC10', 'TTL.RXD', C.tx, 'UART4 TX，921600，日志和 $E 事件');
  add('debug', '调试串口 H7 接收', 'TTL.TXD', 'H7.PC11', C.rx, 'UART4 RX，STOP/START/STATUS/RELEARN');
  add('debug', '调试串口共地', 'TTL.GND', 'H7.GNDA', C.gnd);

  // 七只舵机：信号到 H7，电源到外部电源
  [['SR', '雷达水平舵机', 'H7.PA0', 'TIM2_CH1'],
   ['SAP', 'ESP32-A 水平舵机', 'H7.PA1', 'TIM2_CH2'], ['SAT', 'ESP32-A 俯仰舵机', 'H7.PA2', 'TIM2_CH3'],
   ['SBP', 'ESP32-B 水平舵机', 'H7.PA3', 'TIM2_CH4'], ['SBT', 'ESP32-B 俯仰舵机', 'H7.PC7', 'TIM3_CH2'],
   ['SEP', '84E 水平舵机', 'H7.PB0', 'TIM3_CH3'], ['SET', '84E 俯仰舵机', 'H7.PB1', 'TIM3_CH4']].forEach(function (s) {
    add('servoSig', s[1] + ' 信号', s[0] + '.S', s[2], C.sig, s[3] + '，50 Hz PWM，脉宽须逐只标定');
    add('servoPwr', s[1] + ' 电源', s[0] + '.V', 'BB.V', C.srv, '外部舵机电源，不从开发板取电');
    add('servoPwr', s[1] + ' 地', s[0] + '.N', 'BB.G', C.brown);
  });

  // 电源
  add('power', '舵机电源正极', 'PSU.P', 'BB.V', C.v5, '外部独立电源，型号和电流待确定');
  add('power', '舵机电源负极', 'PSU.N', 'BB.G', C.gnd, '必须与 H7 共地');
  add('power', 'H7 共地', 'H7.GNDB', 'BB.G', C.gnd);
  return n;
})();

window.SIM_GROUPS = {
  uart84: 'H7 ↔ 84E', uartA: 'H7 ↔ ESP32-A', uartB: 'H7 ↔ ESP32-B', radar: '雷达',
  debug: '调试串口', servoSig: '舵机信号', servoPwr: '舵机供电', power: '电源与共地'
};
