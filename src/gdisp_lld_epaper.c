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
  uint8_t x_start = g->p.x / EPD_PPB;
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
           while(P7IN & BIT2); //wait for busy line to go low
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

//#if GDISP_HARDWARE_FLUSH
	LLDSPEC void gdisp_lld_flush(GDisplay *g) {

	    acquire_bus(g);
	    /* start writing frame buffer to ram */
        setMemoryArea(0, 0, LCD_HORIZONTAL_MAX- 1, LCD_VERTICAL_MAX - 1);
        setMemoryPointer(0, 0);
        sendCommand(CMD_WRITE_RAM);
        /* send the image data */
        uint16_t i = 0;
        for (i = 0; i < (200 / 8 ) * 200; i++) {
            sendData(image_buffer[i]);
        }
	  release_bus(g);
}
#endif

#if GDISP_HARDWARE_FILLS
	LLDSPEC void gdisp_lld_fill_area(GDisplay *g) {
		coord_t		sy, ey;
		coord_t		sx, ex;
		coord_t		col;
		unsigned	spage, zpages;
		uint8_t *	base;
		uint8_t		mask;

		switch(g->g.Orientation) {
		default:
		case GDISP_ROTATE_0:
			sx = g->p.x;
			ex = g->p.x + g->p.cx - 1;
			sy = g->p.y;
			ey = sy + g->p.cy - 1;
			break;
		case GDISP_ROTATE_90:
			sx = g->p.y;
			ex = g->p.y + g->p.cy - 1;
			sy = GDISP_SCREEN_HEIGHT - g->p.x - g->p.cx;
			ey = GDISP_SCREEN_HEIGHT-1 - g->p.x;
			break;
		case GDISP_ROTATE_180:
			sx = GDISP_SCREEN_WIDTH - g->p.x - g->p.cx;
			ex = GDISP_SCREEN_WIDTH-1 - g->p.x;
			sy = GDISP_SCREEN_HEIGHT - g->p.y - g->p.cy;
			ey = GDISP_SCREEN_HEIGHT-1 - g->p.y;
			break;
		case GDISP_ROTATE_270:
			sx = GDISP_SCREEN_WIDTH - g->p.y - g->p.cy;
			ex = GDISP_SCREEN_WIDTH-1 - g->p.y;
			sy = g->p.x;
			ey = g->p.x + g->p.cx - 1;
			break;
		}

		spage = sy / 8;
		base = RAM(g) + SSD1306_PAGE_OFFSET + SSD1306_PAGE_WIDTH * spage;
		mask = 0xff << (sy&7);
		zpages = (ey / 8) - spage;

		if (gdispColor2Native(g->p.color) == gdispColor2Native(Black)) {
			while (zpages--) {
				for (col = sx; col <= ex; col++)
					base[col] &= ~mask;
				mask = 0xff;
				base += SSD1306_PAGE_WIDTH;
			}
			mask &= (0xff >> (7 - (ey&7)));
			for (col = sx; col <= ex; col++)
				base[col] &= ~mask;
		} else {
			while (zpages--) {
				for (col = sx; col <= ex; col++)
					base[col] |= mask;
				mask = 0xff;
				base += SSD1306_PAGE_WIDTH;
			}
			mask &= (0xff >> (7 - (ey&7)));
			for (col = sx; col <= ex; col++)
				base[col] |= mask;
		}
		g->flags |= GDISP_FLG_NEEDFLUSH;
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
		if (gdispColor2Native(g->p.color) != gdispColor2Native(Black))
			RAM(g)[xyaddr(x, y)] |= xybit(y);
		else
			RAM(g)[xyaddr(x, y)] &= ~xybit(y);
		g->flags |= GDISP_FLG_NEEDFLUSH;
	}
#endif

#if GDISP_HARDWARE_PIXELREAD
	LLDSPEC color_t gdisp_lld_get_pixel_color(GDisplay *g) {
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
		return (RAM(g)[xyaddr(x, y)] & xybit(y)) ? White : Black;
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
				write_cmd(g, SSD1306_DISPLAYOFF);
				release_bus(g);
				break;
			case powerOn:
				acquire_bus(g);
				write_cmd(g, SSD1306_DISPLAYON);
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
			/* Rotation is handled by the drawing routines */
			case GDISP_ROTATE_0:
			case GDISP_ROTATE_180:
				g->g.Height = GDISP_SCREEN_HEIGHT;
				g->g.Width = GDISP_SCREEN_WIDTH;
				break;
			case GDISP_ROTATE_90:
			case GDISP_ROTATE_270:
				g->g.Height = GDISP_SCREEN_WIDTH;
				g->g.Width = GDISP_SCREEN_HEIGHT;
				break;
			default:
				return;
			}
			g->g.Orientation = (orientation_t)g->p.ptr;
			return;

		case GDISP_CONTROL_CONTRAST:
            if ((unsigned)g->p.ptr > 100)
            	g->p.ptr = (void *)100;
			acquire_bus(g);
			write_cmd2(g, SSD1306_SETCONTRAST, (((unsigned)g->p.ptr)<<8)/101);
			release_bus(g);
            g->g.Contrast = (unsigned)g->p.ptr;
			return;

		// Our own special controller code to inverse the display
		// 0 = normal, 1 = inverse
		case GDISP_CONTROL_INVERSE:
			acquire_bus(g);
			write_cmd(g, g->p.ptr ? SSD1306_INVERTDISPLAY : SSD1306_NORMALDISPLAY);
			release_bus(g);
			return;
		}
	}
#endif // GDISP_NEED_CONTROL

#endif // GFX_USE_GDISP
