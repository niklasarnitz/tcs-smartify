# TCS-Smartify

This Arduino project uses an ESP8266 to connect a TCS 2-wire doorbell system to a MQTT broker. This can then be used in a Home Automation Software like Home Assistant.

## Hardware
You need the following parts to build this TCS-Bus-Monitor:
- ESP8266 WiFi enabled Microcontroller (I'm using a Wemos D1 mini)
- Some sort of level shifter (It has to shift 24V to 3.3V; I'm using a simple voltage divider)

If you want to permanently install this in your circuit breaker box like I did, you will also need some sort of powersupply.
(I'm using a 10 Euro 5V DIN PSU from Amazon.)

The TCS bus is operating on 24V and the polarity is `a: positive` and `b: negative`. (Sometimes there also is a `p` line. This is just another power line. But it is propably unsafe to use, because it is only made for very small currents.)

## Software Setup
After connecting your level shifting contraption to the specified input (By default `busInput` is defined as `14` which is `D0` on the Wemos D1 Mini), you can flash the Arduino Sketch after changing the settings to your preference. (WiFi & MQTT credentials, MQTT topics).

Then open up the serial monitor or a MQTT Client and have a look at the command topic or whats printed on the serial console when you ring your doorbell.
It should be a numeric value like `223391872`. Shorter ones are unlikely. (Some of the short commands are documented below)
When you have your numeric value, you can then change the `RING_CMD` statement to your value and reflash.

You can now read a boolean on the `MQTT_RING_TOPIC` MQTT topic to determine if the doorbell should ring or not. This can then be used with Software like Home Assistant to send a notification to your phone.

## Known Bus Commands
| Command | Use                  |
|---------|----------------------|
| 4608    | Toggle Light         |
| 8448    | End Speaking         |
| 9344    | Unknown              |
| 8832    | Stopped Ringing?     |
| 12416   | Turn on Speak light? |

## Credits
TCS-Smartify is based on the awesome work of [Syralist](https://github.com/Syralist/tcs-monitor) and [atc1441](https://github.com/atc1441/TCSintercomArduino).
Thank you for your hard work on documenting the TCS Bus!