/*
 * usb_manager.c
 *
 * Copyright (C) 2012-2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 *
 *
 *  Created on: 26.10.2012
 *      Author: Julian Schmidt
 */
#include "Soundengine/USB/usb_manager.h"
#include <string.h>
#include "usbd_cdc_if.h"




enum {
	USB_NOT_DETECTED,
	USB_DETECTED,
};
//USBD_HandleTypeDef           USB_OTG_dev;
//-------------------------------------------------------------------------------
#if 0
void usb_init()
{
#if HAL_USB
	  if (USBD_Init(&hUsbDeviceFS, &FS_Desc, DEVICE_FS) != USBD_OK)
	  {
	    Error_Handler();
	  }
	  if (USBD_RegisterClass(&hUsbDeviceFS, &USBD_CDC) != USBD_OK)
	  {
	    Error_Handler();
	  }
	  if (USBD_CDC_RegisterInterface(&hUsbDeviceFS, &USBD_Interface_fops_FS) != USBD_OK)
	  {
	    Error_Handler();
	  }
	  if (USBD_Start(&hUsbDeviceFS) != USBD_OK)
	  {
	    Error_Handler();
	  }
#else

	  usb_start();
#endif
}
//-------------------------------------------------------------------------------
void usb_stop()
{
	//stop usb port
	USBD_DeInit(&USB_OTG_dev);
}
#endif
//-------------------------------------------------------------------------------
void usb_tick()
{
	usb_flushMidi();
}
//-------------------------------------------------------------------------------
#if 0
void usb_start()
{
	//zero init the usb receive buffer
	memset(usb_MidiMessages,0, sizeof(MidiMsg)*USB_MIDI_INPUT_BUFFER_SIZE);

	//start usb port
	 USBD_Init(&USB_OTG_dev,
	            &USR_desc,
				DEVICE_FS);

	 if (USBD_RegisterClass(&USB_OTG_dev, &MIDI_cb) != USBD_OK)

	 // if (USBD_CDC_RegisterInterface(&USB_OTG_dev, &USBD_Interface_fops_FS) != USBD_OK)
	  {
		//Error_Handler();
	  }
	  if (USBD_Start(&USB_OTG_dev) != USBD_OK)
	  {
		//Error_Handler();
	  }
}
#endif
//-------------------------------------------------------------------------------
/*
 * writes a byte to the Usb Tx buffer
 * Will be send out by the usb_flushMidi() call
 */
void usb_sendByte(uint8_t byte)
{

	*usb_MidiInWrPtr++ = byte;
	if(usb_MidiInWrPtr >= (usb_MidiInBuff+(TOTAL_MIDI_BUF_SIZE * NUM_SUB_BUFFERS)))
	{
		usb_MidiInWrPtr = usb_MidiInBuff;
	}

}
//------------------------------------------------------------------------------
/*
 * flush FIFO and send out next block of messages stored in the usb_MidiInBuff
 * called periodically by usb_tick()
 * TODO:  maybe there is a better way to call this automatically by the USB driver when it is ready to TX some more data?
 * probably using the USBD_CDC_TransmitPacket() callback
 */
void usb_flushMidi()
{

	//(was (uint8_t)(Wr - Rd): after the write pointer wrapped this sent up to 255
	//bytes from past the buffer end, and data was dropped when the USB was busy)
	uint8_t* const end = usb_MidiInBuff + (TOTAL_MIDI_BUF_SIZE * NUM_SUB_BUFFERS);
	if(usb_MidiInWrPtr == usb_MidiInRdPtr)
		return;

	//up to the write pointer, or up to the buffer end if it wrapped (the rest is sent next time)
	uint8_t* const stop = usb_MidiInWrPtr > usb_MidiInRdPtr ? usb_MidiInWrPtr : end;
	if(CDC_Transmit_FS(usb_MidiInRdPtr, (uint16_t)(stop - usb_MidiInRdPtr)) != USBD_OK)
		return;	//busy or not connected: try again next time

	usb_MidiInRdPtr = stop >= end ? usb_MidiInBuff : stop;

}
//------------------------------------------------------------------------------
void usb_sendMidi(MidiMsg msg)
{
	//MIDI byte[0] = 4 bits cable number + 4 bits event type code
	usb_sendByte(msg.status>>4);

	//then the 3 midi message bytes
	//USB MIDI msg has to be ALWAYS 4 bytes long
	//even if it is just a single clock byte
	usb_sendByte(msg.status);
	usb_sendByte(msg.data1);
	usb_sendByte(msg.data2);
}
//-------------------------------------------------------------------------------
uint8_t usb_getMidi(MidiMsg* msg)
{
	if(usb_MidiMessagesRead != usb_MidiMessagesWrite)
	{
		//we have unprocessed messages in the queue
		*msg = usb_MidiMessages[usb_MidiMessagesRead];

		usb_MidiMessagesRead++;
		usb_MidiMessagesRead &= USB_MIDI_INPUT_BUFFER_MASK;

		return 1; //return OK
	}
return 0; //no messages
}

