/*-----------------------------------------------
 * Pico-Composite8      Composite Video, 8-bit output
 *
 * 2021-06-04           obstruse@earthlink.net
 *-----------------------------------------------
*/

/* Based on:
 *-----------------------------------------------
 * Title:               Pico-mposite Video Output
 *  Author:             Dean Belfield
 *  Created:            26/01/2021
 *  Last Updated:       15/02/2021
 *-----------------------------------------------
*/


#define TESTPATTERN

#include <stdlib.h>
//#include <stdio.h>
#include "memory.h"

#include "pico/stdlib.h"
#include "pico/multicore.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/irq.h"

void cvideo_configure_pio_dma(PIO pio, uint sm, uint dma_channel, size_t buffer_size_words);
void cvideo_dma_handler(void);
#include "cvideo.pio.h"     // The assembled PIO code

#define width 512           // Bitmap width in pixels
#define height 384          // Bitmap height in pixels

/*-------------------------------------------------------------------*/
/*------------------Video Standard-----------------------------------*/
/*-------------------------------------------------------------------*/

const int   VIDEO_frame_lines = 525;
const int   VIDEO_frame_lines_visible = 480;
const float VIDEO_aspect_ratio = 4.0/3.0;
const float VIDEO_horizontal_freq = 15750.0;
const float VIDEO_h_FP_usec = 1.5;	// front porch
const float VIDEO_h_SYNC_usec = 4.7;	// sync
const float VIDEO_h_BP_usec = 4.7;	// back porch
const float VIDEO_h_EP_usec = 2.3;	// equalizing pulse

/*-------------------------------------------------------------------*/
/*------------------Horizontal Derived-------------------------------*/
/*-------------------------------------------------------------------*/

const int   HORIZ_visible_dots = VIDEO_frame_lines_visible * VIDEO_aspect_ratio;	// full frame width
const float HORIZ_usec = 1000000.0 / VIDEO_horizontal_freq;
const float HORIZ_usec_dot = (HORIZ_usec - VIDEO_h_FP_usec - VIDEO_h_SYNC_usec - VIDEO_h_BP_usec) / HORIZ_visible_dots;
const int   HORIZ_dots = HORIZ_usec / HORIZ_usec_dot;
const int   HORIZ_FP_dots = VIDEO_h_FP_usec / HORIZ_usec_dot;
const int   HORIZ_SYNC_dots = VIDEO_h_SYNC_usec / HORIZ_usec_dot;
const int   HORIZ_BP_dots = VIDEO_h_BP_usec / HORIZ_usec_dot;
const int   HORIZ_EP_dots = VIDEO_h_EP_usec / HORIZ_usec_dot;	// equalizing pulse during vertical sync
const int   HORIZ_pixel_start = (HORIZ_visible_dots - width) / 2 + HORIZ_SYNC_dots + HORIZ_BP_dots;

/*-------------------------------------------------------------------*/
/*------------------Vertical Derived---------------------------------*/
/*-------------------------------------------------------------------*/

const int   VERT_scanlines = VIDEO_frame_lines / 2;				// one field
const int   VERT_vblank    = (VIDEO_frame_lines - VIDEO_frame_lines_visible) / 2;	// vertical blanking, one field

int   VERT_border    = (VERT_scanlines - VERT_vblank - height/2) / 2;
int   VERT_bitmap   = height/2;
//// making the above 'const' causes vertical jitter/defects ////

/*-------------------------------------------------------------------*/
/*------------------PIO----------------------------------------------*/
/*-------------------------------------------------------------------*/

const float PIO_clkdot = 1.0;        	// PIO instructions per dot
const float PIO_sysclk = 125000000.0;	// default Pico system clock
const float PIO_clkdiv = PIO_sysclk / VIDEO_horizontal_freq / PIO_clkdot / HORIZ_dots;

/*-------------------------------------------------------------------*/
/*------------------Gray Scale---------------------------------------*/
/*-------------------------------------------------------------------*/
// NTSC in IRE units+40: SYNC = 0; BLANK = 40; BLACK = 47.5; WHITE = 140
const int WHITE = 255;
const int BLACK = 255.0 / 140.0 * 47.5;
const int BLANK = 255.0 / 140.0 * 40.0;
const int SYNC = 0;

#define border_colour BLACK

#define state_machine 0     // The PIO state machine to use
uint dma_channel;           // DMA channel for transferring hsync data to PIO

uint vline = 9999;          // Current video line being processed
uint bline = 0;             // Line in the bitmap to fetch
uint field = 0;		    // field, even/odd

int bmIndex = 0;
int bmCount = 0;

// bitmap buffer
#ifdef TESTPATTERN
#include "indian.h"
unsigned char * bitmap = (unsigned char *)indian;
#else
unsigned char bitmap[width*height]={[0 ... width*height-1] = WHITE};
#endif

unsigned char * vsync_ll;                             // buffer for a vsync line with a long/long pulse
unsigned char * vsync_ss;                             // Buffer for an equalizing line with a short/short pulse
unsigned char * vsync_bb;                             // Buffer for a vsync blanking
unsigned char * vsync_ssb;                            // Buffer and a half for equalizing/blank line
unsigned char * border;                               // Buffer for a vsync line for the top and bottom borders
unsigned char * pixel_buffer[2];                      // Double-buffer for the pixel data scanlines

volatile bool bitmapReading = false;
volatile bool changeBitmap  = false;

/*-------------------------------------------------------------------*/
void second_core() {
    unsigned char * dataCount = (unsigned char *)0x10050000;
    unsigned char * dataStart = (unsigned char *)0x10050001;
    //int bmMax = *dataCount / 4;		// using the low-res bitmaps for testing
    int bmMax = *dataCount;
    bmIndex = 0; 			

    while (true) {
#ifdef TESTPATTERN
#else
	    while (vline >= VERT_vblank ) {
		busy_wait_us(HORIZ_usec);
	    }

	    changeBitmap = true;	// DMA interrupt now starts sending BLACK instead of bitmap

            bmIndex++;
            if ( bmIndex >= bmMax ) {
                    bmIndex = 0;
            }

	    memcpy(bitmap, dataStart + (width * height * bmIndex), width*height);

	    // settling time
	    busy_wait_us(HORIZ_usec*VERT_scanlines);

            while (vline >= VERT_vblank ) {
                busy_wait_us(HORIZ_usec);
            }

	    changeBitmap = false;	// ...switch back to displaying bitmap

	    //sleep_ms(1000);
	    busy_wait_us(1000000);
#endif
    }
}

/*-------------------------------------------------------------------*/
int main() {
//	stdio_init_all();
//	sleep_ms(2000);
//	printf("Start of program\n");

    gpio_init(8);
    gpio_set_dir(8,GPIO_OUT);
    gpio_init(9);
    gpio_set_dir(9,GPIO_OUT);
    gpio_init(10);
    gpio_set_dir(10,GPIO_OUT);

    multicore_launch_core1(second_core);

    vsync_ll = (unsigned char *)malloc(HORIZ_dots);
    memset(vsync_ll, SYNC, HORIZ_dots);				// vertical sync/serrations
    memset(vsync_ll + (HORIZ_dots>>1) - HORIZ_SYNC_dots, BLANK, HORIZ_SYNC_dots);
    memset(vsync_ll + HORIZ_dots      - HORIZ_SYNC_dots, BLANK, HORIZ_SYNC_dots);

    vsync_ss = (unsigned char *)malloc(HORIZ_dots);
    memset(vsync_ss, BLANK, HORIZ_dots);				// vertical equalizing
    memset(vsync_ss, SYNC, HORIZ_EP_dots);
    memset(vsync_ss + (HORIZ_dots>>1), SYNC, HORIZ_EP_dots);

    vsync_bb = (unsigned char *)malloc(HORIZ_dots);
    memset(vsync_bb, BLANK, HORIZ_dots);				// vertical blanking
    memset(vsync_bb, SYNC, HORIZ_SYNC_dots);

    vsync_ssb = (unsigned char *)malloc(HORIZ_dots+(HORIZ_dots>>1));
    memset(vsync_ssb, BLANK, HORIZ_dots + (HORIZ_dots>>1));		// vertical equalizing/blanking
    memset(vsync_ssb, SYNC, HORIZ_EP_dots);
    memset(vsync_ssb + (HORIZ_dots>>1), SYNC, HORIZ_EP_dots);

    // This bit pre-builds the border scanline and pixel buffers
    border = (unsigned char *)malloc(HORIZ_dots);
    memset(border, border_colour, HORIZ_dots);			// Fill the border with the border colour
    memset(border, SYNC, HORIZ_SYNC_dots);		        // Add the hsync pulse
    memset(border + HORIZ_SYNC_dots,            BLANK, HORIZ_BP_dots);
    memset(border + HORIZ_dots - HORIZ_FP_dots, BLANK, HORIZ_FP_dots);		// front porch

    pixel_buffer[0] = (unsigned char *)malloc(HORIZ_dots);
    memcpy(pixel_buffer[0], border, HORIZ_dots);			// pixel buffer
    pixel_buffer[1] = (unsigned char *)malloc(HORIZ_dots);
    memcpy(pixel_buffer[1], border, HORIZ_dots);			// pixel buffer

    // Initialise the PIO
    PIO pio = pio0;
    uint offset = pio_add_program(pio, &cvideo_program);	// Load up the PIO program
    pio_sm_set_enabled(pio, state_machine, false);              // Disable the PIO state machine
    pio_sm_clear_fifos(pio, state_machine);	                // Clear the PIO FIFO buffers
    cvideo_initialise_pio(pio, state_machine, offset, 0, 8, PIO_clkdiv); // Initialise the PIO (function in cvideo.pio)

    dma_channel = dma_claim_unused_channel(true);		// Claim a DMA channel for the hsync transfer
    cvideo_configure_pio_dma(pio, state_machine, dma_channel, HORIZ_dots); // Hook up the DMA channel to the state machine

    pio_sm_set_enabled(pio, state_machine, true);               // Enable the PIO state machine

    // And kick everything off
    cvideo_dma_handler();       // Call the DMA handler as a one-off to initialise it

    while (true) {              // And then just loop doing nothing
	tight_loop_contents();
    }
}

/*-------------------------------------------------------------------*/
// The DMA interrupt handler
// This is triggered by DMA_IRQ_0

void cvideo_dma_handler(void) {

    if ( ++vline <= VERT_scanlines ) {
    } else {
        vline = 0;
	bline = 0;
	field = ++field & 0x01;
	gpio_put(10,field);
    }

    while (true) {
        if ( vline <= VERT_vblank ) {

            switch(vline) {

            case 0:
		// for some reason interlace fails unless there's a 30usec delay here:
		busy_wait_us(HORIZ_usec/2);

		gpio_put(8,0);
		if ( field ) {
			// odd field - blank, full line
			dma_channel_set_read_addr(dma_channel, vsync_bb, true);

                } else {
			// even field - blank, half line
			dma_channel_set_trans_count(dma_channel, HORIZ_dots/2, false);
			dma_channel_set_read_addr(dma_channel, vsync_bb, true);
                }

		break;

            case 1:
		gpio_put(8,1);
		dma_channel_set_trans_count(dma_channel, HORIZ_dots, false);   // reset transfer size
            case 2 ... 3:
                // send 3 vsync_ss - 'equalizing pulses'
                dma_channel_set_read_addr(dma_channel, vsync_ss, true);

                break;

            case 4 ... 6:
                // send 3 vsync_ll - 'vertical sync/serrations'
                dma_channel_set_read_addr(dma_channel, vsync_ll, true);

                break;

            case 7 ... 8:
                // send 3 vsync_ss - 'equalizing pulses'
                dma_channel_set_read_addr(dma_channel, vsync_ss, true);

                break;

	    case 9:
		gpio_put(9,0);
		if ( field ) {
			// odd field - equalizing pulse, full line
			dma_channel_set_read_addr(dma_channel, vsync_ss, true);

           	} else {
			//even field - equalizing pulse, line and a half
			dma_channel_set_trans_count(dma_channel, HORIZ_dots + HORIZ_dots/2, false);
			dma_channel_set_read_addr(dma_channel, vsync_ssb, true);
     		}

		break;

            case 10:
		gpio_put(9,1);
		// everything back to normal
		dma_channel_set_trans_count(dma_channel, HORIZ_dots, false);   // reset transfer size
	    default:
                // send BLANK till end of vertical blanking
                dma_channel_set_read_addr(dma_channel, vsync_bb, true);

                break;

            }

            break;
        }

        if ( vline <= VERT_vblank + VERT_border ) {

            if (changeBitmap) {
                dma_channel_set_read_addr(dma_channel, vsync_bb, true);
            } else {
                dma_channel_set_read_addr(dma_channel, border, true);
		if ( vline == VERT_vblank + VERT_border ) {
		    memcpy(pixel_buffer[bline & 1] + HORIZ_pixel_start, bitmap+(bline*2+field)*width, width);
		}

            }

            break;
        }

        if ( vline <= VERT_vblank + VERT_border + VERT_bitmap  ) {

	    if (changeBitmap) {
                dma_channel_set_read_addr(dma_channel, vsync_bb, true);
	    } else {
                dma_channel_set_read_addr(dma_channel, pixel_buffer[bline++ & 1], true);    // Set the DMA to read from one of the pixel_buffers
                memcpy(pixel_buffer[bline & 1] + HORIZ_pixel_start, bitmap+(bline*2+field)*width, width);       // And memcpy the next scanline
	    }
            break;
        }

        // otherwise, just output border until end of scanlines
	if (changeBitmap) {
            dma_channel_set_read_addr(dma_channel, vsync_bb, true);
	} else {
            dma_channel_set_read_addr(dma_channel, border, true);
	}

        break;
    }

    // Finally, clear the interrupt request ready for the next horizontal sync interrupt
    dma_hw->ints0 = 1u << dma_channel;
}

/*-------------------------------------------------------------------*/
// Configure the PIO DMA
// Parameters:
// - pio: The PIO to attach this to
// - sm: The state machine number
// - dma_channel: The DMA channel
// - buffer_size_words: Number of bytes to transfer
//
void cvideo_configure_pio_dma(PIO pio, uint sm, uint dma_channel, size_t buffer_size_words) {
    pio_sm_clear_fifos(pio, sm);
    dma_channel_config c = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
    channel_config_set_read_increment(&c, true);
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true));

    dma_channel_configure(dma_channel, &c,
                          &pio->txf[sm],              // Destination pointer
                          NULL,                       // Source pointer
                          buffer_size_words,          // Number of transfers
                          true                        // Start flag (true = start immediately)
                         );

    dma_channel_set_irq0_enabled(dma_channel, true);

    irq_set_exclusive_handler(DMA_IRQ_0, cvideo_dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}
