/*----------- defined in video/tmnt.c -----------*/

WRITE16_HANDLER( tmnt_paletteram_word_w );
WRITE16_HANDLER( tmnt_0a0000_w );
WRITE16_HANDLER( punkshot_0a0020_w );
WRITE16_HANDLER( lgtnfght_0a0018_w );
WRITE16_HANDLER( blswhstl_700300_w );
READ16_HANDLER( glfgreat_rom_r );
WRITE16_HANDLER( glfgreat_122000_w );
WRITE16_HANDLER( ssriders_eeprom_w );
WRITE16_HANDLER( ssriders_1c0300_w );
WRITE16_HANDLER( prmrsocr_122000_w );
WRITE16_HANDLER( tmnt_priority_w );
READ16_HANDLER( glfgreat_ball_r );
READ16_HANDLER( prmrsocr_rom_r );
VIDEO_START( sunsetbl );
VIDEO_START( cuebrick );
VIDEO_START( mia );
VIDEO_START( tmnt );
VIDEO_START( punkshot );
VIDEO_START( lgtnfght );
VIDEO_START( blswhstl );
VIDEO_START( glfgreat );
VIDEO_START( thndrx2 );
VIDEO_START( prmrsocr );
VIDEO_UPDATE( mia );
VIDEO_UPDATE( tmnt );
VIDEO_UPDATE( punkshot );
VIDEO_UPDATE( lgtnfght );
VIDEO_UPDATE( glfgreat );
VIDEO_UPDATE( tmnt2 );
VIDEO_UPDATE( thndrx2 );
VIDEO_EOF( blswhstl );

