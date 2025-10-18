// Raspberry Pi Pico W test sketch.
// Sends time and temperature via MQTT over wifi once a minute.
// J.Christensen 14Mar2025
// Developed using Arduino IDE 1.8.19 and Earle Philhower's Ardino-Pico core,
// https://github.com/earlephilhower/arduino-pico
// Copyright (C) 2025 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html
//

#include <JC_Button.h>          // https://github.com/JChristensen/JC_Button
#include <PicoWifiManager.h>    // https://github.com/JChristensen/PicoWifiManager
#include <PubSubClient.h>       // https://github.com/knolleary/pubsubclient
#include <Streaming.h>          // https://github.com/janelia-arduino/Streaming
#include <Timezone.h>           // https://github.com/JChristensen/Timezone
#include "JC_MQTT.h"
#include "Heartbeat.h"

// mqtt parameters
const char* mqBroker {"z21"};
constexpr uint32_t mqPort {1883};
const char* mqTopic {"timer_main"};

// other constants
constexpr int txPin {4}, rxPin {5};     // Serial pins
constexpr int hbLED {7};                // Heartbeat LED
constexpr int relay {9};                // simulate a relay with an LED for now
constexpr int btnPin {14};              // force prompt for wifi credentials
constexpr uint32_t hbInterval {1000};   // Heartbeat LED blink interval

// object instantiations and globals
HardwareSerial& mySerial {Serial2};     // choose Serial, Serial1 or Serial2 here
PicoWifiManager wifi(mySerial);
WiFiClient picoClient;
JC_MQTT mq(picoClient, mySerial);
Heartbeat hb(hbLED, hbInterval);
Button btn(btnPin);

void setup()
{
    hb.begin();
    btn.begin();
    pinMode(relay, OUTPUT);
    Serial2.setTX(txPin);
    Serial2.setRX(rxPin);
    mySerial.begin(115200); delay(500);
    while (!mySerial && millis() < 2000) delay(10);
    mySerial.printf("\n%s\nCompiled %s %s %s @ %d MHz\n",
        __FILE__, __DATE__, __TIME__, BOARD_NAME, F_CPU/1000000);

    // check to see if the user wants to enter new wifi credentials, else initialize wifi.
    btn.read();
    if (btn.isPressed()) wifi.getCreds();
    
    // initialize wifi
    wifi.begin();
    while (!wifi.run()) delay(50);

    // initialize mqtt
    mq.begin(mqBroker, mqPort, mqTopic, wifi.getHostname());
    mq.setCallback(mqttReceive);
    mq.setConnectCallback(mqttConnect);
}

uint msReset {0};                       // signal from mqttReceive to reset the MCU

void loop()
{
    bool wifiStatus = wifi.run();
    if (wifiStatus) {
        bool mqttStatus = mq.run();
        if (mqttStatus) hb.run();

        // print ntp time to serial once a minute
        static time_t printLast{0};
        time_t ntpNow = time(nullptr);
        if (printLast != ntpNow && second(ntpNow) == 0) {
            printLast = ntpNow;
            mySerial.printf("%d %.4d-%.2d-%.2d %.2d:%.2d:%.2d UTC\n",
                millis(), year(ntpNow), month(ntpNow), day(ntpNow),
                hour(ntpNow), minute(ntpNow), second(ntpNow));
        }
    }    
    if (msReset > 0 && millis() > msReset) {
        mySerial.printf("%d Remote reset!\n", millis());
        rp2040.reboot();
    }
}

// send a message (back to timer_main).
// the format is: <hostname> <msg> <timestamp>, space separated.
// where hostname is this remote's hostname and timestamp is hh:mm:ss
void mqttPublish(char* msg)
{
    constexpr int PUB_SIZE {80};
    static char pub[PUB_SIZE];
    constexpr TimeChangeRule edt {"EDT", Second, Sun, Mar, 2, -240};  // Daylight time = UTC - 4 hours
    constexpr TimeChangeRule est {"EST", First, Sun, Nov, 2, -300};   // Standard time = UTC - 5 hours
    static Timezone eastern(edt, est);

    time_t now = time(nullptr);
    TimeChangeRule* tcr;
    time_t l = eastern.toLocal(now, &tcr);

    sprintf(pub, "%s %s %.2d:%.2d:%.2d",
        wifi.getHostname(), msg, hour(l), minute(l), second(l));
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

    static char msg[16];    // static messsage to publish
    char serial[16];        // copy of the serial number from the incoming message
    char* pSer = serial;    // save the serial number so we can echo it back
    for (uint i=length-8; i<length; ++i) *pSer++ = static_cast<char>(payload[i]);
    *pSer++ = '\0';

    // process the incoming message, note we only use the first character of the command
    switch (payload[0]) {
        case 'T':   //  state True, turn on
        case 't':
            digitalWrite(relay, true);
            strcpy(msg, "ack ");
            strcat(msg, serial);
            mqttPublish(msg);
            break;
        case 'F':   // state False, turn off
        case 'f':
            digitalWrite(relay, false);
            strcpy(msg, "ack ");
            strcat(msg, serial);
            mqttPublish(msg);
            break;
        case 'P':   // received Ping, send pong
        case 'p':
            strcpy(msg, "pong ");
            strcat(msg, serial);
            mqttPublish(msg);
            break;
        case 'R':   // received Reset/Reboot
        case 'r':
            digitalWrite(relay, false);
            strcpy(msg, "ack ");
            strcat(msg, serial);
            mqttPublish(msg);
            mySerial.printf("%d Remote reset requested: Reset in 5 seconds!\n", millis());
            msReset = millis() + 5000;
            break;
        default:    // something we were not expecting
            strcpy(msg, "nack ");
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
