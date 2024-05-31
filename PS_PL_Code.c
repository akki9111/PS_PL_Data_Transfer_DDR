/******************************************************************************
*
* Copyright (C) 2009 - 2014 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* Use of the Software is limited solely to applications:
* (a) running on a Xilinx device, or
* (b) that interact with a Xilinx device through a bus or interconnect.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

/*
 * helloworld.c: simple test application
 *
 * This application configures UART 16550 to baud rate 9600.
 * PS7 UART (Zynq) is not initialized by this application, since
 * bootrom/bsp configures it to baud rate 115200
 *
 * ------------------------------------------------
 * | UART TYPE   BAUD RATE                        |
 * ------------------------------------------------
 *   uartns550   9600
 *   uartlite    Configurable only in HW design
 *   ps7_uart    115200 (configured by bootrom/bsp)
 */

#include <stdio.h>
#include "platform.h"
#include "xil_printf.h"
#include "xil_io.h"
#include "xscugic.h"
#include "xparameters.h"


void StartDMATransfer ( u32 dstAddress, u32 len ) {

	// write destination address to S2MM_DA register
	Xil_Out32 ( XPAR_AXI_DMA_0_BASEADDR + 0x48 , dstAddress );

	// write length to S2MM_LENGTH register
	Xil_Out32 ( XPAR_AXI_DMA_0_BASEADDR + 0x58 , len );

}


//Interrupt Controller
XScuGic InterruptController;
static XScuGic_Config *GicConfig;
u32 global_frame_counter = 0;

//Interrupt Handling

void InterruptHandler ( void ) {

	// if you have a device, which may produce several interrupts one after another, the first thing you should do is to disable interrupts, but axi dma is not this case.
	u32 tmpValue;

	// xil_printf("Interrupt acknowledged.\n\r");

	// clear interrupt. just perform a write to bit no. 12 of S2MM_DMASR
	tmpValue = Xil_In32 ( XPAR_AXI_DMA_0_BASEADDR + 0x34 );
	tmpValue = tmpValue | 0x1000;
	Xil_Out32 ( XPAR_AXI_DMA_0_BASEADDR + 0x34 , tmpValue );

	// Data is in the DRAM ! do your processing here !

	global_frame_counter++;
	if ( global_frame_counter > 10000000 ) {
		xil_printf ( "Frame number : %d \n\r", global_frame_counter );
		return;

	// initiate a new data transfer
	// StartDMATransfer ( 0xa000000 + 128 * global_frame_counter, 512 );
		StartDMATransfer ( 0x0a000000 , 65356 );

	}

}

int SetUpInterruptSystem( XScuGic *XScuGicInstancePtr )
{
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT , (Xil_ExceptionHandler) XScuGic_InterruptHandler , XScuGicInstancePtr);
	Xil_ExceptionEnable();		// enable interrupts in ARM
	return XST_SUCCESS;
}

int InitializeInterruptSystem ( deviceID ) {
	int Status;

	GicConfig = XScuGic_LookupConfig ( deviceID );
	if ( NULL == GicConfig ) {
		return XST_FAILURE;
	}

	Status = XScuGic_CfgInitialize ( &InterruptController, GicConfig, GicConfig->CpuBaseAddress );
	if ( Status != XST_SUCCESS ) {
		return XST_FAILURE;
	}

	Status = SetUpInterruptSystem ( &InterruptController );
		if ( Status != XST_SUCCESS ) {
			return XST_FAILURE;
		}

	Status = XScuGic_Connect ( &InterruptController,
			XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR,
			(Xil_ExceptionHandler)InterruptHandler,
			NULL );
	if ( Status != XST_SUCCESS ) {
		return XST_FAILURE;
	}

	XScuGic_Enable (&InterruptController, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR);

	return XST_SUCCESS;

}



// Initialize AXI DMA..
int InitializeAXIDma(void) {

	u32 tmpVal;
	//S2MM DMACR.RS = 1
	tmpVal = Xil_In32 ( XPAR_AXI_DMA_0_BASEADDR + 0x30 );
	tmpVal = tmpVal | 0x1001;
	Xil_Out32 ( XPAR_AXI_DMA_0_BASEADDR + 0x30 , tmpVal );
	tmpVal = Xil_In32 ( XPAR_AXI_DMA_0_BASEADDR + 0x30 );
	xil_printf ( "Value for DMA control register : %x\n\r", tmpVal );

	return 0;

}

// Enable SampleGenerator
int EnableSampleGenerator( u32 numberOfWords) {

	// set the gpios direction as output
	// the gpio is by default output, so this is not needed

	// set the value for frame size
	Xil_Out32 ( XPAR_AXI_GPIO_0_BASEADDR, numberOfWords );

	// enable the sample generator
	Xil_Out32 ( XPAR_AXI_GPIO_1_BASEADDR, 1 );

}


int main()
{
    init_platform();

    //
    xil_printf("Initialize AXI DMA..\n\r");
    InitializeAXIDma();

    // enable sample generator
    // end of frame will come after 128 bytes (32 words) are transferred
    xil_printf("Setting up Sample Generator..\n\r");
    EnableSampleGenerator(32);

    // set the interrupt system and interrupt handling
    // interrupt
    xil_printf("enabling the interrupt handling system..\n\r");
    InitializeInterruptSystem( XPAR_PS7_SCUGIC_0_DEVICE_ID );

    // start the first dma transfer
    xil_printf ("performing the first dma transfer ... press a key to begin ...\n\r");
    getchar();
    StartDMATransfer ( 0x0a000000, 65356 );




    //int *ptr = 0xa000008;
    //xil_printf("Data is %0x\n\r", *ptr);

    u32 i;
    for(i=0;i<16384;i=i+1)
    {
    	u32 *ptr = 0x0a000000 + (4*i);
    	xil_printf("Data at address: %0x = %d\n\r", ptr, *ptr);
    }

    xil_printf("Hello World\n\r");
    xil_printf("Successfully ran Hello World application");
    cleanup_platform();

    return 0;
}
