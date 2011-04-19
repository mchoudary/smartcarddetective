/*
             LUFA Library
     Copyright (C) Dean Camera, 2010.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
 * File modified for the Smart Card Detective by Omar Choudary
 */

/*
  Copyright 2010  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the VirtualSerial demo. This file contains the main tasks of the demo and
 *  is responsible for the initial application hardware configuration.
 */

#include <util/delay.h>
#include <string.h>
#include <stdlib.h>

#include "VirtualSerial.h"
#include "../scd_io.h"

/** Contains the current baud rate and other settings of the virtual serial port. While this demo does not use
 *  the physical USART and thus does not use these settings, they must still be retained and returned to the host
 *  upon request or the host will assume the device is non-functional.
 *
 *  These values are set by the host via a class-specific request, however they are not required to be used accurately.
 *  It is possible to completely ignore these value or use other settings as the host is completely unaware of the physical
 *  serial link characteristics and instead sends and receives data in endpoint streams.
 */
CDC_LineEncoding_t LineEncoding = { .BaudRateBPS = 0,
                                    .CharFormat  = CDC_LINEENCODING_OneStopBit,
                                    .ParityType  = CDC_PARITY_None,
                                    .DataBits    = 8                            };

/** 
 * Main program entry point implemented in VirtualSerial() in scd.c
 */
//int main(void)

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	//clock_prescale_set(clock_div_1);

	/* Hardware Initialization */
	Joystick_Init();
	LEDs_Init();
	USB_Init();
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Device_Connect(void)
{
	/* Indicate USB enumerating */
	LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the USB management and CDC management tasks.
 */
void EVENT_USB_Device_Disconnect(void)
{
	/* Indicate USB not ready */
	LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host set the current configuration
 *  of the USB device after enumeration - the device endpoints are configured and the CDC management task started.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	/* Setup CDC Data Endpoints */
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPNUM, EP_TYPE_INTERRUPT, ENDPOINT_DIR_IN,
	                                            CDC_NOTIFICATION_EPSIZE, ENDPOINT_BANK_SINGLE);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_TX_EPNUM, EP_TYPE_BULK, ENDPOINT_DIR_IN,
	                                            CDC_TXRX_EPSIZE, ENDPOINT_BANK_SINGLE);
	ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_RX_EPNUM, EP_TYPE_BULK, ENDPOINT_DIR_OUT,
	                                            CDC_TXRX_EPSIZE, ENDPOINT_BANK_SINGLE);

	/* Reset line encoding baud rate so that the host knows to send new values */
	LineEncoding.BaudRateBPS = 0;

	/* Indicate endpoint configuration success or failure */
	LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
	/* Process CDC specific control requests */
	switch (USB_ControlRequest.bRequest)
	{
		case CDC_REQ_GetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Write the line coding data to the control endpoint */
				Endpoint_Write_Control_Stream_LE(&LineEncoding, sizeof(CDC_LineEncoding_t));
				Endpoint_ClearOUT();
			}

			break;
		case CDC_REQ_SetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				/* Read the line coding data in from the host into the global struct */
				Endpoint_Read_Control_Stream_LE(&LineEncoding, sizeof(CDC_LineEncoding_t));
				Endpoint_ClearIN();
			}

			break;
		case CDC_REQ_SetControlLineState:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_ClearStatusStage();

				/* NOTE: Here you can read in the line state mask from the host, to get the current state of the output handshake
				         lines. The mask is read in from the wValue parameter in USB_ControlRequest, and can be masked against the
						 CONTROL_LINE_OUT_* masks to determine the RTS and DTR line states using the following code:
				*/
			}

			break;
	}
}

/** Function to manage CDC data transmission and reception to and from the host. */
void CDC_Task(void)
{
	char*       ReportString    = NULL;
	uint8_t     JoyStatus_LCL   = Joystick_GetStatus();
	static bool ActionSent      = false;
	char        buf[18];
    uint8_t i;
    static uint8_t pos = 0;

	/* Device must be connected and configured for the task to run */
	if (USB_DeviceState != DEVICE_STATE_Configured)
	  return;

	/* Determine if a joystick action has occurred */
	if ((JoyStatus_LCL & JOY_UP) == JOY_UP)
	  ReportString = "Joystick Up\r\n";
	else if ((JoyStatus_LCL & JOY_DOWN) == JOY_DOWN)
	  ReportString = "Joystick Down\r\n";
	else if ((JoyStatus_LCL & JOY_LEFT) == JOY_LEFT)
	  ReportString = "Joystick Left\r\n";
	else if ((JoyStatus_LCL & JOY_RIGHT) == JOY_RIGHT)
	  ReportString = "Joystick Right\r\n";
	else if (JoyStatus_LCL & JOY_PRESS)
	  ReportString = "Joystick Pressed\r\n";
	else
	  ActionSent = false;

	/* Flag management - Only allow one string to be sent per action */
	if ((ReportString != NULL) && (ActionSent == false) && LineEncoding.BaudRateBPS)
	{
		ActionSent = true;

		/* Select the Serial Tx Endpoint */
		Endpoint_SelectEndpoint(CDC_TX_EPNUM);

		/* Write the String to the Endpoint */
		Endpoint_Write_Stream_LE(ReportString, strlen(ReportString));

		/* Remember if the packet to send completely fills the endpoint */
		bool IsFull = (Endpoint_BytesInEndpoint() == CDC_TXRX_EPSIZE);

		/* Finalize the stream transfer to send the last packet */
		Endpoint_ClearIN();

		/* If the last packet filled the endpoint, send an empty packet to release the buffer on
		 * the receiver (otherwise all data will be cached until a non-full packet is received) */
		if (IsFull)
		{
			/* Wait until the endpoint is ready for another packet */
			Endpoint_WaitUntilReady();

			/* Send an empty packet to ensure that the host does not buffer data sent to it */
			Endpoint_ClearIN();
		}
	}

	/* Select the Serial Rx Endpoint */
	Endpoint_SelectEndpoint(CDC_RX_EPNUM);

	/* Print received data from the host */
	if (Endpoint_IsOUTReceived())
    {
        buf[pos++] = Endpoint_Read_Byte();

        if(pos == 16)
        {
            buf[16] = '\n';
            buf[17] = 0;
            fprintf(stderr, "%s", buf);
            for(i = 0; i < 18; i++)
                buf[i] = 0;
            pos = 0;
        }
        else if(buf[pos-1] == '\n' || buf[pos-1] == '\r')
        {
            buf[pos-1] = '\n';
            for(i = pos; i < 18; i++)
                buf[i] = 0;
            fprintf(stderr, "%s", buf);
            for(i = 0; i < 18; i++)
                buf[i] = 0;
            pos = 0;
        }

        Endpoint_ClearOUT();
    }
}

/**
 * Receive a string data from the USB host
 *
 * This function will block until a string data (ended with CR, LF or CRLF)
 * is received from the USB host (the SCD is the USB device). This method
 * removes any trailing characters (CR, LF or CRLF).
 *
 * @param buf a caller-allocated buffer to store the received data
 * @param buf_len the size of the given buffer buf
 * @param len the legth of the received data
 *
 * @return zero is success, non-zero otherwise
 */
uint8_t GetHostData(char *buf, uint8_t buf_len, uint8_t *len)
{
    uint8_t pos = 0;
    uint8_t retval;

    if (buf == NULL || USB_DeviceState != DEVICE_STATE_Configured)
        return 1;

    memset(buf, 0, buf_len);

	/* Select the Serial Rx Endpoint */
	Endpoint_SelectEndpoint(CDC_RX_EPNUM);
	while(!Endpoint_IsOUTReceived());

	/* Print received data from the host */
    while(1){

        while(Endpoint_WaitUntilReady() != ENDPOINT_READYWAIT_NoError);

        retval = Endpoint_Read_Stream_LE(&buf[pos], buf_len - pos);
        if(retval != ENDPOINT_RWSTREAM_NoError && retval != ENDPOINT_RWSTREAM_Timeout)
            continue;

        pos = strlen(buf);

        if(strchr(buf, '\n') != NULL || strchr(buf, '\r') != NULL)
        {
            // remove any possible trailing chars
            if(buf[pos-1] == '\r' || buf[pos-1] == '\n')
            {
                buf[pos-1] = 0;
                pos = pos - 1;
            }
            if(buf[pos-1] == '\r' || buf[pos-1] == '\n')
            {
                buf[pos-1] = 0;
                pos = pos - 1;
            }

            *len = pos;
            break;
        }

        Endpoint_ClearOUT();
    }

    return 0;
}

/**
 * Send a string data to the USB host
 *
 * This function will transmit a string data (without adding any ending)
 * to the USB host (the SCD is the USB device)
 *
 * @param buf a caller-allocated buffer containing the data to transmit
 * @param buf_len the size of the given buffer buf
 *
 * @return zero is success, non-zero otherwise
 */
uint8_t SendHostData(char *buf, uint8_t buf_len)
{
    uint8_t full;

    if (buf == NULL || USB_DeviceState != DEVICE_STATE_Configured)
        return 1;

    /* Select the Serial Tx Endpoint */
    Endpoint_SelectEndpoint(CDC_TX_EPNUM);

    /* Write the String to the Endpoint */
    Endpoint_Write_Stream_LE(buf, buf_len);

    /* Remember if the packet to send completely fills the endpoint */
    full = (Endpoint_BytesInEndpoint() == CDC_TXRX_EPSIZE);

    /* Finalize the stream transfer to send the last packet */
    Endpoint_ClearIN();

    /* If the last packet filled the endpoint, send an empty packet to release the buffer on
     * the receiver (otherwise all data will be cached until a non-full packet is received) */
    if(full)
    {
        /* Wait until the endpoint is ready for another packet */
        Endpoint_WaitUntilReady();

        /* Send an empty packet to ensure that the host does not buffer data sent to it */
        Endpoint_ClearIN();
    }

    return 0;
}
