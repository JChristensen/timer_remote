// TODO: Move mq init stuff out of setup (see note in setup)
// TODO: Send reset message after connecting to MQTT broker.

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
constexpr int wifiLED {7};              // Illuminates to indicate wifi connected
constexpr int mqttLED {8};              // Illuminates to indicate mqtt connected
constexpr int hbLED {9};                // Heartbeat LED
constexpr int btnPin {14};              // force prompt for wifi credentials
constexpr int relay {LED_BUILTIN};      // simulate a relay with an LED for now
constexpr uint32_t hbInterval {1000};   // Heartbeat LED blink interval

// object instantiations and globals
HardwareSerial& mySerial {Serial2};         // choose Serial, Serial1 or Serial2 here
PicoWifiManager wifi(mySerial);
WiFiClient picoClient;
JC_MQTT mq(picoClient, mySerial);
Heartbeat hb(hbLED, hbInterval);
Button btn(btnPin);

void setup()
{
    hb.begin();
    btn.begin();
    pinMode(wifiLED, OUTPUT);
    pinMode(mqttLED, OUTPUT);
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
    
    // initialize wifi & mqtt
    wifi.begin();
    while (!wifi.run()) delay(50);

    //---------TODO: Can this be moved to the class, esp. initial subscribe and publish.
    mq.begin(mqBroker, mqPort, mqTopic, wifi.getHostname());
    while (!mq.run()) delay(50);
    mq.setCallback(mqttReceive);
    mq.subscribe(wifi.getHostname());
    mqttPublish((char*)"reset");
}

void loop()
{
    bool wifiStatus = wifi.run();
    digitalWrite(wifiLED, wifiStatus);
    bool mqttStatus = mq.run();
    digitalWrite(mqttLED, mqttStatus);

    // print ntp time to serial once a minute
    static time_t printLast{0};
    time_t ntpNow = time(nullptr);
    if (printLast != ntpNow && second(ntpNow) == 0) {
        printLast = ntpNow;
        mySerial.printf("%d %.4d-%.2d-%.2d %.2d:%.2d:%.2d UTC\n",
            millis(), year(ntpNow), month(ntpNow), day(ntpNow),
            hour(ntpNow), minute(ntpNow), second(ntpNow));
    }
    
    hb.run();
}

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
    struct tm tminfo;
    gmtime_r(&l, &tminfo);

    sprintf(pub, "%s %s %.2d:%.2d:%.2d",
        wifi.getHostname(), msg, hour(l), minute(l), second(l));
    mq.publish(pub);
}

void mqttReceive(char* topic, byte* payload, unsigned int length)
{
    mySerial.printf("%d Received [%s] ", millis(), topic);
    for (uint i=0; i<length; ++i) mySerial.printf("%c", static_cast<char>(payload[i]));
    mySerial.printf("\n");

    static char msg[16];    // static messsage to publish
    char serial[16];        // copy of the serial number from the incoming message
    char* pSer = serial;    // copy the serial number
    for (uint i=length-8; i<length; ++i) *pSer++ = static_cast<char>(payload[i]);
    *pSer++ = '\0';

    if (payload[0] == 'T') {            // state True, turn on
        digitalWrite(relay, true);
        strcpy(msg, "ack ");
        strcat(msg, serial);
        mqttPublish(msg);
    }
    else if (payload[0] == 'F') {       // state False, turn off
        digitalWrite(relay, false);
        strcpy(msg, "ack ");
        strcat(msg, serial);
        mqttPublish(msg);
    }
    else if (payload[0] == 'P') {       // received Ping, send pong
        strcpy(msg, "pong ");
        strcat(msg, serial);
        mqttPublish(msg);
    }
}
