/*
   Taito Power-JC System

   Skeleton driver. Requires TLCS-900 CPU core to make progress.

   Hardware appears sufficiently different to JC system to warrant
   a separate driver.

   PCB Information (incomplete!)
   ===============

   POWER JC MOTHER-G PCB
   K11X0870A
   OPERATION TIGER

   PowerPC 603E
   CXD1176Q

   TMS320C53PQ80
   40MHz osc
   43256 x 2
   E63-03_H.29 (AT27C512 PLCC)
   E63-04_L.28 (AT27C512 PLCC)

   E63-01 PALCE16V8H
   E63-02 PALCE22V10H

   IC41 E63-06 PALCE16V8H
   IC43 E63-07 PALCE16V8H

   uPD4218160 x 2
   uPD4218160 x 2

   uPD482445 x 4

   CY78991
   IS61LV256AH x 3
   Taito TC0780FPA x 2
   Taito TCG010PJC

   MN1020819
   ZOOM ZSG-2
   ZOOM ZFX-2
   MSM514256


   Second PCB
   ----------

   19 ROMs

   TMP95C063F
   25.0000MHz osc
   1.84320MHz osc
*/
#if 0

#include "driver.h"
#include "cpu/powerpc/ppc.h"


static VIDEO_START( taitopjc )
{

}

static VIDEO_UPDATE( taitopjc )
{
	return 0;
}

static ADDRESS_MAP_START( ppc603e_mem, ADDRESS_SPACE_PROGRAM, 64)
	AM_RANGE(0x00000000, 0x003fffff) AM_RAM // Work RAM
	AM_RANGE(0x40000000, 0x40000007) AM_RAM // Screen RAM (+ others?) data port
	AM_RANGE(0x40000008, 0x4000000f) AM_RAM // Screen RAM (+ others?) address port
	AM_RANGE(0xc0000ff8, 0xc0000fff) AM_RAM
	AM_RANGE(0xffe00000, 0xffffffff) AM_ROM AM_REGION("user1", 0)
ADDRESS_MAP_END


static INPUT_PORTS_START( taitopjc )
INPUT_PORTS_END


static const powerpc_config ppc603e_config =
{
	XTAL_66_6667MHz		/* Multiplier 1.5, Bus = 66MHz, Core = 100MHz */
};

static MACHINE_DRIVER_START( taitopjc )
	MDRV_CPU_ADD("maincpu", PPC603E, 100000000)
	MDRV_CPU_CONFIG(ppc603e_config)
	MDRV_CPU_PROGRAM_MAP(ppc603e_mem)

	/* TMP95C063F I/O CPU */
	/* TMS320C53 DSP */
	/* MN1020819DA sound CPU - NOTE: May have 64kB internal ROM */

	MDRV_SCREEN_ADD("screen", RASTER)
	MDRV_SCREEN_FORMAT(BITMAP_FORMAT_RGB32)
	MDRV_SCREEN_REFRESH_RATE(60)
	MDRV_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MDRV_SCREEN_SIZE(512, 384)
	MDRV_SCREEN_VISIBLE_AREA(0, 511, 0, 383)

	MDRV_VIDEO_START(taitopjc)
	MDRV_VIDEO_UPDATE(taitopjc)
MACHINE_DRIVER_END


ROM_START( optiger )
	ROM_REGION64_BE( 0x200000, "user1", 0 )
	ROM_LOAD32_BYTE( "e63-33-1_p-hh.23", 0x000000, 0x080000, CRC(5ab176e2) SHA1(a0a5b7c0e91928d0a49987f88f6ae647f5cb3e34) )
	ROM_LOAD32_BYTE( "e63-32-1_p-hl.22", 0x000001, 0x080000, CRC(cca8bacc) SHA1(e5a081f5c12a52601745f5b67fe3412033581b00) )
	ROM_LOAD32_BYTE( "e63-31-1_p-lh.8",  0x000002, 0x080000, CRC(ad69e649) SHA1(9fc853d2cb6e7cac87dc06bad91048f191b799c5) )
	ROM_LOAD32_BYTE( "e63-30-1_p-ll.7",  0x000003, 0x080000, CRC(a6183479) SHA1(e556c3edf100342079e680ec666f018fca7a82b0) )

	ROM_REGION64_BE( 0x20000, "user2", 0 )
	/* More PowerPC code? */
	ROM_LOAD16_BYTE( "e63-04_l.29",  0x000001, 0x010000, CRC(eccae391) SHA1(e5293c16342cace54dc4b6dfb827558e18ac25a4) )
	ROM_LOAD16_BYTE( "e63-03_h.28",  0x000000, 0x010000, CRC(58fce52f) SHA1(1e3d9ee034b25e658ca45a8b900de2aa54b00135) )

	ROM_REGION( 0x40000, "io_cpu", 0 )
	ROM_LOAD16_BYTE( "e63-28-1_0.59", 0x000000, 0x020000, CRC(ef41ffaf) SHA1(419621f354f548180d37961b861304c469e43a65) )
	ROM_LOAD16_BYTE( "e63-27-1_1.58", 0x000001, 0x020000, CRC(facc17a7) SHA1(40d69840cfcfe5a509d69824c2994de56a3c6ece) )

	ROM_REGION( 0x80000, "unk1", 0 )
	ROM_LOAD16_BYTE( "e63-17-1_s-l.18", 0x000000, 0x040000, CRC(2a063d5b) SHA1(a2b2fe4d8bad1aef7d9dcc0be607cc4e5bc4f0eb) )
	ROM_LOAD16_BYTE( "e63-18-1_s-h.19", 0x000001, 0x040000, CRC(2f590881) SHA1(7fb827a676f45b24380558b0068b76cb858314f6) )

	ROM_REGION( 0x1000000, "gfx1", 0 )
	ROM_LOAD32_WORD_SWAP( "e63-21_c-h.24", 0x000000, 0x400000, CRC(c818b211) SHA1(dce07bfe71a9ba11c3f028a640226c6e59c6aece) )
	ROM_LOAD32_WORD_SWAP( "e63-15_c-l.9",  0x000002, 0x400000, CRC(4ec6a2d7) SHA1(2ee6270cff7ea2459121961a29d42e000cee2921) )
	ROM_LOAD32_WORD_SWAP( "e63-22_m-h.25", 0x800000, 0x400000, CRC(6d895eb6) SHA1(473795da42fd29841a926f18a93e5992f4feb27c) )
	ROM_LOAD32_WORD_SWAP( "e63-16_m-l.10", 0x800002, 0x400000, CRC(d39c1e34) SHA1(6db0ce2251841db3518a9bd9c4520c3c666d19a0) )

	ROM_REGION( 0xc00000, "poly", 0 )
	ROM_LOAD( "e63-09_poly0.3", 0x000000, 0x400000, CRC(c3e2b1e0) SHA1(ee71f3f59b46e26dbe2ff724da2c509267c8bf2f) )
	ROM_LOAD( "e63-10_poly1.4", 0x400000, 0x400000, CRC(f4a56390) SHA1(fc3c51a7f4639479e66ad50dcc94255d94803c97) )
	ROM_LOAD( "e63-11_poly2.5", 0x800000, 0x400000, CRC(2293d9f8) SHA1(16adaa0523168ee63a7a34b29622c623558fdd82) )
// Poly 3 is not populated

	ROM_REGION( 0x800000, "sound_data", 0 )
	ROM_LOAD( "e63-23_wd0.36", 0x000000, 0x200000, CRC(d69e196e) SHA1(f738bb9e1330f6dabb5e0f0378a1a8eb48a4fa40) )
	ROM_LOAD( "e63-24_wd1.37", 0x200000, 0x200000, CRC(cd55f17b) SHA1(08f847ef2fd592dbaf63ef9e370cdf1f42012f74) )
	ROM_LOAD( "e63-25_wd2.38", 0x400000, 0x200000, CRC(bd35bdac) SHA1(5cde6c1a6b74659507b31fcb88257e65f230bfe2) )
	ROM_LOAD( "e63-26_wd3.39", 0x600000, 0x200000, CRC(346bd413) SHA1(0f6081d22db88eef08180278e7ae97283b5e8452) )

	ROM_REGION( 0x500, "plds", ROMREGION_ERASEFF )
	// TODO: There are 6 PALs in total on the main PCB.
ROM_END
#endif


//GAME( 1998, optiger, 0, taitopjc, taitopjc, 0, ROT0, "Taito", "Operation Tiger", GAME_NOT_WORKING | GAME_NO_SOUND )
