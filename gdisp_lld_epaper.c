/*
 * This file is subject to the terms of the GFX License. If a copy of
 * the license was not distributed with this file, you can obtain one at:
 *
 *              http://ugfx.org/license.html
 */

#include "gfx.h"

//#if GFX_USE_GDISP //TODO: uncomment this.

#define GDISP_DRIVER_VMT			GDISPVMT_EPAPER
#include "gdisp_lld_config.h"
#include "../../../src/gdisp/gdisp_driver.h"

#include "board_MSP432P401R.h" //TODO still need to create this?? no i think this is right
#include "epaper.h" //defines for commands/ LUT

#include <string.h>   // for memset

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#ifndef GDISP_SCREEN_HEIGHT
	#define GDISP_SCREEN_HEIGHT			200
#endif
#ifndef GDISP_SCREEN_WIDTH
	#define GDISP_SCREEN_WIDTH		    200
#endif

/* Every data byte determines 8 pixels. */
#ifndef EPD_PPB
  #define EPD_PPB   8
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/


/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * As this controller can't update on a pixel boundary we need to maintain the
 * the entire display surface in memory so that we can do the necessary bit
 * operations. Fortunately it is a small display in monochrome.
 * 64 * 128 / 8 = 1024 bytes.
 */

LLDSPEC bool_t gdisp_lld_init(GDisplay *g) {
	// The private area is the display surface.
	g->priv = gfxAlloc(GDISP_SCREEN_HEIGHT/8 * GDISP_SCREEN_WIDTH);
	if (!g->priv) {
	    return FALSE;
	}

	  /* Initialize the LL hardware. */
	  init_board(g);

	  acquire_bus(g);

	   /* reset epaper 10ms */
	   uint16_t delay = 0;
	   P7OUT &=~BIT1;
	   for(delay = 0; delay < 12000; delay++); //TODO; replace wthis with gfxsleepmilliseconds()?
	   P7OUT |= BIT1;
	   for(delay = 0; delay < 12000; delay++); //double check with LA to see if equal or greater than 10mS

	 /* ePaper Init Sequence From DataSheet Lots of Magic Numbers */
	  sendCommand(g, CMD_DRIVER_OUTPUT_CONTROL);
	  sendData(g, (LCD_VERTICAL_MAX - 1) & 0xFF);
	  sendData(g, ((LCD_HORIZONTAL_MAX - 1) >> 8) & 0xFF);
	  sendData(g, 0x00);                     // GD = 0; SM = 0; TB = 0;
	  sendCommand(g, CMD_BOOSTER_SOFT_START_CONTROL);
	  sendData(g, 0xD7);
	  sendData(g, 0xD6);
	  sendData(0x9D);
	  sendCommand(g, CMD_VCOM);
      sendData(g, 0xA8);                     // VCOM 7C
	  sendCommand(g, CMD_DUMMY_LINE);
	  sendData(g, 0x1A);                     // 4 dummy lines per gate
	  sendCommand(g, CMD_GATE_TIME); // the busy line is being set high here.
	  sendData(g, 0x08);                    // 2us per line
	  sendCommand(g, CMD_DATA_ENTRY);
	  sendData(g, 0x03);                     // X increment; Y increment


    /* Send LUT - more magic numbers provided by vendor */
      setLUT(g, lut_full_update);

      release_bus(g);

      /* Initialise the GDISP structure */
       g->g.Width = GDISP_SCREEN_WIDTH;
       g->g.Height = GDISP_SCREEN_HEIGHT;
       g->g.Orientation = GDISP_ROTATE_0;
       g->g.Powermode = powerOn;
       return TRUE;
} /* end of init */


static void setMemoryPointer(GDisplay *g) { /* was set_cursor */

    switch(g->g.Orientation) {
        default:
        case GDISP_ROTATE_0:
        sendCommand(g, CMD_X_COUNTER);
        sendData(g, (g->p.x >> 3) & 0xFF);
        sendCommand(g, CMD_Y_COUNTER);
        sendData(g, g->p.y & 0xFF);
        sendData(g, (g->p.y >> 8) & 0xFF);
            break;
        case GDISP_ROTATE_180:
        sendCommand(g, CMD_X_COUNTER);
        sendData(g, g->p.x / EPD_PPB);
        sendCommand(g, CMD_Y_COUNTER);
        sendData(g, g->p.y & 0xFF);
        sendData(g, (g->p.y >> 8) & 0xFF);
            break;
        case GDISP_ROTATE_90:
        case GDISP_ROTATE_270:
        sendCommand(g, CMD_Y_COUNTER);
        sendData(g, g->p.x / EPD_PPB);
        sendCommand(g, CMD_X_COUNTER);
        sendData(g, g->p.y & 0xFF);
        sendData(g, (g->p.y >> 8) & 0xFF);
            break;
    }
}


static void setMemoryArea(GDisplay *g) { /* was set view port*/
 /*
  uint8_t dataX[2];
  uint8_t dataY[4];

  // Fill up the X and Y position buffers.
  dataX[0] = g->p.x / WS29EPD_PPB;
  dataX[1] = (g->p.x + g->p.cx - 1) / WS29EPD_PPB;

  dataY[0] = g->p.y % 256; // Y-data is 9-bits so send in two bytes
  dataY[1] = g->p.y / 256;
  dataY[2] = (g->p.y + g->p.cy - 1) % 256;
  dataY[3] = (g->p.y + g->p.cy - 1) / 256;
*/

    /*
     *    I am assuming that p.y is the start and p.cy is the end
     *    I have like 0 faith this will even compile
     *
     *    I am  not sure if the above and below codes are equal
     *    we have an offset of -1 and yea div 8? i think div 8 is right bc right shift 3 >>3
     *
     *    oh the minus 1 is probably from the original prototype i had where I was passing values so i should probably -1
     * */
  uint8_t x_start = g->p.x / EPD_PPB; //i think the x_start is always 0,0
  uint8_t y_start = g->p.y;
  uint8_t x_end = g->p.cx;
  uint8_t y_end = g->p.cy;



    switch(g->g.Orientation) {
        default:
        case GDISP_ROTATE_0:
           sendCommand(g, CMD_X_ADDR_START);
           sendData(g, (x_start >> 3) & 0xFF);
           sendData(g, (x_end >> 3) & 0xFF); //>>3 is div 8!
           sendCommand(g, CMD_Y_ADDR_START);
           sendData(g, y_start & 0xFF);
           sendData(g, (y_start >> 8) & 0xFF);
           sendData(g, (y_end - 1) & 0xFF);
           sendData(g, ((y_end -1) >> 8) & 0xFF);
           while(P7IN & BIT2); //wait for busy line to go low      // waitNotBusy();
           break;
        case GDISP_ROTATE_180:
            sendCommand(g, CMD_X_ADDR_START );
            sendData(g, (x_start >> 3) & 0xFF);
            sendData(g, (x_end -1 >> 3) & 0xFF); //>>3 is div 8!
            sendCommand(g, CMD_Y_ADDR_START);
            sendData(g, y_start & 0xFF);
            sendData(g, (y_start >> 8) & 0xFF);
            sendData(g, (y_end -1)  & 0xFF);
            sendData(g, ((y_end - 1) >> 8 ) & 0xFF); //todo check this minus 1 if screen is garbage i think this makes sense b/c i was passing in variables first
            //         setMemoryArea(0, 0, LCD_HORIZONTAL_MAX- 1, LCD_VERTICAL_MAX - 1); so -1 happens first then the shift.
          break;
        case GDISP_ROTATE_90:
        case GDISP_ROTATE_270:
            sendCommand(g, CMD_X_ADDR_START);
            sendData(g, y_start & 0xFF);
            sendData(g, (y_start >> 8) & 0xFF);
            sendData(g, (y_end - 1) & 0xFF);
            sendData(g, ((y_end-1) >> 8) & 0xFF);
            sendCommand(g, CMD_Y_ADDR_START );
            sendData(g, (x_start >> 3) & 0xFF);
            sendData(g, (x_end >> 3) & 0xFF); //>>3 is div 8!
          break;
    }
}

#if GDISP_HARDWARE_FLUSH
	LLDSPEC void gdisp_lld_flush(GDisplay *g) { /* sendToDisplay()  combined with the write to ram. */
	    acquire_bus(g);
	    /* Start writing frame buffer to ram. */
	      sendCommand(g, CMD_WRITE_RAM);
	      for(int i=0; i<GDISP_SCREEN_HEIGHT; i++)
	        for(int j=0; j<=((GDISP_SCREEN_WIDTH-1)/8); j++)
	          sendData(g, ((uint8_t *)g->priv)[(GDISP_SCREEN_HEIGHT*j) + i]);
	      /* Update the screen. */
	     sendCommand(g, CMD_DISPLAY_UPDATE_CTRL2);
	     sendData(g, 0xC4);
	     sendCommand(g, CMD_MASTER_ACTV);
	     sendCommand(g, CMD_NOP);

	     while(P7IN & BIT2); //wait for busy line to go low      // waitNotBusy();

	  release_bus(g);
}
#endif


#if GDISP_HARDWARE_DRAWPIXEL
	LLDSPEC void gdisp_lld_draw_pixel(GDisplay *g) {
		coord_t		x, y;

		switch(g->g.Orientation) {
		default:
		case GDISP_ROTATE_0:
			x = g->p.x;
			y = g->p.y;
			break;
		case GDISP_ROTATE_90:
			x = g->p.y;
			y = GDISP_SCREEN_HEIGHT-1 - g->p.x;
			break;
		case GDISP_ROTATE_180:
			x = GDISP_SCREEN_WIDTH-1 - g->p.x;
			y = GDISP_SCREEN_HEIGHT-1 - g->p.y;
			break;
		case GDISP_ROTATE_270:
			x = GDISP_SCREEN_WIDTH-1 - g->p.y;
			y = g->p.x;
			break;
		}
		 /* There is only black and no black (white). */
		  if (gdispColor2Native(g->p.color) != Black) // Indexing in the array is done as described in the init routine
		    ((uint8_t *)g->priv)[(GDISP_SCREEN_HEIGHT*(x/EPD_PPB)) + y] |= (1 << (EPD_PPB-1 - (x % EPD_PPB)));
		  else
		    ((uint8_t *)g->priv)[(GDISP_SCREEN_HEIGHT*(x/EPD_PPB)) + y] &= ~(1 << (EPD_PPB-1 - (x % EPD_PPB)));
	}
#endif

#if GDISP_NEED_CONTROL && GDISP_HARDWARE_CONTROL
LLDSPEC void gdisp_lld_control(GDisplay *g) {
    switch(g->p.x) {
    case GDISP_CONTROL_POWER:
        if (g->g.Powermode == (powermode_t)g->p.ptr)
            return;
        switch((powermode_t)g->p.ptr) {
        case powerOff:
        case powerSleep:
        case powerDeepSleep:
            acquire_bus(g);
            sendCommand(g, CMD_DISPLAY_UPDATE_CTRL2);
            sendData(g, 0x03);
            sendCommand(g, CMD_DEEP_SLEEP);
            sendData(g, 0x01);
            release_bus(g);
            break;
        case powerOn:
            acquire_bus(g);
            sendCommand(g, CMD_DISPLAY_UPDATE_CTRL2);
            sendData(g, 0xc0);
            sendCommand(g, CMD_DEEP_SLEEP);
            sendData(g, 0x00);
            release_bus(g);
            break;
        default:
            return;
        }
        g->g.Powermode = (powermode_t)g->p.ptr;
        return;

        case GDISP_CONTROL_ORIENTATION:
            if (g->g.Orientation == (orientation_t)g->p.ptr)
                return;
            switch((orientation_t)g->p.ptr) {
            case GDISP_ROTATE_0:
                g->g.Height = GDISP_SCREEN_HEIGHT;
                g->g.Width = GDISP_SCREEN_WIDTH;
                break;
            case GDISP_ROTATE_90:
                g->g.Height = GDISP_SCREEN_WIDTH;
                g->g.Width = GDISP_SCREEN_HEIGHT;
                break;
            case GDISP_ROTATE_180:
                g->g.Height = GDISP_SCREEN_HEIGHT;
                g->g.Width = GDISP_SCREEN_WIDTH;
                break;
            case GDISP_ROTATE_270:
                g->g.Height = GDISP_SCREEN_WIDTH;
                g->g.Width = GDISP_SCREEN_HEIGHT;
                break;
            default:
                return;
            }
            g->g.Orientation = (orientation_t)g->p.ptr;
            return;
            default:
                return;
      }
}
#endif // GDISP_NEED_CONTROL

#endif // GFX_USE_GDISP
