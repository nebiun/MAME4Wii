#define MOO_DEBUG 0
#define MOO_DMADELAY (100)

/***************************************************************************

 Wild West C.O.W.boys of Moo Mesa
 Bucky O'Hare
 (c) 1992 Konami
 Driver by R. Belmont and Acho A. Tang based on xexex.c by Olivier Galibert.
 Moo Mesa protection information thanks to ElSemi and OG.

 These are the final Xexex hardware games before the pre-GX/Mystic Warriors
 hardware took over.

Bug Fixes and Outstanding Issues
--------------------------------
Moo:
 - 54338 color blender support. Works fine with Bucky but needs a correct
   enable/disable bit in Moo. (intro gfx missing and fog blocking view)
 - Enemies coming out of the jail cells in the last stage have wrong priority.
   Could be tile priority or the typical "small Z, big pri" sprite masking
   trick currently not supported by K053247_sprites_draw().

Moo (bootleg):
 - No sprites appear, and the game never enables '246 interrupts.  Of course,
   they're using some copy of a '246 on FPGAs, so who knows what's going on...

Bucky:
 - Shadows sometimes have wrong priority. (unsupported priority modes)
 - Gaps between zoomed sprites. (fraction round-off)
 - Rogue sprites keep popping on screen after stage 2. They can usually be
   found near 950000 with sprite code around 5e40 or f400. The GFX viewer
   only shows blanks at 5e40, however. Are they invalid data from bad
   sprite ROMs or markers that aren't supposed to be displayed? These
   artifacts have one thing in common: they all have zero zcode. In fact
   no other sprites in Bucky seems to have zero zcode.

   Update: More garbages seen in later stages with a great variety.
   There's enough indication to assume Bucky simply ignores sprites with
   zero Z. I wonder why nobody reported this.

***************************************************************************/
#if 0

#include "driver.h"
#include "video/konamiic.h"
#include "cpu/m68000/m68000.h"
#include "cpu/z80/z80.h"
#include "machine/eeprom.h"
#include "sound/2151intf.h"
#include "sound/okim6295.h"
#include "sound/k054539.h"
#include "konamipt.h"

VIDEO_START(moo);
VIDEO_UPDATE(moo);

static int init_eeprom_count, init_nosound_count, game_type;
static UINT16 *workram;
static UINT16 protram[16];
static UINT16 cur_control2;


static const eeprom_interface eeprom_intf =
{
	7,			/* address bits */
	8,			/* data bits */
	"011000",		/* read command */
	"011100",		/* write command */
	"0100100000000",	/* erase command */
	"0100000000000",	/* lock command */
	"0100110000000" 	/* unlock command */
};

static NVRAM_HANDLER( moo )
{
	if (read_or_write)
		eeprom_save(file);
	else
	{
		eeprom_init(machine, &eeprom_intf);

		if (file)
		{
			init_eeprom_count = 0;
			eeprom_load(file);
		}
		else
			init_eeprom_count = 10;
	}
}

static READ16_HANDLER( control1_r )
{
	int res;

	/* bit 0 is EEPROM data */
	/* bit 1 is EEPROM ready */
	/* bit 3 is service button */
	/* bits 4-7 are DIP switches */
	res = input_port_read(space->machine, "IN1");

	if (init_eeprom_count)
	{
		init_eeprom_count--;
		res &= 0xf7;
	}

	return res;
}

static READ16_HANDLER( control2_r )
{
	return cur_control2;
}

static WRITE16_HANDLER( control2_w )
{
	/* bit 0  is data */
	/* bit 1  is cs (active low) */
	/* bit 2  is clock (active high) */
	/* bit 5  is enable irq 5 (unconfirmed) */
	/* bit 8  is enable sprite ROM reading */
	/* bit 10 is watchdog */
	/* bit 11 is enable irq 4 (unconfirmed) */

	COMBINE_DATA(&cur_control2);

	eeprom_write_bit(cur_control2 & 0x01);
	eeprom_set_cs_line((cur_control2 & 0x02) ? CLEAR_LINE : ASSERT_LINE);
	eeprom_set_clock_line((cur_control2 & 0x04) ? ASSERT_LINE : CLEAR_LINE);

	if (data & 0x100)
	{
		K053246_set_OBJCHA_line(ASSERT_LINE);
	}
	else
	{
		K053246_set_OBJCHA_line(CLEAR_LINE);
	}
}


static void moo_objdma(int type)
{
	int counter, num_inactive;
	UINT16 *src, *dst, zmask;

	K053247_export_config(&dst, 0, 0, 0, &counter);
	src = spriteram16;
	num_inactive = counter = 256;

	zmask = (type) ? 0x00ff : 0xffff;

	do {
		if ((*src & 0x8000) && (*src & zmask))
		{
			memcpy(dst, src, 0x10);
			dst += 8;
			num_inactive--;
		}
		src += 0x80;
	}
	while (--counter);

	if (num_inactive) do { *dst = 0; dst += 8; } while (--num_inactive);
}

static TIMER_CALLBACK( dmaend_callback )
{
	if (cur_control2 & 0x800)
		cputag_set_input_line(machine, "maincpu", 4, HOLD_LINE);
}

static INTERRUPT_GEN(moo_interrupt)
{
	if (K053246_is_IRQ_enabled())
	{
		moo_objdma(game_type);

		// schedule DMA end interrupt (delay shortened to catch up with V-blank)
		timer_set(device->machine, ATTOTIME_IN_USEC(MOO_DMADELAY), NULL, 0, dmaend_callback);
	}

	// trigger V-blank interrupt
	if (cur_control2 & 0x20)
		cpu_set_input_line(device, 5, HOLD_LINE);
}

static INTERRUPT_GEN(moobl_interrupt)
{
	moo_objdma(game_type);

	// schedule DMA end interrupt (delay shortened to catch up with V-blank)
	timer_set(device->machine, ATTOTIME_IN_USEC(MOO_DMADELAY), NULL, 0, dmaend_callback);

	// trigger V-blank interrupt
	cpu_set_input_line(device, 5, HOLD_LINE);
}

static WRITE16_HANDLER( sound_cmd1_w )
{
	if((data & 0x00ff0000) == 0) {
		data &= 0xff;
		soundlatch_w(space, 0, data);
	}
}

static WRITE16_HANDLER( sound_cmd2_w )
{
	if((data & 0x00ff0000) == 0) {
		soundlatch2_w(space, 0, data & 0xff);
	}
}

static WRITE16_HANDLER( sound_irq_w )
{
	cputag_set_input_line(space->machine, "soundcpu", 0, HOLD_LINE);
}

static READ16_HANDLER( sound_status_r )
{
	return soundlatch3_r(space,0);
}

static WRITE8_HANDLER( sound_bankswitch_w )
{
	memory_set_bankptr(space->machine, 1, memory_region(space->machine, "soundcpu") + 0x10000 + (data&0xf)*0x4000);
}


#if 0 // (for reference; do not remove)

/* the interface with the 053247 is weird. The chip can address only 0x1000 bytes */
/* of RAM, but they put 0x10000 there. The CPU can access them all. */
static READ16_HANDLER( K053247_scattered_word_r )
{
	if (offset & 0x0078)
		return spriteram16[offset];
	else
	{
		offset = (offset & 0x0007) | ((offset & 0x7f80) >> 4);
		return K053247_word_r(space,offset,mem_mask);
	}
}

static WRITE16_HANDLER( K053247_scattered_word_w )
{
	if (offset & 0x0078)
		COMBINE_DATA(spriteram16+offset);
	else
	{
		offset = (offset & 0x0007) | ((offset & 0x7f80) >> 4);

		K053247_word_w(space,offset,data,mem_mask);
	}
}

#endif


static WRITE16_HANDLER( moo_prot_w )
{
	UINT32 src1, src2, dst, length, a, b, res;

	COMBINE_DATA(&protram[offset]);

	if (offset == 0xc)	// trigger operation
	{
		src1 = (protram[1]&0xff)<<16 | protram[0];
		src2 = (protram[3]&0xff)<<16 | protram[2];
		dst = (protram[5]&0xff)<<16 | protram[4];
		length = protram[0xf];

		while (length)
		{
			a = memory_read_word(space, src1);
			b = memory_read_word(space, src2);
			res = a+2*b;

			memory_write_word(space, dst, res);

			src1 += 2;
			src2 += 2;
			dst += 2;
			length--;
		}
	}
}


static WRITE16_DEVICE_HANDLER( moobl_oki_bank_w )
{
	logerror("%x to OKI bank\n", data);

	okim6295_set_bank_base(device, (data & 0x0f)* 0x40000);
}

static ADDRESS_MAP_START( moo_map, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x07ffff) AM_ROM
	AM_RANGE(0x0c0000, 0x0c003f) AM_WRITE(K056832_word_w)
	AM_RANGE(0x0c2000, 0x0c2007) AM_WRITE(K053246_word_w)

	AM_RANGE(0x0c4000, 0x0c4001) AM_READ(K053246_word_r)
	AM_RANGE(0x0ca000, 0x0ca01f) AM_WRITE(K054338_word_w)		/* K054338 alpha blending engine */
	AM_RANGE(0x0cc000, 0x0cc01f) AM_WRITE(K053251_lsb_w)
	AM_RANGE(0x0ce000, 0x0ce01f) AM_WRITE(moo_prot_w)
	AM_RANGE(0x0d0000, 0x0d001f) AM_WRITEONLY					/* CCU regs (ignored) */
	AM_RANGE(0x0d4000, 0x0d4001) AM_WRITE(sound_irq_w)
	AM_RANGE(0x0d600c, 0x0d600d) AM_WRITE(sound_cmd1_w)
	AM_RANGE(0x0d600e, 0x0d600f) AM_WRITE(sound_cmd2_w)
	AM_RANGE(0x0d6014, 0x0d6015) AM_READ(sound_status_r)
	AM_RANGE(0x0d6000, 0x0d601f) AM_RAM							/* sound regs fall through */
	AM_RANGE(0x0d8000, 0x0d8007) AM_WRITE(K056832_b_word_w)		/* VSCCS regs */
	AM_RANGE(0x0da000, 0x0da001) AM_READ_PORT("P1_P3")
	AM_RANGE(0x0da002, 0x0da003) AM_READ_PORT("P2_P4")
	AM_RANGE(0x0dc000, 0x0dc001) AM_READ_PORT("IN0")
	AM_RANGE(0x0dc002, 0x0dc003) AM_READ(control1_r)
	AM_RANGE(0x0de000, 0x0de001) AM_READWRITE(control2_r,control2_w)
	AM_RANGE(0x100000, 0x17ffff) AM_ROM
	AM_RANGE(0x180000, 0x18ffff) AM_RAM	AM_BASE(&workram)		/* Work RAM */
	AM_RANGE(0x190000, 0x19ffff) AM_RAM AM_BASE(&spriteram16)	/* Sprite RAM */
	AM_RANGE(0x1a0000, 0x1a1fff) AM_READWRITE(K056832_ram_word_r,K056832_ram_word_w)	/* Graphic planes */
	AM_RANGE(0x1a2000, 0x1a3fff) AM_READWRITE(K056832_ram_word_r,K056832_ram_word_w)	/* Graphic planes mirror */
	AM_RANGE(0x1b0000, 0x1b1fff) AM_READ(K056832_rom_word_r)	/* Passthrough to tile roms */
	AM_RANGE(0x1c0000, 0x1c1fff) AM_RAM_WRITE(paletteram16_xrgb_word_be_w) AM_BASE(&paletteram16)
#if MOO_DEBUG
	AM_RANGE(0x0c0000, 0x0c003f) AM_READ(K056832_word_r)
	AM_RANGE(0x0c2000, 0x0c2007) AM_READ(K053246_reg_word_r)
	AM_RANGE(0x0ca000, 0x0ca01f) AM_READ(K054338_word_r)
	AM_RANGE(0x0cc000, 0x0cc01f) AM_READ(K053251_lsb_r)
	AM_RANGE(0x0d0000, 0x0d001f) AM_RAM
	AM_RANGE(0x0d8000, 0x0d8007) AM_READ(K056832_b_word_r)
#endif
ADDRESS_MAP_END

static ADDRESS_MAP_START( moobl_map, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x07ffff) AM_ROM
	AM_RANGE(0x0c0000, 0x0c003f) AM_WRITE(K056832_word_w)
	AM_RANGE(0x0c2000, 0x0c2007) AM_WRITE(K053246_word_w)
	AM_RANGE(0x0c2f00, 0x0c2f01) AM_READNOP						/* heck if I know, but it's polled constantly */
	AM_RANGE(0x0c4000, 0x0c4001) AM_READ(K053246_word_r)
	AM_RANGE(0x0ca000, 0x0ca01f) AM_WRITE(K054338_word_w)       /* K054338 alpha blending engine */
	AM_RANGE(0x0cc000, 0x0cc01f) AM_WRITE(K053251_lsb_w)
	AM_RANGE(0x0d0000, 0x0d001f) AM_WRITEONLY		            /* CCU regs (ignored) */
	AM_RANGE(0x0d6ffc, 0x0d6ffd) AM_DEVWRITE("oki", moobl_oki_bank_w)
	AM_RANGE(0x0d6ffe, 0x0d6fff) AM_DEVREADWRITE8("oki", okim6295_r,okim6295_w, 0x00ff)
	AM_RANGE(0x0d8000, 0x0d8007) AM_WRITE(K056832_b_word_w)     /* VSCCS regs */
	AM_RANGE(0x0da000, 0x0da001) AM_READ_PORT("P1_P3")
	AM_RANGE(0x0da002, 0x0da003) AM_READ_PORT("P2_P4")
	AM_RANGE(0x0dc000, 0x0dc001) AM_READ_PORT("IN0")
	AM_RANGE(0x0dc002, 0x0dc003) AM_READ(control1_r)
	AM_RANGE(0x0de000, 0x0de001) AM_READWRITE(control2_r,control2_w)
	AM_RANGE(0x100000, 0x17ffff) AM_ROM
	AM_RANGE(0x180000, 0x18ffff) AM_RAM AM_BASE(&workram)		 /* Work RAM */
	AM_RANGE(0x190000, 0x19ffff) AM_RAM AM_BASE(&spriteram16) 	 /* Sprite RAM */
	AM_RANGE(0x1a0000, 0x1a1fff) AM_READWRITE(K056832_ram_word_r,K056832_ram_word_w) /* Graphic planes */
	AM_RANGE(0x1a2000, 0x1a3fff) AM_READWRITE(K056832_ram_word_r,K056832_ram_word_w)	/* Graphic planes mirror */
	AM_RANGE(0x1b0000, 0x1b1fff) AM_READ(K056832_rom_word_r)	/* Passthrough to tile roms */
	AM_RANGE(0x1c0000, 0x1c1fff) AM_RAM_WRITE(paletteram16_xrgb_word_be_w) AM_BASE(&paletteram16)
ADDRESS_MAP_END

static ADDRESS_MAP_START( bucky_map, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x07ffff) AM_ROM
	AM_RANGE(0x080000, 0x08ffff) AM_RAM
	AM_RANGE(0x090000, 0x09ffff) AM_RAM AM_BASE(&spriteram16)	/* Sprite RAM */
	AM_RANGE(0x0a0000, 0x0affff) AM_RAM							/* extra sprite RAM? */
	AM_RANGE(0x0c0000, 0x0c003f) AM_WRITE(K056832_word_w)
	AM_RANGE(0x0c2000, 0x0c2007) AM_WRITE(K053246_word_w)
	AM_RANGE(0x0c4000, 0x0c4001) AM_READ(K053246_word_r)
	AM_RANGE(0x0ca000, 0x0ca01f) AM_WRITE(K054338_word_w)		/* K054338 alpha blending engine */
	AM_RANGE(0x0cc000, 0x0cc01f) AM_WRITE(K053251_lsb_w)
	AM_RANGE(0x0ce000, 0x0ce01f) AM_WRITE(moo_prot_w)
	AM_RANGE(0x0d0000, 0x0d001f) AM_WRITEONLY					/* CCU regs (ignored) */
	AM_RANGE(0x0d2000, 0x0d20ff) AM_READWRITE(K054000_lsb_r,K054000_lsb_w)
	AM_RANGE(0x0d4000, 0x0d4001) AM_WRITE(sound_irq_w)
	AM_RANGE(0x0d600c, 0x0d600d) AM_WRITE(sound_cmd1_w)
	AM_RANGE(0x0d600e, 0x0d600f) AM_WRITE(sound_cmd2_w)
	AM_RANGE(0x0d6014, 0x0d6015) AM_READ(sound_status_r)
	AM_RANGE(0x0d6000, 0x0d601f) AM_RAM							/* sound regs fall through */
	AM_RANGE(0x0d8000, 0x0d8007) AM_WRITE(K056832_b_word_w)		/* VSCCS regs */
	AM_RANGE(0x0da000, 0x0da001) AM_READ_PORT("P1_P3")
	AM_RANGE(0x0da002, 0x0da003) AM_READ_PORT("P2_P4")
	AM_RANGE(0x0dc000, 0x0dc001) AM_READ_PORT("IN0")
	AM_RANGE(0x0dc002, 0x0dc003) AM_READ(control1_r)
	AM_RANGE(0x0de000, 0x0de001) AM_READWRITE(control2_r,control2_w)
	AM_RANGE(0x180000, 0x181fff) AM_READWRITE(K056832_ram_word_r,K056832_ram_word_w)	/* Graphic planes */
	AM_RANGE(0x182000, 0x183fff) AM_READWRITE(K056832_ram_word_r,K056832_ram_word_w)	/* Graphic planes mirror */
	AM_RANGE(0x184000, 0x187fff) AM_RAM							/* extra tile RAM? */
	AM_RANGE(0x190000, 0x191fff) AM_READ(K056832_rom_word_r)	/* Passthrough to tile roms */
	AM_RANGE(0x1b0000, 0x1b3fff) AM_RAM_WRITE(paletteram16_xrgb_word_be_w) AM_BASE(&paletteram16)
	AM_RANGE(0x200000, 0x23ffff) AM_ROM							/* data */
#if MOO_DEBUG
	AM_RANGE(0x0c0000, 0x0c003f) AM_READ(K056832_word_r)
	AM_RANGE(0x0c2000, 0x0c2007) AM_READ(K053246_reg_word_r)
	AM_RANGE(0x0ca000, 0x0ca01f) AM_READ(K054338_word_r)
	AM_RANGE(0x0cc000, 0x0cc01f) AM_READ(K053251_lsb_r)
	AM_RANGE(0x0d0000, 0x0d001f) AM_RAM
	AM_RANGE(0x0d8000, 0x0d8007) AM_READ(K056832_b_word_r)
#endif
ADDRESS_MAP_END

static ADDRESS_MAP_START( sound_map, ADDRESS_SPACE_PROGRAM, 8 )
	AM_RANGE(0x0000, 0x7fff) AM_ROM
	AM_RANGE(0x8000, 0xbfff) AM_ROMBANK(1)
	AM_RANGE(0xc000, 0xdfff) AM_RAM
	AM_RANGE(0xe000, 0xe22f) AM_DEVREADWRITE("konami", k054539_r,k054539_w)
	AM_RANGE(0xec00, 0xec01) AM_DEVREADWRITE("ym", ym2151_r,ym2151_w)
	AM_RANGE(0xf000, 0xf000) AM_WRITE(soundlatch3_w)
	AM_RANGE(0xf002, 0xf002) AM_READ(soundlatch_r)
	AM_RANGE(0xf003, 0xf003) AM_READ(soundlatch2_r)
	AM_RANGE(0xf800, 0xf800) AM_WRITE(sound_bankswitch_w)
ADDRESS_MAP_END

static INPUT_PORTS_START( moo )
	PORT_START("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_SERVICE2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_SERVICE3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_SERVICE4 )

	PORT_START("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SPECIAL ) PORT_CUSTOM(eeprom_bit_r, NULL)	/* EEPROM data */
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_SPECIAL )	/* EEPROM ready (always 1) */
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNKNOWN )
	PORT_SERVICE_NO_TOGGLE(0x08, IP_ACTIVE_LOW)
	PORT_DIPNAME( 0x10, 0x00, "Sound Output")
	PORT_DIPSETTING(    0x10, DEF_STR( Mono ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Stereo ) )
	PORT_DIPNAME( 0x20, 0x20, "Coin Mechanism")
	PORT_DIPSETTING(    0x20, "Common")
	PORT_DIPSETTING(    0x00, "Independent")
	PORT_DIPNAME( 0xc0, 0x80, "Number of Players")
	PORT_DIPSETTING(    0xc0, "2")
	PORT_DIPSETTING(    0x40, "3")
	PORT_DIPSETTING(    0x80, "4")

	PORT_START("P1_P3")
	KONAMI16_LSB( 1, IPT_UNKNOWN, IPT_START1 )
	KONAMI16_MSB( 3, IPT_UNKNOWN, IPT_START3 )

	PORT_START("P2_P4")
	KONAMI16_LSB( 2, IPT_UNKNOWN, IPT_START2 )
	KONAMI16_MSB( 4, IPT_UNKNOWN, IPT_START4 )
INPUT_PORTS_END

/* Same as 'moo', but additional "Button 3" for all players */
static INPUT_PORTS_START( bucky )
	PORT_INCLUDE( moo )

	PORT_MODIFY("P1_P3")
	KONAMI16_LSB( 1, IPT_BUTTON3, IPT_START1 )
	KONAMI16_MSB( 3, IPT_BUTTON3, IPT_START3 )

	PORT_MODIFY("P2_P4")
	KONAMI16_LSB( 2, IPT_BUTTON3, IPT_START2 )
	KONAMI16_MSB( 4, IPT_BUTTON3, IPT_START4 )
INPUT_PORTS_END


static MACHINE_START( moo )
{
	state_save_register_global(machine, cur_control2);
	state_save_register_global_array(machine, protram);
}

static MACHINE_RESET( moo )
{
	init_nosound_count = 0;
}


static MACHINE_DRIVER_START( moo )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", M68000, 16000000)
	MDRV_CPU_PROGRAM_MAP(moo_map)
	MDRV_CPU_VBLANK_INT("screen", moo_interrupt)

	MDRV_CPU_ADD("soundcpu", Z80, 8000000)
	MDRV_CPU_PROGRAM_MAP(sound_map)

	MDRV_MACHINE_START(moo)
	MDRV_MACHINE_RESET(moo)

	MDRV_NVRAM_HANDLER(moo)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_HAS_SHADOWS | VIDEO_HAS_HIGHLIGHTS | VIDEO_UPDATE_AFTER_VBLANK)

	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(1200))	 // should give IRQ4 sufficient time to update scroll registers
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_RGB32)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(40, 40+384-1, 16, 16+224-1)

	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(moo)
	MDRV_VIDEO_UPDATE(moo)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MDRV_SOUND_ADD("ym", YM2151, 4000000)
	MDRV_SOUND_ROUTE(0, "lspeaker", 0.50)
	MDRV_SOUND_ROUTE(1, "rspeaker", 0.50)

	MDRV_SOUND_ADD("konami", K054539, 48000)
	MDRV_SOUND_ROUTE(0, "lspeaker", 0.75)
	MDRV_SOUND_ROUTE(1, "rspeaker", 0.75)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( moobl )

	/* basic machine hardware */
	MDRV_CPU_ADD("maincpu", M68000, 16100000)
	MDRV_CPU_PROGRAM_MAP(moobl_map)
	MDRV_CPU_VBLANK_INT("screen", moobl_interrupt)

	MDRV_MACHINE_START(moo)
	MDRV_MACHINE_RESET(moo)
	MDRV_NVRAM_HANDLER(moo)

	/* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_HAS_SHADOWS | VIDEO_HAS_HIGHLIGHTS | VIDEO_UPDATE_AFTER_VBLANK)

	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(1200)) // should give IRQ4 sufficient time to update scroll registers
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_RGB32)
	MDRV_SCREEN_SIZE(64*8, 32*8)
	MDRV_SCREEN_VISIBLE_AREA(40, 40+384-1, 16, 16+224-1)

	MDRV_PALETTE_LENGTH(2048)

	MDRV_VIDEO_START(moo)
	MDRV_VIDEO_UPDATE(moo)

	/* sound hardware */
	MDRV_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MDRV_SOUND_ADD("oki", OKIM6295, 1056000)
	MDRV_SOUND_CONFIG(okim6295_interface_pin7high) // clock frequency & pin 7 not verified
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "lspeaker", 1.0)
	MDRV_SOUND_ROUTE(ALL_OUTPUTS, "rspeaker", 1.0)
MACHINE_DRIVER_END

static MACHINE_DRIVER_START( bucky )
	MDRV_IMPORT_FROM(moo)

	MDRV_CPU_MODIFY("maincpu")
	MDRV_CPU_PROGRAM_MAP(bucky_map)

	/* video hardware */
	MDRV_PALETTE_LENGTH(4096)
MACHINE_DRIVER_END



ROM_START( moo ) /* Version EA - Euro Ver A */
	ROM_REGION( 0x180000, "maincpu", 0 )
	/* main program */
	ROM_LOAD16_BYTE( "151b01",    0x000000,  0x40000, CRC(fb2fa298) SHA1(f03b24681a2b329ba797fd2780ac9a3cf862ebcb) )
	ROM_LOAD16_BYTE( "151b02.ea", 0x000001,  0x40000, CRC(37b30c01) SHA1(cb91739097a4a36f8f8d92998d822ffc851e1279) )

	/* data */
	ROM_LOAD16_BYTE( "151a03", 0x100000,  0x40000, CRC(c896d3ea) SHA1(ea83c63e2c3dbc4f1e1d49f1852a78ffc1f0ea4b) )
	ROM_LOAD16_BYTE( "151a04", 0x100001,  0x40000, CRC(3b24706a) SHA1(c2a77944284e35ff57f0774fa7b67e53d3b63e1f) )

	ROM_REGION( 0x050000, "soundcpu", 0 )
	/* Z80 sound program */
	ROM_LOAD( "151a07", 0x000000, 0x040000, CRC(cde247fc) SHA1(cdee0228db55d53ae43d7cd2d9001dadd20c2c61) )
	ROM_RELOAD(         0x010000, 0x040000 )

	ROM_REGION( 0x200000, "gfx1", 0 )
	/* tilemaps */
	ROM_LOAD( "151a05", 0x000000, 0x100000, CRC(bc616249) SHA1(58c1f1a03ce9bead8f79d12ce4b2d342432b24b5) )
	ROM_LOAD( "151a06", 0x100000, 0x100000, CRC(38dbcac1) SHA1(c357779733921695b20ac586db5b475f5b2b8f4c) )

	ROM_REGION( 0x800000, "gfx2", 0 )
	/* sprites */
	ROM_LOAD( "151a10", 0x000000, 0x200000, CRC(376c64f1) SHA1(eb69c5a27f9795e28f04a503955132f0a9e4de12) )
	ROM_LOAD( "151a11", 0x200000, 0x200000, CRC(e7f49225) SHA1(1255b214f29b6507540dad5892c60a7ae2aafc5c) )
	ROM_LOAD( "151a12", 0x400000, 0x200000, CRC(4978555f) SHA1(d9871f21d0c8a512b408e137e2e80e9392c2bf6f) )
	ROM_LOAD( "151a13", 0x600000, 0x200000, CRC(4771f525) SHA1(218d86b6230919b5db0304dac00513eb6b27ba9a) )

	ROM_REGION( 0x200000, "konami", 0 )
	/* K054539 samples */
	ROM_LOAD( "151a08", 0x000000, 0x200000, CRC(962251d7) SHA1(32dccf515d2ca8eeffb45cada3dcc60089991b77) )
ROM_END

ROM_START( mooua ) /* Version UA - USA Ver A */
	ROM_REGION( 0x180000, "maincpu", 0 )
	/* main program */
	ROM_LOAD16_BYTE( "151b01",    0x000000,  0x40000, CRC(fb2fa298) SHA1(f03b24681a2b329ba797fd2780ac9a3cf862ebcb) )
	ROM_LOAD16_BYTE( "151b02.ua", 0x000001,  0x40000, CRC(3d9f4d59) SHA1(db47044bd4935fce94ec659242c9819c30eb6d0f) )

	/* data */
	ROM_LOAD16_BYTE( "151a03", 0x100000,  0x40000, CRC(c896d3ea) SHA1(ea83c63e2c3dbc4f1e1d49f1852a78ffc1f0ea4b) )
	ROM_LOAD16_BYTE( "151a04", 0x100001,  0x40000, CRC(3b24706a) SHA1(c2a77944284e35ff57f0774fa7b67e53d3b63e1f) )

	ROM_REGION( 0x050000, "soundcpu", 0 )
	/* Z80 sound program */
	ROM_LOAD( "151a07", 0x000000, 0x040000, CRC(cde247fc) SHA1(cdee0228db55d53ae43d7cd2d9001dadd20c2c61) )
	ROM_RELOAD(         0x010000, 0x040000 )

	ROM_REGION( 0x200000, "gfx1", 0 )
	/* tilemaps */
	ROM_LOAD( "151a05", 0x000000, 0x100000, CRC(bc616249) SHA1(58c1f1a03ce9bead8f79d12ce4b2d342432b24b5) )
	ROM_LOAD( "151a06", 0x100000, 0x100000, CRC(38dbcac1) SHA1(c357779733921695b20ac586db5b475f5b2b8f4c) )

	ROM_REGION( 0x800000, "gfx2", 0 )
	/* sprites */
	ROM_LOAD( "151a10", 0x000000, 0x200000, CRC(376c64f1) SHA1(eb69c5a27f9795e28f04a503955132f0a9e4de12) )
	ROM_LOAD( "151a11", 0x200000, 0x200000, CRC(e7f49225) SHA1(1255b214f29b6507540dad5892c60a7ae2aafc5c) )
	ROM_LOAD( "151a12", 0x400000, 0x200000, CRC(4978555f) SHA1(d9871f21d0c8a512b408e137e2e80e9392c2bf6f) )
	ROM_LOAD( "151a13", 0x600000, 0x200000, CRC(4771f525) SHA1(218d86b6230919b5db0304dac00513eb6b27ba9a) )

	ROM_REGION( 0x200000, "konami", 0 )
	/* K054539 samples */
	ROM_LOAD( "151a08", 0x000000, 0x200000, CRC(962251d7) SHA1(32dccf515d2ca8eeffb45cada3dcc60089991b77) )
ROM_END

ROM_START( mooaa ) /* Version AA - Asia Ver A */
	ROM_REGION( 0x180000, "maincpu", 0 )
	/* main program */
	ROM_LOAD16_BYTE( "151b01",    0x000000,  0x40000, CRC(fb2fa298) SHA1(f03b24681a2b329ba797fd2780ac9a3cf862ebcb) )
	ROM_LOAD16_BYTE( "151b02.aa", 0x000001,  0x40000, CRC(2162d593) SHA1(a6cfe4a57b3f22b2aa0f04f91acefe3b7bea9e76) )

	/* data */
	ROM_LOAD16_BYTE( "151a03", 0x100000,  0x40000, CRC(c896d3ea) SHA1(ea83c63e2c3dbc4f1e1d49f1852a78ffc1f0ea4b) )
	ROM_LOAD16_BYTE( "151a04", 0x100001,  0x40000, CRC(3b24706a) SHA1(c2a77944284e35ff57f0774fa7b67e53d3b63e1f) )

	ROM_REGION( 0x050000, "soundcpu", 0 )
	/* Z80 sound program */
	ROM_LOAD( "151a07", 0x000000, 0x040000, CRC(cde247fc) SHA1(cdee0228db55d53ae43d7cd2d9001dadd20c2c61) )
	ROM_RELOAD(         0x010000, 0x040000 )

	ROM_REGION( 0x200000, "gfx1", 0 )
	/* tilemaps */
	ROM_LOAD( "151a05", 0x000000, 0x100000, CRC(bc616249) SHA1(58c1f1a03ce9bead8f79d12ce4b2d342432b24b5) )
	ROM_LOAD( "151a06", 0x100000, 0x100000, CRC(38dbcac1) SHA1(c357779733921695b20ac586db5b475f5b2b8f4c) )

	ROM_REGION( 0x800000, "gfx2", 0 )
	/* sprites */
	ROM_LOAD( "151a10", 0x000000, 0x200000, CRC(376c64f1) SHA1(eb69c5a27f9795e28f04a503955132f0a9e4de12) )
	ROM_LOAD( "151a11", 0x200000, 0x200000, CRC(e7f49225) SHA1(1255b214f29b6507540dad5892c60a7ae2aafc5c) )
	ROM_LOAD( "151a12", 0x400000, 0x200000, CRC(4978555f) SHA1(d9871f21d0c8a512b408e137e2e80e9392c2bf6f) )
	ROM_LOAD( "151a13", 0x600000, 0x200000, CRC(4771f525) SHA1(218d86b6230919b5db0304dac00513eb6b27ba9a) )

	ROM_REGION( 0x200000, "konami", 0 )
	/* K054539 samples */
	ROM_LOAD( "151a08", 0x000000, 0x200000, CRC(962251d7) SHA1(32dccf515d2ca8eeffb45cada3dcc60089991b77) )
ROM_END

ROM_START( bucky )
	ROM_REGION( 0x240000, "maincpu", 0 )
	/* main program */
	ROM_LOAD16_BYTE( "173_ea_b01.q5", 0x000000,  0x40000, CRC(7785ac8a) SHA1(ef78d14f54d3a0b724b9702a18c67891e2d366a7) )
	ROM_LOAD16_BYTE( "173_ea_b02.q6", 0x000001,  0x40000, CRC(9b45f122) SHA1(325af1612e6f90ef9ae9353c43dc645be1f3465c) )

	/* data */
	ROM_LOAD16_BYTE( "t5", 0x200000,  0x20000, CRC(cd724026) SHA1(525445499604b713da4d8bc0a88e428654ceab95) )
	ROM_LOAD16_BYTE( "t6", 0x200001,  0x20000, CRC(7dd54d6f) SHA1(b0ee8ec445b92254bca881eefd4449972fed506a) )

	ROM_REGION( 0x050000, "soundcpu", 0 )
	/* Z80 sound program */
	ROM_LOAD("173.a07", 0x000000, 0x40000, CRC(4cdaee71) SHA1(bdc05d4475415f6fac65d7cdbc48df398e57845e) )
	ROM_RELOAD(         0x010000, 0x040000 )

	ROM_REGION( 0x200000, "gfx1", 0 )
	/* tilemaps */
	ROM_LOAD( "173a05.t8",  0x000000, 0x100000, CRC(d14333b4) SHA1(d1a15ead2d156e1fceca0bf202ab3962411caf11) )
	ROM_LOAD( "173a06.t10", 0x100000, 0x100000, CRC(6541a34f) SHA1(15cf481498e3b7e0b2f7bfe5434121cc3bd65662) )

	ROM_REGION( 0x800000, "gfx2", 0 )
	/* sprites */
	ROM_LOAD( "173a10.b8",  0x000000, 0x200000, CRC(42fb0a0c) SHA1(d68c932cfabdec7896698b433525fe47ef4698d0) )
	ROM_LOAD( "173a11.a8",  0x200000, 0x200000, CRC(b0d747c4) SHA1(0cf1ee1b9a35ded31a81c321df2a076f7b588971) )
	ROM_LOAD( "173a12.b10", 0x400000, 0x200000, CRC(0fc2ad24) SHA1(6eda1043ee1266b8ba938a03a90bc7787210a936) )
	ROM_LOAD( "173a13.a10", 0x600000, 0x200000, CRC(4cf85439) SHA1(8c298bf0e659a830a1830a1180f4ce71215ade45) )

	ROM_REGION( 0x400000, "konami", 0 )
	/* K054539 samples */
	ROM_LOAD("173a08.b6", 0x000000, 0x200000, CRC(dcdded95) SHA1(8eeb546a0b60a35a6dce36c5ee872e6c93c577c9) )
	ROM_LOAD("173a09.a6", 0x200000, 0x200000, CRC(c93697c4) SHA1(0528a604868267a30d281b822c187df118566691) )
ROM_END

ROM_START( buckyua )
	ROM_REGION( 0x240000, "maincpu", 0 )
	/* main program */
	ROM_LOAD16_BYTE( "173_ua_b01.q5", 0x000000,  0x40000, CRC(dcaecca0) SHA1(c41847c9d89cdaf7cfa81ad9cc018c32592a882f) )
	ROM_LOAD16_BYTE( "173_ua_b02.q6", 0x000001,  0x40000, CRC(e3c856a6) SHA1(33cc8a29643e44b31ee280015c0c994bed72a0e3) )

	/* data */
	ROM_LOAD16_BYTE( "t5", 0x200000,  0x20000, CRC(cd724026) SHA1(525445499604b713da4d8bc0a88e428654ceab95) )
	ROM_LOAD16_BYTE( "t6", 0x200001,  0x20000, CRC(7dd54d6f) SHA1(b0ee8ec445b92254bca881eefd4449972fed506a) )

	ROM_REGION( 0x050000, "soundcpu", 0 )
	/* Z80 sound program */
	ROM_LOAD("173.a07", 0x000000, 0x40000, CRC(4cdaee71) SHA1(bdc05d4475415f6fac65d7cdbc48df398e57845e) )
	ROM_RELOAD(         0x010000, 0x040000 )

	ROM_REGION( 0x200000, "gfx1", 0 )
	/* tilemaps */
	ROM_LOAD( "173a05.t8",  0x000000, 0x100000, CRC(d14333b4) SHA1(d1a15ead2d156e1fceca0bf202ab3962411caf11) )
	ROM_LOAD( "173a06.t10", 0x100000, 0x100000, CRC(6541a34f) SHA1(15cf481498e3b7e0b2f7bfe5434121cc3bd65662) )

	ROM_REGION( 0x800000, "gfx2", 0 )
	/* sprites */
	ROM_LOAD( "173a10.b8",  0x000000, 0x200000, CRC(42fb0a0c) SHA1(d68c932cfabdec7896698b433525fe47ef4698d0) )
	ROM_LOAD( "173a11.a8",  0x200000, 0x200000, CRC(b0d747c4) SHA1(0cf1ee1b9a35ded31a81c321df2a076f7b588971) )
	ROM_LOAD( "173a12.b10", 0x400000, 0x200000, CRC(0fc2ad24) SHA1(6eda1043ee1266b8ba938a03a90bc7787210a936) )
	ROM_LOAD( "173a13.a10", 0x600000, 0x200000, CRC(4cf85439) SHA1(8c298bf0e659a830a1830a1180f4ce71215ade45) )

	ROM_REGION( 0x400000, "konami", 0 )
	/* K054539 samples */
	ROM_LOAD("173a08.b6", 0x000000, 0x200000, CRC(dcdded95) SHA1(8eeb546a0b60a35a6dce36c5ee872e6c93c577c9) )
	ROM_LOAD("173a09.a6", 0x200000, 0x200000, CRC(c93697c4) SHA1(0528a604868267a30d281b822c187df118566691) )
ROM_END

ROM_START( buckyaa )
	ROM_REGION( 0x240000, "maincpu", 0 )
	/* main program */
	ROM_LOAD16_BYTE( "173_aa_b01.q5", 0x000000,  0x40000, CRC(9193e89f) SHA1(574d6eb4097cd10c3dea99060ee09f220d41f1dc) )
	ROM_LOAD16_BYTE( "173_aa_b02.q6", 0x000001,  0x40000, CRC(2567f3eb) SHA1(ccdb2a4b3ad1464f70d1442df8a3a7a7e34f6cd7) )

	/* data */
	ROM_LOAD16_BYTE( "t5", 0x200000,  0x20000, CRC(cd724026) SHA1(525445499604b713da4d8bc0a88e428654ceab95) )
	ROM_LOAD16_BYTE( "t6", 0x200001,  0x20000, CRC(7dd54d6f) SHA1(b0ee8ec445b92254bca881eefd4449972fed506a) )

	ROM_REGION( 0x050000, "soundcpu", 0 )
	/* Z80 sound program */
	ROM_LOAD("173.a07", 0x000000, 0x40000, CRC(4cdaee71) SHA1(bdc05d4475415f6fac65d7cdbc48df398e57845e) )
	ROM_RELOAD(         0x010000, 0x040000 )

	ROM_REGION( 0x200000, "gfx1", 0 )
	/* tilemaps */
	ROM_LOAD( "173a05.t8",  0x000000, 0x100000, CRC(d14333b4) SHA1(d1a15ead2d156e1fceca0bf202ab3962411caf11) )
	ROM_LOAD( "173a06.t10", 0x100000, 0x100000, CRC(6541a34f) SHA1(15cf481498e3b7e0b2f7bfe5434121cc3bd65662) )

	ROM_REGION( 0x800000, "gfx2", 0 )
	/* sprites */
	ROM_LOAD( "173a10.b8",  0x000000, 0x200000, CRC(42fb0a0c) SHA1(d68c932cfabdec7896698b433525fe47ef4698d0) )
	ROM_LOAD( "173a11.a8",  0x200000, 0x200000, CRC(b0d747c4) SHA1(0cf1ee1b9a35ded31a81c321df2a076f7b588971) )
	ROM_LOAD( "173a12.b10", 0x400000, 0x200000, CRC(0fc2ad24) SHA1(6eda1043ee1266b8ba938a03a90bc7787210a936) )
	ROM_LOAD( "173a13.a10", 0x600000, 0x200000, CRC(4cf85439) SHA1(8c298bf0e659a830a1830a1180f4ce71215ade45) )

	ROM_REGION( 0x400000, "konami", 0 )
	/* K054539 samples */
	ROM_LOAD("173a08.b6", 0x000000, 0x200000, CRC(dcdded95) SHA1(8eeb546a0b60a35a6dce36c5ee872e6c93c577c9) )
	ROM_LOAD("173a09.a6", 0x200000, 0x200000, CRC(c93697c4) SHA1(0528a604868267a30d281b822c187df118566691) )
ROM_END


static DRIVER_INIT( moo )
{
	konami_rom_deinterleave_2(machine, "gfx1");
	konami_rom_deinterleave_4(machine, "gfx2");

	game_type = (!strcmp(machine->gamedrv->name, "bucky") || !strcmp(machine->gamedrv->name, "buckyua"));
}

ROM_START( moobl )
	ROM_REGION( 0x180000, "maincpu", 0 )
	ROM_LOAD16_WORD_SWAP( "moo03.rom", 0x000000, 0x80000, CRC(fed6a1cb) SHA1(be58e266973930d643b5e15dcc974a82e1a3ae35) )
	ROM_LOAD16_WORD_SWAP( "moo04.rom", 0x100000, 0x80000, CRC(ec45892a) SHA1(594330cbbfbca87e61ddf519e565018b6eaf5a20) )

	ROM_REGION( 0x100000, "user1", 0 )
	ROM_LOAD16_WORD_SWAP( "moo03.rom", 0x000000, 0x80000, CRC(fed6a1cb) SHA1(be58e266973930d643b5e15dcc974a82e1a3ae35) )
	ROM_LOAD16_WORD_SWAP( "moo04.rom", 0x080000, 0x80000, CRC(ec45892a) SHA1(594330cbbfbca87e61ddf519e565018b6eaf5a20) )

	ROM_REGION( 0x200000, "gfx1", 0 )
	ROM_LOAD( "moo05.rom", 0x000000, 0x080000, CRC(8c045f9c) SHA1(cde81a722a4bc2efac09a26d7e300664059ec7bb) )
	ROM_LOAD( "moo06.rom", 0x080000, 0x080000, CRC(1261aa89) SHA1(b600916911bc0d8b6348e2ad4a16ed1a1c528261) )
	ROM_LOAD( "moo07.rom", 0x100000, 0x080000, CRC(b9e29f50) SHA1(c2af095df0af45064d49210085370425b319b82b) )
	ROM_LOAD( "moo08.rom", 0x180000, 0x080000, CRC(e6937229) SHA1(089b3d4af33e8d8fbc1f3abb81e047a7a590567c) )

	// sprites from bootleg not included in dump, taken from original game
	ROM_REGION( 0x800000, "gfx2", 0 )
	ROM_LOAD( "151a10", 0x000000, 0x200000, CRC(376c64f1) SHA1(eb69c5a27f9795e28f04a503955132f0a9e4de12) )
	ROM_LOAD( "151a11", 0x200000, 0x200000, CRC(e7f49225) SHA1(1255b214f29b6507540dad5892c60a7ae2aafc5c) )
	ROM_LOAD( "151a12", 0x400000, 0x200000, CRC(4978555f) SHA1(d9871f21d0c8a512b408e137e2e80e9392c2bf6f) )
	ROM_LOAD( "151a13", 0x600000, 0x200000, CRC(4771f525) SHA1(218d86b6230919b5db0304dac00513eb6b27ba9a) )

	ROM_REGION( 0x340000, "oki", 0 )
	ROM_LOAD( "moo01.rom", 0x000000, 0x040000, CRC(3311338a) SHA1(c0b5cd16f0275b5b93a2ea4fc64013c848c5fa43) )//bank 0 lo & hi
	ROM_CONTINUE(          0x040000+0x30000, 0x010000)//bank 1 hi
	ROM_CONTINUE(          0x080000+0x30000, 0x010000)//bank 2 hi
	ROM_CONTINUE(          0x0c0000+0x30000, 0x010000)//bank 3 hi
	ROM_CONTINUE(          0x100000+0x30000, 0x010000)//bank 4 hi
	ROM_RELOAD(            0x040000, 0x30000 )//bank 1 lo
	ROM_RELOAD(            0x080000, 0x30000 )//bank 2 lo
	ROM_RELOAD(            0x0c0000, 0x30000 )//bank 3 lo
	ROM_RELOAD(            0x100000, 0x30000 )//bank 4 lo
	ROM_RELOAD(            0x140000, 0x30000 )//bank 5 lo
	ROM_RELOAD(            0x180000, 0x30000 )//bank 6 lo
	ROM_RELOAD(            0x1c0000, 0x30000 )//bank 7 lo
	ROM_RELOAD(            0x200000, 0x30000 )//bank 8 lo
	ROM_RELOAD(            0x240000, 0x30000 )//bank 9 lo
	ROM_RELOAD(            0x280000, 0x30000 )//bank a lo
	ROM_RELOAD(            0x2c0000, 0x30000 )//bank b lo
	ROM_RELOAD(            0x300000, 0x30000 )//bank c lo

	ROM_LOAD( "moo02.rom", 0x140000+0x30000, 0x010000, CRC(2cf3a7c6) SHA1(06f495ba8250b34c32569d49c8b84e6edef562d3) )//bank 5 hi
	ROM_CONTINUE(          0x180000+0x30000, 0x010000)//bank 6 hi
	ROM_CONTINUE(          0x1c0000+0x30000, 0x010000)//bank 7 hi
	ROM_CONTINUE(          0x200000+0x30000, 0x010000)//bank 8 hi
	ROM_CONTINUE(          0x240000+0x30000, 0x010000)//bank 9 hi
	ROM_CONTINUE(          0x280000+0x30000, 0x010000)//bank a hi
	ROM_CONTINUE(          0x2c0000+0x30000, 0x010000)//bank b hi
	ROM_CONTINUE(          0x300000+0x30000, 0x010000)//bank c hi
ROM_END
#endif

//GAME( 1992, moo,     0,       moo,     moo,     moo,      ROT0, "Konami", "Wild West C.O.W.-Boys of Moo Mesa (ver EA)", GAME_IMPERFECT_GRAPHICS)
//GAME( 1992, mooua,   moo,     moo,     moo,     moo,      ROT0, "Konami", "Wild West C.O.W.-Boys of Moo Mesa (ver UA)", GAME_IMPERFECT_GRAPHICS)
//GAME( 1992, mooaa,   moo,     moo,     moo,     moo,      ROT0, "Konami", "Wild West C.O.W.-Boys of Moo Mesa (ver AA)", GAME_IMPERFECT_GRAPHICS)
//GAME( 1992, moobl,   moo,     moobl,   moo,     moo,      ROT0, "bootleg", "Wild West C.O.W.-Boys of Moo Mesa (bootleg ver AA)", GAME_NOT_WORKING)
//GAME( 1992, bucky,   0,       bucky,   bucky,   moo,      ROT0, "Konami", "Bucky O'Hare (ver EA)", 0)
//GAME( 1992, buckyua, bucky,   bucky,   bucky,   moo,      ROT0, "Konami", "Bucky O'Hare (ver UA)", 0)
//GAME( 1992, buckyaa, bucky,   bucky,   bucky,   moo,      ROT0, "Konami", "Bucky O'Hare (ver AA)", 0)
