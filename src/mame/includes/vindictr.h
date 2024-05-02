/*************************************************************************

    Atari Vindicators hardware

*************************************************************************/

/*----------- defined in video/vindictr.c -----------*/

WRITE16_HANDLER( vindictr_paletteram_w );

VIDEO_START( vindictr );
VIDEO_UPDATE( vindictr );

void vindictr_scanline_update(const device_config *screen, int scanline);
