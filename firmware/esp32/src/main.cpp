/*
 * ПЧ-1500 — модуль связи на ESP32.
 * Работает как ведущий Modbus RTU по UART2 (GPIO16 RX, GPIO17 TX, 115200 8N1) с платой управления (STM32)
 * и предоставляет:
 *   - веб-интерфейс мониторинга и настройки (точка доступа «PCH-1500» и/или домашняя сеть Wi-Fi);
 *   - JSON API (/api/status, /api/params, /api/write, /api/cmd, /api/wifi);
 *   - шлюз Modbus TCP (порт 502);
 *   - текстовую консоль по Bluetooth (SPP), имя устройства «PCH-1500».
 * Привод полностью работоспособен и без этого модуля.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#if USE_BLUETOOTH
#include <BluetoothSerial.h>
static BluetoothSerial bt;
#endif
extern "C" {
#include "params.h"
#include "modbus.h"
}
#include "web_page.h"

#define LINK_RX 16
#define LINK_TX 17
#define SLAVE   1

static WebServer http(80);
static WiFiServer mbtcp(502);
static WiFiClient mbClient;
static Preferences prefs;
static uint16_t st[ST_COUNT];
static int16_t pv[P_COUNT];
static bool linkOk = false;
static uint32_t tStatus, tParams, okTime;

/* в этой сборке ядро используется только ради таблицы параметров; обработчики регистров не нужны */
extern "C" int mb_reg_read(uint16_t, uint16_t *) { return 2; }
extern "C" int mb_reg_write(uint16_t, uint16_t) { return 2; }

/* ---- ведущий Modbus RTU: один запрос — один ответ ---- */
static int transact(const uint8_t *pdu, int n, uint8_t *resp, int maxResp)
{
    uint8_t f[260];
    f[0] = SLAVE; memcpy(f + 1, pdu, n);
    uint16_t c = mb_crc16(f, n + 1); f[n + 1] = c & 0xFF; f[n + 2] = c >> 8;
    while (Serial2.available()) Serial2.read();
    Serial2.write(f, n + 3); Serial2.flush();
    int len = 0; uint32_t t0 = millis(), tl = t0;
    while (millis() - t0 < 150) {
        while (Serial2.available() && len < (int)sizeof f) { f[len++] = Serial2.read(); tl = millis(); }
        if (len >= 5 && millis() - tl >= 4) break;
        delay(1);
    }
    if (len < 5 || f[0] != SLAVE || mb_crc16(f, len - 2) != (uint16_t)(f[len - 2] | (f[len - 1] << 8))) { linkOk = (millis() - okTime < 2000); return -1; }
    linkOk = true; okTime = millis();
    int m = len - 3; if (m > maxResp) m = maxResp;
    memcpy(resp, f + 1, m);
    return m;                                  /* PDU ответа (функция + данные) */
}
static bool readRegs(uint8_t fc, uint16_t addr, int n, uint16_t *out)
{
    uint8_t q[5] = {fc, (uint8_t)(addr >> 8), (uint8_t)addr, 0, (uint8_t)n}, r[256];
    int m = transact(q, 5, r, sizeof r);
    if (m != 2 + 2 * n || r[0] != fc) return false;
    for (int i = 0; i < n; i++) out[i] = (r[2 + 2 * i] << 8) | r[3 + 2 * i];
    return true;
}
/* 0 — успех, >0 — код исключения Modbus, -1 — нет связи */
static int writeReg(uint16_t addr, uint16_t val)
{
    uint8_t q[5] = {6, (uint8_t)(addr >> 8), (uint8_t)addr, (uint8_t)(val >> 8), (uint8_t)val}, r[8];
    int m = transact(q, 5, r, sizeof r);
    if (m < 2) return -1;
    return (r[0] & 0x80) ? r[1] : 0;
}
static void poll()
{
    if (millis() - tStatus >= 250) { tStatus = millis(); readRegs(4, REG_STATUS, ST_COUNT, st); }
    if (millis() - tParams >= 3000) { tParams = millis(); readRegs(3, 0, P_COUNT, (uint16_t *)pv); }
}

/* ---- JSON ---- */
static String jsonEscape(const char *s) { String o; for (; *s; s++) { if (*s == '"' || *s == '\\') o += '\\'; o += *s; } return o; }
static String statusJson()
{
    char b[640];
    snprintf(b, sizeof b,
        "{\"link\":%d,\"word\":%u,\"fault\":%u,\"f_out\":%.2f,\"f_ref\":%.2f,\"i\":%.2f,\"u\":%.1f,\"udc\":%.1f,\"t\":%.1f,"
        "\"rpm\":%d,\"torque\":%.1f,\"p\":%d,\"di\":%u,\"ai\":%.1f,\"ol\":%u,\"log\":[%u,%u,%u,%u],\"hours\":%u,\"ver\":%u,\"state\":%u,\"sp\":%.1f,\"fb\":%.1f}",
        linkOk ? 1 : 0, st[ST_WORD], st[ST_FAULT], (int16_t)st[ST_F_OUT] / 100.0, (int16_t)st[ST_F_REF] / 100.0, st[ST_CURRENT] / 100.0,
        st[ST_VOLT] / 10.0, st[ST_VDC] / 10.0, (int16_t)st[ST_TEMP] / 10.0, (int16_t)st[ST_RPM], (int16_t)st[ST_TORQUE] / 10.0,
        (int16_t)st[ST_POWER], st[ST_DI], st[ST_AI] / 10.0, st[ST_OL], st[ST_FLT1], st[ST_FLT2], st[ST_FLT3], st[ST_FLT4],
        st[ST_HOURS], st[ST_VERSION], st[ST_STATE], st[ST_PID_SP] / 10.0, st[ST_PID_FB] / 10.0);
    return String(b);
}
static String paramsJson()
{
    readRegs(3, 0, P_COUNT, (uint16_t *)pv);
    String o = "[";
    for (int i = 0; i < P_COUNT; i++) {
        const param_def_t *d = &param_defs[i];
        if (i) o += ',';
        o += "{\"n\":\""; o += jsonEscape(d->name_ru); o += "\",\"u\":\""; o += d->unit;
        o += "\",\"min\":"; o += d->min; o += ",\"max\":"; o += d->max; o += ",\"dec\":"; o += d->dec;
        o += ",\"v\":"; o += pv[i]; o += ",\"o\":";
        if (d->opts_ru) { o += '"'; o += jsonEscape(d->opts_ru); o += '"'; } else o += "null";
        o += '}';
    }
    return o + "]";
}

/* ---- HTTP ---- */
static void sendResult(int e)
{
    http.send(200, "application/json", String("{\"err\":") + e + "}");
}
static void setupHttp()
{
    http.on("/", []() { http.send_P(200, "text/html; charset=utf-8", WEB_PAGE); });
    http.on("/api/status", []() { http.send(200, "application/json", statusJson()); });
    http.on("/api/params", []() { http.send(200, "application/json", paramsJson()); });
    http.on("/api/write", []() {                       /* ?reg=N&val=V — запись регистра */
        if (!http.hasArg("reg") || !http.hasArg("val")) { http.send(400, "text/plain", "reg,val"); return; }
        sendResult(writeReg((uint16_t)http.arg("reg").toInt(), (uint16_t)(int16_t)http.arg("val").toInt()));
    });
    http.on("/api/cmd", []() {                         /* ?run=0|1&rev=0|1&f=Гц&reset=1&save=1 */
        int e = 0;
        if (http.hasArg("f")) e |= writeReg(REG_FREQ_REF, (uint16_t)(int16_t)lround(http.arg("f").toFloat() * 100));
        if (http.hasArg("reset")) { writeReg(REG_CTRL, 4); e |= writeReg(REG_CTRL, 0); }
        if (http.hasArg("run")) e |= writeReg(REG_CTRL, (http.arg("run").toInt() ? 1 : 0) | (http.arg("rev").toInt() ? 2 : 0));
        if (http.hasArg("save")) e |= writeReg(REG_SAVE, 1);
        sendResult(e);
    });
    http.on("/api/wifi", []() {                        /* ?ssid=..&pass=..&appass=.. — сохранить и перезагрузиться */
        if (http.hasArg("ssid")) { prefs.putString("ssid", http.arg("ssid")); prefs.putString("pass", http.arg("pass")); }
        if (http.hasArg("appass") && http.arg("appass").length() >= 8) prefs.putString("appass", http.arg("appass"));
        http.send(200, "application/json", "{\"err\":0}");
        delay(300); ESP.restart();
    });
    http.begin();
}

/* ---- шлюз Modbus TCP -> RTU ---- */
static void pollModbusTcp()
{
    if (!mbClient || !mbClient.connected()) { mbClient = mbtcp.available(); return; }
    if (mbClient.available() < 8) return;
    uint8_t h[7], pdu[256], r[256];
    mbClient.readBytes(h, 7);
    int n = ((h[4] << 8) | h[5]) - 1;
    if (n < 1 || n > 253 || (int)mbClient.readBytes(pdu, n) != n) { mbClient.stop(); return; }
    int m = transact(pdu, n, r, sizeof r);
    if (m < 2) { r[0] = pdu[0] | 0x80; r[1] = 0x0B; m = 2; }          /* нет ответа от привода */
    h[4] = (m + 1) >> 8; h[5] = (m + 1) & 0xFF;
    mbClient.write(h, 7); mbClient.write(r, m);
}

/* ---- текстовая консоль (Bluetooth и USB) ---- */
static void console(Stream &s, String line)
{
    line.trim(); if (!line.length()) return;
    String cmd = line, a1, a2;
    int sp = line.indexOf(' ');
    if (sp > 0) { cmd = line.substring(0, sp); a1 = line.substring(sp + 1); a1.trim(); sp = a1.indexOf(' ');
                  if (sp > 0) { a2 = a1.substring(sp + 1); a2.trim(); a1 = a1.substring(0, sp); } }
    cmd.toLowerCase();
    int e = 0;
    if (cmd == "status") { s.println(statusJson()); return; }
    else if (cmd == "get") {
        int i = a1.toInt(); uint16_t v;
        if (i < 0 || i >= P_COUNT || !readRegs(3, i, 1, &v)) { s.println("ERR"); return; }
        s.printf("P%02d %s = %d (x10^-%d %s)\n", i, param_defs[i].name_en, (int16_t)v, param_defs[i].dec, param_defs[i].unit); return;
    }
    else if (cmd == "set") e = writeReg(a1.toInt(), (uint16_t)(int16_t)a2.toInt());
    else if (cmd == "freq") e = writeReg(REG_FREQ_REF, (uint16_t)(int16_t)lround(a1.toFloat() * 100));
    else if (cmd == "run") e = writeReg(REG_CTRL, 1);
    else if (cmd == "rev") e = writeReg(REG_CTRL, 3);
    else if (cmd == "stop") e = writeReg(REG_CTRL, 0);
    else if (cmd == "reset") { writeReg(REG_CTRL, 4); e = writeReg(REG_CTRL, 0); }
    else if (cmd == "save") e = writeReg(REG_SAVE, 1);
    else if (cmd == "ip") { s.printf("AP %s, STA %s\n", WiFi.softAPIP().toString().c_str(), WiFi.localIP().toString().c_str()); return; }
    else { s.println("status | get N | set N V | freq Hz | run | rev | stop | reset | save | ip"); return; }
    if (e == 0) s.println("OK"); else s.printf("ERR %d\n", e);
}
static void pollConsole(Stream &s, String &buf)
{
    while (s.available()) { char c = s.read(); if (c == '\n' || c == '\r') { console(s, buf); buf = ""; } else if (buf.length() < 80) buf += c; }
}

void setup()
{
    Serial.begin(115200);
    Serial2.begin(115200, SERIAL_8N1, LINK_RX, LINK_TX);
    prefs.begin("pch", false);
    String ssid = prefs.getString("ssid", ""), pass = prefs.getString("pass", ""), appass = prefs.getString("appass", "12345678");
    WiFi.mode(ssid.length() ? WIFI_AP_STA : WIFI_AP);
    WiFi.softAP("PCH-1500", appass.c_str());
    if (ssid.length()) { WiFi.setAutoReconnect(true); WiFi.begin(ssid.c_str(), pass.c_str()); }
    setupHttp();
    mbtcp.begin();
#if USE_BLUETOOTH
    bt.begin("PCH-1500");
#endif
}

void loop()
{
    static String bufUsb, bufBt;
    poll();
    http.handleClient();
    pollModbusTcp();
    pollConsole(Serial, bufUsb);
#if USE_BLUETOOTH
    pollConsole(bt, bufBt);
#endif
    delay(1);
}
