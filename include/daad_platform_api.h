/*
	Copyright (c) 2019 Natalia Pujol Cremades
	natypclicense@gmail.com

	See LICENSE file.

	DAAD is a trademark of Andrés Samudio

	==========================================================
		System dependent API functions
	==========================================================
*/
#ifndef  __DAAD_PLATFORM_API_H__
#define  __DAAD_PLATFORM_API_H__

#include <stdint.h>
#if defined(MSX2)
	#include "daad_platform_msx2.h"
#elif defined(CPM)
	#include "daad_platform_cpm.h"
#elif defined(PC_TXT)
	#include "daad_platform_pctxt.h"
#endif

#ifndef VERSION
	#error "VERSION define must be declared in the daad_platform_XXX.c file."
#endif

// Macro helpers
#define STRINGIFY2(x)	#x
#define STRINGIFY(x) STRINGIFY2(x)

// Tools
#define ADDR_POINTER_BYTE(X)	(*((uint8_t*)X))
#define ADDR_POINTER_WORD(X)	(*((uint16_t*)X))


// System functions
bool     checkPlatformSystem();
uint16_t getFreeMemory();
char*    getCharsTranslation();
void     setTime(uint16_t time);
uint16_t getTime();
uint16_t checkKeyboardBuffer();
void     clearKeyboardBuffer();
uint8_t  getKeyInBuffer();
void     waitingForInput();


// Filesystem
void     loadFilesBin(int argc, char **argv);
uint16_t loadFile(char *filename, uint8_t *destaddress, uint16_t size);
uint16_t fileSize(char *filename);


// External texts
void printXMES(uint16_t address);


// GFX functions
void gfxSetScreen();
void gfxSetScreenModeFlags();
void gfxClearScreenBlock(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void gfxClearWindow();
void gfxClearCurrentLine();
void gfxScrollUp();
void gfxSetPaperCol(uint8_t col);
void gfxSetInkCol(uint8_t col);
void gfxSetBorderCol(uint8_t col);
void gfxSetGraphCharset(bool value);
void gfxPutChWindow(uint8_t c);
void gfxPutChPixels(uint8_t c, uint16_t dx, uint16_t dy);
void gfxPutInputEcho(char c, bool keepPos);
void gfxSetPalette(uint8_t index, uint8_t red, uint8_t green, uint8_t blue);
bool gfxPicturePrepare(uint8_t location);
bool gfxPictureShow();
void gfxRoutines(uint8_t routine, uint8_t value);


// SFX functions
void sfxInit();
void sfxWriteRegister(uint8_t reg, uint8_t value);
void sfxTone(uint8_t value1, uint8_t value2);


#endif  //__DAAD_PLATFORM_API_H__
