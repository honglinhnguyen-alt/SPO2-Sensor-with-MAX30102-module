/*
  Capstone oximeter - MAX30102 + Arduino UNO R4 WiFi

  Install: SparkFun MAX3010x Pulse and Proximity Sensor Library.
  Select: Arduino UNO R4 WiFi (Arduino UNO R4 Boards package).
  WiFiS3 is included with that board package. No other libraries required.

  1. Upload this complete sketch. Existing SDA/SCL wiring is unchanged.
  2. Join Wi-Fi "Capstone-Oximeter", password "Capstone16".
  3. Choose to stay connected when the phone reports "No internet".
  4. Open http://192.168.4.1/ in a browser (HTTP, not HTTPS).
     Serial Monitor at 115200 also prints the actual address at startup.

  The page and graph are stored on the Arduino; no router, cloud or internet
  is required. Results update about once per second after a fresh four-second
  acquisition. USB Serial Monitor output is retained; IDE 2 Serial Plotter
  continues to recognise only the compact SpO2:value label.

  Temperature is sensor chip temperature, NOT body temperature.
  Algorithm validity flags are not clinical accuracy validation.
  Presence thresholds are starting values; adjust using actual IR counts.

  Implementation references:
  https://github.com/arduino/ArduinoCore-renesas/tree/main/libraries/WiFiS3/examples/AP_SimpleWebServer
  https://github.com/sparkfun/SparkFun_MAX3010x_Sensor_Library/tree/master/examples/Example8_SPO2

  Networking is serviced incrementally, with sensor polling between chunks.
  WiFiS3 modem calls themselves can still block. If samples may have been
  lost, the window is discarded and acquired again instead of calculating
  from discontinuous data. Wireless timing still requires hardware testing.
*/

#include <Arduino.h>
#include <Wire.h>
#include <WiFiS3.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <string.h>
#include <stdio.h>

// Change these two values if desired. Password must be 8-63 characters.
const char AP_SSID[] = "Capstone-Oximeter";
const char AP_PASSWORD[] = "Capstone16";
IPAddress AP_IP(192, 168, 4, 1);

MAX30105 particleSensor;
WiFiServer webServer(80);
WiFiClient webClient;
bool apReady = false;
bool sensorReady = false;

const uint16_t SAMPLE_COUNT = 100;
const uint16_t UPDATE_COUNT = 25;
// More sensitive contact detection; thresholds are raw IR ADC counts.
// Tune above your uncovered-sensor IR level. OFF stays below ON for hysteresis.
const uint32_t FINGER_ON_IR = 10000;
const uint32_t FINGER_OFF_IR = 5000;
const uint8_t PRESENCE_CONFIRM_SAMPLES = 2; // About 80 ms at 25 pairs/second.
const unsigned long PRINT_INTERVAL_MS = 1000;
const unsigned long TEMP_INTERVAL_MS = 2000;
const unsigned long TEMP_TIMEOUT_MS = 100;
const unsigned long DATA_TIMEOUT_MS = 1000;
// SparkFun's software ring contains four slots (three unread sample pairs).
const unsigned long MAX_SERVICE_GAP_MS = 110;

uint32_t redBuffer[SAMPLE_COUNT];
uint32_t irBuffer[SAMPLE_COUNT];
uint16_t samplesStored = 0;
int32_t heartRate = 0;
int32_t spo2 = 0;
int8_t validHeartRate = 0;
int8_t validSPO2 = 0;
bool calculationReady = false;
bool fingerPresent = false;
bool signalClipped = false;
bool discardFirstSample = true;
bool samplingInterrupted = false;
uint8_t presenceChangeCount = 0;
uint32_t latestIR = 0;
uint32_t measurementSequence = 0;
uint32_t samplingResets = 0;
float temperatureC = 0;
bool temperatureValid = false;
bool temperaturePending = false;
bool temperatureStarted = false;
unsigned long temperatureStartTime = 0;
unsigned long lastPrintTime = 0;
unsigned long lastSampleTime = 0;
unsigned long lastSensorServiceTime = 0;
unsigned long lastCalculationTime = 0;

// Fixed-size request/header buffers and one small JSON body; no full-page copy.
const size_t HTTP_CHUNK_BYTES = 256;
const unsigned long HTTP_IDLE_TIMEOUT_MS = 2000;
const unsigned long HTTP_TOTAL_TIMEOUT_MS = 10000;
char requestLine[160];
size_t requestLineLength = 0;
size_t requestBytes = 0;
uint32_t headerEnding = 0;
bool requestLineComplete = false;
uint8_t httpPhase = 0; // 0 idle, 1 read, 2 send headers, 3 send body.
char responseHeader[256];
size_t responseHeaderLength = 0;
size_t responseOffset = 0;
const char *responseBody = nullptr;
size_t responseBodyLength = 0;
String jsonBody;
unsigned long clientStartTime = 0;
unsigned long clientProgressTime = 0;
unsigned long lastWebServiceTime = 0;

// Entirely offline dashboard: no fonts, scripts, chart libraries or CDN calls.
const char DASHBOARD_HTML[] = R"HTML(<!doctype html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#0b1521"><link rel="icon" href="data:,">
<title>Capstone | Oximeter</title>
<style>
:root{color-scheme:dark;font-family:system-ui,-apple-system,Segoe UI,sans-serif;background:#08111b;color:#e6eff8}
*{box-sizing:border-box}body{margin:0}main{max-width:1000px;margin:auto;padding:30px 20px}
header{display:flex;align-items:center;justify-content:space-between;gap:16px;margin-bottom:26px}
h1{font-size:clamp(25px,5vw,36px);margin:5px 0;letter-spacing:-1px}h2{font-size:18px;margin:0}
.eyebrow{color:#5edbe5;letter-spacing:2px;font-size:11px;font-weight:700}p{line-height:1.5}
.sub,.small{color:#9fb2c5;font-size:13px}.sub{margin:0}.badge{font-size:12px;border:1px solid #34465b;border-radius:30px;padding:9px 12px;white-space:nowrap}
.badge.online{border-color:#276c62;color:#82e2c8}.badge.offline{border-color:#8c5b45;color:#ffcda4}
.status{padding:17px 18px;border:1px solid #29475b;background:#102536;border-radius:14px;margin-bottom:18px}
.status.warn{border-color:#74583a;background:#292319}.status.good{border-color:#28594e;background:#102c28}
#state{font-weight:650}#message{margin-top:5px;color:#bbcbd8;font-size:13px}
.metrics{display:grid;grid-template-columns:repeat(3,1fr);gap:14px;margin:18px 0}
.card,.chart{background:#0e1d2b;border:1px solid #23384b;border-radius:16px;padding:20px}
.label{color:#adc1d2;font-size:13px}.value{font-size:clamp(36px,8vw,58px);font-weight:650;letter-spacing:-2px;margin-top:12px;font-variant-numeric:tabular-nums}
.unit{font-size:16px;color:#9fb2c5;letter-spacing:0;margin-left:6px}.note{font-size:11px;color:#8ea6bb;margin-top:8px}
.chart{margin-top:18px}.chart-top{display:flex;justify-content:space-between;align-items:center;gap:10px;margin-bottom:16px}
canvas{display:block;width:100%;height:230px}.details{display:flex;gap:12px 25px;flex-wrap:wrap;margin-top:18px;color:#9fb2c5;font-size:12px}
footer{margin-top:22px;color:#8da5b9;font-size:12px;line-height:1.6}.progress{margin-top:12px;height:4px;border-radius:4px;background:#283c4b;overflow:hidden}.progress div{height:100%;background:#5edbe5;width:0;transition:width .2s}
@media(max-width:600px){main{padding:22px 14px}header{align-items:flex-start}.metrics{grid-template-columns:1fr 1fr}.card{padding:16px}.card:last-child{grid-column:span 2}.card:last-child .value{font-size:36px}canvas{height:200px}.chart{padding:16px}.badge{font-size:10px;padding:8px}}
</style></head><body><main>
<header><div><div class="eyebrow">TEAM 16 / CAPSTONE</div><h1>Oximeter</h1><p class="sub">MAX30102 physiological monitoring prototype</p></div><div id="connection" class="badge">Connecting...</div></header>
<div id="statusBox" class="status" role="status" aria-live="polite"><div id="state">Waiting for the Arduino</div><div id="message">Keep this device connected to the Arduino Wi-Fi network.</div><div class="progress"><div id="progress"></div></div></div>
<div class="metrics">
<section class="card"><div class="label">Pulse rate</div><div class="value"><span id="bpm">--</span><span class="unit">bpm</span></div><div class="note">From the optical pulse signal</div></section>
<section class="card"><div class="label">Oxygen saturation</div><div class="value"><span id="spo2">--</span><span class="unit">%</span></div><div class="note">Estimated SpO&#8322;</div></section>
<section class="card"><div class="label">Sensor chip temperature</div><div class="value"><span id="temp">--</span><span class="unit">&deg;C</span></div><div class="note">Internal chip temperature</div></section>
</div>
<section class="chart"><div class="chart-top"><h2>SpO&#8322; history</h2><span class="small">Last 2 minutes</span></div><canvas id="plot" role="img" aria-label="SpO2 estimates over the last two minutes"></canvas><div class="small">Four-second measurement window &middot; approximately one update per second</div></section>
<div class="details"><span>Contact: <b id="contact">--</b></span><span>IR: <b id="ir">--</b></span><span>Last received: <b id="updated">--</b></span></div>
<footer>Connect to <b>Capstone-Oximeter</b> &middot; Local connection, no internet required.<br>Research prototype. Chip temperature is not body temperature; measurement accuracy has not been validated against a reference instrument.</footer>
</main><script>
'use strict';
const el=id=>document.getElementById(id);
const history=[];
let lastSequence=null,lastUptime=null,lastReceived=0,lastGap=0;
function show(id,value,digits){el(id).textContent=Number.isFinite(value)?(digits===undefined?String(value):value.toFixed(digits)):'--';}
function addPoint(value){const now=Date.now();history.push({t:now,v:value});while(history.length&&history[0].t<now-120000)history.shift();draw();}
function clearReadings(){['bpm','spo2','temp','ir','contact'].forEach(id=>el(id).textContent='--');el('progress').style.width='0%';}
function disconnected(){clearReadings();el('connection').textContent='Disconnected';el('connection').className='badge offline';el('statusBox').className='status warn';el('state').textContent='Connection lost';el('message').textContent='Rejoin the Arduino Wi-Fi network. This page will reconnect automatically.';el('updated').textContent=lastReceived?Math.floor((Date.now()-lastReceived)/1000)+' s ago':'--';if(Date.now()-lastGap>900){addPoint(null);lastGap=Date.now();}}
function update(d){
 if(lastUptime!==null&&d.uptime_ms<lastUptime){history.length=0;lastSequence=null;}
 lastUptime=d.uptime_ms;lastReceived=Date.now();
 el('connection').textContent='Connected';el('connection').className='badge online';
 const titles={ok:'Receiving measurements',no_finger:'No finger detected',collecting:'Collecting a fresh measurement',invalid:'Measurement unavailable',clipped:'Optical signal clipped',sensor_missing:'Sensor not found',data_missing:'Sensor data missing',sampling_gap:'Reacquiring after a sampling interruption'};
 el('state').textContent=titles[d.status]||'Measurement status';el('message').textContent=d.message;
 el('statusBox').className='status'+(d.status==='ok'?' good':d.status==='collecting'?'':' warn');
 show('bpm',d.bpm);show('spo2',d.spo2);show('temp',d.chip_temperature_c,2);show('ir',d.ir);
 el('contact').textContent=d.finger_present?'Detected':'Absent';el('updated').textContent=new Date().toLocaleTimeString();
 el('progress').style.width=(d.status==='ok'||d.status==='invalid'?100:Math.min(100,d.samples_collected*100/d.sample_target))+'%';
 if(d.sequence!==lastSequence||!Number.isFinite(d.spo2)){addPoint(Number.isFinite(d.spo2)?d.spo2:null);lastSequence=d.sequence;}else draw();
}
function draw(){
 const canvas=el('plot'),r=canvas.getBoundingClientRect(),scale=window.devicePixelRatio||1;
 canvas.width=Math.round(r.width*scale);canvas.height=Math.round(r.height*scale);
 const c=canvas.getContext('2d');c.scale(scale,scale);
 const left=36,right=r.width-8,top=12,bottom=r.height-26,now=Date.now();
 const vals=history.filter(p=>p.t>=now-120000&&Number.isFinite(p.v)).map(p=>p.v);
 const low=vals.length?Math.max(0,Math.min(90,Math.floor((Math.min(...vals)-2)/5)*5)):90;
 const x=t=>left+(t-(now-120000))/120000*(right-left),y=v=>bottom-(v-low)/(100-low)*(bottom-top);
 c.font='11px system-ui';c.textAlign='right';c.textBaseline='middle';
 for(let i=0;i<3;i++){const value=low+(100-low)*i/2,yy=y(value);c.strokeStyle='#243a4b';c.beginPath();c.moveTo(left,yy);c.lineTo(right,yy);c.stroke();c.fillStyle='#9fb2c5';c.fillText(String(Math.round(value)),left-7,yy);}
 c.textAlign='left';c.fillText('2 min ago',left,bottom+17);c.textAlign='right';c.fillText('Now',right,bottom+17);
 c.save();c.beginPath();c.rect(left,top,right-left,bottom-top);c.clip();c.strokeStyle='#5edbe5';c.lineWidth=2;
 let previous=null;
 for(const p of history){if(!Number.isFinite(p.v)){previous=null;continue;}if(previous&&p.t-previous.t<2500){c.beginPath();c.moveTo(x(previous.t),y(previous.v));c.lineTo(x(p.t),y(p.v));c.stroke();}c.fillStyle='#5edbe5';c.beginPath();c.arc(x(p.t),y(p.v),2.4,0,Math.PI*2);c.fill();previous=p;}
 c.restore();if(!vals.length){c.fillStyle='#9fb2c5';c.textAlign='center';c.fillText('Waiting for valid SpO2 measurements',(left+right)/2,(top+bottom)/2);}
}
async function poll(){
 const controller=new AbortController(),timeout=setTimeout(()=>controller.abort(),2500);
 try{const response=await fetch('/api',{cache:'no-store',signal:controller.signal});if(!response.ok)throw new Error('HTTP '+response.status);const data=await response.json();if(typeof data.uptime_ms!=='number'||typeof data.status!=='string')throw new Error('Invalid response');update(data);}catch(error){disconnected();}finally{clearTimeout(timeout);setTimeout(poll,1000);}
}
setInterval(()=>{if(lastReceived&&Date.now()-lastReceived>3500)disconnected();},1000);
window.addEventListener('resize',draw);draw();poll();
</script></body></html>)HTML";

void resetMeasurements();
void processSample(uint32_t red, uint32_t ir);
void updateTemperature(unsigned long now);
void printResults(unsigned long now);
void serviceSensor();
bool usableMeasurement(unsigned long now);
const char *measurementStatus(unsigned long now);
const char *statusMessage(const char *status);
void startAccessPoint();
void buildJson(unsigned long now);
void closeWebClient();
void prepareResponse(const char *status, const char *type, const char *body, size_t length);
void routeRequest();
void serviceWeb();

void resetMeasurements() {
  samplesStored = 0;
  heartRate = 0;
  spo2 = 0;
  validHeartRate = 0;
  validSPO2 = 0;
  calculationReady = false;
}

void startAccessPoint() {
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println(F("ERROR: Wi-Fi module unavailable; USB readings remain available."));
    return;
  }
  WiFi.config(AP_IP);
  int status = WiFi.beginAP(AP_SSID, AP_PASSWORD);
  if (status != WL_AP_LISTENING && status != WL_AP_CONNECTED) {
    Serial.println(F("ERROR: Access point failed. Reset the board to retry."));
    return;
  }
  webServer.begin();
  apReady = true;
  Serial.print(F("INFO: Wi-Fi network = ")); Serial.println(AP_SSID);
  Serial.print(F("INFO: Wi-Fi password = ")); Serial.println(AP_PASSWORD);
  Serial.print(F("INFO: Open http://")); Serial.print(WiFi.localIP()); Serial.println(F("/"));
  Serial.println(F("INFO: Stay connected if your device reports no internet."));
}

void setup() {
  Serial.begin(115200);
  unsigned long start = millis();
  while (!Serial && millis() - start < 3000) delay(10);
  jsonBody.reserve(640);

  // Start networking before optical acquisition so startup cannot fill the FIFO.
  startAccessPoint();
  sensorReady = particleSensor.begin(Wire, I2C_SPEED_FAST);
  if (sensorReady) {
    // Same optical configuration as the combined USB sketch: 25 pairs/second.
    particleSensor.setup(60, 4, 2, 100, 411, 4096);
    particleSensor.enableDIETEMPRDY();
    particleSensor.clearFIFO();
    Serial.println(F("INFO: Place your finger steadily on the sensor."));
    Serial.println(F("INFO: BPM/SpO2 need about 4 seconds after detection."));
  } else {
    Serial.println(F("ERROR: MAX30102 not found. Check power/SDA/SCL, then reset."));
  }
  lastSampleTime = millis();
  lastSensorServiceTime = millis();
}

void updateTemperature(unsigned long now) {
  if (!sensorReady) return;
  if (!temperaturePending) {
    if (temperatureStarted && now - temperatureStartTime < TEMP_INTERVAL_MS) return;
    particleSensor.getINT2();
    particleSensor.writeRegister8(MAX30105_ADDRESS, 0x21, 0x01);
    temperatureStartTime = now;
    temperatureStarted = true;
    temperaturePending = true;
    return;
  }
  if (particleSensor.getINT2() & 0x02) {
    int8_t whole = (int8_t)particleSensor.readRegister8(MAX30105_ADDRESS, 0x1F);
    uint8_t fraction = particleSensor.readRegister8(MAX30105_ADDRESS, 0x20);
    temperatureC = whole + (fraction & 0x0F) * 0.0625f;
    temperatureValid = true;
    temperaturePending = false;
  } else if (now - temperatureStartTime >= TEMP_TIMEOUT_MS) {
    temperatureValid = false;
    temperaturePending = false;
  }
}

void processSample(uint32_t red, uint32_t ir) {
  latestIR = ir;
  bool detected = fingerPresent ? ir >= FINGER_OFF_IR : ir >= FINGER_ON_IR;
  if (detected != fingerPresent) {
    resetMeasurements();
    if (++presenceChangeCount >= PRESENCE_CONFIRM_SAMPLES) {
      fingerPresent = detected;
      presenceChangeCount = 0;
    }
    return;
  }
  presenceChangeCount = 0;
  if (!fingerPresent) {
    signalClipped = false;
    samplingInterrupted = false;
    resetMeasurements();
    return;
  }
  signalClipped = red >= 262140UL || ir >= 262140UL;
  if (signalClipped) { resetMeasurements(); return; }
  redBuffer[samplesStored] = red;
  irBuffer[samplesStored] = ir;
  if (++samplesStored == SAMPLE_COUNT) {
    maxim_heart_rate_and_oxygen_saturation(irBuffer, SAMPLE_COUNT, redBuffer,
        &spo2, &validSPO2, &heartRate, &validHeartRate);
    calculationReady = true;
    samplingInterrupted = false;
    lastCalculationTime = millis();
    ++measurementSequence;
    for (uint16_t i = UPDATE_COUNT; i < SAMPLE_COUNT; ++i) {
      redBuffer[i - UPDATE_COUNT] = redBuffer[i];
      irBuffer[i - UPDATE_COUNT] = irBuffer[i];
    }
    samplesStored = SAMPLE_COUNT - UPDATE_COUNT;
  }
}

void serviceSensor() {
  if (!sensorReady) return;
  unsigned long now = millis();
  if (now - lastSensorServiceTime > MAX_SERVICE_GAP_MS) {
    // Never pass irregularly spaced samples to the fixed-rate Maxim algorithm.
    resetMeasurements();
    presenceChangeCount = 0;
    samplingInterrupted = true;
    ++samplingResets;
    while (particleSensor.available()) particleSensor.nextSample();
    particleSensor.clearFIFO();
    discardFirstSample = true;
  }
  lastSensorServiceTime = millis();
  if (now - lastSampleTime > 200) resetMeasurements();
  if (now - lastSampleTime >= DATA_TIMEOUT_MS) {
    fingerPresent = false;
    presenceChangeCount = 0;
    temperatureValid = false;
  }
  uint16_t fetched = particleSensor.check();
  if (fetched >= 4) {
    // The default library ring cannot retain four unread pairs.
    while (particleSensor.available()) particleSensor.nextSample();
    resetMeasurements();
    presenceChangeCount = 0;
    samplingInterrupted = true;
    ++samplingResets;
    return;
  }
  while (particleSensor.available()) {
    uint32_t red = particleSensor.getFIFORed();
    uint32_t ir = particleSensor.getFIFOIR();
    particleSensor.nextSample();
    lastSampleTime = millis();
    if (discardFirstSample) { discardFirstSample = false; continue; }
    processSample(red, ir);
  }
}

bool usableMeasurement(unsigned long now) {
  return sensorReady && now - lastSampleTime <= 200 && fingerPresent &&
         !signalClipped && calculationReady;
}

const char *measurementStatus(unsigned long now) {
  if (!sensorReady) return "sensor_missing";
  if (now - lastSampleTime >= DATA_TIMEOUT_MS) return "data_missing";
  if (!fingerPresent) return "no_finger";
  if (signalClipped) return "clipped";
  if (samplingInterrupted && !calculationReady) return "sampling_gap";
  if (!calculationReady || now - lastSampleTime > 200) return "collecting";
  if (validHeartRate != 1 || validSPO2 != 1) return "invalid";
  return "ok";
}

const char *statusMessage(const char *status) {
  if (!strcmp(status, "sensor_missing")) return "MAX30102 not found. Check power/SDA/SCL, then reset the board.";
  if (!strcmp(status, "data_missing")) return "No sensor samples received. Check the sensor connection.";
  if (!strcmp(status, "no_finger")) return "WARNING: No finger/presence detected. Rest your finger gently on the sensor.";
  if (!strcmp(status, "clipped")) return "Signal clipped. Reduce LED brightness or adjust the ADC range in the sketch.";
  if (!strcmp(status, "sampling_gap")) return "Sampling was interrupted. Collecting a new continuous four-second window.";
  if (!strcmp(status, "collecting")) return "Keep your finger steady. The first estimate needs about four seconds.";
  if (!strcmp(status, "invalid")) return "The algorithm rejected one or both readings. Keep your finger steady.";
  return "Live estimates received. Chip temperature is not body temperature.";
}

void printResults(unsigned long now) {
  bool usable = usableMeasurement(now);
  bool dataMissing = !sensorReady || now - lastSampleTime >= DATA_TIMEOUT_MS;
  Serial.print(F("BPM: "));
  if (usable && validHeartRate == 1) Serial.print(heartRate);
  else Serial.print(F("--"));
  Serial.print(F(" | SpO2:"));
  if (usable && validSPO2 == 1) { Serial.print(spo2); Serial.print(F(" %")); }
  else Serial.print(F("--"));
  Serial.print(F(" | Chip temp: "));
  if (temperatureValid && !dataMissing) { Serial.print(temperatureC, 2); Serial.print(F(" C")); }
  else Serial.print(F("--"));
  Serial.print(F(" | IR: ")); Serial.print(latestIR);
  if (dataMissing) Serial.print(F(" | WARNING: Sensor data missing"));
  else if (!fingerPresent) Serial.print(F(" | WARNING: No finger/presence detected"));
  else if (signalClipped) Serial.print(F(" | WARNING: Signal clipped; reduce LED brightness"));
  else if (!usable) {
    Serial.print(F(" | Collecting samples: ")); Serial.print(samplesStored); Serial.print(F("/100"));
  } else if (validHeartRate != 1 || validSPO2 != 1) Serial.print(F(" | Reading invalid; keep finger steady"));
  Serial.println();
}

void buildJson(unsigned long now) {
  bool usable = usableMeasurement(now);
  bool fresh = sensorReady && now - lastSampleTime < DATA_TIMEOUT_MS;
  const char *status = measurementStatus(now);
  jsonBody = F("{\"uptime_ms\":"); jsonBody += String(now);
  jsonBody += F(",\"sequence\":"); jsonBody += String(measurementSequence);
  jsonBody += F(",\"status\":\""); jsonBody += status;
  jsonBody += F("\",\"message\":\""); jsonBody += statusMessage(status);
  jsonBody += F("\",\"bpm\":");
  if (usable && validHeartRate == 1) jsonBody += String(heartRate); else jsonBody += F("null");
  jsonBody += F(",\"spo2\":");
  if (usable && validSPO2 == 1) jsonBody += String(spo2); else jsonBody += F("null");
  jsonBody += F(",\"chip_temperature_c\":");
  if (temperatureValid && fresh) jsonBody += String(temperatureC, 2); else jsonBody += F("null");
  jsonBody += F(",\"ir\":");
  if (fresh) jsonBody += String(latestIR); else jsonBody += F("null");
  jsonBody += F(",\"finger_present\":"); jsonBody += (fingerPresent && fresh) ? F("true") : F("false");
  jsonBody += F(",\"samples_collected\":"); jsonBody += String(samplesStored);
  jsonBody += F(",\"sample_target\":100,\"sample_age_ms\":"); jsonBody += String(now - lastSampleTime);
  jsonBody += F(",\"measurement_age_ms\":");
  if (calculationReady) jsonBody += String(now - lastCalculationTime); else jsonBody += F("null");
  jsonBody += F(",\"sampling_resets\":"); jsonBody += String(samplingResets);
  jsonBody += '}';
}

void closeWebClient() {
  webClient.stop();
  webClient = WiFiClient();
  httpPhase = 0;
  responseBody = nullptr;
}

void prepareResponse(const char *status, const char *type, const char *body, size_t length) {
  responseBody = body;
  responseBodyLength = length;
  int count = snprintf(responseHeader, sizeof(responseHeader),
      "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %lu\r\n"
      "Cache-Control: no-store\r\nConnection: close\r\n\r\n", status, type, (unsigned long)length);
  if (count < 0 || (size_t)count >= sizeof(responseHeader)) { closeWebClient(); return; }
  responseHeaderLength = (size_t)count;
  responseOffset = 0;
  httpPhase = 2;
}

void routeRequest() {
  if (strncmp(requestLine, "GET ", 4)) {
    prepareResponse("405 Method Not Allowed", "text/plain", "GET required", 12);
    return;
  }
  char *path = requestLine + 4;
  char *end = strchr(path, ' ');
  if (!end) { prepareResponse("400 Bad Request", "text/plain", "Bad request", 11); return; }
  *end = '\0';
  char *query = strchr(path, '?');
  if (query) *query = '\0';
  if (!strcmp(path, "/")) {
    prepareResponse("200 OK", "text/html; charset=utf-8", DASHBOARD_HTML, sizeof(DASHBOARD_HTML) - 1);
  } else if (!strcmp(path, "/api")) {
    buildJson(millis());
    prepareResponse("200 OK", "application/json", jsonBody.c_str(), jsonBody.length());
  } else if (!strcmp(path, "/favicon.ico")) {
    prepareResponse("204 No Content", "text/plain", "", 0);
  } else {
    prepareResponse("404 Not Found", "text/plain", "Not found", 9);
  }
}

void serviceWeb() {
  if (!apReady) return;
  unsigned long now = millis();
  if (now - lastWebServiceTime < 5) return;
  lastWebServiceTime = now;
  if (!httpPhase) {
    webClient = webServer.available();
    if (!webClient) return;
    requestLineLength = 0;
    requestBytes = 0;
    headerEnding = 0;
    requestLineComplete = false;
    clientStartTime = now;
    clientProgressTime = now;
    httpPhase = 1;
    return;
  }
  if (now - clientProgressTime > HTTP_IDLE_TIMEOUT_MS ||
      now - clientStartTime > HTTP_TOTAL_TIMEOUT_MS) { closeWebClient(); return; }
  if (httpPhase == 1) {
    int availableBytes = webClient.available();
    if (availableBytes <= 0) return;
    uint8_t input[128];
    size_t count = (size_t)availableBytes;
    if (count > sizeof(input)) count = sizeof(input);
    int got = webClient.read(input, count);
    if (got <= 0) return;
    clientProgressTime = millis();
    for (int i = 0; i < got; ++i) {
      char ch = (char)input[i];
      if (++requestBytes > 2048) {
        prepareResponse("431 Request Header Fields Too Large", "text/plain", "Headers too large", 17); return;
      }
      if (!requestLineComplete) {
        if (ch == '\n') { requestLine[requestLineLength] = '\0'; requestLineComplete = true; }
        else if (ch != '\r') {
          if (requestLineLength >= sizeof(requestLine) - 1) {
            prepareResponse("414 URI Too Long", "text/plain", "URI too long", 12); return;
          }
          requestLine[requestLineLength++] = ch;
        }
      }
      headerEnding = (headerEnding << 8) | (uint8_t)ch;
      if (headerEnding == 0x0D0A0D0AUL) { routeRequest(); return; }
    }
    return;
  }
  // One bounded write per call; sensor acquisition runs before the next chunk.
  const char *data = httpPhase == 2 ? responseHeader : responseBody;
  size_t length = httpPhase == 2 ? responseHeaderLength : responseBodyLength;
  if (responseOffset == length) {
    if (httpPhase == 2) { httpPhase = 3; responseOffset = 0; }
    else closeWebClient();
    return;
  }
  size_t count = length - responseOffset;
  if (count > HTTP_CHUNK_BYTES) count = HTTP_CHUNK_BYTES;
  size_t sent = webClient.write((const uint8_t *)data + responseOffset, count);
  if (sent > 0) { responseOffset += sent; clientProgressTime = millis(); }
}

void loop() {
  serviceSensor();
  unsigned long now = millis();
  updateTemperature(now);
  serviceSensor();
  serviceWeb();
  serviceSensor();
  now = millis();
  if (now - lastPrintTime >= PRINT_INTERVAL_MS) {
    lastPrintTime = now;
    printResults(now);
  }
  delay(1);
}
