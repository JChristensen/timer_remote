// Remote wifi timer using Raspberry Pi Pico W or 2W.
// A class to control two relays. The first (main) relay is meant to
// control AC power to a lamp, etc. The second (optional) relay is meant
// to control some auxiliary switching function. If only one relay
// is needed, the second need not be present.
// By default, there is a 1000ms dwell time between the closing and
// opening of the two relays.
// When closing, the sequence is aux relay first, then the main relay.
// When opening, the main relay is first, then the aux relay.
// Copyright (C) 2025 by Jack Christensen and licensed under
// GNU GPL v3.0, https://www.gnu.org/licenses/gpl.html

#pragma once

class Relay
{
    enum m_states_t {WAIT, DWELL_ON, DWELL_OFF};
    enum m_commands_t {NONE, TURN_ON, TURN_OFF};
    public:
        Relay(int relayAC, int relayAUX, uint dwellTime=1000)
            : m_relayAC(relayAC), m_relayAUX(relayAUX), m_dwell(dwellTime) {}
        void begin();
        void run();
        void set(bool state);
        bool toggle();
    
    private:
        m_states_t m_state {WAIT};
        m_commands_t m_cmd {NONE};
        bool m_relayState {false};
        int m_relayAC;          // pin for the AC (primary) relay
        int m_relayAUX;         // pin for the auxiliary relay
        uint m_dwell;           // the time in milliseconds between changing the relays
        uint m_ms {};           // used to time the dwell time
};

// initialization: call once in setup, etc.
void Relay::begin()
{
    pinMode(m_relayAC, OUTPUT);
    pinMode(m_relayAUX, OUTPUT);
    digitalWrite(m_relayAC, false);
    digitalWrite(m_relayAUX, false);
}

// main state machine. call frequently.
void Relay::run()
{
    switch(m_state) {
        case WAIT:
            if (m_cmd == TURN_ON){
                digitalWrite(m_relayAUX, true);
                m_ms = millis();
                m_relayState = true;
                m_state = DWELL_ON;
            }
            else if (m_cmd == TURN_OFF) {
                digitalWrite(m_relayAC, false);
                m_ms = millis();
                m_relayState = false;
                m_state = DWELL_OFF;
            }
            break;

        case DWELL_ON:
            if (millis() >= m_ms + m_dwell) {
                digitalWrite(m_relayAC, true);
                m_cmd = NONE;
                m_state = WAIT;
            }
            break;

        case DWELL_OFF:
            if (millis() >= m_ms + m_dwell) {
                digitalWrite(m_relayAUX, false);
                m_cmd = NONE;
                m_state = WAIT;
            }
            break;
    }
}

// call set() to turn the relays on (closed) or off (open).
void Relay::set(bool state)
{
    m_cmd = state ? TURN_ON : TURN_OFF;
}

// call toggle() to set the relays to the opposite of the current state.
// the current state (after toggle) is returned.
bool Relay::toggle()
{
    m_cmd = m_relayState ? TURN_OFF : TURN_ON;
    return !m_relayState;
}

// Heartbeat LED class
class Heartbeat
{
    public:
        Heartbeat(uint8_t pin, uint32_t interval)
            : m_pin(pin), m_onTime(interval), m_offTime(interval), m_state(true) {}
        Heartbeat(uint8_t pin, uint32_t onTime, uint32_t offTime)
            : m_pin(pin), m_onTime(onTime), m_offTime(offTime), m_state(true) {}
        void begin();
        void run();
        void setInterval(uint32_t onTime, uint32_t offTime)
            {m_onTime = onTime; m_offTime = offTime;}
        void set(bool state);
    
    private:
        uint8_t m_pin;
        uint32_t m_onTime;
        uint32_t m_offTime;
        uint32_t m_lastHB;
        bool m_state;
};

void Heartbeat::begin()
{
    pinMode(m_pin, OUTPUT);
    digitalWrite(m_pin, m_state);
    m_lastHB = millis();
}

void Heartbeat::run()
{
    switch (m_state) {
        case true:
            if ( millis() - m_lastHB >= m_onTime ) {
                m_state = false;
                m_lastHB = millis();
                digitalWrite(m_pin, m_state);
            }
            break;

        case false:
            if ( millis() - m_lastHB >= m_offTime ) {
                m_state = true;
                m_lastHB = millis();
                digitalWrite(m_pin, m_state);
            }
            break;
    }
}

// force the LED on or off. will only persist until the
// run() method changes it.
void Heartbeat::set(bool state)
{
    m_state = state;
    digitalWrite(m_pin, m_state);
}
