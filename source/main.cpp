/*---------------------------------------------------------------------------------

	$Id: main.cpp,v 1.13 2008-12-02 20:21:20 dovoto Exp $

	Simple console print demo
	-- dovoto


---------------------------------------------------------------------------------*/
#include <nds.h>
#include <stdio.h>

#include "skyBackground.h"
#include "roomBackground.h"
#include "moonSprite.h"

//a simple sprite structure
//it is generally preferred to separate your game object
//from OAM
typedef struct
{
   u16* gfx;
   SpriteSize size;
   SpriteColorFormat format;
   int rotationIndex;
   int paletteAlpha;
   int x;
   int y;
} MySprite;

volatile int frame = 0;
int bg[2];

// fn for the interrupt
void Vblank() {
	frame++;
}
	
int main(void) {
	touchPosition touchXY;

	irqSet(IRQ_VBLANK, Vblank);

	// set video mode for 2 text layers and 2 extended background layers
	videoSetMode(MODE_0_2D);
	// set sub video mode for 4 text layers
	videoSetModeSub(MODE_0_2D);

	// map vram bank A to main engine background (slot 0)
	vramSetBankA(VRAM_A_MAIN_BG_0x06000000);
	// map vram bank B to main engine sprites (slot 0)
	vramSetBankB(VRAM_B_MAIN_SPRITE);

	// enable extended palettes
	bgExtPaletteEnable();

	// debug init
	consoleDemoInit();

	// set brightness on bottom screen to completely dark (no visible image)
	setBrightness(2, -16);

	// initialize backgrounds
	// check https://mtheall.com/vram.html to ensure bg fit in vram
	bg[0] = bgInit(0, BgType_Text8bpp, BgSize_T_256x256, 31, 4);	// room
	bg[1] = bgInit(1, BgType_Text8bpp, BgSize_T_256x256, 27, 0);	// sky

	// copy graphics to vram
	dmaCopy(roomBackgroundTiles,  bgGetGfxPtr(bg[0]), roomBackgroundTilesLen);
  	dmaCopy(skyBackgroundTiles, bgGetGfxPtr(bg[1]), skyBackgroundTilesLen);

	// copy maps to vram
	dmaCopy(roomBackgroundMap,  bgGetMapPtr(bg[0]), roomBackgroundMapLen);
  	dmaCopy(skyBackgroundMap, bgGetMapPtr(bg[1]), skyBackgroundMapLen);

	vramSetBankE(VRAM_E_LCD); // for main engine

	// copy palettes to extended palette area
	dmaCopy(roomBackgroundPal,  &VRAM_E_EXT_PALETTE[0][0],  roomBackgroundPalLen);  // bg 0, slot 0
	dmaCopy(skyBackgroundPal, &VRAM_E_EXT_PALETTE[1][12], skyBackgroundPalLen); // bg 1, slot 12 (specified slot in .grit file)

	// map vram to extended palette
	vramSetBankE(VRAM_E_BG_EXT_PALETTE);
	bgUpdate();

	// showing moon as 3 sprites
	MySprite sprites[] = {
		{0, SpriteSize_64x64, SpriteColorFormat_256Color, 0, 15, 0, 0},
		{0, SpriteSize_64x64, SpriteColorFormat_256Color, 0, 0, 64, 0},
		{0, SpriteSize_64x64, SpriteColorFormat_256Color, 0, 1, 128, 0}
	};

	// initialize sub sprite engine with 1D mapping, 128 byte boundry, no external palette support
	oamInit(&oamMain, SpriteMapping_1D_128, false);
	
	// allocating space for sprite graphics
	u16* moonSpriteGfxPtr = oamAllocateGfx(&oamMain, SpriteSize_64x64, SpriteColorFormat_256Color);
	dmaCopy(moonSpriteTiles, moonSpriteGfxPtr, moonSpriteTilesLen);
    dmaCopy(moonSpritePal, SPRITE_PALETTE, moonSpritePalLen);

	for(int i = 0; i < 3; i++)
      sprites[i].gfx = moonSpriteGfxPtr;

	for(int i = 0; i < 3; i++) {
		oamSet(
		&oamMain, 						// main display
		i,       						// oam entry to set
		sprites[i].x, sprites[i].y, 	// position
		0, 								// priority
		sprites[i].paletteAlpha, 		// palette for 16 color sprite or alpha for bmp sprite
		sprites[i].size,
		sprites[i].format,
		sprites[i].gfx,
		sprites[i].rotationIndex,
		true, 							// double the size of rotated sprites
		false, 							// don't hide the sprite
		false, false, 					// vflip, hflip
		false 							// apply mosaic
		);
	}

	// update display to show sprites
	oamUpdate(&oamMain);
	
	// text uses ansi escape sequences
	iprintf("Taha Rashid\n");
	iprintf("\033[31;1;4mFeb 18, 2025\n\x1b[39m");
	iprintf("Line 3\n");
	iprintf("\x1b[32;1mLine 4\n\x1b[39m");
	iprintf("\x1b[31;1;4mLine 5\n\x1b[39m");
	iprintf("Line 6\n");

	// NOTE: bottom screen has 24 lines, 32 columns (from 0 -> 23, 0 -> 32)
	iprintf("\x1b[23;31HTest!");
	iprintf("\x1b[11;11HPress Start");

	// fade top screen in
	// blend control. takes effect mode / source / destination
	REG_BLDCNT = BLEND_ALPHA | BLEND_SRC_BG1 | BLEND_DST_BACKDROP;
	for(int i = 0; i <= 16; i++) {
		// source opacity / dest opacity. They should add up to 16
		REG_BLDALPHA = i | ((16 - i) << 8);
	
		// wait for duration amount of frames
		for (int frame = 0; frame <= 6; frame++) {
			swiWaitForVBlank();
		}
	}

	// for bottom screen text animation
	bool animateText = true;
	int duration = 4;
	int durationCounter = 0;
	int brightness = 16;
	int brightnessCounter = 0;
 
	while(pmMainLoop()) {
		swiWaitForVBlank();
		scanKeys();
		int keys = keysDown();

		// cancel text animation on start btn or touchscreen input
		if (keys & (KEY_START | KEY_TOUCH)) {
			setBrightness(2, 0);
			animateText = false;
		}
		
		touchRead(&touchXY);

		// print at using ansi escape sequence \x1b[line;columnH 
		iprintf("\x1b[10;0HFrame = %d",frame);
		iprintf("\x1b[16;0HTouch x = %04X, %04X\n", touchXY.rawx, touchXY.px);
		iprintf("Touch y = %04X, %04X\n", touchXY.rawy, touchXY.py);

		
		// animate text (fade in/out)
		if (!animateText) {
			continue;
		}

		durationCounter++;
		if (durationCounter >= duration) {
			durationCounter = 0;
			brightnessCounter++;
			setBrightness(2, brightnessCounter - 16);
		}

		if (brightnessCounter >= brightness) {
			brightnessCounter = 0;
		}
	}

	return 0;
}
