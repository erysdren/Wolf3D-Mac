/**********************************

	Burger library for the Macintosh.
	Use Think.c or Code Warrior to compile.
	Use SMART linking to link in just what you need

**********************************/

#include <stdint.h>

#include "wolfdef.h"		/* Get the prototypes */
#include <string.h>
#include <stdio.h>
#include "SoundMusicSystem.h"
#include "PickAMonitor.h"

/**********************************

	Variables used by my global library

**********************************/

Word DoEvent(EventRecord *event);
void DoMacEvents(void);
void BlastScreen(void);
static Word FreeStage(Word Stage,LongWord Size);

extern Boolean MouseHit;		/* True if a mouse down event occured */
Word NoSystemMem;
unsigned char *VideoPointer;	/* Pointer to video memory */
extern Word QuitFlag;			/* Did the application quit? */
Word VideoWidth;				/* Width to each video scan line */
Word SystemState=3;				/* Sound on/off flags */
Word KilledSong;				/* Song that's currently playing */
Word KeyModifiers;				/* Keyboard modifier flags */
LongWord LastTick;				/* Last system tick (60hz) */
Word FontX;						/* X Coord of font */
Word FontY;						/* Y Coord of font */
unsigned char *FontPtr;			/* Pointer to font image data */
unsigned char *FontWidths;		/* Pointer to font width table */
Word FontHeight;				/* Point size of current font */
Word FontLast;					/* Number of font entries */
Word FontFirst;					/* ASCII value of first char */
Word FontLoaded;				/* Rez number of loaded font (0 if none) */
Word FontInvisible;				/* Allow masking? */
unsigned char FontOrMask[16];	/* Colors for font */
LongWord YTable[480];			/* Offsets to the screen */
SndChannelPtr myPaddleSndChan;	/* Sound channel */
Word ScanCode;
CWindowPtr GameWindow;
CGrafPtr GameGWorld;
extern GDHandle gMainGDH;
extern CTabHandle MainColorHandle;
extern Boolean DoQuickDraw;

/**********************************

	Wait a single system tick

**********************************/

static Word QuickTicker;

void DoMacEvents(void)
{
	EventRecord MyEvent;
	if (!DoQuickDraw) {
		if ((ReadTick() - QuickTicker) < 30) {
			return;
		}
		QuickTicker = ReadTick();
	}
	PurgeAllSounds(85000);		/* Try to keep some memory free */
	if (WaitNextEvent2(updateMask|diskMask|driverMask|networkMask|activMask|app4Mask,&MyEvent,0,0)) {
		DoEvent(&MyEvent);
	}
}

/**********************************

	Wait a single system tick

**********************************/

void WaitTick(void)
{
	do {
		DoMacEvents();			/* Allow backgrounding */
	} while (ReadTick()==LastTick);	/* Tick changed? */
	LastTick=ReadTick();		/* Save it */
}

/**********************************

	Wait a specific number of system ticks
	from a time mark before you get control

**********************************/

void WaitTicks(Word Count)
{
	LongWord TickMark;		/* Temp tick mark */
	
	do {
		DoMacEvents();		/* Allow other tasks to execute */
		TickMark = ReadTick();	/* Get the mark */
	} while ((TickMark-LastTick)<=Count);	/* Time up? */
	LastTick = TickMark;	/* Save the new time mark */
}

/**********************************

	Get the current system tick

**********************************/

LongWord ReadTick(void)
{
	return(TickCount());	/* Just get it from the Mac OS */
}

/**********************************

	Wait for a mouse/keyboard event

**********************************/

Word WaitEvent(void)
{
	Word Temp;
	do {
		Temp = WaitTicksEvent(6000);	/* Wait 10 minutes */
	} while (!Temp);	/* No event? */
	return Temp;		/* Return the event code */
}

/**********************************

	Wait for an event or a timeout

**********************************/

Word WaitTicksEvent(Word Time)
{
	LongWord TickMark;
	LongWord NewMark;
	Word RetVal;

	MouseHit = FALSE;
	TickMark = ReadTick();	/* Get the initial time mark */
	for (;;) {
		DoMacEvents();		/* Allow other tasks a shot! */
		NewMark = ReadTick();		/* Get the new time mark */
		if (Time) {
			if ((NewMark-TickMark)>=Time) {	/* Time up? */
				RetVal = 0;	/* Return timeout */
				break;
			}
		}
		RetVal = GetAKey();
		if (RetVal) {
			break;
		}
		if (MouseHit) {
			RetVal = 1;		/* Hit the mouse */
			break;
		}
	}
	LastTick = NewMark;
	return RetVal;
}

/**********************************

	Get a key from the keyboard

**********************************/

Word GetAKey(void)
{
	EventRecord MyRecord;

	if (WaitNextEvent2(everyEvent,&MyRecord,0,0)) {
		if (!DoEvent(&MyRecord)) {
			KeyModifiers = MyRecord.modifiers;
			return 0;
		}
		return FixMacKey(&MyRecord);
	}
	return 0;
}

/**********************************

	Check if all keys are released

**********************************/

Word WaitKey(void)
{
	Word Key;
	do {
		Key = GetAKey();
	} while (!Key);
	return (Key);
}

/**********************************

	Check if all keys are released

**********************************/

Word AllKeysUp(void)
{
	KeyMap KeyArray;

	GetKeys(KeyArray);
	if (KeyArray[0] || KeyArray[1] || KeyArray[2] || KeyArray[3]) {
		return 0;
	}
	return 1;
}

Word FixMacKey(EventRecord *Event)
{
	Word NewKey;
	NewKey = Event->message & 0xff;
	ScanCode = (Event->message>>8) & 0xff;
	switch (NewKey) {
	case 0x1c :
		NewKey = 0x08;
		break;
	case 0x1d :
		NewKey = 0x15;
		break;
	case 0x1e :
		NewKey = 0x0b;
		break;
	case 0x1f :
		NewKey = 0x0a;
		break;
	}
	KeyModifiers = Event->modifiers;
	if (NewKey == 'Q' || NewKey == 'q') {
		if (KeyModifiers & cmdKey) {
			QuitFlag = 1;
		}
	}
	return NewKey;
}	
			
/**********************************

	Flush out the keyboard buffer

**********************************/

void FlushKeys(void)
{
	while (GetAKey()) {}
}

/**********************************

	Convert a long value into a ascii string

**********************************/

static LongWord Tens[] = {
	1,
	10,
	100,
	1000,
	10000,
	100000,
	1000000,
	10000000,
	100000000,
	1000000000};

void ultoa(LongWord Val,char *Text)
{
	Word Index;		/* Index to Tens table */
	Word Hit;		/* Printing? */
	Word Letter;	/* Letter to print */
	LongWord LongVal;	/* Tens value */

	Index = 9;		/* Start at the millions */
	Hit = 0;		/* Didn't print anything yet! */
	do {
		Letter = '0';	/* Init the char */
		LongVal = Tens[Index];	/* Init the value into a local */
		while (Val >= LongVal) {	/* Is the number in question larger? */
			Val -= LongVal;		/* Sub the tens value */
			++Letter;			/* Inc the char */
			Hit=1;				/* I must draw! */
		}
		if (Hit) {	/* Remove the leading zeros */
			Text[0] = Letter;	/* Save char in the string */
			++Text;			/* Inc dest */
		}
	} while (--Index);		/* All the tens done? */
	Text[0] = Val + '0';	/* Must print FINAL digit */
	Text[1] = 0;			/* End the string */
}

/**********************************

	Sound sub-system

**********************************/

/**********************************

	Shut down the sound

**********************************/

void SoundOff(void)
{
	PlaySound(0);
}

/**********************************

	Play a sound resource

**********************************/

void PlaySound(Word SoundNum)
{
	if (SoundNum && (SystemState&SfxActive)) {
		SoundNum+=127;
		if (SoundNum&0x8000) {		/* Mono sound */
			EndSound(SoundNum&0x7fff);
		}
		BeginSound(SoundNum&0x7fff,11127<<17L);
	} else {
		EndAllSound();
	}
}

/**********************************

	Stop playing a sound resource

**********************************/

void StopSound(Word SoundNum)
{
	EndSound(SoundNum+127);
}

static Word LastSong = -1;

void PlaySong(Word Song)
{
	if (Song) {
		KilledSong = Song;
		if (SystemState&MusicActive) {
			if (Song!=LastSong) {
				BeginSongLooped(Song);	
				LastSong = Song;
			}
			return;
		}
	} 
	EndSong();
	LastSong = -1;
}

/**********************************

	Graphics subsystem

**********************************/

/**********************************

	Draw a masked shape

**********************************/

void InitYTable(void)
{
	Word i;
	LongWord Offset;

	i = 0;
	Offset = 0;
	do {
		YTable[i] = Offset;
		Offset+=VideoWidth;
	} while (++i<480);
}

/**********************************

	Draw a shape

**********************************/

void DrawShape(Word x,Word y,void *ShapePtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *ShapePtr2;
	unsigned short *ShapePtr3;
	Word Width;
	Word Height;
	Word Width2;

	ShapePtr3 = ShapePtr;
	Width = ShapePtr3[0];		/* 16 bit width */
	Height = ShapePtr3[1];		/* 16 bit height */
	ShapePtr2 = (unsigned char *) &ShapePtr3[2];
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	do {
		Width2 = Width;
		Screenad = ScreenPtr;
		do {
			*Screenad++ = *ShapePtr2++;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
	} while (--Height);
}

/**********************************

	Draw a masked shape

**********************************/

void DrawMShape(Word x,Word y,void *ShapePtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *MaskPtr;
	unsigned char *ShapePtr2;
	Word Width;
	Word Height;
	Word Width2;

	ShapePtr2 = ShapePtr;
	Width = ShapePtr2[1];
	Height = ShapePtr2[3];
	ShapePtr2 +=4;
	MaskPtr = &ShapePtr2[Width*Height];
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	do {
		Width2 = Width;
		Screenad = ScreenPtr;
		do {
			*Screenad = (*Screenad & *MaskPtr++) | *ShapePtr2++;
			++Screenad;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
	} while (--Height);
}

/**********************************

	Draw a masked shape with an offset

**********************************/

void DrawXMShape(Word x,Word y,void *ShapePtr)
{
	unsigned short *ShapePtr2;
	ShapePtr2 = ShapePtr;
	x += ShapePtr2[0];
	y += ShapePtr2[1];
	DrawMShape(x,y,&ShapePtr2[2]);
}

/**********************************

	Erase a masked shape

**********************************/

void EraseMBShape(Word x,Word y, void *ShapePtr, void *BackPtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *Backad;
	unsigned char *BackPtr2;
	unsigned char *MaskPtr;
	Word Width;
	Word Height;
	Word Width2;

	MaskPtr = ShapePtr;		/* Get the pointer to the mask */
	Width = MaskPtr[1];		/* Get the width of the shape */
	Height = MaskPtr[3];	/* Get the height of the shape */
	MaskPtr = &MaskPtr[(Width*Height)+4];	/* Index to the mask */
							/* Point to the screen */
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	BackPtr2 = BackPtr;
	BackPtr2 = &BackPtr2[(y*SCREENWIDTH)+x];	/* Index to the erase buffer */
	do {
		Width2 = Width;		/* Init width count */
		Screenad = ScreenPtr;
		Backad = BackPtr2;
		do {
			if (!*MaskPtr++) {
				*Screenad = *Backad;
			}
			++Screenad;
			++Backad;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
		BackPtr2 += SCREENWIDTH;
	} while (--Height);
}

/**********************************

	Test for a shape collision

**********************************/

Word TestMShape(Word x,Word y,void *ShapePtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *MaskPtr;
	unsigned char *ShapePtr2;
	Word Width;
	Word Height;
	Word Width2;

	ShapePtr2 = ShapePtr;
	Width = ShapePtr2[0];
	Height = ShapePtr2[1];
	ShapePtr2 +=2;
	MaskPtr = &ShapePtr2[Width*Height];
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	do {
		Width2 = Width;
		Screenad = ScreenPtr;
		do {
			if (!*MaskPtr++) {
				if (*Screenad != *ShapePtr2) {
					return 1;
				}
			}
			++ShapePtr2;
			++Screenad;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
	} while (--Height);
	return 0;
}

/**********************************

	Test for a masked shape collision

**********************************/

Word TestMBShape(Word x,Word y,void *ShapePtr,void *BackPtr)
{
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *Backad;
	unsigned char *BackPtr2;
	unsigned char *MaskPtr;
	Word Width;
	Word Height;
	Word Width2;

	MaskPtr = ShapePtr;		/* Get the pointer to the mask */
	Width = MaskPtr[0];		/* Get the width of the shape */
	Height = MaskPtr[1];	/* Get the height of the shape */
	MaskPtr = &MaskPtr[(Width*Height)+2];	/* Index to the mask */
							/* Point to the screen */
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[y]+x];
	BackPtr2 = BackPtr;
	BackPtr2 = &BackPtr2[(y*SCREENWIDTH)+x];	/* Index to the erase buffer */
	do {
		Width2 = Width;		/* Init width count */
		Screenad = ScreenPtr;
		Backad = BackPtr2;
		do {
			if (!*MaskPtr++) {
				if (*Screenad != *Backad) {
					return 1;
				}
			}
			++Screenad;
			++Backad;
		} while (--Width2);
		ScreenPtr +=VideoWidth;
		BackPtr2 += SCREENWIDTH;
	} while (--Height);
	return 0;
}

/**********************************

	Show a full screen picture

**********************************/

void ShowPic(Word PicNum)
{
	DrawShape(0,0,LoadAResource(PicNum));	/* Load the resource and show it */
	ReleaseAResource(PicNum);			/* Release it */
	BlastScreen();
}

/**********************************

	Clear the screen to a specific color

**********************************/

void ClearTheScreen(Word Color)
{
	Word x,y;
	unsigned char *TempPtr;

	TempPtr = VideoPointer;
	y = SCREENHEIGHT;		/* 200 lines high */
	do {
		x = 0;
		do {
			TempPtr[x] = Color;	/* Fill color */
		} while (++x<SCREENWIDTH);
		TempPtr += VideoWidth;	/* Next line down */
	} while (--y);
}

/**********************************

	Draw a text string

**********************************/

void DrawAString(char *TextPtr)
{
	while (TextPtr[0]) {		/* At the end of the string? */
		DrawAChar(TextPtr[0]);	/* Draw the char */
		++TextPtr;			/* Continue */
	}
}

/**********************************

	Set the X/Y to the font system

**********************************/

void SetFontXY (Word x,Word y)
{
	FontX = x;
	FontY = y;
}

/**********************************

	Make color zero invisible

**********************************/

void FontUseMask(void)
{
	FontInvisible = 0;
	FontSetColor(0,0);
}

/**********************************

	Make color zero a valid color

**********************************/

void FontUseZero(void)
{
	FontInvisible = -1;
	FontSetColor(0,BLACK);
}

/**********************************

	Set the color entry for the font

**********************************/

void FontSetColor(Word Num,Word Color)
{
	FontOrMask[Num] = Color;
}

/**********************************

	Install a font into memory

**********************************/

typedef struct FontStruct {
	unsigned short FHeight;
	unsigned short FLast;
	unsigned short FFirst;
	unsigned char FData;
} FontStruct;

void InstallAFont(Word FontNum)
{
	FontStruct *FPtr;

	if (FontLoaded) {
		if (FontLoaded == FontNum) {
			return;
		}
		ReleaseAResource(FontLoaded);
	}
	FontLoaded = FontNum;
	FPtr = LoadAResource(FontNum);
	FontHeight = SwapUShort(FPtr->FHeight);
	FontLast = SwapUShort(FPtr->FLast);
	FontFirst = SwapUShort(FPtr->FFirst);
	FontWidths = &FPtr->FData;
	FontPtr = &FontWidths[FontLast];
}

/**********************************

	Draw a char to the screen

**********************************/

void DrawAChar(Word Letter)
{
	Word XWidth;
	Word Offset;
	Word Width;
	Word Height;
	int Width2;
	unsigned char *Font;
	unsigned char *ScreenPtr;
	unsigned char *Screenad;
	unsigned char *FontOr;
	Word Temp;
	Word Temp2;

	Letter -= FontFirst;		/* Offset from the first entry */
	if (Letter>=FontLast) {		/* In the font? */
		return;					/* Exit then! */
	}
	XWidth = FontWidths[Letter];	/* Get the pixel width of the entry */
	Width = (XWidth-1)/2;
	Font = &FontPtr[Letter*2];
	Offset = (Font[1]*256) + Font[0];
	Font = &FontPtr[Offset];
	ScreenPtr = (unsigned char *) &VideoPointer[YTable[FontY]+FontX];
	FontX+=XWidth;
	Height = FontHeight;
	FontOr = &FontOrMask[0];

	do {
		Screenad = ScreenPtr;
		Width2 = Width;
		do {
			Temp = *Font++;
			Temp2 = Temp>>4;
			if (Temp2 != FontInvisible) {
				Screenad[0] = FontOr[Temp2];
			}
			Temp &= 0x0f;
			if (Temp != FontInvisible) {
				Screenad[1] = FontOr[Temp];
			}
			Screenad+=2;		/* Next address */
		} while(--Width2>=0);
		ScreenPtr += VideoWidth;
	} while (--Height);
}

/**********************************

	Palette Manager

**********************************/

/**********************************

	Load and set a palette resource

**********************************/

void SetAPalette(Word PalNum)
{
	SetAPalettePtr(LoadAResource(PalNum));		/* Set the current palette */
	ReleaseAResource(PalNum);					/* Release the resource */
}

/**********************************

	Load and set a palette from a pointer

**********************************/

Byte CurrentPal[768];

void SetAPalettePtr(unsigned char *PalPtr)
{
	CTabHandle ColorHandle;		/* Handle to the main palette */
	Handle PalHand;			/* Handle to palette */
	Word i;					/* Temp */
	CSpecArray *Colors;		/* Pointer to color array */
	GDHandle OldDevice;
	
	memcpy(CurrentPal,PalPtr,768);
	ColorHandle = MainColorHandle;
	HLock((Handle) ColorHandle);
	Colors = &(*ColorHandle)->ctTable;
	++Colors;		/* Go to color #0 */
	i = 1;			/* Skip color #0 */
	PalPtr+=3;
	do {			/* Fill in all the color entries */
		Colors[0]->rgb.red = (Word) (PalPtr[0]<<8) | PalPtr[0];
		Colors[0]->rgb.green = (Word) (PalPtr[1]<<8) | PalPtr[1];
		Colors[0]->rgb.blue = (Word) (PalPtr[2]<<8) | PalPtr[2];
		if (!Colors[0]->rgb.blue) {
			Colors[0]->rgb.blue = 0x0101;
		}
	    PalPtr+=3;
	    ++Colors;
	} while (++i<255);	/* All done? */
	OldDevice = GetGDevice();
	SetGDevice(gMainGDH);
	SetEntries(0,255-1,(*ColorHandle)->ctTable);	/* Set the color entries */
	PalHand = (Handle) (**(*GameGWorld).portPixMap).pmTable;
	PtrToXHand(*ColorHandle,PalHand,8+(8*256));
	PalHand = (Handle) (**(*GameWindow).portPixMap).pmTable;
	PtrToXHand(*ColorHandle,PalHand,8+(8*256));
	HUnlock((Handle)ColorHandle);	/* Release the main handle */
	MakeITable(0,0,0);				/* Create the proper color table */
	SetGDevice(OldDevice);
}

/**********************************

	Fade the screen to black

**********************************/

void FadeToBlack(void)
{
	unsigned char MyPal[768];

	memset(MyPal,0,sizeof(MyPal));	/* Fill with black */
	MyPal[0] = MyPal[1] = MyPal[2] = 255;
	FadeToPtr(MyPal);
}

/**********************************

	Fade the screen to a palette

**********************************/

void FadeTo(Word RezNum)
{
	FadeToPtr(LoadAResource(RezNum));
	ReleaseAResource(RezNum);
}

/**********************************

	Fade the palette

**********************************/

void FadeToPtr(unsigned char *PalPtr)
{
	int DestPalette[768];				/* Dest offsets */
	Byte WorkPalette[768];		/* Palette to draw */
	Byte SrcPal[768];
	Word Count;
	Word i;
	
	if (!memcmp(PalPtr,&CurrentPal,768)) {	/* Same palette? */
		return;
	}
	memcpy(SrcPal,CurrentPal,768);
	i = 0;
	do {		/* Convert the source palette to ints */
		DestPalette[i] = PalPtr[i];			
	} while (++i<768);

	i = 0;
	do {
		DestPalette[i] -= SrcPal[i];	/* Convert to delta's */
	} while (++i<768);

	Count = 1;
	do {
		i = 0;
		do {
			WorkPalette[i] = ((DestPalette[i] * (int)(Count)) / 16) + SrcPal[i];
		} while (++i<768);
		SetAPalettePtr(WorkPalette);
		WaitTicks(1);
	} while (++Count<17);
}

/**********************************

	Resource manager subsystem

**********************************/

/**********************************

	Load a personal resource

**********************************/

void *LoadAResource(Word RezNum) 
{
	return(LoadAResource2(RezNum,'BRGR'));
}

/**********************************

	Load a global resource

**********************************/

Handle RezHandle;

void *LoadAResource2(Word RezNum,LongWord Type)
{
	Handle MyHand;
	Word Stage;
	
	Stage = 0;
	do {
		Stage = FreeStage(Stage,128000);
		MyHand = GetResource(Type,RezNum);
		if (MyHand) {
			RezHandle = MyHand;
			HLock(MyHand);
			return *MyHand;
		}
	} while (Stage);
	return 0;
}

/**********************************

	Allow a resource to be purged

**********************************/

void ReleaseAResource(Word RezNum)
{
	ReleaseAResource2(RezNum,'BRGR');
}

/**********************************

	Release a global resource

**********************************/

void ReleaseAResource2(Word RezNum,LongWord Type)
{
	Handle MyHand;
	
	MyHand = GetResource(Type,RezNum);	/* Get the resource if available */
	HUnlock(MyHand);	
	HPurge(MyHand);			/* Mark handle as purgeable */
}

/**********************************

	Force a resource to be destroyed

**********************************/

void KillAResource(Word RezNum)
{
	KillAResource2(RezNum,'BRGR');
}

/**********************************

	Kill a global resource

**********************************/

void KillAResource2(Word RezNum,LongWord Type)
{
	Handle MyHand;
	MyHand = GetResource(Type,RezNum);	/* Get the resource if available */
	ReleaseResource(MyHand);
}

void SaveJunk(void *AckPtr,Word Length)
{
static Word Count=1;
	FILE *fp;
	char SaveName[40];
	sprintf(SaveName,"SprRec%d",Count);
	fp = fopen(SaveName,"wb");
	fwrite(AckPtr,1,Length,fp);
	fclose(fp);
	++Count;
}

/**********************************

	Kill a global resource

**********************************/

unsigned short SwapUShort(unsigned short Val)
{
	return ((Val<<8) | (Val>>8));
}

/**********************************

	Decompress using LZSS

**********************************/

#if 1
void DLZSS(Byte *Dest,Byte *Src,LongWord Length)
{
	Word BitBucket;
	Word RunCount;
	Word Fun;
	Byte *BackPtr;
	
	if (!Length) {
		return;
	}
	BitBucket = (Word) Src[0] | 0x100;
	++Src;
	do {
		if (BitBucket&1) {
			Dest[0] = Src[0];
			++Src;
			++Dest;
			--Length;
		} else {
			RunCount = (Word) Src[0] | ((Word) Src[1]<<8);
			Fun = 0x1000-(RunCount&0xfff);
			BackPtr = Dest-Fun;
			RunCount = ((RunCount>>12) & 0x0f) + 3;
			if (Length >= RunCount) {
				Length -= RunCount;
			} else {
				Length = 0;
			}
			do {
				*Dest++ = *BackPtr++;
			} while (--RunCount);
			Src+=2;
		}
		BitBucket>>=1;
		if (BitBucket==1) {
			BitBucket = (Word)Src[0] | 0x100;
			++Src;
		}
	} while (Length);
}
#endif

/**********************************

	Allocate some memory

**********************************/

void *AllocSomeMem(LongWord Size)
{
	return SDL_malloc(Size);
#if 0
	void *MemPtr;
	Word Stage;
	
	Stage = 0;
	do {
		Stage = FreeStage(Stage,Size);
		MemPtr = NewPtr(Size);		/* Get some memory */
		if (MemPtr) {
			return MemPtr;			/* Return it */
		}
	} while (Stage);
	if (!NoSystemMem) {
		MemPtr = NewPtrSys(Size);
	}
	return MemPtr;
#endif
}

/**********************************

	Allocate some memory

**********************************/

static Word FreeStage(Word Stage,LongWord Size) 
{
	switch (Stage) {
	case 1:
		PurgeAllSounds(Size);		/* Kill off sounds until I can get memory */
		break;
	case 2:
		PlaySound(0);				/* Shut down all sounds... */
		PurgeAllSounds(Size);		/* Purge them */
		break;
	case 3:
		PlaySong(0);				/* Kill music */
		FreeSong();					/* Purge it */
		PurgeAllSounds(Size);		/* Make SURE it's gone! */
		break;
	case 4:
		return 0;
	}
	return Stage+1;
}

/**********************************

	Release some memory

**********************************/

void FreeSomeMem(void *MemPtr)
{
	SDL_free(MemPtr);
}
