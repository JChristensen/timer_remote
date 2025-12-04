// Remote wifi timer using Raspberry Pi Pico W or 2W.
// Controls one or two relays based on messages received via MQTT.
// J.Christensen 23Sep2025
// Developed using Arduino IDE 1.8.19 and Earle Philhower's Arduino-Pico core,
// https://github.com/earlephilhower/arduino-pico
// Copyright (C) 2025 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <PicoWifiManager.h>    // https://github.com/JChristensen/PicoWifiManager
#include <PubSubClient.h>       // https://github.com/knolleary/pubsubclient
#include <Streaming.h>          // https://github.com/janelia-arduino/Streaming
#include <Timezone.h>           // https://github.com/JChristensen/Timezone
#include "JC_MQTT.h"
#include "Relay.h"

// pin assignments for v1 board
constexpr int txPin {4}, rxPin {5};     // serial pins
constexpr int relayAC {8};              // relay that switches the AC power
constexpr int relayAUX {9};             // auxiliary or low voltage relay
constexpr int ledHB {18};               // heartbeat LED
constexpr int ledManual {19};           // manual mode indicator
constexpr int ledON {20};               // relay ON indicator
constexpr int btnManual {21};           // override/manual button

// object instantiations and globals
HardwareSerial& mySerial {Serial2};     // choose Serial, Serial1 or Serial2 here
PicoWifiManager wifi(mySerial);
WiFiClient picoClient;
JC_MQTT mq(picoClient, mySerial);
Relay relay(relayAC, relayAUX, 0);
Heartbeat hb(ledHB, 100, 900);
Button btn(btnManual);

constexpr TimeChangeRule edt {"EDT", Second, Sun, Mar, 2, -240};  // Daylight time = UTC - 4 hours
constexpr TimeChangeRule est {"EST", First, Sun, Nov, 2, -300};   // Standard time = UTC - 5 hours
Timezone eastern(edt, est);
TimeChangeRule* tcr;

void setup()
{
    hb.begin();
    btn.begin();
    relay.begin();
    pinMode(ledManual, OUTPUT);
    pinMode(ledON, OUTPUT);
    digitalWrite(ledManual, HIGH);  // lamp test
    digitalWrite(ledON, HIGH);
    delay(2000);
    digitalWrite(ledManual, LOW);
    digitalWrite(ledON, LOW);
    pinMode(relayAC, OUTPUT);
    pinMode(relayAUX, OUTPUT);
    Serial2.setTX(txPin);
    Serial2.setRX(rxPin);
    mySerial.begin(115200); delay(500);
    while (!mySerial && millis() < 2000) delay(10);
    mySerial.printf("\n%s\nCompiled %s %s %s @ %d MHz\n",
        __FILE__, __DATE__, __TIME__, BOARD_NAME, F_CPU/1000000);

    // check to see if the user wants to enter new wifi credentials, else initialize wifi.
    btn.read();
    if (btn.isPressed()) wifi.getCreds();
    btn.read();

    // initialize wifi
    wifi.begin();
    while (!wifi.run()) delay(50);

    // initialize mqtt
    mq.begin(wifi.getMqBroker(), wifi.getMqPort(),
        wifi.getMqTopic(), wifi.getHostname());
    mq.setCallback(mqttReceive);
    mq.setConnectCallback(mqttConnect);
}

uint msReset {0};   // signal from mqttReceive to reset the MCU
bool manualMode {false};

void loop()
{
    static char msg[32] {};
    if (wifi.run()) {
        if (mq.run()) {
            btn.read();
            relay.run();
            hb.run();

            if (btn.wasReleased()) {
                if (relay.toggle()) {
                    digitalWrite(ledON, HIGH);
                    strcpy(msg, "manual_on");
                }
                else {
                    digitalWrite(ledON, LOW);
                    strcpy(msg, "manual_off");
                }
                mqttPublish(msg);
            }
            else if (btn.pressedFor(1000)) {
                digitalWrite(ledManual, manualMode = !manualMode);
                // wait for the button to be released
                while (btn.isPressed()) btn.read();
                // tell the control program
                if (manualMode) {
                    strcpy(msg, "manual_mode");
                }
                else {
                    strcpy(msg, "automatic_mode");
                }
                mqttPublish(msg);
            }
        }
        else {
            hb.set(true);
        }

        // print time to serial once a minute
        static time_t printLast{0};
        time_t utc = time(nullptr);
        if (printLast != utc && second(utc) == 0) {
            printLast = utc;
            time_t local = eastern.toLocal(utc, &tcr);
            mySerial.printf("%d %.4d-%.2d-%.2d %.2d:%.2d:%.2d %s\n",
                millis(), year(local), month(local), day(local),
                hour(local), minute(local), second(local), tcr->abbrev);
        }
    }
    if (msReset > 0 && millis() > msReset) {
        mySerial.printf("%d Remote reset!\n", millis());
        rp2040.reboot();
    }
}

// send a message (back to timer_main).
// the format is: <hostname> <msg> <timestamp> <rssi>, space separated.
// where hostname is this remote's hostname and timestamp is hh:mm:ss
void mqttPublish(char* msg)
{
    constexpr int PUB_SIZE {80};
    static char pub[PUB_SIZE];

    time_t utc = time(nullptr);
    time_t local = eastern.toLocal(utc, &tcr);

    sprintf(pub, "%s %s %.2d:%.2d:%.2d %ld",
        wifi.getHostname(), msg, hour(local), minute(local), second(local), WiFi.RSSI());
    mq.publish(pub);
}

// process a received message. the timer_main program sends simple messages with
// two fields separated by a space: <command> <serial_number>
// where serial_number is eight hex digits.
void mqttReceive(char* topic, byte* payload, unsigned int length)
{
    mySerial.printf("%d Received [%s] ", millis(), topic);
    for (uint i=0; i<length; ++i) mySerial.printf("%c", static_cast<char>(payload[i]));
    mySerial.printf("\n");

    static char msg[40];    // static messsage to publish
    char serial[16];        // copy of the serial number from the incoming message
    char* pSer = serial;    // save the serial number so we can echo it back
    for (uint i=length-8; i<length; ++i) *pSer++ = static_cast<char>(payload[i]);
    *pSer++ = '\0';

    // process the incoming message, note we only use the first character of the command
    switch (payload[0]) {
        case 'T':   //  state True, turn on
        case 't':
            if (manualMode) {
                strcpy(msg, "ack_manual ");
                strcat(msg, serial);
                mqttPublish(msg);
            }
            else {
                digitalWrite(ledON, true);
                relay.set(true);
                strcpy(msg, "ack ");
                strcat(msg, serial);
                mqttPublish(msg);
            }
            break;
        case 'F':   // state False, turn off
        case 'f':
            if (manualMode) {
                strcpy(msg, "ack_manual ");
                strcat(msg, serial);
                mqttPublish(msg);
            }
            else {
                digitalWrite(ledON, false);
                relay.set(false);
                strcpy(msg, "ack ");
                strcat(msg, serial);
                mqttPublish(msg);
            }
            break;
        case 'P':   // received Ping, send pong
        case 'p':
            strcpy(msg, "pong ");
            strcat(msg, serial);
            mqttPublish(msg);
            break;
        case 'R':   // received Reset/Reboot
        case 'r':
            digitalWrite(ledON, false);
            relay.set(false);
            strcpy(msg, "ack ");
            strcat(msg, serial);
            mqttPublish(msg);
            mySerial.printf("%d Remote reset requested: Reset in 5 seconds!\n", millis());
            msReset = millis() + 5000;
            break;
        default:    // something we were not expecting
            strcpy(msg, "nak ");
            strcat(msg, serial);
            mqttPublish(msg);
            break;
    }
}

// this function is called when mqtt (re)connects.
// send a message to timer_main to tell it we are here.
void mqttConnect()
{
    mqttPublish((char*)"connected");
}
