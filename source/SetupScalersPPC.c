#include "wolfdef.h"
#include <string.h>
#include "SoundMusicSystem.h"
#include "mixedmode.h"

Word ScaleDiv[2048];			/* Divide table for scalers */

/**********************************

	Create the compiled scalers

**********************************/

Boolean SetupScalers(void)
{
	Word i;
	
	if (!ScaleDiv[1]) {		/* Divide table inited already? */
		i = 1;
		do {
			ScaleDiv[i] = 0x40000000/i;		/* Get the recipocal for the scaler */
		} while (++i<2048);
	}
	MaxScaler = 2048;		/* Init the highest scaler value */
	return TRUE;			/* No errors possible */
}

/**********************************

	Release the memory from the scalers

**********************************/

void ReleaseScalers(void)
{
}

/**********************************

	Draw a vertical line with a scaler
	(Used for WALL drawing)
	This is now in assembly language
	
**********************************/

/* void ScaleGlue(void *a,t_compscale *b,void *c); */
void ScaleGlue(void *a,Word b,void *c,Word d,Word e,Word f,Word g);
/*
; R3 = ArtPtr
; R4 = Maxlines
; R5 = ScreenPtr
; R6 = Frac
; R7 = Integer
; R8 = VideoWidth
; R9 = Delta
*/

#if 0
void IO_ScaleWallColumn(Word x,Word scale,LongWord column)
{
	Word TheFrac;
	Word TheInt;
	Word y;
	Byte *ArtStart;
	
	if (scale) {		/* Uhh.. Don't bother */
		scale*=2;
		TheFrac = 0x80000000UL / scale;

		ArtStart = &ArtData[(column>>7)&0x3f][(column&127)<<7];
		if (scale<VIEWHEIGHT) {
			y = (VIEWHEIGHT-scale)/2;
			TheInt = TheFrac>>24;
			TheFrac <<= 8;
			ScaleGlue(ArtStart,scale,
				&VideoPointer[(y*VideoWidth)+x],
				TheFrac,
				TheInt,
				VideoWidth,
				0
			);
			return;
		}
		y = (scale-VIEWHEIGHT)/2;		/* How manu lines to remove */
		y = y*TheFrac;
		TheInt = TheFrac>>24;
		TheFrac <<= 8;
		ScaleGlue(&ArtStart[y>>24],VIEWHEIGHT,
			&VideoPointer[x],
			TheFrac,
			TheInt,
			VideoWidth,
			y<<8
		);
	}
}
#endif
/**********************************

	Draw a vertical line with a masked scaler
	(Used for SPRITE drawing)
	
**********************************/

typedef struct {
	unsigned short Topy;
	unsigned short Boty;
	unsigned short Shape;
} SpriteRun;

void SpriteGlue(Byte *a,Word b,Word c,Byte *d,Word e,Word f);
/*
SGArtStart	EQU	R3		;Pointer to the 6 byte run structure
SGFrac	EQU	R4		;Pointer to the scaler
SGInteger	EQU R5		;Pointer to the video
SGScreenPtr	EQU	R6		;Pointer to the run base address
SGCount	EQU R7
SGDelta	EQU	R8
*/

void IO_ScaleMaskedColumn(Word x,Word scale,unsigned short *CharPtr,Word column) 
{
	Byte * CharPtr2;
	int Y1,Y2;
	Byte *Screenad;
	SpriteRun *RunPtr;
	Word TheFrac;
	Word TFrac;
	Word TInt;
	Word RunCount;
	int TopY;
	Word Index;
	Word Delta;
	
	if (!scale) {
		return;
	}
	CharPtr2 = (Byte *) CharPtr;
	TheFrac = ScaleDiv[scale];		/* Get the scale fraction */ 
	RunPtr = (SpriteRun *) &CharPtr[CharPtr[column+1]/2];	/* Get the offset to the RunPtr data */
	Screenad = &VideoPointer[x];		/* Set the base screen address */
	TFrac = TheFrac<<8;
	TInt = TheFrac>>24;
	TopY = (VIEWHEIGHT/2)-scale;		/* Number of pixels for 128 pixel shape */

	while (RunPtr->Topy != (unsigned short) -1) {		/* Not end of record? */
		Y1 = scale*(LongWord)RunPtr->Topy/128+TopY;
		if (Y1<(int)VIEWHEIGHT) {		/* Clip top? */
		Y2 = scale*(LongWord)RunPtr->Boty/128+TopY;
		if (Y2>0) {
		
		if (Y2>(int)VIEWHEIGHT) {
			Y2 = VIEWHEIGHT;
		}
		Index = RunPtr->Shape+(RunPtr->Topy/2);
		Delta = 0;
		if (Y1<0) {
			Delta = (0-(Word)Y1)*TheFrac;
			Index += (Delta>>24);
			Delta <<= 8;
			Y1 = 0;
		}
		RunCount = Y2-Y1;
		if (RunCount) {
			SpriteGlue(
			&CharPtr2[Index],	/* Pointer to art data */
			TFrac,				/* Fractional value */ 
			TInt,				/* Integer value */
			&Screenad[Y1*VideoWidth],			/* Pointer to screen */
			RunCount,			/* Number of lines to draw */
			Delta					/* Delta value */
			);
		}
		}
		} 
		RunPtr++;						/* Next record */
	}	
}

/**********************************

	Draw an automap tile
	
**********************************/

Byte *SmallFontPtr;

void DrawSmall(Word x,Word y,Word tile)
{
	Byte *Screenad;
	Byte *ArtStart;
	Word Width,Height;

	if (!SmallFontPtr) {
		return;
	}	
	x*=16;
	y*=16;
	Screenad = &VideoPointer[YTable[y]+x];
	ArtStart = &SmallFontPtr[tile*(16*16)];
	Height = 0;
	do {
		Width = 16;
		do {
			Screenad[0] = ArtStart[0];
			++Screenad;
			++ArtStart;
		} while (--Width);
		Screenad+=VideoWidth-16;
	} while (++Height<16);
}

void MakeSmallFont(void)
{
	Word i,j,Width,Height;
	Byte *DestPtr,*ArtStart;
	Byte *TempPtr;
	
	SmallFontPtr = AllocSomeMem(16*16*65);
	if (!SmallFontPtr) {
		return;
	}
	memset(SmallFontPtr,0,16*16*65);		/* Erase the font */
	i = 0;
	DestPtr = SmallFontPtr;
	do {
		ArtStart = &ArtData[i][0];
		if (!ArtStart) {
			DestPtr+=(16*16);
		} else {
			Height = 0;
			do {
				Width = 16;
				j = Height*8;
				do {
					DestPtr[0] = ArtStart[j];
					++DestPtr;
					j+=(WALLHEIGHT*8);
				} while (--Width);
			} while (++Height<16);
		}
	} while (++i<64);
	TempPtr = LoadAResource(MyBJFace);
	memcpy(DestPtr,TempPtr,16*16);
	ReleaseAResource(MyBJFace);
	
#if 0
	TempPtr = AllocSomeMem(16*16*128);
	memset(TempPtr,-1,16*16*128);
	i = 0;
	do {
		memcpy(&TempPtr[(i+1)*256],&SmallFontPtr[(i*2)*256],256);
	} while (++i<29);
	i = 0;
	do { 
		memcpy(&TempPtr[((i*2)+90)*256],&SmallFontPtr[(i+59)*256],256);
		memcpy(&TempPtr[((i*2)+91)*256],&SmallFontPtr[(i+59)*256],256);
	} while (++i<4);
	SaveJunk(TempPtr,256*128);
	FreeSomeMem(TempPtr);
#endif
}

void KillSmallFont(void)
{	
	if (SmallFontPtr) {
		FreeSomeMem(SmallFontPtr);
		SmallFontPtr=0;		
	}
}

