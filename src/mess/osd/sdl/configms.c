//============================================================
//
//	configms.c - Win32 MESS specific options
//
//============================================================

// MESS headers
#include "driver.h"
#include "windows/config.h"
#include "menu.h"
#include "device.h"
#include "configms.h"
#include "mscommon.h"
#include "pool.h"
#include "config.h"

//============================================================
//	LOCAL VARIABLES
//============================================================

static const options_entry win_mess_opts[] =
{
	{ NULL,							NULL,   OPTION_HEADER,		"SDL MESS SPECIFIC OPTIONS" },
//	{ "newui;nu",                   "1",    OPTION_BOOLEAN,		"use the new MESS UI" },
	{ "natural;nat",				"0",	OPTION_BOOLEAN,		"specifies whether to use a natural keyboard or not" },
	{ NULL }
};

int win_use_natural_keyboard;
extern char *osd_get_startup_cwd(void);

//============================================================

void osd_mess_options_init(void)
{
	options_add_entries(mame_options(), win_mess_opts);
}


void sdl_mess_options_parse(void)
{
	win_use_natural_keyboard = options_get_bool(mame_options(), "natural");
}

void osd_get_emulator_directory(char *dir, size_t dir_size)
{
	strncpy(dir, osd_get_startup_cwd(), dir_size);
}

