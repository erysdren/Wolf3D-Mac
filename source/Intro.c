#include "wolfdef.h"
#include <ctype.h>
#include <stdlib.h>

/**********************************

	Main game introduction

**********************************/

void Intro(void)
{
	LongWord *PackPtr;
	LongWord PackLength;
	Byte *ShapePtr;
	
	NewGameWindow(1);	/* Set to 512 mode */

	FadeToBlack();		/* Fade out the video */
	PackPtr = LoadAResource(rMacPlayPic);
	PackLength = PackPtr[0];
	ShapePtr = AllocSomeMem(PackLength);
	DLZSS(ShapePtr,(Byte *) &PackPtr[1],PackLength);
	DrawShape(0,0,ShapePtr);
	FreeSomeMem(ShapePtr);
	ReleaseAResource(rMacPlayPic);
	
	BlastScreen();
	StartSong(SongListPtr[0]);	/* Play the song */
	FadeTo(rMacPlayPal);	/* Fade in the picture */
	WaitTicksEvent(240);		/* Wait for event */
	FadeTo(rIdLogoPal);
	if (toupper(WaitTicksEvent(240))=='B') {		/* Wait for event */
		FadeToBlack();
		ClearTheScreen(BLACK);
		BlastScreen();
		PackPtr = LoadAResource(rYummyPic);
		PackLength = PackPtr[0];
		ShapePtr = AllocSomeMem(PackLength);
		DLZSS(ShapePtr,(Byte *) &PackPtr[1],PackLength);
		DrawShape((SCREENWIDTH-320)/2,(SCREENHEIGHT-200)/2,ShapePtr);
		FreeSomeMem(ShapePtr);
		ReleaseAResource(rYummyPic);
		BlastScreen();
		FadeTo(rYummyPal);
		WaitTicksEvent(600);
	}	
}
