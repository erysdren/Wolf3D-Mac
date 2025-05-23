#include "wolfdef.h"
#include <string.h>
#include <stdlib.h>

LongWord ScaleDiv[2048];

/**********************************

	Create the compiled scalers

**********************************/

Boolean SetupScalers(void)
{
	Word i;
		
	if (!ScaleDiv[1]) {		/* Divide table inited already? */
		i = 1;
		do {
			ScaleDiv[i] = 0x400000UL/(LongWord)i;		/* Get the recipocal for the scaler */
		} while (++i<2048);
	}
	MaxScaler = 2048;
	return TRUE;
}

void ReleaseScalers(void)
{
}

/**********************************

	Draw a vertical line with a scaler
	(Used for WALL drawing)
	
**********************************/

void ScaleGlue(void *a,LongWord scale,void *c,LongWord d,LongWord e,LongWord f,LongWord g);
	
#if 0
void IO_ScaleWallColumn(Word x,Word scale,LongWord column)
{
	LongWord TheFrac;
	LongWord TheInt;
	LongWord y;
	Byte *ArtStart;
	
	if (scale) {		/* Uhh.. Don't bother */
		TheFrac = ScaleDiv[scale];
		scale*=2;
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
		TheFrac <<= 8L;
		ScaleGlue(&ArtStart[y>>24L],VIEWHEIGHT,
			&VideoPointer[x],
			TheFrac,
			TheInt,
			VideoWidth,
			y<<8L
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

void SpriteGlue(Byte *a,LongWord b,Byte *c,Word d,Word e);
/*
SGArtStart	EQU	A0		;Pointer to the 6 byte run structure
SGFrac	EQU	D2		;Pointer to the scaler
SGInteger	EQU D3		;Pointer to the video
SGScreenPtr	EQU	A1		;Pointer to the run base address
SGCount	EQU D6
SGDelta	EQU	D4
*/
void IO_ScaleMaskedColumn(Word x,Word scale,unsigned short *CharPtr,Word column) 
{
	Byte * CharPtr2;
	int Y1,Y2;
	Byte *Screenad;
	SpriteRun *RunPtr;
	LongWord TheFrac;
	Word RunCount;
	Word TopY;
	Word Index;
	LongWord Delta;
	
	if (!scale) {
		return;
	}
	CharPtr2 = (Byte *) CharPtr;
	TheFrac = ScaleDiv[scale];		/* Get the scale fraction */ 
	RunPtr = (SpriteRun *) &CharPtr[CharPtr[column+1]/2];	/* Get the offset to the RunPtr data */
	Screenad = &VideoPointer[x];		/* Set the base screen address */;
	TopY = (VIEWHEIGHT/2)-scale;		/* Number of pixels for 128 pixel shape */

	while (RunPtr->Topy != (unsigned short) -1) {		/* Not end of record? */
		Y1 = (LongWord)scale*RunPtr->Topy/128+TopY;
		if (Y1<(int)VIEWHEIGHT) {		/* Clip top? */
		Y2 = (LongWord)scale*RunPtr->Boty/128+TopY;
		if (Y2>0) {
		
		if (Y2>(int)VIEWHEIGHT) {
			Y2 = VIEWHEIGHT;
		}
		Index = RunPtr->Shape+(RunPtr->Topy/2);
		Delta = 0;
		if (Y1<0) {
			Delta = (LongWord)(0-Y1)*TheFrac;
			Index += (Delta>>16);
			Y1 = 0;
		}
		RunCount = Y2-Y1;		/* How many lines to draw?*/
		if (RunCount) {
			SpriteGlue(
			&CharPtr2[Index],	/* Pointer to art data */
			TheFrac,				/* Fixed point fractional value */ 
			&Screenad[YTable[Y1]],			/* Pointer to screen */
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
}

void KillSmallFont(void)
{	
	if (SmallFontPtr) {
		FreeSomeMem(SmallFontPtr);
		SmallFontPtr=0;		
	}
}

