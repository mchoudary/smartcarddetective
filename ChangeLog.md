# ChangeLog (first item is newest) #


---

2 May 2011

In [r26](https://code.google.com/p/smartcarddetective/source/detail?r=26) there are many new features added:
> - python command line interface using the serial (or USB virtual serial) port to connect the SCD. This includes many options to interact with the SCD from the PC.
> - USB communication added using the LUFA library. Based on this I created a virtual serial port application and extra functionality.
> - Enhanced the terminal application to support logging.


---

19 April 2011

In [r15](https://code.google.com/p/smartcarddetective/source/detail?r=15) I added some python scripts (see the new tools/ folder) to process  SCD-EMV traces retrieved from the AVR's EEPROM.


In [r8](https://code.google.com/p/smartcarddetective/source/detail?r=8) I added the LUFA USB library (http://www.fourwalledcubicle.com/LUFA.php) to the SCD project. There is now a Virtual Serial Port application.

Connect the SCD to a PC/Host via usb, select the Virtual Serial application from the menu and then communicate from the PC/host to the SCD using a terminal (tty, serial emulator). On the host side I can warmly recommend
gtkterm which can be used for example in Ubuntu as follows:
`gtkterm -p /dev/ttyACM0`
(if ttyACM0 is the virtual serial port in your system). For more information please refer to this link:
http://code.google.com/p/micropendous/wiki/SerialPortUsageLinux

Also now the targets scd.hex and scd-DFU.hex (includes the DFU bootloader) have been added to the source code, so that you can program the SCD without the need to build the code (in case you are in a hurry). Even more, now the Makefile has been updated to automatically create the scd-DFU.hex file, which contains the compiled application as well as the DFU bootloader so you don't need to manually append the hex file anymore.


---
