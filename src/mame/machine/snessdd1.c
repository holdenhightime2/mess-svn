static UINT8 sdd1_read(running_machine* machine, UINT32 addr);

typedef struct //Input Manager
{
	running_machine* machine;
	UINT32 byte_ptr;
	UINT8 bit_count;
} SDD1_IM;

static SDD1_IM* SDD1_IM_ctor(running_machine* machine)
{
	SDD1_IM* newclass = (SDD1_IM*)auto_alloc_array(machine, UINT8, sizeof(SDD1_IM));
	newclass->machine = machine;
	return newclass;
}

static void SDD1_IM_prepareDecomp(SDD1_IM* this, UINT32 in_buf)
{
	this->byte_ptr = in_buf;
	this->bit_count = 4;
}

static UINT8 SDD1_IM_getCodeword(SDD1_IM* this, const UINT8 code_len)
{
	UINT8 codeword;

	codeword = sdd1_read(this->machine, this->byte_ptr) << this->bit_count;

	++this->bit_count;

	if (codeword & 0x80)
	{
		codeword |= sdd1_read(this->machine, this->byte_ptr + 1) >> (9 - this->bit_count);
		this->bit_count += code_len;
	}

	if (this->bit_count & 0x08)
	{
		this->byte_ptr++;
		this->bit_count &= 0x07;
	}

	return codeword;
}

typedef struct //Golomb-Code Decoder
{
	running_machine* machine;
	SDD1_IM* IM;
} SDD1_GCD;

static SDD1_GCD* SDD1_GCD_ctor(running_machine* machine, SDD1_IM* associatedIM)
{
	SDD1_GCD* newclass = (SDD1_GCD*)auto_alloc_array(machine, UINT8, sizeof(SDD1_GCD));
	newclass->machine = machine;
	newclass->IM = associatedIM;
	return newclass;
}

static void SDD1_GCD_getRunCount(SDD1_GCD* this, UINT8 code_num, UINT8* MPScount, UINT8* LPSind)
{
	const UINT8 run_count[] =
	{
		0x00, 0x00, 0x01, 0x00, 0x03, 0x01, 0x02, 0x00,
		0x07, 0x03, 0x05, 0x01, 0x06, 0x02, 0x04, 0x00,
		0x0f, 0x07, 0x0b, 0x03, 0x0d, 0x05, 0x09, 0x01,
		0x0e, 0x06, 0x0a, 0x02, 0x0c, 0x04, 0x08, 0x00,
		0x1f, 0x0f, 0x17, 0x07, 0x1b, 0x0b, 0x13, 0x03,
		0x1d, 0x0d, 0x15, 0x05, 0x19, 0x09, 0x11, 0x01,
		0x1e, 0x0e, 0x16, 0x06, 0x1a, 0x0a, 0x12, 0x02,
		0x1c, 0x0c, 0x14, 0x04, 0x18, 0x08, 0x10, 0x00,
		0x3f, 0x1f, 0x2f, 0x0f, 0x37, 0x17, 0x27, 0x07,
		0x3b, 0x1b, 0x2b, 0x0b, 0x33, 0x13, 0x23, 0x03,
		0x3d, 0x1d, 0x2d, 0x0d, 0x35, 0x15, 0x25, 0x05,
		0x39, 0x19, 0x29, 0x09, 0x31, 0x11, 0x21, 0x01,
		0x3e, 0x1e, 0x2e, 0x0e, 0x36, 0x16, 0x26, 0x06,
		0x3a, 0x1a, 0x2a, 0x0a, 0x32, 0x12, 0x22, 0x02,
		0x3c, 0x1c, 0x2c, 0x0c, 0x34, 0x14, 0x24, 0x04,
		0x38, 0x18, 0x28, 0x08, 0x30, 0x10, 0x20, 0x00,
		0x7f, 0x3f, 0x5f, 0x1f, 0x6f, 0x2f, 0x4f, 0x0f,
		0x77, 0x37, 0x57, 0x17, 0x67, 0x27, 0x47, 0x07,
		0x7b, 0x3b, 0x5b, 0x1b, 0x6b, 0x2b, 0x4b, 0x0b,
		0x73, 0x33, 0x53, 0x13, 0x63, 0x23, 0x43, 0x03,
		0x7d, 0x3d, 0x5d, 0x1d, 0x6d, 0x2d, 0x4d, 0x0d,
		0x75, 0x35, 0x55, 0x15, 0x65, 0x25, 0x45, 0x05,
		0x79, 0x39, 0x59, 0x19, 0x69, 0x29, 0x49, 0x09,
		0x71, 0x31, 0x51, 0x11, 0x61, 0x21, 0x41, 0x01,
		0x7e, 0x3e, 0x5e, 0x1e, 0x6e, 0x2e, 0x4e, 0x0e,
		0x76, 0x36, 0x56, 0x16, 0x66, 0x26, 0x46, 0x06,
		0x7a, 0x3a, 0x5a, 0x1a, 0x6a, 0x2a, 0x4a, 0x0a,
		0x72, 0x32, 0x52, 0x12, 0x62, 0x22, 0x42, 0x02,
		0x7c, 0x3c, 0x5c, 0x1c, 0x6c, 0x2c, 0x4c, 0x0c,
		0x74, 0x34, 0x54, 0x14, 0x64, 0x24, 0x44, 0x04,
		0x78, 0x38, 0x58, 0x18, 0x68, 0x28, 0x48, 0x08,
		0x70, 0x30, 0x50, 0x10, 0x60, 0x20, 0x40, 0x00,
	};

	UINT8 codeword = SDD1_IM_getCodeword(this->IM, code_num);

	if (codeword & 0x80)
	{
		*LPSind = 1;
		*MPScount = run_count[codeword >> (code_num ^ 0x07)];
	}
	else
	{
		*MPScount = (1 << code_num);
	}
}

typedef struct // Bits Generator
{
	running_machine* machine;
	UINT8 code_num;
	UINT8 MPScount;
	UINT8 LPSind;
	SDD1_GCD* GCD;
} SDD1_BG;

static SDD1_BG* SDD1_BG_ctor(running_machine* machine, SDD1_GCD* associatedGCD, UINT8 code)
{
	SDD1_BG* newclass = (SDD1_BG*)auto_alloc_array(machine, UINT8, sizeof(SDD1_BG));
	newclass->machine = machine;
	newclass->code_num = code;
	newclass->GCD = associatedGCD;
	return newclass;
}

static void SDD1_BG_prepareDecomp(SDD1_BG* this)
{
	this->MPScount = 0;
	this->LPSind = 0;
}

static UINT8 SDD1_BG_getBit(SDD1_BG* this, UINT8* endOfRun)
{
	UINT8 bit;

	if (!(this->MPScount || this->LPSind))
	{
		SDD1_GCD_getRunCount(this->GCD, this->code_num, &(this->MPScount), &(this->LPSind));
	}

	if (this->MPScount)
	{
		bit = 0;
		this->MPScount--;
	}
	else
	{
		bit = 1;
		this->LPSind = 0;
	}

	if (this->MPScount || this->LPSind)
	{
		(*endOfRun) = 0;
	}
	else
	{
		(*endOfRun) = 1;
	}

	return bit;
}


typedef struct
{
	UINT8 code_num;
	UINT8 nextIfMPS;
	UINT8 nextIfLPS;
} SDD1_PEM_state;

SDD1_PEM_state SDD1_PEM_evolution_table[33] =
{
	{ 0,25,25},
	{ 0, 2, 1},
	{ 0, 3, 1},
	{ 0, 4, 2},
	{ 0, 5, 3},
	{ 1, 6, 4},
	{ 1, 7, 5},
	{ 1, 8, 6},
	{ 1, 9, 7},
	{ 2,10, 8},
	{ 2,11, 9},
	{ 2,12,10},
	{ 2,13,11},
	{ 3,14,12},
	{ 3,15,13},
	{ 3,16,14},
	{ 3,17,15},
	{ 4,18,16},
	{ 4,19,17},
	{ 5,20,18},
	{ 5,21,19},
	{ 6,22,20},
	{ 6,23,21},
	{ 7,24,22},
	{ 7,24,23},
	{ 0,26, 1},
	{ 1,27, 2},
	{ 2,28, 4},
	{ 3,29, 8},
	{ 4,30,12},
	{ 5,31,16},
	{ 6,32,18},
	{ 7,24,22}
};

typedef struct
{
	UINT8 status;
	UINT8 MPS;
} SDD1_PEM_ContextInfo;

typedef struct //Probability Estimation Module
{
	running_machine* machine;
	SDD1_PEM_ContextInfo contextInfo[32];
	SDD1_BG* BG[8];
} SDD1_PEM;

static SDD1_PEM* SDD1_PEM_ctor(running_machine* machine,
							   SDD1_BG* associatedBG0, SDD1_BG* associatedBG1,
							   SDD1_BG* associatedBG2, SDD1_BG* associatedBG3,
							   SDD1_BG* associatedBG4, SDD1_BG* associatedBG5,
							   SDD1_BG* associatedBG6, SDD1_BG* associatedBG7)
{
	SDD1_PEM* newclass = (SDD1_PEM*)auto_alloc_array(machine, UINT8, sizeof(SDD1_PEM));
	newclass->machine = machine;
	newclass->BG[0] = associatedBG0;
	newclass->BG[1] = associatedBG1;
	newclass->BG[2] = associatedBG2;
	newclass->BG[3] = associatedBG3;
	newclass->BG[4] = associatedBG4;
	newclass->BG[5] = associatedBG5;
	newclass->BG[6] = associatedBG6;
	newclass->BG[7] = associatedBG7;
	return newclass;
}

static void SDD1_PEM_prepareDecomp(SDD1_PEM* this)
{
	UINT8 i;
	for(i = 0; i < 32; i++)
	{
		this->contextInfo[i].status = 0;
		this->contextInfo[i].MPS = 0;
	}
}

static UINT8 SDD1_PEM_getBit(SDD1_PEM* this, UINT8 context)
{
	UINT8 endOfRun;
	UINT8 bit;

	SDD1_PEM_ContextInfo *pContInfo = &(this->contextInfo)[context];
	UINT8 currStatus = pContInfo->status;
	SDD1_PEM_state* pState = &(SDD1_PEM_evolution_table[currStatus]);
	UINT8 currentMPS = pContInfo->MPS;

	bit = SDD1_BG_getBit(this->BG[pState->code_num], &endOfRun);

	if (endOfRun)
	{
		if (bit)
		{
			if (!(currStatus & 0xfe))
			{
				(pContInfo->MPS) ^= 0x01;
			}
			pContInfo->status = pState->nextIfLPS;
		}
		else
		{
			pContInfo->status = pState->nextIfMPS;
		}
	}

	return bit ^ currentMPS;
}

typedef struct
{
	running_machine* machine;
	UINT8 bitplanesInfo;
	UINT8 contextBitsInfo;
	UINT8 bit_number;
	UINT8 currBitplane;
	UINT16 prevBitplaneBits[8];
	SDD1_PEM* PEM;
} SDD1_CM;

static SDD1_CM* SDD1_CM_ctor(running_machine* machine, SDD1_PEM* associatedPEM)
{
	SDD1_CM* newclass = (SDD1_CM*)auto_alloc_array(machine, UINT8, sizeof(SDD1_CM));
	newclass->machine = machine;
	newclass->PEM = associatedPEM;
	return newclass;
}

static void SDD1_CM_prepareDecomp(SDD1_CM* this, UINT32 first_byte)
{
	INT32 i = 0;
	this->bitplanesInfo = sdd1_read(this->machine, first_byte) & 0xc0;
	this->contextBitsInfo = sdd1_read(this->machine, first_byte) & 0x30;
	this->bit_number = 0;
	for (i = 0; i < 8; i++)
	{
		this->prevBitplaneBits[i] = 0;
	}
	switch(this->bitplanesInfo)
	{
		case 0x00:
			this->currBitplane = 1;
			break;
		case 0x40:
			this->currBitplane = 7;
			break;
		case 0x80:
			this->currBitplane = 3;
			break;
	}
}

static UINT8 SDD1_CM_getBit(SDD1_CM* this)
{
	UINT8 currContext;
	UINT16 *context_bits;
	UINT8 bit = 0;

	switch (this->bitplanesInfo)
	{
		case 0x00:
			this->currBitplane ^= 0x01;
			break;
		case 0x40:
			this->currBitplane ^= 0x01;
			if (!(this->bit_number & 0x7f))
			{
				this->currBitplane = ((this->currBitplane + 2) & 0x07);
			}
			break;
		case 0x80:
			this->currBitplane ^= 0x01;
			if (!(this->bit_number & 0x7f))
			{
				this->currBitplane ^= 0x02;
			}
			break;
		case 0xc0:
			this->currBitplane = this->bit_number & 0x07;
			break;
	}

	context_bits = &(this->prevBitplaneBits)[this->currBitplane];

	currContext = (this->currBitplane & 0x01) << 4;
	switch (this->contextBitsInfo)
	{
		case 0x00:
			currContext |= ((*context_bits & 0x01c0) >> 5) | (*context_bits & 0x0001);
			break;
		case 0x10:
		    currContext |= ((*context_bits & 0x0180) >> 5) | (*context_bits & 0x0001);
			break;
		case 0x20:
			currContext |= ((*context_bits & 0x00c0) >> 5) | (*context_bits & 0x0001);
			break;
		case 0x30:
			currContext |= ((*context_bits & 0x0180) >> 5) | (*context_bits & 0x0003);
			break;
	}

	bit = SDD1_PEM_getBit(this->PEM, currContext);

	*context_bits <<= 1;
	*context_bits |= bit;

	this->bit_number++;

	return bit;
}

typedef struct
{
	running_machine* machine;
	UINT8 bitplanesInfo;
	UINT16 length;
	UINT8* buffer;
	SDD1_CM* CM;
} SDD1_OL;

static SDD1_OL* SDD1_OL_ctor(running_machine* machine, SDD1_CM* associatedCM)
{
	SDD1_OL* newclass = (SDD1_OL*)auto_alloc_array(machine, UINT8, sizeof(SDD1_OL));
	newclass->machine = machine;
	newclass->CM = associatedCM;
	return newclass;
}

static void SDD1_OL_prepareDecomp(SDD1_OL* this, UINT32 first_byte, UINT16 out_len, UINT8 *out_buf)
{
	this->bitplanesInfo = sdd1_read(this->machine, first_byte) & 0xc0;
	this->length = out_len;
	this->buffer = out_buf;
}

static void SDD1_OL_launch(SDD1_OL* this)
{
	UINT8 i;
	UINT8 register1, register2;

	switch(this->bitplanesInfo)
	{
		case 0x00:
		case 0x40:
		case 0x80:
			i = 1;
			do
			{	// if length == 0, we output 2^16 bytes
				if(!i)
				{
					*(this->buffer++) = register2;
					i = ~i;
				}
				else
				{
					for(register1 = register2 = 0, i = 0x80; i; i >>= 1)
					{
						if(SDD1_CM_getBit(this->CM))
						{
							register1 |= i;
						}
						if(SDD1_CM_getBit(this->CM))
						{
							register2 |= i;
						}
					}
					*(this->buffer++) = register1;
				}
			} while(--(this->length));
			break;
		case 0xc0:
			do
			{
				for(register1 = 0, i = 0x01; i; i <<= 1)
				{
					if(SDD1_CM_getBit(this->CM))
					{
						register1 |= i;
					}
				}
				*(this->buffer++) = register1;
			} while(--(this->length));
			break;
	}
}

typedef struct
{
	running_machine* machine;
	SDD1_IM* IM;
	SDD1_GCD* GCD;
	SDD1_BG* BG0;	SDD1_BG* BG1;	SDD1_BG* BG2; 	SDD1_BG* BG3;
	SDD1_BG* BG4;	SDD1_BG* BG5;	SDD1_BG* BG6; 	SDD1_BG* BG7;
	SDD1_PEM* PEM;
	SDD1_CM* CM;
	SDD1_OL* OL;
} SDD1emu;

static SDD1emu* SDD1emu_ctor(running_machine *machine)
{
	SDD1emu* newclass = (SDD1emu*)auto_alloc_array(machine, UINT8, sizeof(SDD1emu));
	newclass->machine = machine;

	newclass->IM = SDD1_IM_ctor(machine);
	newclass->GCD = SDD1_GCD_ctor(machine, newclass->IM);
	newclass->BG0 = SDD1_BG_ctor(machine, newclass->GCD, 0);
	newclass->BG1 = SDD1_BG_ctor(machine, newclass->GCD, 1);
	newclass->BG2 = SDD1_BG_ctor(machine, newclass->GCD, 2);
	newclass->BG3 = SDD1_BG_ctor(machine, newclass->GCD, 3);
	newclass->BG4 = SDD1_BG_ctor(machine, newclass->GCD, 4);
	newclass->BG5 = SDD1_BG_ctor(machine, newclass->GCD, 5);
	newclass->BG6 = SDD1_BG_ctor(machine, newclass->GCD, 6);
	newclass->BG7 = SDD1_BG_ctor(machine, newclass->GCD, 7);
	newclass->PEM = SDD1_PEM_ctor(machine, newclass->BG0, newclass->BG1, newclass->BG2, newclass->BG3,
										   newclass->BG4, newclass->BG5, newclass->BG6, newclass->BG7);
	newclass->CM = SDD1_CM_ctor(machine, newclass->PEM);
	newclass->OL = SDD1_OL_ctor(machine, newclass->CM);
	return newclass;
}

static void SDD1emu_decompress(SDD1emu* this, UINT32 in_buf, UINT16 out_len, UINT8 *out_buf)
{
	SDD1_IM_prepareDecomp(this->IM, in_buf);
	SDD1_BG_prepareDecomp(this->BG0);
	SDD1_BG_prepareDecomp(this->BG1);
	SDD1_BG_prepareDecomp(this->BG2);
	SDD1_BG_prepareDecomp(this->BG3);
	SDD1_BG_prepareDecomp(this->BG4);
	SDD1_BG_prepareDecomp(this->BG5);
	SDD1_BG_prepareDecomp(this->BG6);
	SDD1_BG_prepareDecomp(this->BG7);
	SDD1_PEM_prepareDecomp(this->PEM);
	SDD1_CM_prepareDecomp(this->CM, in_buf);
	SDD1_OL_prepareDecomp(this->OL, in_buf, out_len, out_buf);

	SDD1_OL_launch(this->OL);
}

typedef struct
{
	UINT8 sdd1_enable;	// channel bit-mask
	UINT8 xfer_enable;	// channel bit-mask
	UINT32 mmc[4];		// memory map controller ROM indices

	struct
	{
		UINT32 addr;	// $43x2-$43x4 -- DMA transfer address
		UINT16 size;	// $43x5-$43x6 -- DMA transfer size
	} dma[8];

	SDD1emu* sdd1emu;
	struct
	{
		UINT8 *data;	// pointer to decompressed S-DD1 data (65536 bytes)
		UINT16 offset;	// read index into S-DD1 decompression buffer
		UINT32 size;	// length of data buffer; reads decrement counter, set ready to false at 0
		UINT8 ready;	// 1 when data[] is valid; 0 to invoke sdd1emu.decompress()
	} buffer;
} _snes_sdd1_t;

_snes_sdd1_t snes_sdd1;

static void sdd1_init(running_machine* machine)
{
	snes_sdd1.sdd1emu = SDD1emu_ctor(machine);
	snes_sdd1.buffer.data = auto_alloc_array_clear(machine, UINT8, 0x10000);
}

static UINT8 sdd1_mmio_read(const address_space *space, UINT16 addr)
{
	UINT8 val = 0xff;
	addr &= 0xffff;

	switch(addr)
	{
		case 0x4804:
			val = (snes_sdd1.mmc[0] >> 20) & 7;
		case 0x4805:
			val = (snes_sdd1.mmc[1] >> 20) & 7;
		case 0x4806:
			val = (snes_sdd1.mmc[2] >> 20) & 7;
		case 0x4807:
			val = (snes_sdd1.mmc[3] >> 20) & 7;
	}

	printf("SDD1::mmio_read(%04x) = %02x\n", addr, val);
	return val;
}

static void sdd1_mmio_write(const address_space *space, UINT16 addr, UINT8 data)
{
	printf("SDD1::mmio_write(%04x, %02x)\n", addr, data);
	addr &= 0xffff;

	if((addr & 0x4380) == 0x4300)
	{
		UINT8 channel = (addr >> 4) & 7;
		switch(addr & 15)
		{
			case 2:
				snes_sdd1.dma[channel].addr = (snes_sdd1.dma[channel].addr & 0xffff00) + (data <<  0);
				printf( "dma[%d].addr = %06x\n", channel, snes_sdd1.dma[channel].addr );
				break;
			case 3:
				snes_sdd1.dma[channel].addr = (snes_sdd1.dma[channel].addr & 0xff00ff) + (data <<  8);
				printf( "dma[%d].addr = %06x\n", channel, snes_sdd1.dma[channel].addr );
				break;
			case 4:
				snes_sdd1.dma[channel].addr = (snes_sdd1.dma[channel].addr & 0x00ffff) + (data << 16);
				printf( "dma[%d].addr = %06x\n", channel, snes_sdd1.dma[channel].addr );
				break;

			case 5:
				snes_sdd1.dma[channel].size = (snes_sdd1.dma[channel].size &   0xff00) + (data <<  0);
				printf( "dma[%d].size = %04x\n", channel, snes_sdd1.dma[channel].size );
				break;
			case 6:
				snes_sdd1.dma[channel].size = (snes_sdd1.dma[channel].size &   0x00ff) + (data <<  8);
				printf( "dma[%d].size = %04x\n", channel, snes_sdd1.dma[channel].size );
				break;
		}
		snes_w_io(space, addr & 0x7f, data);
		return;
	}

	switch(addr)
	{
		case 0x4800:
			snes_sdd1.sdd1_enable = data;
			break;
		case 0x4801:
			snes_sdd1.xfer_enable = data;
			break;

		case 0x4804:
			snes_sdd1.mmc[0] = (data & 7) << 20;
			break;
		case 0x4805:
			snes_sdd1.mmc[1] = (data & 7) << 20;
			break;
		case 0x4806:
			snes_sdd1.mmc[2] = (data & 7) << 20;
			break;
		case 0x4807:
			snes_sdd1.mmc[3] = (data & 7) << 20;
			break;
	}
}

static UINT8 sdd1_read(running_machine* machine, UINT32 addr)
{
	printf( "sdd1.read(%06x)\n", addr );
	if(snes_sdd1.sdd1_enable & snes_sdd1.xfer_enable)
	{
		// at least one channel has S-DD1 decompression enabled...
		UINT32 i;
		printf( "    sdd1_enable and xfer_enable are set, let's go\n" );
		for(i = 0; i < 8; i++)
		{
			if(snes_sdd1.sdd1_enable & snes_sdd1.xfer_enable & (1 << i))
			{
				printf( "    channel %d is enabled\n", i );
				// S-DD1 always uses fixed transfer mode, so address will not change during transfer
				if(addr == snes_sdd1.dma[i].addr)
				{
					UINT8 data;
					printf( "    addresses match\n" );
					if(!snes_sdd1.buffer.ready)
					{
						UINT8 temp;
						// first byte read for channel performs full decompression.
						// this really should stream byte-by-byte, but it's not necessary since the size is known
						snes_sdd1.buffer.offset = 0;
						snes_sdd1.buffer.size = snes_sdd1.dma[i].size ? snes_sdd1.dma[i].size : 65536;

						// sdd1emu calls this function; it needs to access uncompressed data;
						// so temporarily disable decompression mode for decompress() call.
						temp = snes_sdd1.sdd1_enable;
						snes_sdd1.sdd1_enable = 0;
						printf( "    buffer is not ready, decompressing %05x bytes at addr %06x\n", snes_sdd1.buffer.size, addr );
						SDD1emu_decompress(snes_sdd1.sdd1emu, addr, snes_sdd1.buffer.size, snes_sdd1.buffer.data);
						snes_sdd1.sdd1_enable = temp;

						snes_sdd1.buffer.ready = 1;
					}

					// fetch a decompressed byte; once buffer is depleted, disable channel and invalidate buffer
					data = snes_sdd1.buffer.data[(UINT16)snes_sdd1.buffer.offset++];
					printf( "    fetched byte: %02x\n", data );
					if(snes_sdd1.buffer.offset >= snes_sdd1.buffer.size)
					{
						printf( "    we're done\n" );
						snes_sdd1.buffer.ready = 0;
						snes_sdd1.xfer_enable &= ~(1 << i);
					}

					return data;
				} // address matched
			} // channel enabled
		} // channel loop
	} // S-DD1 decompressor enabled

	printf( "returning a generic memory read from %06x\n", snes_sdd1.mmc[(addr >> 20) & 3] + (addr & 0x0fffff) );
	return memory_read_byte(cputag_get_address_space(machine, "maincpu", ADDRESS_SPACE_PROGRAM), snes_sdd1.mmc[(addr >> 20) & 3] + (addr & 0x0fffff));
}

static void sdd1_reset(running_machine *machine)
{
	UINT8 i;

	snes_sdd1.sdd1_enable = 0;
	snes_sdd1.xfer_enable = 0;

	snes_sdd1.mmc[0] = 0 << 20;
	snes_sdd1.mmc[1] = 1 << 20;
	snes_sdd1.mmc[2] = 2 << 20;
	snes_sdd1.mmc[3] = 3 << 20;

	for(i = 0; i < 8; i++)
	{
		snes_sdd1.dma[i].addr = 0;
		snes_sdd1.dma[i].size = 0;
	}

	snes_sdd1.buffer.ready = 0;
}
