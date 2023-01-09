#include <ESP8266WiFi.h>
#include <AsyncMqttClient.h>
#include <Ticker.h>

// Commands:
// Light Toggle: 4608
// Keepalive? Stopped Ringing?: 8832
// Stopped Speaking?: 8448
// Keepalive?: 9344

// Configuration
#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASSWORD "WIFI_PASSWORD"
#define MQTT_HOST IPAddress(192, 168, 178, 123)
#define MQTT_PORT 1883
#define MQTT_USER "foobar"
#define MQTT_PASSWORD "foobar"
#define MQTT_CMD_TOPIC "doorbell/command" // The topic to publish any command to
#define MQTT_RING_TOPIC "doorbell/ring"   // The topic to publish the ring boolean to
#define busInput 14                       // D0 on Wemos D1 Mini, the logic-level shifted input where the bus is connected

#define startBit 6
#define oneBit 4
#define zeroBit 2
#define RING_CMD 223391872 // The ring command for your doorbell, you can find this by opening the serial monitor and then ringing your doorbell

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;

volatile uint32_t CMD = 0;
volatile uint8_t lengthCMD = 0;
volatile bool cmdReady;

void connectToWifi()
{
    Serial.println("Connecting to Wi-Fi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void onWifiConnect(const WiFiEventStationModeGotIP &event)
{
    Serial.println("Connected to Wi-Fi.");
    connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected &event)
{
    Serial.println("Disconnected from Wi-Fi.");
    mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt()
{
    Serial.println("Connecting to MQTT...");
    mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
    Serial.println("Connected to MQTT.");
    Serial.print("Session present: ");
    Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
    Serial.println("Disconnected from MQTT.");

    if (WiFi.isConnected())
    {
        mqttReconnectTimer.once(2, connectToMqtt);
    }
}

void onMqttPublish(uint16_t packetId)
{
    Serial.print("Publish acknowledged.");
    Serial.print("  packetId: ");
    Serial.println(packetId);
}

void ICACHE_RAM_ATTR analyzeCMD()
{
    static uint32_t curCMD;
    static uint32_t usLast;
    static byte curCRC;
    static byte calCRC;
    static byte curLength;
    static byte cmdIntReady;
    static byte curPos;
    uint32_t usNow = micros();
    uint32_t timeInUS = usNow - usLast;
    usLast = usNow;
    byte curBit = 4;
    if (timeInUS >= 1000 && timeInUS <= 2999)
    {
        curBit = 0;
    }
    else if (timeInUS >= 3000 && timeInUS <= 4999)
    {
        curBit = 1;
    }
    else if (timeInUS >= 5000 && timeInUS <= 6999)
    {
        curBit = 2;
    }
    else if (timeInUS >= 7000 && timeInUS <= 24000)
    {
        curBit = 3;
        curPos = 0;
    }

    if (curPos == 0)
    {
        if (curBit == 2)
        {
            curPos++;
        }
        curCMD = 0;
        curCRC = 0;
        calCRC = 1;
        curLength = 0;
    }
    else if (curBit == 0 || curBit == 1)
    {
        if (curPos == 1)
        {
            curLength = curBit;
            curPos++;
        }
        else if (curPos >= 2 && curPos <= 17)
        {
            if (curBit)
                bitSet(curCMD, (curLength ? 33 : 17) - curPos);
            calCRC ^= curBit;
            curPos++;
        }
        else if (curPos == 18)
        {
            if (curLength)
            {
                if (curBit)
                    bitSet(curCMD, 33 - curPos);
                calCRC ^= curBit;
                curPos++;
            }
            else
            {
                curCRC = curBit;
                cmdIntReady = 1;
            }
        }
        else if (curPos >= 19 && curPos <= 33)
        {
            if (curBit)
                bitSet(curCMD, 33 - curPos);
            calCRC ^= curBit;
            curPos++;
        }
        else if (curPos == 34)
        {
            curCRC = curBit;
            cmdIntReady = 1;
        }
    }
    else
    {
        curPos = 0;
    }
    if (cmdIntReady)
    {
        cmdIntReady = 0;
        if (curCRC == calCRC)
        {
            cmdReady = 1;
            lengthCMD = curLength;
            CMD = curCMD;
        }
        curCMD = 0;
        curPos = 0;
    }
}

void printHEX(uint32_t DATA)
{
    uint8_t numChars = lengthCMD ? 8 : 4;
    uint32_t mask = 0x0000000F;
    mask = mask << 4 * (numChars - 1);
    for (uint32_t i = numChars; i > 0; --i)
    {
        Serial.print(((DATA & mask) >> (i - 1) * 4), HEX);
        mask = mask >> 4;
    }
}

void setup()
{
    Serial.begin(115200);
    pinMode(busInput, INPUT);
    attachInterrupt(digitalPinToInterrupt(busInput), analyzeCMD, CHANGE);

    wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
    wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    // mqttClient.onSubscribe(onMqttSubscribe);
    // mqttClient.onUnsubscribe(onMqttUnsubscribe);
    mqttClient.onPublish(onMqttPublish);
    mqttClient.setServer(MQTT_HOST, MQTT_PORT);
    mqttClient.setCredentials(MQTT_USER, MQTT_PASSWORD);

    connectToWifi();
}

void loop()
{
    if (cmdReady)
    {
        cmdReady = 0;
        Serial.write(0x01);
        Serial.print("$");
        printHEX(CMD);
        Serial.write(0x04);
        Serial.println();
        Serial.println(CMD);
        mqttClient.publish(MQTT_CMD_TOPIC, 1, true, String(CMD).c_str());

        if (CMD == RING_CMD)
        {
            mqttClient.publish(MQTT_RING_TOPIC, 1, true, String(true).c_str());
        }
        else
        {
            mqttClient.publish(MQTT_RING_TOPIC, 1, true, String(false).c_str());
        }
    }
}