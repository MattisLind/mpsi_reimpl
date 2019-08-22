# mpsi_reimpl
This is a re-implementation of the MPSI project by Brent Hilpert

Brent Hilpert has created a [combination of hardware and software that allows a HP 9830](http://madrona.ca/e/HP9830/mpsi/index.html) 
to transfer data back a forth a modern host computer. It ias based on several TTL chips and a serial link to a host computer. 
The ideas here is to reimplement the same type of system while integrating a Raspbery Pi Zero W on board as well as 
susbstituting most of the glue logic with a CPLD. Then also to create a KiCAD layout for this and a 3D model for the box to 
printed in a 3D printer.

Instead of using a serial protocol the intention is to use a prallell protocol since we have enough pins on the Raspberry Pi Zero W. 
Input data is latched in two 74LVC574 which also give 5V to 3.3V conversion. Outputs will still be 4 pieces of 74LS38 open collector drivers.
One more extra 74LVC245 is added to make it possible to have a dipswitch on the board. The dipswitch can then be enabled onto the bus
and provide selection of various software of tape images to serve from the Raspberry Pi Zero W. The remaining glue logic will be implemented 
in one single Atmel ATF1502 CPLD programmed in CUPL. This will give a compact layout of the card while still use mostly DIP socketed 
chips to ease soldering among hobbyists.

As of now a sketch of the schematic has been created as well as a first draft of the CUPL code.
