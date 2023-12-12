## Description
Authors: Cole Carley and Madeline Veric
This project is a swamp cooler implemented on an Arduino MEGA. 


## Overview of Design
This swamp cooler uses the components listed below to create a system that can be used to cool a room. The system is controlled by an Arduino MEGA and has a user interface that allows the user to move the vent, disable and enable the cooler, and reset the system when in an error state. There is an LCD screen attached that notifies the user of the current water level, temperature, and humidity in the room. The system will start running when the temperature is over 23 degrees celsius, and return to and idle state when the temperature dips below that threshold. When there is not enough water to cool the room, the system will enter an error state and notify the user. Functionality can be restored by refilling the water tank and resetting the system. A user can turn the system on and off using a button on the breadboard.

### Constraints
ERROR STATE: RED LED

RUNNING STATE: BLUE LED

IDLE STATE: GREEN LED

DISABLED STATE: YELLOW LED


The system will automatically enter a running state when the room temperature is above 23 degrees celsius. The water is measured in an arbitrary unit, and will enter an error state when it dips below 100 units. The system can be in a disabled state if the user turns it off using the button on the breadboard. If none of the previous states are active, the system will be in an idle state, just displaying the temperature, humidity, and water level.

## Github
https://github.com/colecarley/swamp_cooler/blob/main/main/main.ino

## Components
- [Arduino MEGA Controller Board](https://docs.arduino.cc/resources/datasheets/A000067-datasheet.pdf)
- [LCD1602 Module](https://www.waveshare.com/datasheet/LCD_en_PDF/LCD1602.pdf)
- [Stepper Motor](https://www.mouser.com/datasheet/2/758/stepd-01-data-sheet-1143075.pdf)
- [ULN2003 Stepper Motor Driver Module](https://www.electronicoscaldas.com/datasheet/ULN2003A-PCB.pdf)
- [Water Level Detection Sensor Module](https://curtocircuito.com.br/datasheet/sensor/nivel_de_agua_analogico.pdf)
- [DS1307 RTC Module](https://www.analog.com/media/en/technical-documentation/data-sheets/ds1307.pdf)
- [DHT11 Temperature and Humidity Module](https://www.mouser.com/datasheet/2/758/DHT11-Technical-Data-Sheet-Translated-Version-1143054.pdf)
- [L293D](https://www.ti.com/lit/ds/symlink/l293.pdf)
- [Fan Blade and 3-6V Motor](https://wiki-content.arduino.cc/documents/datasheets/DCmotor6_9V.pdf)
- Potentiometer 10K
- 10K Resistor 3
- 330 Resistor 5
- Red LED
- Yellow LED
- Blue LED
- Green LED

## Schematic
![](https://i.imgur.com/LZizyra.png)

## Video
![video demonstration](https://www.youtube.com/watch?v=gsJyTYYRlOk&ab_channel=ColeCarley)

## Photos
![](https://i.imgur.com/NUT4M3L.jpg)

![](https://i.imgur.com/2aZ6v2r.jpg)

![](https://i.imgur.com/93MwZ59.jpg)

![](https://i.imgur.com/89fDgaj.jpg)

![](https://i.imgur.com/KrJWxsu.jpg)

![](https://i.imgur.com/8XnFwTF.jpg)
