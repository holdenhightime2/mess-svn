#ifndef __DAVE_SOUND_CHIP_HEADER_INCLUDED__
#define __DAVE_SOUND_CHIP_HEADER_INCLUDED__

#include "sound/custom.h"
#include "streams.h"

/******************************
DAVE SOUND CHIP
*******************************/

#define DAVE_INT_SELECTABLE     0
#define DAVE_INT_1KHZ_50HZ_TG	1
#define DAVE_INT_1HZ 2
#define DAVE_INT_INT1 3
#define DAVE_INT_INT2 4


typedef struct DAVE_INTERFACE
{
	void (*reg_r)(running_machine *, offs_t);
	void (*reg_w)(running_machine *, offs_t, int);
	void (*int_callback)(running_machine *, int);
} DAVE_INTERFACE;

#define DAVE_FIFTY_HZ_COUNTER_RELOAD 20
#define DAVE_ONE_HZ_COUNTER_RELOAD 1000

typedef struct DAVE
{
	unsigned char Regs[32];


	/* int latches (used by 1Hz, int1 and int2) */
	unsigned long int_latch;
	/* int enables */
	unsigned long int_enable;
	/* int inputs */
	unsigned long int_input;

	unsigned long int_irq;

	/* INTERRUPTS */

	/* internal timer */
	/* bit 2: 1kHz timer irq */
	/* bit 1: 50kHz timer irq */
	int timer_irq;
	/* 1khz timer - divided into 1kHz, 50Hz and 1Hz timer */
	void	*int_timer;
	/* state of 1kHz timer */
	unsigned long one_khz_state;
	/* state of 50Hz timer */
	unsigned long fifty_hz_state;

	/* counter used to trigger 50Hz from 1kHz timer */
	unsigned long fifty_hz_count;
	/* counter used to trigger 1Hz from 1kHz timer */
	unsigned long one_hz_count;


	/* SOUND SYNTHESIS */
	int Period[4];
	int Count[4];
	int	level[4];

	/* these are used to force channels on/off */
	/* if one of the or values is 0x0ff, this means
	the volume will be forced on,else it is dependant on
	the state of the wave */
	int level_or[8];
	/* if one of the values is 0x00, this means the
	volume is forced off, else it is dependant on the wave */
	int level_and[8];

	/* these are the current channel volumes in MAME form */
	int mame_volumes[8];

	/* update step */
	int UpdateStep;

	sound_stream *sound_stream;
} DAVE;

extern CUSTOM_START( Dave_sh_start );

/* id's of external ints */
enum
{
	DAVE_INT1_ID,
	DAVE_INT2_ID
};

void Dave_Init(running_machine *machine);
/* set external int state */
void Dave_SetExternalIntState(running_machine *machine, int IntID, int State);

extern int Dave_getreg(int);
extern void Dave_setreg(running_machine *machine, offs_t offset, UINT8 data);

extern READ8_HANDLER ( Dave_reg_r );
extern WRITE8_HANDLER (	Dave_reg_w );

void Dave_SetIFace(const struct DAVE_INTERFACE *newInterface);

#endif
