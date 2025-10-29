/*
 * Some simple tests
 */
#include "ch32fun.h"
#include "ch32v003_GPIO_branchless.h"
#include <stdio.h>

#define PIXELS 8
#define COLORS (PIXELS * 3)
#define BUFLEN (COLORS * 24)

uint8_t buf[BUFLEN];
/* green pixels */
uint8_t pixel[COLORS] = {
	0x0, 0x0, 0x0,	// Black
	0x0, 0x0, 0x0,	// Blue
	0x0, 0x0, 0x0,	// Red
	0x0, 0x0, 0x0,	// Magenta
	0x0, 0x0, 0x0,	// Green
	0x0, 0x0, 0x0,	// Cyan
	0x0, 0x0, 0x0,	// Yellow
	0x0, 0x0, 0x0,	// White
};

/* This just points at the GPIO Port C output data register */
uint32_t *port_c = (uint32_t *)(0x4001110C);

/* Convert the pixel data into a buffer. 
 *
 * This iterates over the 3 color components (G, R, B), and then the 8 
 * bits of those colors, sending them MSB to LSB, one after the other. 
 * PC1 is our data path so 0x2 is on, 0x0 is off, PC2 is our trigger 
 * indicator so 0x4 is we'er xmitting, 0x0 is we aren't.
 */
void
build_buffer() {
	for (int i = 0, ndx = 0; i < COLORS; i++) {
		uint8_t c = pixel[i];	// color
		for (int bit = 0; bit < 8; bit++) {
			buf[ndx++] = 6;
			buf[ndx++] = 4 + ((c & 0x80) >> 6);
			buf[ndx++] = 4;
			c = c << 1;
		}
	}
}

/*
 * Then this updates the display with the current buffer
 */
void
update_pixels()
{
	*port_c = 0x0;
	/* generate the 50uS 'reset' signal by holding input low */
	for (int i = 0; i < 400; i++) {
		asm("nop"); // spinloop delay for 50 uS
	}

	for (int i = 0; i < BUFLEN; i++) {
		*port_c = buf[i];
		// So full speed is 288nS per bit time. Need 400ns 
		asm(" nop ");
		asm(" nop ");
		asm(" nop ");
		asm(" nop ");
		asm(" nop ");
		asm(" nop ");
		/* With 6 NOPs we are at 406.5nS which seems good enough */
	}
	*port_c = 0x2;
}

/*
 * Set pixel 'n' (for n being 0 to PIXELS-1) to the color
 * (r, g, b)
 */
void
set_pixel(int n, uint8_t r, uint8_t g, uint8_t b)
{
	if ((n < 0) || (n >= PIXELS)) {
		return; // bad pixel number
	}

	pixel[n*3] = g;
	pixel[n*3+1] = r;
	pixel[n*3+2] = b;
}

#define PIXEL_VALUE	0x22
struct colors {
	uint8_t	r;
	uint8_t g;
	uint8_t b;
} clr[8] = {
 { 0x00,        0x00,        PIXEL_VALUE },
 { 0x00,        PIXEL_VALUE, 0x00 },
 { 0x00,        PIXEL_VALUE, PIXEL_VALUE },
 { PIXEL_VALUE, 0x00,        0x00 },
 { PIXEL_VALUE, 0x00,        PIXEL_VALUE },
 { PIXEL_VALUE, PIXEL_VALUE, 0x00 },
 { PIXEL_VALUE, PIXEL_VALUE, PIXEL_VALUE },
 { 0x10, 0x40, 0x10 },
};

int main() {
	int step = 0;
	int direction = 1;
	int cc = 0;
	SystemInit();

    /* Set the clock for the ADC (HBCLK/4 but maybe by 2 is okay?) */
    RCC->CFGR0 = (RCC->CFGR0 & ~(0x1f << 11)) | (0x0 << 11);

    /* Enable the ADC & GPIOC */
    RCC->APB2PCENR |= (RCC_ADC1EN | RCC_IOPCEN);
	/* Set up the GPIO pins */
    /* pulled up input is 0x8           */
    /* Floating input is 0x4            */
    /* push-pull 10Mhz output is 0x1    */
    /* Analog input is 0x0              */
    /* 8 pin CH3V003 only has PC1,2,4 coming out to pins */
    GPIOC->CFGLR = 0x44404114;
    /* Set up the ADC for continuous sampling, PC4 */
    /* Using pixels as debug agents :-)
     */
    set_pixel(4, 0xff, 0x0, 0x0);   // start of ADC setup  
    RCC->APB2PRSTR |= (1<<9);   // reset the ADC
    RCC->APB2PRSTR &= ~(1<<9);  // reset the ADC
    ADC1->CTLR1 = 0;            // independent mode
    ADC1->CTLR2 = (1 << 1);     // continuous mode.
    ADC1->RSQR1 = (1 << 20);    // Only 1 channel
    ADC1->RSQR3 = 2;            // Make it channel #2
    ADC1->SAMPTR2 = (7 << 6);   // Channel 2, 241 clocks/sample.
    ADC1->CTLR2 |= 1;           // Turn on the ADC.
    set_pixel(3, 0xff, 0x0, 0x0); // start of ADC cal 
    ADC1->CTLR2 |= (1 << 3);    // Start cal
    while ((ADC1->CTLR2 & (1 << 3)) != 0) { 
        asm(" nop ");
    }
    set_pixel(2, 0x00, 0xff, 0x00); // calibrated!
    ADC1->CTLR2 |= (1 << 2);
    while ((ADC1->CTLR2 & (1 << 2)) != 0) {
        asm(" nop ");
    }
    set_pixel(1, 0x00, 0x00, 0xff); // calibrated!
    /* and start it up */
    ADC1->CTLR2 |= 1;

	while (1) {
        int delay;
		switch (step) {
			case 0:
				if (direction == -1) {
					direction = 1;
				}
                set_pixel(1, 0, 0, 0);
				cc = (cc + 1) % 8;
				set_pixel(0, clr[cc].r, clr[cc].g, clr[cc].b);
				break;
			case 7:
				if (direction == 1) {
					direction = -1;
				}
				cc = (cc + 1) % 8;
                set_pixel(6, 0, 0, 0);
				set_pixel(7, clr[cc].r, clr[cc].g, clr[cc].b);
				break;
			default:
				set_pixel(step - direction, 0, 0, 0);
				set_pixel(step, clr[cc].r, clr[cc].g, clr[cc].b);
				break;
		}
		step = step + direction;
		build_buffer();
		update_pixels();
        delay = ADC1->RDATAR;
		Delay_Ms(delay);
	}
}
