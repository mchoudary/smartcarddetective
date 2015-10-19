Welcome to the Smart Card Detective (SCD) open source project on Google Code.

# Introduction #

The SCD is a general framework for research and digital forensics on smartcards. It allows you to monitor any smartcard application (including Chip and PIN / EMV transactions) as well as create your custom applications (such as your own terminal or card).

# Hardware #

Notice: since 6 August 2013 the license to manufacture and commercialize the
SCD is revoked. After the current stock is exhausted, the SCD will not be sold anymore through
[Smart Architects](http://www.smartarchitects.co.uk/opencart/).

Universities or other research institutions interested in this project
should contact me directly. Please note that I shall not provide any hardware or
other details to individuals so please do not contact me with such requests.

# License #
The software is free, given under the FreeBSD (2-clause BSD) license:
http://opensource.org/licenses/BSD-2-Clause
If you require a difference license then get in touch with me.
If you do some updates to the code that are useful to other people then please consider contributing back to the project.

Tutorials and other information is available on [the forum](http://www.smartcarddetective.com/forum/).

# Details #

The SCD has started as the [MPhil thesis](http://www.cl.cam.ac.uk/~osc22/docs/mphil_acs_osc22.pdf) of Omar Choudary at University of Cambridge. In its first versions it has been used for several experiments on the EMV system.

The current hardware and software provide USB communication (based on [LUFA](http://www.fourwalledcubicle.com/LUFA.php)), comprehensive log capabilities and python tools for easy interaction with the SCD (the "Virtual Serial" application).

The device has a smartcard interface as well as a terminal/reader interface, allowing the SCD to act as a passive/active monitor between a card and a reader or emulate a card or a terminal.

Using the python command line interface you can interact with the SCD using a PC. This allows a more flexible operation, although the SCD also features many stand-alone applications. With a battery you can use the SCD without a PC.