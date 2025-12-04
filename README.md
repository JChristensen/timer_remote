# Remote WiFi Timer
https://github.com/JChristensen/timer_remote  
README file  
Jack Christensen  
Oct-2025

## License
Remote WiFi Timer Copyright (C) 2025 Jack Christensen GNU GPL v3.0

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License v3.0 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/gpl.html>

## Overview
This is the firmware for the remote units controlled by the timer_main program (see link below.)

Each remote unit communicates via MQTT with the main program, which manages all the schedules. AC power is controlled by a relay. The PCB is designed to handle a load of 5 amperes or less. A second, optional relay is meant to control low voltages only (e.g. 12VDC).

There are two modes of operation, automatic or manual. In automatic mode, the schedule managed by the main program applies. However, using the pushbutton, the load can be turned on or off at any time, temporarily overriding the schedule. When the next schedule time occurs, the corresponding on or off command is sent to the remote and it will switch the load accordingly.

Manual mode is enabled by a long press on the pushbutton. In manual mode, the remote will not respond to on or off commands sent from the main program, but only to the pushbutton, until such time as automatic mode is enabled again by a long press.

This firmware was developed for Raspberry Pi Pico W (or 2W) microcontrollers, using [Earle Philhower's Arduino-Pico core](https://github.com/earlephilhower/arduino-pico).

## See also
[Main control program.](https://github.com/JChristensen/timer_main)  
[PCB for the remote units.](https://github.com/JChristensen/remote_wifi_timer)  
[A small PCB for the LEDs and pushbutton.](https://github.com/JChristensen/ac-timer-panel)