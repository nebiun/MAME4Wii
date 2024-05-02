/*----------- defined in video/equites.c -----------*/

extern UINT16 *equites_bg_videoram;

extern READ16_HANDLER(equites_fg_videoram_r);
extern WRITE16_HANDLER(equites_fg_videoram_w);
extern WRITE16_HANDLER(equites_bg_videoram_w);
extern WRITE16_HANDLER(equites_scrollreg_w);
extern WRITE16_HANDLER(equites_bgcolor_w);
extern WRITE16_HANDLER(splndrbt_selchar0_w);
extern WRITE16_HANDLER(splndrbt_selchar1_w);
extern WRITE16_HANDLER(equites_flip0_w);
extern WRITE16_HANDLER(equites_flip1_w);
extern WRITE16_HANDLER(splndrbt_flip0_w);
extern WRITE16_HANDLER(splndrbt_flip1_w);
extern WRITE16_HANDLER(splndrbt_bg_scrollx_w);
extern WRITE16_HANDLER(splndrbt_bg_scrolly_w);
extern PALETTE_INIT( equites );
extern VIDEO_START( equites );
extern VIDEO_UPDATE( equites );
extern PALETTE_INIT( splndrbt );
extern VIDEO_START( splndrbt );
extern VIDEO_UPDATE( splndrbt );
