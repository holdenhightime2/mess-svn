#pragma once

#ifndef __MPZ80__
#define __MPZ80__

#define ADDRESS_MAP_MODERN

#include "emu.h"
#include "cpu/z80/z80.h"
#include "machine/ram.h"
#include "machine/terminal.h"

#define Z80_TAG			"17a"
#define AM9512_TAG		"17d"
#define TERMINAL_TAG	"terminal"

class mpz80_state : public driver_device
{
public:
	mpz80_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		  m_maincpu(*this, Z80_TAG),
		  m_ram(*this, RAM_TAG)
	{ }

	required_device<cpu_device> m_maincpu;
	required_device<device_t> m_ram;

	virtual void machine_start();
	virtual void machine_reset();

	DECLARE_READ8_MEMBER( trap_addr_r );
	DECLARE_READ8_MEMBER( keyboard_r );
	DECLARE_READ8_MEMBER( switch_r );
	DECLARE_READ8_MEMBER( status_r );
	DECLARE_WRITE8_MEMBER( disp_seg_w );
	DECLARE_WRITE8_MEMBER( disp_col_w );
	DECLARE_WRITE8_MEMBER( task_w );
	DECLARE_WRITE8_MEMBER( mask_w );
};

#endif