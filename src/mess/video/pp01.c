/***************************************************************************

        PP-01 driver by Miodrag Milanovic

        08/09/2008 Preliminary driver.

****************************************************************************/


#include "driver.h"
#include "includes/pp01.h"

VIDEO_START( pp01 )
{
}

VIDEO_UPDATE( pp01 )
{
    UINT8 code_r,code_g,code_b;
    UINT8 col;
    int y, x, b;

    for (y = 0; y < 256; y++)
    {
	    for (x = 0; x < 32; x++)
	    {
            code_r = mess_ram[0x6000 + y*32 + x];
            code_g = mess_ram[0xa000 + y*32 + x];
            code_b = mess_ram[0xe000 + y*32 + x];
            for (b = 0; b < 8; b++)
            {
                col = (((code_r >> b) & 0x01) ? 4 : 0) + (((code_g >> b) & 0x01) ? 2 : 0) + (((code_b >> b) & 0x01) ? 1 : 0);
                *BITMAP_ADDR16(bitmap, y, x*8+(7-b)) =  col;
            }
        }
    }   
	return 0;
}

static const rgb_t pp01_palette[8] = {
	MAKE_RGB(0x00, 0x00, 0x00), // 0
	MAKE_RGB(0x00, 0x00, 0x80), // 1
	MAKE_RGB(0x00, 0x80, 0x00), // 2
	MAKE_RGB(0x00, 0x80, 0x80), // 3
	MAKE_RGB(0x80, 0x00, 0x00), // 4
	MAKE_RGB(0x80, 0x00, 0x80), // 5
	MAKE_RGB(0x80, 0x80, 0x00), // 6
	MAKE_RGB(0x80, 0x80, 0x80), // 7
};

PALETTE_INIT( pp01 )
{
	palette_set_colors(machine, 0, pp01_palette, ARRAY_LENGTH(pp01_palette));
}
