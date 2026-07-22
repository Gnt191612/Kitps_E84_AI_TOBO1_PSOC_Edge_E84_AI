/**
 * @file network_manager.c
 * @brief WiFi 连接 + WebSocket 服务端/客户端 + 雷达界面
 *
 * 一套代码烧两个板，运行时通过 GPIO_ROLE_DETECT 引脚检测角色:
 *   ESP32-A (GPIO4=高): WS 服务器(:80) + 雷达页面
 *   ESP32-B (GPIO4=低): WS 客户端连 A
 *
 * ws_handler 分流 (服务器):
 *   "CMD:params" → 浏览器控制指令 → 回调
 *   "{...}"      → ESP32-B 跟踪数据 → 广播给浏览器
 */

#include "network_manager.h"
#include "data_logger/logger.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "esp_websocket_client.h"
#include "mdns.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/gpio.h"

static const char *TAG = "NET";

/* ──── WiFi 配置 ──── */
#define WIFI_SSID       "Ning.T"
#define WIFI_PASS       "gnt13836369619"
#define MAX_RETRY       5

static int s_retry_count = 0;
static int s_connected = 0;
static char s_local_ip[16] = "0.0.0.0";

/* ──── WS 命令/转发回调 ──── */
static Network_CommandCallback_t s_ws_cmd_cb = NULL;
static Network_CommandCallback_t s_relay_cb = NULL;

/* ──── 服务器上下文 ──── */
static httpd_handle_t s_server = NULL;
#define MAX_WS_CLIENTS  8
static int s_fd_table[MAX_WS_CLIENTS];
static int s_fd_count = 0;
static SemaphoreHandle_t s_ws_mutex = NULL;

/* ──── 客户端上下文 ──── */
static esp_websocket_client_handle_t s_ws_client = NULL;
static int s_ws_connected = 0;

/* ──── WiFi 事件处理 ──── */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = 0;
        if (s_retry_count < MAX_RETRY) {
            s_retry_count++;
            LOG_WARN("WiFi disconnect, retry %d/%d", s_retry_count, MAX_RETRY);
            esp_wifi_connect();
        } else {
            LOG_ERROR("WiFi connect failed after %d retries", MAX_RETRY);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_connected = 1;
        s_retry_count = 0;
        snprintf(s_local_ip, sizeof(s_local_ip), IPSTR, IP2STR(&event->ip_info.ip));
        LOG_INFO("WiFi connected! IP: %s", s_local_ip);
    }
}

static void wifi_common_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                         &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                         &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();

    LOG_INFO("Connecting to WiFi: %s...", WIFI_SSID);
    int wait_ms = 0;
    while (!s_connected && wait_ms < 15000) {
        vTaskDelay(pdMS_TO_TICKS(100));
        wait_ms += 100;
    }
    if (!s_connected) {
        LOG_WARN("WiFi not connected after 15s, continuing without network");
    }
}

/* ════════════════ 服务器端代码 ════════════════ */

static void ws_add_client(int fd)
{
    if (!s_ws_mutex) return;
    xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    if (s_fd_count < MAX_WS_CLIENTS)
        s_fd_table[s_fd_count++] = fd;
    xSemaphoreGive(s_ws_mutex);
}

static void ws_remove_client(int fd)
{
    if (!s_ws_mutex) return;
    xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    for (int i = 0; i < s_fd_count; i++) {
        if (s_fd_table[i] == fd) {
            s_fd_table[i] = s_fd_table[--s_fd_count];
            break;
        }
    }
    xSemaphoreGive(s_ws_mutex);
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ws_add_client(httpd_req_to_sockfd(req));
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = { 0 };
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) { ws_remove_client(httpd_req_to_sockfd(req)); return ESP_FAIL; }
    if (ws_pkt.type != HTTPD_WS_TYPE_TEXT || ws_pkt.len == 0 || ws_pkt.len > 511)
        return ESP_OK;

    uint8_t buf[512];
    ws_pkt.payload = buf;
    if (httpd_ws_recv_frame(req, &ws_pkt, sizeof(buf) - 1) != ESP_OK)
        return ESP_FAIL;
    buf[ws_pkt.len] = '\0';

    /* 分流: {JSON} → ESP32-B 数据广播给浏览器; 其他 → 指令 */
    if (buf[0] == '{') {
        Network_BroadcastText((const char *)buf, ws_pkt.len);
    } else {
        char *str = (char *)buf, *colon = strchr(str, ':');
        if (colon) { *colon = '\0'; if (s_ws_cmd_cb) s_ws_cmd_cb(str, colon + 1); }
        else       { if (s_ws_cmd_cb) s_ws_cmd_cb(str, ""); }
    }
    return ESP_OK;
}

static const char *s_index_html =
    "<!DOCTYPE html><html lang='zh-CN'><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>多板雷达跟踪系统</title><style>"
    "*{margin:0;padding:0;box-sizing:border-box}"
    "body{background:#0a0e17;color:#a0d0ff;font-family:'Segoe UI',sans-serif;overflow:hidden;height:100vh;display:flex;flex-direction:column;user-select:none}"
    "#header{background:linear-gradient(90deg,#0d1a2b,#162a45);padding:8px 20px;display:flex;align-items:center;justify-content:space-between;border-bottom:1px solid #1e3a5f;flex-shrink:0}"
    "#header h1{font-size:18px;font-weight:600;color:#7fc8ff;letter-spacing:2px}"
    "#header h1 span{color:#4af;font-size:12px;margin-left:10px;opacity:.7}"
    "#status-bar{display:flex;gap:20px;font-size:13px}"
    ".status-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px}"
    ".dot-green{background:#0f0;box-shadow:0 0 6px #0f0}.dot-red{background:#f00;box-shadow:0 0 6px #f00}"
    ".dot-yellow{background:#ff0;box-shadow:0 0 6px #ff0}.dot-blue{background:#4af;box-shadow:0 0 6px #4af}"
    "#main{display:flex;flex:1;min-height:0}"
    "#radar-area{flex:1;position:relative;display:flex;align-items:center;justify-content:center;background:radial-gradient(ellipse at center,#0d1a2b 0%,#060b12 100%)}"
    "#radar-canvas{width:100%;height:100%;display:block}"
    "#side-panel{width:280px;background:#0d1a2b;border-left:1px solid #1e3a5f;display:flex;flex-direction:column;flex-shrink:0}"
    "#target-panel{padding:15px;border-bottom:1px solid #1a2d45}"
    "#target-panel h3{font-size:13px;color:#5a9acf;margin-bottom:10px;text-transform:uppercase;letter-spacing:1px;border-bottom:1px solid #1a2d45;padding-bottom:6px}"
    "#target-info{font-size:13px;line-height:1.8}"
    "#target-info .label{color:#5a8ab5}#target-info .value{color:#7fc8ff;font-weight:600}#target-info .lost{color:#f55}"
    "#controls{padding:15px;display:flex;flex-direction:column;gap:8px}"
    "#controls button{padding:10px 14px;border:1px solid #1e3a5f;border-radius:6px;background:linear-gradient(180deg,#162a45,#0d1a2b);color:#7fc8ff;font-size:13px;cursor:pointer;transition:all .2s;font-family:inherit}"
    "#controls button:hover{background:linear-gradient(180deg,#1e3a5f,#162a45);border-color:#3a7abf;box-shadow:0 0 10px rgba(60,130,200,.2)}"
    "#controls button:active{background:#1e3a5f}#controls button.danger{border-color:#5f1e1e;color:#f77}"
    "#controls button.danger:hover{border-color:#a33;box-shadow:0 0 10px rgba(200,50,50,.2)}"
    "#log-area{flex:1;display:flex;flex-direction:column;min-height:0}"
    "#log-header{padding:8px 15px;font-size:12px;color:#5a8ab5;border-bottom:1px solid #1a2d45;flex-shrink:0}"
    "#log{flex:1;overflow-y:auto;padding:8px 15px;font-family:'Consolas','Courier New',monospace;font-size:12px;line-height:1.6;background:#060b12}"
    "#log .info{color:#4a8}#log .warn{color:#ca4}#log .err{color:#e44}#log .track{color:#4af}#log .cmd{color:#a8f}"
    "#log::-webkit-scrollbar{width:4px}#log::-webkit-scrollbar-track{background:#060b12}#log::-webkit-scrollbar-thumb{background:#1e3a5f;border-radius:2px}"
    "@media(max-width:768px){#side-panel{width:220px}#header h1{font-size:15px}}"
    "</style></head><body><div id='header'>"
    "<h1>📡 多板目标跟踪 <span>v2.0 · 180° 来回扫</span></h1>"
    "<div id='status-bar'>"
    "<span><span class='status-dot dot-red' id='ws-dot'></span><span id='ws-status'>断开</span></span>"
    "<span><span class='status-dot dot-red' id='track-dot'></span><span id='track-status'>空闲</span></span>"
    "</div></div><div id='main'>"
    "<div id='radar-area'><canvas id='radar-canvas'></canvas></div>"
    "<div id='side-panel'>"
    "<div id='target-panel'><h3>🎯 目标信息</h3><div id='target-info'>"
    "<div><span class='label'>编号</span> <span class='value' id='info-id'>--</span></div>"
    "<div><span class='label'>来源</span> <span class='value' id='info-src'>--</span></div>"
    "<div><span class='label'>方位角</span> <span class='value' id='info-angle'>--°</span></div>"
    "<div><span class='label'>距离</span> <span class='value' id='info-range'>-- cm</span></div>"
    "<div><span class='label'>X</span> <span class='value' id='info-x'>-- mm</span></div>"
    "<div><span class='label'>Y</span> <span class='value' id='info-y'>-- mm</span></div>"
    "<div><span class='label'>状态</span> <span class='value' id='info-state'>空闲</span></div>"
    "</div></div><div id='controls'>"
    "<button id='btn-track' onclick='sendCmd(\"TRACK:0:0:0\")'>▶ 跟踪</button>"
    "<button id='btn-relock' onclick='sendCmd(\"RELOAD\")'>🔄 复锁</button>"
    "<button id='btn-release' class='danger' onclick='sendCmd(\"RELEASE\")'>⏹ 释放</button>"
    "<button id='btn-switch' onclick='nextTarget()'>🔁 切换</button>"
    "</div><div id='log-area'><div id='log-header'>📋 日志</div><div id='log'></div></div></div></div>"
    "<script>(function(){"
    "var ws=null,rt=null,wu='ws://'+location.host+'/ws';"
    "var targets={},sel=null,sa=0,sd=1,lt=0;"
    "var cv=document.getElementById('radar-canvas'),cx=cv.getContext('2d');"
    "var le=document.getElementById('log'),lm=200;"
    "function cw(){if(ws&&ws.readyState===WebSocket.OPEN)return;"
    "try{ws=new WebSocket(wu)}catch(e){return sc()}"
    "ws.onopen=function(){document.getElementById('ws-dot').className='status-dot dot-green';"
    "document.getElementById('ws-status').textContent='已连接';if(rt){clearTimeout(rt);rt=null};al('🟢 已连接','info')};"
    "ws.onclose=function(){document.getElementById('ws-dot').className='status-dot dot-red';"
    "document.getElementById('ws-status').textContent='断开';al('🔴 断开','err');sc()};"
    "ws.onerror=function(){ws&&ws.close()};ws.onmessage=function(e){onD(e.data)}}"
    "function sc(){if(!rt)rt=setTimeout(function(){rt=null;cw()},3000)}"
    "function onD(r){var d;try{d=JSON.parse(r)}catch(e){al(r,'info');return}"
    "if(d.t!==undefined){var s=d.s||'A',id=s+d.t;"
    "if(!targets[id])targets[id]={id:id,src:s,num:d.t,life:1};"
    "var t=targets[id];t.x=d.x||0;t.y=d.y||0;t.lost=d.l||0;t.state=d.s||1;"
    "t.range=Math.sqrt(t.x*t.x+t.y*t.y)/10;"
    "t.angle=Math.atan2(t.x,t.y)*180/Math.PI;"
    "t.time=Date.now();t.life=1;t.visible=true;sel=id;ui(id);ub(id);"
    "al('🎯 '+id+(t.lost?'丢失':'跟踪')+' 方位'+t.angle.toFixed(1)+'° 距离'+t.range.toFixed(0)+'cm','track')}}"
    "function ui(id){var t=targets[id];if(!t)return;"
    "document.getElementById('info-id').textContent='#'+t.num;"
    "document.getElementById('info-src').textContent=t.src;"
    "document.getElementById('info-angle').textContent=(t.angle||0).toFixed(1)+'°';"
    "document.getElementById('info-range').textContent=(t.range||0).toFixed(0)+' cm';"
    "document.getElementById('info-x').textContent=(t.x||0).toFixed(0)+' mm';"
    "document.getElementById('info-y').textContent=(t.y||0).toFixed(0)+' mm';"
    "document.getElementById('info-state').textContent=t.lost?'丢失':'跟踪中';"
    "document.getElementById('info-state').style.color=t.lost?'#f55':'#4a8'}"
    "function ub(id){var t=targets[id];if(!t)return;"
    "var d=document.getElementById('track-dot'),x=document.getElementById('track-status');"
    "d.className=t.lost?'status-dot dot-yellow':'status-dot dot-green';x.textContent=t.lost?'丢失':id}"
    "function scmd(s){if(!ws||ws.readyState!==WebSocket.OPEN){al('⚠️ WS未连','warn');return}"
    "try{ws.send(s);al('→ '+s,'cmd')}catch(e){al('❌ 发送失败','err')}}"
    "function nt(){var ks=Object.keys(targets).filter(function(k){return targets[k].visible!==false});"
    "if(ks.length===0){scmd('TRACK:0:0:0');return}var idx=0;"
    "if(sel!==null)for(var i=0;i<ks.length;i++)if(ks[i]===sel){idx=(i+1)%ks.length;break}"
    "scmd('SWITCH:'+targets[ks[idx]].num)}"
    "function al(t,c){c=c||'info';var d=document.createElement('div');d.className=c;d.textContent=t;"
    "le.appendChild(d);if(le.children.length>lm)le.removeChild(le.firstChild);le.scrollTop=le.scrollHeight}"
    "function rs(){var r=cv.parentElement.getBoundingClientRect();var s=Math.min(r.width,r.height)-20;"
    "cv.width=s*devicePixelRatio;cv.height=s*devicePixelRatio;cv.style.width=s+'px';cv.style.height=s+'px';"
    "cx.setTransform(1,0,0,1,0,0);cx.scale(devicePixelRatio,devicePixelRatio)}"
    "rs();window.addEventListener('resize',rs);"
    "function dr(now){var s=parseInt(cv.style.width),cx2=s/2,cy2=s/2,r=s/2-15;"
    "cx.clearRect(0,0,s,s);cx.save();cx.translate(cx2,cy2);"
    "var bg=cx.createRadialGradient(0,0,0,0,0,r);"
    "bg.addColorStop(0,'rgba(10,20,40,0.95)');bg.addColorStop(0.7,'rgba(6,14,28,0.98)');bg.addColorStop(1,'rgba(2,6,12,1)');"
    "cx.fillStyle=bg;cx.beginPath();cx.arc(0,0,r,0,Math.PI*2);cx.fill();"
    "cx.strokeStyle='rgba(30,80,140,0.5)';cx.lineWidth=1.5;cx.beginPath();cx.arc(0,0,r,0,Math.PI*2);cx.stroke();"
    "cx.strokeStyle='rgba(60,180,255,0.15)';cx.lineWidth=1;cx.setLineDash([4,4]);"
    "cx.beginPath();cx.moveTo(0,0);cx.lineTo(0,-r);cx.stroke();cx.setLineDash([]);"
    "var SW=-Math.PI/2;cx.fillStyle='rgba(0,40,80,0.08)';"
    "cx.beginPath();cx.moveTo(0,0);cx.arc(0,0,r,SW-Math.PI/2,SW+Math.PI/2);cx.closePath();cx.fill();"
    "for(var a=0;a<360;a+=15){var rd=a*Math.PI/180;var ic=a%90===0;"
    "var inn=r-(ic?15:10);cx.strokeStyle=ic?'rgba(60,180,255,0.4)':'rgba(60,180,255,0.15)';cx.lineWidth=ic?1.5:1;"
    "cx.beginPath();cx.moveTo(Math.sin(rd)*inn,-Math.cos(rd)*inn);cx.lineTo(Math.sin(rd)*r,-Math.cos(rd)*r);cx.stroke()}"
    "cx.fillStyle='rgba(100,200,255,0.6)';cx.font='11px sans-serif';cx.textAlign='center';cx.textBaseline='middle';"
    "cx.fillText('0°',0,-r+18);cx.fillText('90°',r-18,0);cx.fillText('180°',0,r-18);cx.fillText('270°',-r+18,0);"
    "for(var rr=1;rr<=4;rr++){var rrr=r*rr/5;"
    "cx.strokeStyle='rgba(30,80,140,'+(0.15+0.08*(5-rr))+')';cx.lineWidth=0.5;cx.beginPath();cx.arc(0,0,rrr,0,Math.PI*2);cx.stroke();"
    "cx.fillStyle='rgba(100,200,255,0.25)';cx.font='9px sans-serif';cx.textAlign='left';cx.textBaseline='top';"
    "cx.fillText((rr*30)+'cm',3,rrr+2)}"
    "if(lt===0)lt=now;var dt=(now-lt)/1000;lt=now;sa+=sd*80*dt;"
    "if(sa>=180){sa=180;sd=-1}if(sa<=0){sa=0;sd=1}var ar=(sa-90)*Math.PI/180;"
    "var gr=cx.createLinearGradient(0,0,Math.sin(ar)*r,-Math.cos(ar)*r);"
    "gr.addColorStop(0,'rgba(0,200,255,0.8)');gr.addColorStop(0.3,'rgba(0,200,255,0.3)');gr.addColorStop(1,'rgba(0,200,255,0)');"
    "cx.strokeStyle=gr;cx.lineWidth=2.5;cx.shadowColor='rgba(0,200,255,0.5)';cx.shadowBlur=10;"
    "cx.beginPath();cx.moveTo(0,0);cx.lineTo(Math.sin(ar)*r,-Math.cos(ar)*r);cx.stroke();cx.shadowBlur=0;"
    "cx.fillStyle='rgba(0,150,255,0.03)';cx.beginPath();cx.moveTo(0,0);"
    "var sf=sd>0?0:sa,st=sd>0?sa:0,sr1=(sf-90)*Math.PI/180,sr2=(st-90)*Math.PI/180;"
    "if(sd>0)cx.arc(0,0,r,sr1,sr2);else cx.arc(0,0,r,sr2,sr1);cx.closePath();cx.fill();"
    "cx.fillStyle='#4af';cx.shadowColor='rgba(0,200,255,0.8)';cx.shadowBlur=12;"
    "cx.beginPath();cx.arc(Math.sin(ar)*r,-Math.cos(ar)*r,4,0,Math.PI*2);cx.fill();cx.shadowBlur=0;"
    "for(var k in targets){var t=targets[k];if(!t||!t.visible)continue;"
    "var ag=(Date.now()-t.time)/1000;if(ag>10){t.visible=false;continue}"
    "var al2=Math.max(0.2,1-ag/10);var ta=t.angle||0,tr=Math.min(t.range||0,120);"
    "var trad=ta*Math.PI/180,tR=r*(tr/120);"
    "var norm=((ta%360)+360)%360;if(!(norm>=270||norm<=90))continue;"
    "var px=Math.sin(trad)*tR,py=-Math.cos(trad)*tR;"
    "var h=t.src==='B'?'rgba(255,200,0,':'rgba(0,220,255,';"
    "cx.fillStyle=h+al2+')';cx.shadowColor=h+(al2*0.6)+')';cx.shadowBlur=15;"
    "cx.beginPath();cx.arc(px,py,t.lost?4:6,0,Math.PI*2);cx.fill();"
    "if(!t.lost){cx.fillStyle=h+(al2*0.3)+')';cx.beginPath();cx.arc(px,py,12,0,Math.PI*2);cx.fill()}"
    "cx.shadowBlur=0;cx.fillStyle='rgba(180,230,255,'+al2+')';cx.font='bold 12px sans-serif';cx.textAlign='left';cx.textBaseline='bottom';"
    "cx.fillText(t.id,px+10,py-2);cx.fillStyle='rgba(100,200,255,'+(al2*0.6)+')';cx.font='10px sans-serif';"
    "cx.fillText((t.range||0).toFixed(0)+'cm',px+10,py+12);"
    "if(sel===k){cx.strokeStyle='rgba(0,255,200,'+(al2*0.5)+')';cx.lineWidth=1;cx.setLineDash([3,3]);cx.beginPath();cx.arc(px,py,16,0,Math.PI*2);cx.stroke();cx.setLineDash([])}}"
    "cx.fillStyle='rgba(0,200,255,0.25)';cx.font='10px sans-serif';cx.textAlign='center';cx.textBaseline='bottom';"
    "cx.fillText(sd>0?'▶ 正扫':'◀ 回扫',Math.sin(ar)*r*0.85,-Math.cos(ar)*r*0.85-5);"
    "cx.restore();requestAnimationFrame(dr)}"
    "al('📡 雷达已加载','info');al('⏳ 连接中...','info');cw();requestAnimationFrame(dr);"
    "cv.addEventListener('click',function(){nt()})"
    "})()</script></body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, s_index_html, strlen(s_index_html));
    return ESP_OK;
}

static void start_mdns(void)
{
    esp_err_t ret = mdns_init();
    if (ret != ESP_OK) {
        LOG_WARN("mDNS init failed");
        return;
    }
    mdns_hostname_set("esp32-tracker-a");
    mdns_instance_name_set("ESP32 Tracker Server A");
    mdns_service_add(NULL, "_ws", "_tcp", 80, NULL, 0);
    LOG_INFO("mDNS started: esp32-tracker-a.local");
}

static int start_websocket_server(void)
{
    s_ws_mutex = xSemaphoreCreateMutex();
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_uri_handlers = 3;
    config.lru_purge_enable = true;
    if (httpd_start(&s_server, &config) != ESP_OK) return -1;

    httpd_uri_t uri_root = {
        .uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &uri_root);

    httpd_uri_t uri_ws = {
        .uri = "/ws", .method = HTTP_GET, .handler = ws_handler,
        .user_ctx = NULL, .is_websocket = true,
    };
    httpd_register_uri_handler(s_server, &uri_ws);

    start_mdns();
    LOG_INFO("WebSocket server started on port 80");
    return 0;
}

/* ════════════════ 客户端代码 ════════════════ */

static void ws_client_event_handler(void *arg, esp_event_base_t base,
                                     int32_t id, void *data)
{
    esp_websocket_event_data_t *evt = (esp_websocket_event_data_t *)data;
    switch (id) {
    case WEBSOCKET_EVENT_CONNECTED:
        s_ws_connected = 1;
        LOG_INFO("WS client connected");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        s_ws_connected = 0;
        LOG_WARN("WS client disconnected");
        break;
    case WEBSOCKET_EVENT_DATA:
        if (evt->data_len > 0 && evt->data_ptr && evt->data_len < 512) {
            char buf[512];
            memcpy(buf, evt->data_ptr, evt->data_len);
            buf[evt->data_len] = '\0';
            if (buf[0] != '{') {
                char *colon = strchr(buf, ':');
                if (colon) { *colon = '\0'; if (s_relay_cb) s_relay_cb(buf, colon + 1); }
                else       { if (s_relay_cb) s_relay_cb(buf, ""); }
            }
        }
        break;
    default: break;
    }
}

/* 通过 mDNS 解析服务器 IP，构造 WS URI */
static void build_ws_uri(char *buf, size_t buf_size)
{
    esp_ip4_addr_t addr;
    esp_err_t ret = mdns_query_a("esp32-tracker-a", 3000, &addr);
    if (ret == ESP_OK && addr.addr != 0) {
        snprintf(buf, buf_size, "ws://" IPSTR "/ws", IP2STR(&addr));
        LOG_INFO("mDNS resolved esp32-tracker-a -> " IPSTR, IP2STR(&addr));
    } else {
        /* mDNS 失败，降级到硬编码后备 IP */
        LOG_WARN("mDNS query failed, fallback to 192.168.43.2");
        snprintf(buf, buf_size, "ws://192.168.43.2/ws");
    }
}

static int start_ws_client(void)
{
    char uri[64];
    build_ws_uri(uri, sizeof(uri));

    esp_websocket_client_config_t cfg = {
        .uri = uri,
        .task_stack = 4096,
        .network_timeout_ms = 10000,
    };
    s_ws_client = esp_websocket_client_init(&cfg);
    esp_websocket_register_events(s_ws_client, WEBSOCKET_EVENT_ANY,
                                   ws_client_event_handler, NULL);
    esp_websocket_client_start(s_ws_client);
    LOG_INFO("WS client connecting to %s...", uri);
    return 0;
}

/* ════════════════ 公共 API ════════════════ */

int Network_Init(void)
{
    if (!s_ws_mutex) s_ws_mutex = xSemaphoreCreateMutex();
    wifi_common_init();
    if (!s_connected) return -1;
    return start_websocket_server();
}

int Network_InitClient(void)
{
    wifi_common_init();
    if (!s_connected) return -1;
    return start_ws_client();
}

int Network_BroadcastText(const char *data, int len)
{
    if (!s_server || !s_ws_mutex) return -1;
    if (len <= 0) len = strlen(data);
    xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    httpd_ws_frame_t ws_pkt = {
        .payload = (uint8_t *)data, .len = len, .type = HTTPD_WS_TYPE_TEXT,
    };
    for (int i = 0; i < s_fd_count; i++) {
        esp_err_t ret = httpd_ws_send_frame_async(s_server, s_fd_table[i], &ws_pkt);
        if (ret != ESP_OK) httpd_sess_trigger_close(s_server, s_fd_table[i]);
    }
    xSemaphoreGive(s_ws_mutex);
    return 0;
}

int Network_ClientCount(void)
{
    if (!s_ws_mutex) return 0;
    int cnt;
    xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    cnt = s_fd_count;
    xSemaphoreGive(s_ws_mutex);
    return cnt;
}

int Network_SendJSON(const char *json, int len)
{
    if (!s_ws_client || !s_ws_connected) return -1;
    if (len <= 0) len = strlen(json);
    return esp_websocket_client_send_text(s_ws_client, json, len, portMAX_DELAY);
}

void Network_RegisterCommandCallback(Network_CommandCallback_t cb) { s_ws_cmd_cb = cb; }
void Network_RegisterRelayCallback(Network_CommandCallback_t cb)   { s_relay_cb = cb; }
const char *Network_GetLocalIP(void) { return s_local_ip; }
