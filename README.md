# triggerbox

This system allows for triggering up to six relays in a specific, as precice-as-possible rhythm.
It is used for high speed photography or high speed cinema shootings,
where it can release water droplets, trigger explosions or anything that can be started with up to 30V DC.

Each relay can be tuned to start and stop being closed, starting when a button is pushed.
As in my hardware setup, each relay can hold up to 30V @ 5amps and is able to trigger any external DC powered motor, electro magnet, or the like.
The accuracy of the timing has to be determined. Theoretically you can set on / off timestamps in a microsecond fashion (0.000001s), but this is heavily influenced by the program efficiency, the relay reaction time and the microcontrollers speed. More important than the accuracy to your entered timestamps though is the repeatability. If you do not change the hardware, the system should always trigger the relays with the same lag.

This system can be triggered with a regular harwired push button, or be connected to a camera flash system or wired to two metal connectors.


## Hardware

This project needs eg. an Adafruit Metro M0 Express to wrok properly. Its program size and the demands for speed exeeds the capabilities of an arduino leonardo or UNO.

- Metro M0 Express - https://www.adafruit.com/product/3505
- TFT Touchscreen Breakout Board - HXD8357D - https://www.adafruit.com/product/2050
- 6 Solid state relays - https://www.digikey.ch/product-detail/en/sensata-crydom/CMX60D5/CC1667-ND/751911
- 6 DC/DC 15A Buck Adjustable 4-32V 12V to 1.2-32V 5V Converter Step Down Module DT - https://ebay.us/CVko1q
- Optional: 6 Generic 50KOhm potentiometer
- Optional: 6 LED Digital Voltmeter 0-100V - https://de.aliexpress.com/item/33017235960.html?spm=a2g0s.9042311.0.0.64224c4dII9GZf
- 30V power supply 8amp - https://www.components-store.com/product/MEAN-WELL/SP-240-30.html
- 5V power supply 5.0amp - https://www.components-store.com/product/MEAN-WELL/RS-25-5.html
- 7 generic LEDs
- Generic rotary encoder
- Generic push button
- Some generic resistors
- Generic AC cold appliance socket

Keep an eye on the fact that the solid state relay can switch loads from 0-30V.
Cheaper relays may only trigger loads from eg. 14V-30V, which may work for your use case.

Although this setup allows for each relay to hold 5amps, but the total of active load cannot overshoot 8 amps.
The bottleneck is the 30V power supply with only 8amps. If this is beefier, you can trigger more loads @ 5amps in parallel.
