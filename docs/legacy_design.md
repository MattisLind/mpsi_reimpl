# mpsi_reimpl
This is a re-implementation of the MPSI project by Brent Hilpert

Brent Hilpert has created a [combination of hardware and software that allows a HP 9830](http://madrona.ca/e/HP9830/mpsi/index.html) 
to transfer data back a forth a modern host computer. It is based on several TTL chips and a serial link to a host computer. 
~~The ideas here is to reimplement the same type of system while integrating a Raspbery Pi Zero W on board as well as 
susbstituting most of the glue logic with a CPLD. Then also to create a KiCAD layout for this and a 3D model for the box to 
printed in a 3D printer.
Instead of using a serial protocol the intention is to use a prallell protocol since we have enough pins on the Raspberry Pi Zero W. 
Input data is latched in two 74LVC574 which also give 5V to 3.3V conversion. Outputs will still be 4 pieces of 74LS38 open collector drivers.
One more extra 74LVC245 is added to make it possible to have a dipswitch on the board. The dipswitch can then be enabled onto the bus
and provide selection of various software of tape images to serve from the Raspberry Pi Zero W. The remaining glue logic will be implemented 
in one single Atmel ATF1502 CPLD programmed in CUPL. This will give a compact layout of the card while still use mostly DIP socketed 
chips to ease soldering among hobbyists.
As of now a sketch of the schematic has been created as well as a first draft of the CUPL code.~~

I have changed my mind concerning ATMEL. The only free tool to use with ATMEL is the WinCUPL program. WinCUPL is OLD. Very OLD! And crashes very often. Simulating is almost impossible. I decided to use Xilinx ISE suite instead and use the more modern XC2C32A CPLD. The design fits easily in this CPLD.

I also reconsidered the use of Raspi Zero. Although it might be easier to port the code Raspi Zero I think that the STM32F407VET chip is a better match. Abundant GPIOs. SD card reader. USB IO integrated. I2C.

With this I could replace all hard wired jumpers of the original design with software controlled settings. The IO addresses and interrupt ID could be set programatically.

To control the thing I envision a small control device connected over I2C. This control device will the contain a few buttons, a few LEDs and a small I2C display. Connected over a simple four wire cable.

The SD Card would contain a FAT file system that contain a configuration file and then all all the paper tape files and cassette tape files that one would like to make available to the HP9830. The small user interface control device would then be used to select among those files as well as set up the configuration of the mpsi device, io addresses and interrupt ids.

USB can be used as a virtual serial port and connect to host terminal program for printer output. 

Two 74LVC574 is used to latch data and control signals from the HP9830. Five 74LS38 OC drviers are used to drive the bus. One Xilinx XC3C32A CPLD is used to replace all glue logic. A STM32F407VET6 SoC is used for managing all data flows. a SD card connector and USB port is needed and some small components.

![Schematic](https://raw.githubusercontent.com/MattisLind/mpsi_reimpl/master/MPSIBOARD/MPSIBOARD.png)
