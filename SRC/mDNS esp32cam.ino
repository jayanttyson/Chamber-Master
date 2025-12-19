/*  ESP32-CAM 3D Printer Live Cam – FINAL CLEAN & WORKING  */
#include "esp_camera.h"
#include <WiFi.h>
#include <ESPmDNS.h>
#include "img_converters.h"
#include "esp_http_server.h"

// ---------- PINS ----------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
#define FLASH_LED_PIN      4

// ---------- WIFI ----------
const char* ssid = "*********";
const char* password = "*********";

// ---------- GLOBALS ----------
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;
bool flashOn = false;
unsigned long flashOnTime = 0;
const unsigned long FLASH_TIMEOUT = 4UL * 60UL * 1000UL;  // 4 minutes

// ---------- HTML ----------
static const char PROGMEM INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><title>3D Print LIVE</title>
<style>
  body{background:#111;color:#fff;font-family:Arial;text-align:center;margin:0;padding:20px;}
  button,select{padding:10px 16px;margin:6px;border:none;border-radius:8px;background:#444;color:#fff;cursor:pointer;font-size:16px;}
  button:hover,select:hover{background:#666;}
  .stream-container{width:1280px;height:1024px;max-width:98vw;margin:10px auto;background:#000;border-radius:10px;overflow:hidden;}
  #stream{width:100%;height:100%;object-fit:contain;}
  #snapshot{display:none;margin-top:20px;max-width:98vw;border-radius:10px;}
</style></head><body>
<h2>3D Printer LIVE Feed</h2>
<div class="stream-container"><img id="stream" src=""></div><br>
<select id="framesize" onchange="set('framesize',this.value)">
  <option value="9" selected>SXGA (1280x1024)</option>
  <option value="8">XGA (1024x768)</option>
  <option value="7">SVGA (800x600)</option>
  <option value="6">VGA (640x480)</option>
</select>
<select id="jpegquality" onchange="set('jpegquality',this.value)">
  <option value="5">High (5)</option>
  <option value="8" selected>Good (8)</option>
  <option value="12">Medium (12)</option>
  <option value="15">Fast (15)</option>
</select><br>
<button onclick="snap()">Snapshot</button>
<button id="flashBtn" onclick="toggleFlash()">Flash: OFF</button><br>
<img id="snapshot" src="">
<script>
const ip=location.hostname;
document.getElementById('stream').src=`http://${ip}:81/stream?t=${Date.now()}`;
const sizes={'9':{w:1280,h:1024},'8':{w:1024,h:768},'7':{w:800,h:600},'6':{w:640,h:480}};
let blobUrl=null;
async function snap(){
  document.getElementById('snapshot').style.display='none';
 const b=await (await fetch(`/capture?t=${Date.now()}`,{cache:'no-store'})).blob();
 if(blobUrl)URL.revokeObjectURL(blobUrl);
 blobUrl=URL.createObjectURL(b);
 document.getElementById('snapshot').src=blobUrl;
 document.getElementById('snapshot').style.display='block';
}
async function toggleFlash(){
 const on=(await (await fetch('/flash')).text()).trim()==='ON';
 document.getElementById('flashBtn').innerText='Flash: '+(on?'ON':'OFF');
}
function set(p,v){
 fetch(`/control?var=${p}&val=${v}`);
 if(p==='framesize'){
  const s=sizes[v];
  document.querySelector('.stream-container').style.width=s.w+'px';
  document.querySelector('.stream-container').style.height=s.h+'px';
  document.getElementById('stream').src=`http://${ip}:81/stream?t=${Date.now()}`;
 }
 if(p==='jpegquality') document.getElementById('stream').src=`http://${ip}:81/stream?t=${Date.now()}`;
}
// NO auto toggleFlash() here → flash stays OFF on page load
</script></body></html>
)rawliteral";

// ---------- HANDLERS ----------
static esp_err_t stream_handler(httpd_req_t *req){
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t jpg_len = 0;
  uint8_t *jpg_buf = NULL;
  char part_buf[128];

  httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  while(true){
    fb = esp_camera_fb_get();
    if(!fb){ delay(10); continue; }

    if(fb->format != PIXFORMAT_JPEG){
      frame2jpg(fb, 80, &jpg_buf, &jpg_len);
      esp_camera_fb_return(fb);
    } else {
      jpg_len = fb->len;
      jpg_buf = fb->buf;
    }

    snprintf(part_buf, sizeof(part_buf), "\r\n--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned int)jpg_len);
    httpd_resp_send_chunk(req, part_buf, strlen(part_buf));
    res = httpd_resp_send_chunk(req, (const char*)jpg_buf, jpg_len);

    if(fb->format != PIXFORMAT_JPEG) free(jpg_buf);
    esp_camera_fb_return(fb);
    if(res != ESP_OK) break;
  }
  return res;
}

static esp_err_t capture_handler(httpd_req_t *req){
  camera_fb_t *fb = esp_camera_fb_get();
  if(!fb){ httpd_resp_send_500(req); return ESP_FAIL; }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_send(req, (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return ESP_OK;
}

static esp_err_t flash_handler(httpd_req_t *req){
  flashOn = !flashOn;
  digitalWrite(FLASH_LED_PIN, flashOn ? HIGH : LOW);
  if(flashOn) flashOnTime = millis();
  httpd_resp_send(req, flashOn ? "ON" : "OFF", -1);
  return ESP_OK;
}

static esp_err_t cmd_handler(httpd_req_t *req){
  char var[32]={}, val[32]={};
  size_t len = httpd_req_get_url_query_len(req) + 1;
  if(len > 1){
    char* buf = (char*)malloc(len);
    if(buf){
      httpd_req_get_url_query_str(req, buf, len);
      if(httpd_query_key_value(buf,"var",var,sizeof(var))==ESP_OK &&
         httpd_query_key_value(buf,"val",val,sizeof(val))==ESP_OK){
        int v = atoi(val);
        sensor_t *s = esp_camera_sensor_get();
        if(strcmp(var,"framesize")==0) s->set_framesize(s, (framesize_t)v);
        else if(strcmp(var,"jpegquality")==0) s->set_quality(s, v);
      }
      free(buf);
    }
  }
  httpd_resp_send(req, "OK", 2);
  return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req){
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, INDEX_HTML, strlen(INDEX_HTML));
}

void startCameraServer(){
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.max_open_sockets = 7;
  config.stack_size = 8192;
  config.lru_purge_enable = true;

  httpd_uri_t index_uri   = { "/",      HTTP_GET, index_handler };
  httpd_uri_t capture_uri = { "/capture", HTTP_GET, capture_handler };
  httpd_uri_t flash_uri   = { "/flash",   HTTP_GET, flash_handler };
  httpd_uri_t control_uri = { "/control", HTTP_GET, cmd_handler };

  if(httpd_start(&camera_httpd, &config)==ESP_OK){
    httpd_register_uri_handler(camera_httpd, &index_uri);
    httpd_register_uri_handler(camera_httpd, &capture_uri);
    httpd_register_uri_handler(camera_httpd, &flash_uri);
    httpd_register_uri_handler(camera_httpd, &control_uri);
  }

  httpd_config_t stream_cfg = HTTPD_DEFAULT_CONFIG();
  stream_cfg.server_port = 81;
  stream_cfg.ctrl_port   = 82;
  stream_cfg.max_open_sockets = 4;
  stream_cfg.stack_size = 8192;

  httpd_uri_t stream_uri = { "/stream", HTTP_GET, stream_handler };
  httpd_start(&stream_httpd, &stream_cfg);
  httpd_register_uri_handler(stream_httpd, &stream_uri);
}

// ---------- SETUP ----------
void setup(){
  Serial.begin(115200);
  pinMode(FLASH_LED_PIN, OUTPUT);
  digitalWrite(FLASH_LED_PIN, LOW);

  camera_config_t cfg = {
    .pin_pwdn  = PWDN_GPIO_NUM,
    .pin_reset  = RESET_GPIO_NUM,
    .pin_xclk   = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM, .pin_d6 = Y8_GPIO_NUM, .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM, .pin_d3 = Y5_GPIO_NUM, .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM, .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href  = HREF_GPIO_NUM,
    .pin_pclk  = PCLK_GPIO_NUM,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_SXGA,
    .jpeg_quality = 8,
    .fb_count = 2,
    .grab_mode = CAMERA_GRAB_LATEST
  };

  esp_camera_init(&cfg);

  sensor_t *s = esp_camera_sensor_get();
  s->set_hmirror(s, 1);
  s->set_vflip(s, 0);

  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED){ delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected → http://"+WiFi.localIP());

  MDNS.begin("3d-print-live");
  startCameraServer();
  Serial.println("Ready! Also at http://3d-print-live.local");
}

// ---------- LOOP ----------
void loop(){
  if(flashOn && (millis() - flashOnTime >= FLASH_TIMEOUT)){
    flashOn = false;
    digitalWrite(FLASH_LED_PIN, LOW);
  }
  static uint32_t last = 0;
  if(millis()-last>30000){ last=millis(); if(WiFi.status()!=WL_CONNECTED) WiFi.reconnect(); }
  delay(50);
}
