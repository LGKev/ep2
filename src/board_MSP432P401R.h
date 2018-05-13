/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 *
 *     @Note: Future Kevin: this board file was created by them, but I modify it for my mcu and spi driver I have. UL3820
 *      compare this to CPU20's forum post, where he has board_2s9epd_example.h
 */

#ifndef _GDISP_LLD_BOARD_H
#define _GDISP_LLD_BOARD_H

// Include msp
#include "msp.h"
#include "epaper.h" //eventually will rename. all the defines for LUT and commands

static void configureePaperPins(void)
{
 //probably delete since init_board is just like my function initEpaper()
}

static GFXINLINE void init_board(GDisplay* g)
{
	(void) g;

	/* Configure gpio and spi module  B3 */
	/*
	 *  Port 10 for the LaunchPad
	 *  GPIO        10.0        =        Data Command pin, Data is HIGH     == Green Ch5 on Logic Analyzer (LA)
	 *  PRIMARY     10.1        =        SCK         ==          yellow wire connected to channel 3 on Logic Analyzer
	 *  PRIMARY     10.2        =        MISO        ==          BLUE - logic analyzer Ch. 8
	 *  GPIO        10.3        =        CS          ==          ORANGE, channel 4 on Logic analyzer
	 *
	 *  GPIO 7.1    =   ePaper RESET     =      WHITE, CH7 LA //NOTE active LOW
	 *  GPIO 7.2    = ePaper BUSY        =      PURPLE, CH4   //NOTE active HIGH
	 *
	 *  SPI module on UCB3
	 *  Polarity: rising edge
	 *  clock is low when idle
	 *  master only, and STE is not used
	 */

	       /* configure spi bus  B3*/
	       EUSCI_B3_SPI->CTLW0 |= UCSWRST; // set to a 1, unlock
	       EUSCI_B3_SPI->CTLW0 &= ~(UCCKPL  | UC7BIT | UCMODE0   ); // polarity:0, phase:0, 8 bits, spi mode (vs i2c)
	       EUSCI_B3_SPI->CTLW0 |= (UCCKPH | UCMSB | UCMST |  UCSYNC | UCSSEL__SMCLK); // MSB, master, sync (vs uart), system clock : 3Mhz

	       /* configure the pins */
	          P7SEL0 &=~(BIT1 | BIT2);        // busy and reset
	          P7SEL1  &= ~(BIT1 | BIT2);

	          //pull down on BUSY
	          P7REN |= BIT2;
	          P7OUT &= ~BIT2; // pulldown

	          P10SEL0 &=~(BIT0 | BIT3); // D/C and CS
	          P10SEL1 &=~(BIT0 | BIT3);

	          P10SEL0 |= (BIT1 | BIT2); //primary mode for MISO and SCK
	          P10SEL1 &= ~(BIT1 | BIT2);

	          P7DIR &= ~(BIT2); //busy is an input
	          P7DIR |= BIT1;
	          P10DIR |= BIT0 | BIT1 | BIT2 | BIT3;

	          P10OUT |= (BIT0 | BIT1 | BIT2 | BIT3); // set all high according to data sheet 21/29 good display
	          P7OUT &= ~(BIT2);   //busy is active high!
	          P7OUT |= (BIT1);      //reset is active low!

	          //may need pull down resistor on the Busy pin, and pull up on the RESET

	          EUSCI_B3_SPI->CTLW0 &= ~UCSWRST; // set to a 0 lock

	          //enable interrupt for the TX
	          EUSCI_B3_SPI ->IFG = 0;     //clear any interrupts
	          EUSCI_B3_SPI -> IE |= UCTXIE;

	          NVIC_EnableIRQ(EUSCIB3_IRQn); // TODO do i make another .c file with the interrupt routine in it? like STM did


	          /* CS HIGH to disable display */
	          P10OUT |= (BIT3); // set all high according to data sheet 21/29 good display
}


static GFXINLINE void acquire_bus(GDisplay *g) {
    (void) g;
    P10OUT &= ~BIT3;     // CS low
}

static GFXINLINE void release_bus(GDisplay *g) {
    (void) g;
    P10OUT |= BIT3;     //cs high
}

static GFXINLINE void write_data(GDisplay *g, uint8_t data) {
    (void) g;

    /* Wait for the Busy pin to go low. */
    while(P7IN & BIT2);

    /* wait for spi to be ready */
 while(EUSCI_B3_SPI->IFG & UCTXIFG);
    P10OUT |=   BIT0;     // D/C
    P10OUT &= ~BIT3;     // CS
    EUSCI_B3_SPI->TXBUF = data;
    P10OUT |= BIT3;
}


static GFXINLINE void write_cmd(GDisplay *g, uint8_t command){
  (void) g;
  /* Wait for the Busy pin to go low. */
  while(P7IN & BIT2);
  /* wait for spi driver to be ready */
  while(EUSCI_B3_SPI->IFG & UCTXIFG);
  P10OUT &= ~BIT0;     // D/C
  P10OUT &= ~BIT3;     // CS
  EUSCI_B3_SPI->TXBUF = command;
  P10OUT |= BIT3;

}


#endif /* _GDISP_LLD_BOARD_H */
