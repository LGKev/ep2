/*
 * main.c
 * Author: Kevin Kuwata
 * Date: 5/8/18
 *
 * This file is to be paired with the hardware V2 watch. To develop the device, I will be using the launchpad as the main platform.
 * This means that the pins for the ePaper are going to be the pins that are readily available on the launchpad and not the PCB.
 * Differentiate which code by using the #defines PCB_FW and Launchpad_FW
 */
#include "msp.h"
#include "ePaper.h"
#include "gfx.h" //required, but gfx.h is excluded?




/* == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == *//* == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == */
/* == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == *//* == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == */
//                                                          MAIN.c
/* == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == *//* == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == */
//* == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == *//* == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == == */
void main(void)
{
	WDT_A->CTL = WDT_A_CTL_PW | WDT_A_CTL_HOLD;		// stop watchdog timer


	coord_t width, height;
	coord_t i,j;

	__enable_interrupts();

	gfxInit();

	width = gdispGetWidth();
	height =gdispGetHeight();

	gdispDrawBox(10,10, width/2, height/2, Black);

	for(i =5; j = 0; i < width && j < height; i+=7, j +=i/20){
	    gdispDrawPixel(i,j, Black);
	}

	while(1);
}



/*============================================================================================================*/
/*============================================================================================================*/
/*============================================================================================================*/



/* ================================================================ */
//                              ISRs
/* ================================================================ */
/* ================================================================ */

void EUSCIB3_IRQHandler(void)
{
    if(EUSCI_B3_SPI->IFG & EUSCI_B_IFG_TXIFG){
        EUSCI_B3_SPI->IFG &= ~EUSCI_B_IFG_TXIFG;
    }
}



