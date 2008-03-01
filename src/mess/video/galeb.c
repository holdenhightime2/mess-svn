/***************************************************************************

		Galeb video driver by Miodrag Milanovic

		01/03/2008 Updated to work with latest SVN code
		22/02/2008 Preliminary driver.
		     
****************************************************************************/


#include "driver.h"
  
#define GALEB_VIDEO_MEMORY		0xB000  
    
const rgb_t galeb_palette[2] =
{
	MAKE_RGB(0x00, 0x00, 0x00),		/* Black */
	MAKE_RGB(0xff, 0xff, 0xff)		/* White */
};

PALETTE_INIT( galeb )
{
	palette_set_colors(machine, 0, galeb_palette, ARRAY_LENGTH(galeb_palette));
}

const gfx_layout galeb_charlayout =
{
	8, 8,				/* 8x8 characters */
	256,				/* 256 characters */
	1,				  /* 1 bits per pixel */
	{0},				/* no bitplanes; 1 bit per pixel */
	{7, 6, 5, 4, 3, 2, 1, 0},
	{0 * 8, 1 * 8, 2 * 8, 3 * 8, 4 * 8, 5 * 8, 6 * 8, 7 * 8},
	8*8					/* size of one char */
};

VIDEO_START( galeb )
{
}
 
VIDEO_UPDATE( galeb )
{
	int x,y;

	for(y = 0; y < 16; y++ )
	{
		for(x = 0; x < 48; x++ )
		{
			int code = program_read_byte(GALEB_VIDEO_MEMORY + 15 + x + y*64);		
			drawgfx(bitmap, machine->gfx[0],  code , 0, 0,0, x*8,y*8,
				NULL, TRANSPARENCY_NONE, 0);
		}
	}
	return 0;
}

