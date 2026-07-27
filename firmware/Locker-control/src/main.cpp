/*
  IoT mini SmartLocker - Arduino Uno locker controller
*/

#define AP_SSID     "(Wi-Fi ID)"
#define AP_PASS     "(Wi-Fi PASSWORD)"
#define SERVER_NAME "(SERVER IP)"
#define SERVER_PORT 5000
#define LOGID       "LOCKER"
#define PASSWD      "(PASSWORD)"

#define ENABLE_WIFI         1
#define ENABLE_SERIAL_DEBUG 0
#define ENABLE_SERVO        1
#define ENABLE_INA219       1
#define ENABLE_SENSORS      1
#define REQUIRE_DOOR_CLOSED_TO_LOCK 0

#define LOCKER_NO "01"
#define RFID_TAG  "(RFID UID)"

#define WIFIRX    6
#define WIFITX    7
#define SERVO_PIN 3
#define HALL_PIN  2
#define FLEX_PIN  A1

#define RFID_SDA_PIN 10
#define RFID_RST_PIN 9

#define CMD_SIZE 128
#define ARR_CNT  6
#define ID_SIZE  16

#define SERVO_LOCK_ANGLE    140
#define SERVO_UNLOCK_ANGLE  0
#define DOOR_TIMEOUT_MS     60000UL
#define STATUS_PERIOD_MS    10000UL
#define SENSOR_PERIOD_MS    300UL
#define WIFI_RECONNECT_MS   5000UL
#define WIFI_POLL_MS        500UL
#define WIFI_INIT_RETRY_MAX 5
#define WIFI_JOIN_RETRY_MAX 5
#define RFID_COOLDOWN_MS    1500UL
#define SERVO_DRIVE_MS      700UL

#define FLEX_DETECT_DELTA       8
#define FLEX_RELEASE_DELTA      5
#define FLEX_SAMPLE_COUNT       10
#define SERVO_CURRENT_ACTIVE_MA 40

#include <Arduino.h>
#if ENABLE_WIFI
#include <SoftwareSerial.h>
#include <WiFiEsp.h>
#endif
#if ENABLE_SERVO
#include <Servo.h>
#endif
#include <SPI.h>
#include <MFRC522.h>
#if ENABLE_INA219
#include <Wire.h>
#include <Adafruit_INA219.h>
#endif
#include <string.h>
#include <stdlib.h>
#include <avr/pgmspace.h>

#if ENABLE_WIFI
SoftwareSerial wifiSerial(WIFIRX, WIFITX);
WiFiEspClient client;
#endif
#if ENABLE_SERVO
Servo lockServo;
#endif
MFRC522 rfid(RFID_SDA_PIN, RFID_RST_PIN);
#if ENABLE_INA219
Adafruit_INA219 ina219;
#endif

char recvBuf[CMD_SIZE];
char sensorRecvId[ID_SIZE] = LOGID;

bool wifiReady;
bool inaReady;
bool doorClosed;
bool itemPresent;
bool lockOpen;
bool alertSent;
bool doorOpenedDuringAccess;
bool forcedOpenAlertSent;
bool itemTamperAlertSent;
bool activeRfidValid;
uint8_t lastRfidUid[10];
uint8_t activeRfidUid[10];
uint8_t lastRfidUidSize;
uint8_t activeRfidUidSize;

int flexBase;
int flexValue;
int flexDiff;
int sensorReportPeriodSec;
int current_mA;
unsigned long unlockStartMs;
unsigned long lastStatusMs;
unsigned long lastSensorMs;
unsigned long lastReconnectMs;
unsigned long lastWifiPollMs;
unsigned long lastRfidMs;
unsigned long lastSensorReportMs;

#if ENABLE_WIFI
void wifi_Setup(void);
void wifi_Init(void);
int server_Connect(void);
void socketEvent(void);
#endif
void sendState(const __FlashStringHelper *eventName);
void sendAlert(const __FlashStringHelper *reason);
void sendClientAlert(const __FlashStringHelper *reason);
void sendClientAlertTo(const char *targetId, const __FlashStringHelper *reason);
void sendLockerLog(const __FlashStringHelper *result);
void sendSensor(const char *targetId);
void sendRfidAuthRequest(void);
void sendKioskAuthRequest(const char *lockerNo);
void handleRfidScan(void);
bool formatLastRfidUid(char *out, size_t outSize);
const __FlashStringHelper *doorStateText(void);
const __FlashStringHelper *lockStateText(void);
const __FlashStringHelper *itemStateText(void);
void sendBuffer(void);
void readSensors(void);
void calibrateFlex(void);
bool readDoorClosed(void);
bool readItemPresent(void);
bool checkRfid(void);
bool loadRfidUidFromText(const char *uidText);
int hexValue(char c);
bool isUidSeparator(char c);
void processRfidAuth(void);
bool isSameActiveRfid(void);
void rememberActiveRfid(void);
void clearActiveRfid(void);
int driveServoAndReadCurrent(uint8_t angle);
void openLocker(void);
void closeLocker(void);
void handleCommand(const char *senderId, const char *cmd, const char *arg, const char *arg2);
void debugEvent(const __FlashStringHelper *eventName);

void setup() {
#if ENABLE_SERIAL_DEBUG
    Serial.begin(115200);
    Serial.println(F("BOOT"));
#endif
    pinMode(HALL_PIN, INPUT_PULLUP);

    delay(200);
#if ENABLE_SERVO
    driveServoAndReadCurrent(SERVO_LOCK_ANGLE);
#endif

    pinMode(RFID_SDA_PIN, OUTPUT);
    digitalWrite(RFID_SDA_PIN, HIGH);
    SPI.begin();
    rfid.PCD_Init();
    delay(4);

#if ENABLE_INA219
    Wire.begin();
    inaReady = ina219.begin();
#else
    inaReady = false;
#endif

#if ENABLE_SENSORS
    calibrateFlex();
    doorClosed = readDoorClosed();
    itemPresent = readItemPresent();
#else
    flexBase = 0;
    flexValue = 0;
    flexDiff = 0;
    doorClosed = true;
    itemPresent = false;
#endif
    current_mA = 0;
    lockOpen = false;
    alertSent = false;
    doorOpenedDuringAccess = false;
    forcedOpenAlertSent = false;
    itemTamperAlertSent = false;
    clearActiveRfid();

#if ENABLE_WIFI
    wifi_Setup();
#endif
    sendState(F("BOOT"));
}

void loop() {
    unsigned long now = millis();

#if ENABLE_WIFI
    if ((now - lastWifiPollMs) >= WIFI_POLL_MS) {
        lastWifiPollMs = now;

        if (client.available()) {
            socketEvent();
        }

        if (!client.connected() && ((now - lastReconnectMs) >= WIFI_RECONNECT_MS)) {
            lastReconnectMs = now;
            wifiReady = (server_Connect() == 1);
        }
    }
#endif

#if ENABLE_SENSORS
    if ((now - lastSensorMs) >= SENSOR_PERIOD_MS) {
        lastSensorMs = now;
        readSensors();
    }
#endif

    if (checkRfid()) {
        handleRfidScan();
    }

    if (lockOpen && !doorClosed && ((now - unlockStartMs) >= DOOR_TIMEOUT_MS)) {
        if (!alertSent) {
            sendAlert(F("DOOR_TIMEOUT"));
            alertSent = true;
        }
    }

    if ((now - lastStatusMs) >= STATUS_PERIOD_MS) {
        lastStatusMs = now;
        sendState(F("PERIODIC"));
    }

    if ((sensorReportPeriodSec > 0) && ((now - lastSensorReportMs) >= ((unsigned long)sensorReportPeriodSec * 1000UL))) {
        lastSensorReportMs = now;
        sendSensor(sensorRecvId);
    }
}

#if ENABLE_WIFI
void socketEvent() {
    int i = 0;
    char *pToken;
    char *pArray[ARR_CNT] = {0};

    memset(recvBuf, 0, sizeof(recvBuf));
    client.readBytesUntil('\n', recvBuf, sizeof(recvBuf) - 1);
    client.flush();

#if ENABLE_SERIAL_DEBUG
    Serial.print(F("RX "));
    Serial.println(recvBuf);
#endif

    pToken = strtok(recvBuf, "[@]");
    while (pToken != NULL) {
        while ((*pToken == ' ') || (*pToken == '\r') || (*pToken == '\n') || (*pToken == '\t')) pToken++;

        char *pEnd = pToken + strlen(pToken);
        while ((pEnd > pToken) &&
               ((*(pEnd - 1) == ' ') || (*(pEnd - 1) == '\r') ||
                (*(pEnd - 1) == '\n') || (*(pEnd - 1) == '\t'))) {
            *--pEnd = '\0';
        }

        pArray[i] = pToken;
        if (++i >= ARR_CNT) break;
        pToken = strtok(NULL, "[@]");
    }

    if (pArray[0] == NULL) return;

    for (int cmdIdx = 0; cmdIdx < i; cmdIdx++) {
        if (!strcmp(pArray[cmdIdx], "KIOSK_AUTH") ||
            !strcmp(pArray[cmdIdx], "RFID_AUTH") ||
            !strcmp(pArray[cmdIdx], "RFID_DENY")) {
            const char *senderId = (cmdIdx > 0) ? pArray[cmdIdx - 1] : LOGID;
            const char *arg1 = ((cmdIdx + 1) < ARR_CNT) ? pArray[cmdIdx + 1] : NULL;
            const char *arg2 = ((cmdIdx + 2) < ARR_CNT) ? pArray[cmdIdx + 2] : NULL;

            handleCommand(senderId, pArray[cmdIdx], arg1, arg2);
            return;
        }
    }

    if (pArray[1] == NULL) return;

    if (!strncmp(pArray[1], " New connected", 4)) {
        return;
    }

    if (!strncmp(pArray[1], " Alr", 4)) {
        client.stop();
        server_Connect();
        return;
    }

    handleCommand(pArray[0], pArray[1], pArray[2], pArray[3]);
}
#endif

void handleCommand(const char *senderId, const char *cmd, const char *arg, const char *arg2) {
#if ENABLE_SERIAL_DEBUG
    Serial.print(F("CMD sender="));
    Serial.print(senderId ? senderId : "NULL");
    Serial.print(F(" cmd="));
    Serial.print(cmd ? cmd : "NULL");
    Serial.print(F(" arg="));
    Serial.print(arg ? arg : "NULL");
    Serial.print(F(" arg2="));
    Serial.println(arg2 ? arg2 : "NULL");
#endif

    if (!strcmp(cmd, "KIOSK_AUTH")) {
        const char *lockerNo = arg;
        const char *uidText = arg2;

        if ((lockerNo == NULL) || (lockerNo[0] == '\0')) {
            lockerNo = senderId;
            uidText = arg;
        }

        if ((lockerNo == NULL) || strcmp(lockerNo, LOCKER_NO)) {
            sendAlert(F("KIOSK_LOCKER_MISMATCH"));
            return;
        }

        if ((uidText != NULL) && loadRfidUidFromText(uidText)) {
            processRfidAuth();
        }
        else {
            openLocker();
        }
        return;

        /*
         * Previous UID-carrying KIOSK_AUTH format:
         * [LOCKER]KIOSK_AUTH@01@(RFID UID)
         * KIOSK_AUTH@01@(RFID UID)
         *
         * const char *lockerNo = senderId;
         * const char *uidText = arg;
         *
         * if ((arg2 != NULL) && (arg2[0] != '\0')) {
         *     lockerNo = arg;
         *     uidText = arg2;
         * }
         * else if ((senderId != NULL) && !strcmp(senderId, LOGID)) {
         *     lockerNo = arg;
         *     uidText = arg2;
         * }
         *
         * if ((lockerNo == NULL) || strcmp(lockerNo, LOCKER_NO)) {
         *     sendAlert(F("KIOSK_LOCKER_MISMATCH"));
         *     return;
         * }
         *
         * if ((uidText != NULL) && loadRfidUidFromText(uidText)) {
         *     processRfidAuth();
         * }
         * else {
         *     sendAlert(F("INVALID_RFID_UID"));
         * }
         * return;
         */
    }

    if (!strcmp(cmd, "RFID_AUTH")) {
        const char *lockerNo = arg;
        const char *uidText = arg2;

        if ((lockerNo == NULL) || (lockerNo[0] == '\0')) {
            lockerNo = senderId;
            uidText = arg;
        }

        if ((lockerNo == NULL) || strcmp(lockerNo, LOCKER_NO)) {
            sendAlert(F("RFID_LOCKER_MISMATCH"));
            return;
        }

        if ((uidText != NULL) && loadRfidUidFromText(uidText)) {
            rememberActiveRfid();
        }

        openLocker();
        return;
    }

    if (!strcmp(cmd, "RFID")) {
        if ((arg != NULL) && loadRfidUidFromText(arg)) {
            handleRfidScan();
        }
        else {
            sendAlert(F("INVALID_RFID_UID"));
        }
        return;
    }

    if (!strcmp(cmd, "RFID_DENY")) {
        sendAlert(F("RFID_DB_DENIED"));
        return;
    }

    if (!strcmp(cmd, "UNLOCK")) {
        openLocker();
        return;
    }

    if (!strcmp(cmd, "DOOR")) {
        if ((arg != NULL) && !strcmp(arg, "OPEN")) {
            openLocker();
        }
        else if ((arg != NULL) && !strcmp(arg, "CLOSE")) {
            closeLocker();
        }
        else {
            sendAlert(F("UNKNOWN_DOOR_COMMAND"));
        }
        return;
    }

    if (!strcmp(cmd, "LOCK")) {
        closeLocker();
        return;
    }

    if (!strcmp(cmd, "GETSTATE")) {
        sendState(F("GETSTATE"));
        return;
    }

    if (!strcmp(cmd, "GETSENSOR")) {
        if ((senderId != NULL) && (senderId[0] != '\0')) {
            strncpy(sensorRecvId, senderId, sizeof(sensorRecvId) - 1);
            sensorRecvId[sizeof(sensorRecvId) - 1] = '\0';
        }
        sensorReportPeriodSec = (arg != NULL) ? atoi(arg) : 0;
        if (sensorReportPeriodSec < 0) sensorReportPeriodSec = 0;
        lastSensorReportMs = millis();
        sendSensor(sensorRecvId);
        return;
    }

    if (!strcmp(cmd, "CALFLEX")) {
        calibrateFlex();
        sendState(F("CALFLEX"));
        return;
    }
}

bool checkRfid() {
    unsigned long now = millis();
    if ((now - lastRfidMs) < RFID_COOLDOWN_MS) return false;
    if (!rfid.PICC_IsNewCardPresent()) return false;
    if (!rfid.PICC_ReadCardSerial()) return false;

    lastRfidMs = now;
    lastRfidUidSize = rfid.uid.size;
    if (lastRfidUidSize > sizeof(lastRfidUid)) lastRfidUidSize = sizeof(lastRfidUid);
    memcpy(lastRfidUid, rfid.uid.uidByte, lastRfidUidSize);

#if ENABLE_SERIAL_DEBUG
    Serial.print(F("RFID_AUTH UID="));
    for (uint8_t i = 0; i < lastRfidUidSize; i++) {
        if (lastRfidUid[i] < 0x10) Serial.print('0');
        Serial.print(lastRfidUid[i], HEX);
        if ((i + 1) < lastRfidUidSize) Serial.print(':');
    }
    Serial.println();
#endif

    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return true;
}

bool loadRfidUidFromText(const char *uidText) {
    uint8_t uid[10];
    uint8_t uidSize = 0;
    int highNibble = -1;

    if (uidText == NULL) return false;

    while (*uidText != '\0') {
        int value = hexValue(*uidText);
        uidText++;

        if (value < 0) {
            if (isUidSeparator(*(uidText - 1))) continue;
            return false;
        }

        if (highNibble < 0) {
            highNibble = value;
            continue;
        }

        if (uidSize >= sizeof(uid)) return false;
        uid[uidSize++] = (uint8_t)((highNibble << 4) | value);
        highNibble = -1;
    }

    if ((highNibble >= 0) || (uidSize == 0)) return false;

    memcpy(lastRfidUid, uid, uidSize);
    lastRfidUidSize = uidSize;
    lastRfidMs = millis();
    return true;
}

int hexValue(char c) {
    if ((c >= '0') && (c <= '9')) return c - '0';
    if ((c >= 'A') && (c <= 'F')) return c - 'A' + 10;
    if ((c >= 'a') && (c <= 'f')) return c - 'a' + 10;
    return -1;
}

bool isUidSeparator(char c) {
    return (c == ':') || (c == '-') || (c == ' ') || (c == '\r') || (c == '\n') || (c == '\t');
}

void handleRfidScan() {
    char uidText[sizeof(lastRfidUid) * 2 + 1];

    if (lockOpen) {
        processRfidAuth();
        return;
    }

    if (formatLastRfidUid(uidText, sizeof(uidText)) && !strcmp(uidText, RFID_TAG)) {
        rememberActiveRfid();
        openLocker();
        return;
    }

    sendRfidAuthRequest();
    sendState(F("RFID_SCAN"));
}

bool formatLastRfidUid(char *out, size_t outSize) {
    static const char hex[] = "0123456789ABCDEF";

    if ((out == NULL) || (outSize < ((size_t)lastRfidUidSize * 2U + 1U))) return false;

    for (uint8_t i = 0; i < lastRfidUidSize; i++) {
        out[i * 2] = hex[(lastRfidUid[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[lastRfidUid[i] & 0x0F];
    }
    out[lastRfidUidSize * 2] = '\0';
    return lastRfidUidSize > 0;
}

void processRfidAuth() {
    char uidText[sizeof(lastRfidUid) * 2 + 1];

    sendState(F("RFID_AUTH"));
    if (lockOpen) {
        if (isSameActiveRfid() ||
            (formatLastRfidUid(uidText, sizeof(uidText)) && !strcmp(uidText, RFID_TAG))) {
            closeLocker();
        }
        else {
            sendAlert(F("RFID_MISMATCH"));
        }
    }
    else {
        rememberActiveRfid();
        openLocker();
    }
}

bool isSameActiveRfid() {
    if (!activeRfidValid) return false;
    if (activeRfidUidSize != lastRfidUidSize) return false;
    return memcmp(activeRfidUid, lastRfidUid, activeRfidUidSize) == 0;
}

void rememberActiveRfid() {
    activeRfidUidSize = lastRfidUidSize;
    memcpy(activeRfidUid, lastRfidUid, activeRfidUidSize);
    activeRfidValid = activeRfidUidSize > 0;
}

void clearActiveRfid() {
    activeRfidValid = false;
    activeRfidUidSize = 0;
}

int driveServoAndReadCurrent(uint8_t angle) {
    int peakCurrent = 0;
    unsigned long startMs = millis();

#if ENABLE_SERVO
    lockServo.attach(SERVO_PIN);
    delay(20);
    lockServo.write(angle);
#else
    (void)angle;
#endif
    do {
#if ENABLE_INA219
        if (inaReady) {
            int sample = (int)ina219.getCurrent_mA();
            if (sample < 0) sample = -sample;
            if (sample > peakCurrent) peakCurrent = sample;
        }
#endif
        delay(50);
    } while ((millis() - startMs) < SERVO_DRIVE_MS);

#if ENABLE_SERVO
    lockServo.detach();
#endif

    return peakCurrent;
}

void openLocker() {
    debugEvent(F("SERVO_UNLOCK_START"));
    current_mA = driveServoAndReadCurrent(SERVO_UNLOCK_ANGLE);
    debugEvent(F("SERVO_UNLOCK_DONE"));

    lockOpen = true;
    unlockStartMs = millis();
    alertSent = false;
    doorOpenedDuringAccess = false;
    forcedOpenAlertSent = false;
    itemTamperAlertSent = false;

    if (inaReady && (current_mA < SERVO_CURRENT_ACTIVE_MA)) {
        sendAlert(F("UNLOCK_CURRENT_LOW"));
    }

    sendState(F("UNLOCK"));
}

void closeLocker() {
#if REQUIRE_DOOR_CLOSED_TO_LOCK
    if (!doorClosed) {
        sendAlert(F("LOCK_REQUEST_WITH_DOOR_OPEN"));
        return;
    }
#endif

    debugEvent(F("SERVO_LOCK_START"));
    current_mA = driveServoAndReadCurrent(SERVO_LOCK_ANGLE);
    debugEvent(F("SERVO_LOCK_DONE"));

    lockOpen = false;
    alertSent = false;
    doorOpenedDuringAccess = false;
    forcedOpenAlertSent = false;
    itemTamperAlertSent = false;
    clearActiveRfid();

    if (inaReady && (current_mA < SERVO_CURRENT_ACTIVE_MA)) {
        sendAlert(F("LOCK_CURRENT_LOW"));
    }

    sendState(F("LOCK"));
}

void readSensors() {
    bool prevDoorClosed = doorClosed;
    bool prevItemPresent = itemPresent;

    doorClosed = readDoorClosed();
    itemPresent = readItemPresent();
#if ENABLE_INA219
    current_mA = inaReady ? (int)ina219.getCurrent_mA() : current_mA;
#endif

    if (doorClosed != prevDoorClosed) {
        sendState(doorClosed ? F("DOOR_CLOSED") : F("DOOR_OPENED"));

        if (lockOpen && !doorClosed) {
            doorOpenedDuringAccess = true;
        }

        if (!lockOpen && !doorClosed && !forcedOpenAlertSent) {
            sendAlert(F("FORCED_OPEN"));
            // Client alert send point: FORCED_OPEN -> USER and MANAGER.
            sendClientAlert(F("FORCED_OPEN"));
            forcedOpenAlertSent = true;
        }
    }

    if (itemPresent != prevItemPresent) {
        sendState(itemPresent ? F("ITEM_STORED") : F("ITEM_REMOVED"));

        if (!lockOpen && !itemTamperAlertSent) {
            sendAlert(F("ITEM_CHANGED_WHILE_LOCKED"));
            // Client alert send point: ITEM_CHANGED_WHILE_LOCKED -> USER and MANAGER.
            sendClientAlert(F("ITEM_CHANGED_WHILE_LOCKED"));
            itemTamperAlertSent = true;
        }
    }
}

bool readDoorClosed() {
    return digitalRead(HALL_PIN) == LOW;
}

bool readItemPresent() {
    long sum = 0;
    for (int i = 0; i < FLEX_SAMPLE_COUNT; i++) {
        sum += analogRead(FLEX_PIN);
        delay(2);
    }

    flexValue = (int)(sum / FLEX_SAMPLE_COUNT);
    flexDiff = abs(flexValue - flexBase);

    if (itemPresent) {
        return flexDiff >= FLEX_RELEASE_DELTA;
    }

    return flexDiff >= FLEX_DETECT_DELTA;
}

void calibrateFlex() {
    long sum = 0;
    for (int i = 0; i < 50; i++) {
        sum += analogRead(FLEX_PIN);
        delay(5);
    }
    flexBase = (int)(sum / 50L);
    flexValue = flexBase;
    flexDiff = 0;
}

void sendState(const __FlashStringHelper *eventName) {
#if ENABLE_SERIAL_DEBUG
    Serial.print(F("STATE "));
    Serial.println(eventName);
#endif

#if ENABLE_WIFI
    if (!client.connected()) return;

    snprintf_P(
        recvBuf,
        sizeof(recvBuf),
        PSTR("[LOCKER_SQL]LOCKER_STATE@%s@%S@%S@%S\n"),
        LOCKER_NO,
        (PGM_P)doorStateText(),
        (PGM_P)lockStateText(),
        (PGM_P)itemStateText());
    sendBuffer();

    sendLockerLog(eventName);
#endif
}

void sendLockerLog(const __FlashStringHelper *result) {
#if ENABLE_WIFI
    if (!client.connected()) return;

    snprintf_P(
        recvBuf,
        sizeof(recvBuf),
        PSTR("[LOCKER_SQL]LOCKER_LOG@%s@%S@%s@%S@%S@%S@%S\n"),
        LOCKER_NO,
        result,
        RFID_TAG,
        (PGM_P)doorStateText(),
        (PGM_P)lockStateText(),
        (PGM_P)itemStateText(),
        result);
    sendBuffer();
#endif
}

void sendAlert(const __FlashStringHelper *reason) {
#if ENABLE_SERIAL_DEBUG
    Serial.print(F("ALERT "));
    Serial.println(reason);
#endif

#if ENABLE_WIFI
    if (!client.connected()) return;

    snprintf_P(
        recvBuf,
        sizeof(recvBuf),
        PSTR("[LOCKER_SQL]ALERT@%s@%S@%S@%S@%S@%S\n"),
        LOCKER_NO,
        (PGM_P)reason,
        (PGM_P)doorStateText(),
        (PGM_P)lockStateText(),
        (PGM_P)itemStateText(),
        (PGM_P)reason);
    sendBuffer();
#endif
}

void sendClientAlert(const __FlashStringHelper *reason) {
    // USER/MANAGER client alert sender for readSensors() abnormal cases.
    sendClientAlertTo("USER", reason);
    sendClientAlertTo("MANAGER", reason);
}

void sendClientAlertTo(const char *targetId, const __FlashStringHelper *reason) {
#if ENABLE_SERIAL_DEBUG
    Serial.print(F("CLIENT_ALERT "));
    Serial.print(targetId);
    Serial.print(F(" "));
    Serial.println(reason);
#endif

#if ENABLE_WIFI
    if (!client.connected()) return;

    snprintf_P(
        recvBuf,
        sizeof(recvBuf),
        PSTR("[%s]ALERT@%s@%S@%S@%S@%S\n"),
        targetId,
        LOCKER_NO,
        (PGM_P)reason,
        (PGM_P)doorStateText(),
        (PGM_P)lockStateText(),
        (PGM_P)itemStateText());
    sendBuffer();
#else
    (void)targetId;
    (void)reason;
#endif
}

void sendRfidAuthRequest() {
#if ENABLE_WIFI
    char uidText[sizeof(lastRfidUid) * 2 + 1];

    if (!client.connected()) {
        sendAlert(F("RFID_DB_OFFLINE"));
        return;
    }

    if (!formatLastRfidUid(uidText, sizeof(uidText))) {
        sendAlert(F("INVALID_RFID_UID"));
        return;
    }

    snprintf_P(
        recvBuf,
        sizeof(recvBuf),
        PSTR("[LOCKER_SQL]RFID_AUTH@%s@%s\n"),
        LOCKER_NO,
        uidText);
    sendBuffer();
#else
    sendAlert(F("RFID_DB_OFFLINE"));
#endif
}

void sendKioskAuthRequest(const char *lockerNo) {
#if ENABLE_WIFI
    if (!client.connected()) {
        sendAlert(F("RFID_DB_OFFLINE"));
        return;
    }

    if ((lockerNo == NULL) || (lockerNo[0] == '\0')) {
        sendAlert(F("KIOSK_LOCKER_MISMATCH"));
        return;
    }

    snprintf_P(
        recvBuf,
        sizeof(recvBuf),
        PSTR("[LOCKER_SQL]KIOSK@%s\n"),
        lockerNo);
    sendBuffer();
#else
    (void)lockerNo;
    sendAlert(F("RFID_DB_OFFLINE"));
#endif
}

void sendSensor(const char *targetId) {
#if ENABLE_SERIAL_DEBUG
    Serial.print(F("SENSOR "));
    Serial.print(targetId);
    Serial.print(F(" "));
    Serial.println(itemStateText());
#endif

#if ENABLE_WIFI
    if (!client.connected()) return;

    snprintf_P(recvBuf, sizeof(recvBuf), PSTR("[%s]SENSOR@%S\n"), targetId, (PGM_P)itemStateText());
    sendBuffer();
#else
    (void)targetId;
#endif
}

#if ENABLE_WIFI
void sendBuffer() {
#if ENABLE_SERIAL_DEBUG
    Serial.print(F("TX "));
    Serial.print(recvBuf);
#endif
    client.write((const uint8_t *)recvBuf, strlen(recvBuf));
    client.flush();
}
#else
void sendBuffer() {
}
#endif

const __FlashStringHelper *doorStateText() {
    return doorClosed ? F("CLOSED") : F("OPEN");
}

const __FlashStringHelper *lockStateText() {
    return lockOpen ? F("UNLOCKED") : F("LOCKED");
}

const __FlashStringHelper *itemStateText() {
    return itemPresent ? F("EXIST") : F("EMPTY");
}

void debugEvent(const __FlashStringHelper *eventName) {
#if ENABLE_SERIAL_DEBUG
    Serial.println(eventName);
#else
    (void)eventName;
#endif
}

#if ENABLE_WIFI
void wifi_Setup() {
    wifiSerial.begin(38400);
    wifi_Init();
    wifiReady = (server_Connect() == 1);
}

void wifi_Init() {
    for (int retry = 0; retry < WIFI_INIT_RETRY_MAX; retry++) {
        WiFi.init(&wifiSerial);
        if (WiFi.status() != WL_NO_SHIELD) {
            break;
        }
        delay(1000);
    }

    if (WiFi.status() == WL_NO_SHIELD) {
        return;
    }

    for (int retry = 0; retry < WIFI_JOIN_RETRY_MAX; retry++) {
        if (WiFi.begin(AP_SSID, AP_PASS) == WL_CONNECTED) {
            return;
        }
        delay(1000);
    }
}

int server_Connect() {
    if (client.connect(SERVER_NAME, SERVER_PORT)) {
        client.print(F("[" LOGID ":" PASSWD "]"));
        return 1;
    }

    return 0;
}
#endif
