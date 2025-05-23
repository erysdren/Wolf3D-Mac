/*********************

	Main Macintosh code

*********************/

#include "Wolfdef.h"		/* All the game equates */
#include <string.h>
#include <ctype.h>
#include <setjmp.h>
#include <palettes.h>
#include <qdoffscreen.h>
#include <Gestalt.h>
#include "SoundMusicSystem.h"
#include "PickAMonitor.h"
#include "Hidemenubar.h"
#include "Prefs.h"
#include <AppleEvents.h>

static Word DoMyAlert(Word AlertNum);
static void CenterSFWindow(Point *MyPoint,Word x,Word y);
static void CenterAWindow(WindowPtr MyWindow);
static void FixPauseMenu(void);
Boolean LoadGame(void);
pascal Boolean StandardModalFilterProc(DialogPtr theDialog, EventRecord *theEvent, short *itemHit);
Word ChooseGameDiff(void);
Word DoEvent(EventRecord *event);
void DoUpdate(WindowPtr window);
void DrawWindow(WindowPtr window);
void AdjustMenus(void);
void DoMenuCommand(LongWord menuResult);
Boolean DoCloseWindow(WindowPtr window);
void InitTools(void);
void SaveGame(void);
void LoadPrefs(void);
void SavePrefs(void);
Boolean ChooseLoadGame(void);
Boolean ChooseSaveGame(void);
OSErr MyEventHandler(AppleEvent *MyAppleEvent,AppleEvent *AppleReply,long MyRefCon);

Boolean MouseHit;		/* True if a mouse was pressed */
static MenuHandle PauseMenu;
static Boolean WaitNextOK;		/* True if WaitNextEvent is present */
static Boolean LowOnMem;		/* True if sound is disabled for low memory */
static Boolean OpenPending;		/* True if a load game apple event occured */
static Byte *SaveFileName;		/* Pointer to the save file name */
static short SaveFileVol;		/* Directory referance for save file name */	
static long SaveFileParID;		/* Parent directory id of save-game file */
static Boolean ValidRects;		/* Are there blank rects in the window? */
static RgnHandle BlackRgn;		/* Blank region for window blanking */
static Rect QDGameRect;			/* Rect of the game playing area */
static Rect GameRect = {0,0,200,320};	/* Size of the playfield */
static Rect BlackRect = {0,0,480,640};	/* Rect for the Black Window (Set to main window size) */
static Rect BlackRect2 = {0,0,480,640};	/* True rect for the Black window (Global) */
static Word OldPixDepth;		/* Previous pixel depth of screen */
Word MacWidth;					/* Width of play screen (Same as GameRect.right) */
Word MacHeight;					/* Height of play screen (Same as GameRect.bottom) */
Word MacViewHeight;				/* Height of 3d screen (Bottom of 3D view */
static Word MonitorWidth=640;		/* Width of the monitor in pixels */
static Word MonitorHeight=480;		/* Height of the monitor in pixels */
Word QuitFlag;					/* I quit if true */
Word TrueVideoWidth;			/* Width of video monitor in bytes */
Byte *TrueVideoPointer;			/* Pointer to the video monitor memory */
Byte *GameVideoPointer;			/* Pointer to start of 3D view on monitor memory (Used by BlastScreen) */
Boolean DoQuickDraw = TRUE;		/* Use quickdraw for updates? */	
static Boolean DimQD;			/* Force QuickDraw forever! */
Word IgnoreMessage;				/* Variable set by prefs to ignore messages */
static CursHandle WatchHandle;	/* Handle to a watch cursor */
#define RectX ((MonitorWidth-MacWidth)/2)
#define RectY ((MonitorHeight-MacHeight)/2)
Word VidXs[] = {320,512,640,640};	/* Screen sizes to play with */
Word VidYs[] = {200,384,400,480};
Word VidVs[] = {160,320,320,400};
Word VidPics[] = {rFaceShapes,rFace512,rFace640,rFace640};		/* Resource #'s for art */
static Word MouseBaseX;
static Word MouseBaseY;
static Boolean InPause;			/* Pause active? */

extern jmp_buf ResetJmp;
extern Boolean JumpOK;
extern CWindowPtr GameWindow;	/* Pointer to main game window */
extern CGrafPtr GameGWorld;		/* Grafport to offscreen buffer */
GDHandle gMainGDH;				/* Main graphics handle */
CTabHandle MainColorHandle;		/* Main color table handle */

static void WaitCursor(void);
static void ResetPalette(void);
static void DoBackground(EventRecord *event);
static void InitAEStuff(void);
static void FixTheCursor(void);

/***************************

	Dialog filter for 68020/30 version speed warning

***************************/

#ifndef __powerc
static pascal Boolean InitFilter(DialogPtr theDialog, EventRecord *theEvent, short *itemHit)
{
	char	theChar;
	Handle	theHandle;
	short	theType;
	Rect	theRect;

	switch (theEvent->what) {
	case keyDown:
		theChar = theEvent->message & charCodeMask;
		switch (theChar) {
		case 0x0D:		/* Return */
		case 0x03:		/* Enter */
			*itemHit = 1;
			return (TRUE);
		}

	case updateEvt:
		GetDialogItem(theDialog,1, &theType, &theHandle, &theRect);
		InsetRect(&theRect, -4, -4);
		PenSize(3,3);
		FrameRoundRect(&theRect, 16, 16);
	}
	return (FALSE);		/* Tells dialog manager to handle the event */
}
#endif

/***************************

	Init the Macintosh and all the
	system tools

***************************/

void InitTools(void)
{
	Handle menuBar;				/* Handle to menu bar record */
	Word i;						/* Temp */
	long Feature;				/* Gestalt return value */
	short int *SoundListPtr;	/* Pointer to sound list for Halestorm driver */

	MaxApplZone();	/* Expand the heap so code segments load at the top */
	InitGraf((Ptr) &qd.thePort);	/* Init the graphics system */
	InitFonts();					/* Init the font manager */
	InitWindows();					/* Init the window manager */
	InitMenus();					/* Init the menu manager */
	TEInit();						/* Init text edit */
	InitDialogs(nil);				/* Init the dialog manager */
	i = 16;							/* Make 64*16 handle records */
	do {
		MoreMasters();				/* Create a BUNCH of new handles */
	} while (--i);
#ifdef __powerc
	WaitNextOK = TRUE;
#else
	if (NGetTrapAddress(0xA860,1) != (void *)0xa89F) {
		WaitNextOK = TRUE;
	}
#endif

	InitAEStuff();					/* Apple events shit */
	InitCursor();					/* Reset the cursor */
	WatchHandle = GetCursor(watchCursor);	/* Get the watch cursor */
	HLock((Handle)WatchHandle);		/* Lock it down */
	WaitCursor();					/* Set the watch cursor */
	LoadPrefs();					/* Load the prefs file */
	
	/* Now that the mac is started up, let's make sure I can run on this particular mac */

#ifndef __powerc		/* Power macs REQUIRE 7.1 to run so don't even check on power macs */

	if (Gestalt(gestaltVersion,&Feature)) {	/* Is gestalt working? */
		goto Not607;			/* Oh oh... */
	}
		
	/* Check for System 6.0.7 or later */
	
	if (Gestalt(gestaltSystemVersion, &Feature) ||
		(short)Feature < 0x0607) {	/* Is it 6.0.7 or later? */
		goto Not607;
	}

#endif

	/* Check for Color QuickDraw.  If there's no color, quit.  Below, we check for
		32-bit color QuickDraw.  */
		
	if (Gestalt(gestaltQuickdrawVersion, &Feature)) {
Not607:
		DoMyAlert(Not607Win);		/* Alert the user */
		GoodBye();				/* Exit now */
	}
	
	/* Check for 32-bit Color QuickDraw */
	
	if( (Feature & 0x0000ffff) < 0x0200) {	/* Not 32 bit? */
		DoMyAlert(Not32QDWin);	/* I need 32 bit quickdraw! */
		GoodBye();				/* Exit */
	}
	
		/*  Check the Mac's processor.  If it's a 68030 or earlier, warn the user it
		might be slow, and note the best ways to speed it up.  */
	
#ifndef __powerc		/* Power macs don't need this message */
	Gestalt(gestaltProcessorType,&Feature);
	i = 0;
	switch(Feature) {
	case gestalt68000:
		++i;
		ParamText("\p68000","\p","\p","\p");
		break;
	case gestalt68020:
		++i;
		ParamText("\p68020","\p","\p","\p");
		break;
	case gestalt68030:
		++i;
		ParamText("\p68030","\p","\p","\p");
		break;
	}
	
	if (i && !IgnoreMessage) {
		short 	theValue,itemHit,iType;
		Handle	iHndl;
		Rect	iRect;
		DialogPtr MyDialog;
		ModalFilterUPP theDialogFilter;

		theDialogFilter = NewModalFilterProc(InitFilter);	/* Create a code pointer */
		MyDialog = GetNewDialog(SlowWarnWin,0L,(WindowPtr)-1);	/* Load my dialog from disk */
		CenterAWindow(MyDialog);
		ShowWindow(MyDialog);	/* Display with OK button framed */
		SetPort((WindowPtr) MyDialog);
		InitCursor();					/* Init the cursor */
		do {
			ModalDialog(theDialogFilter,&itemHit);		/* Handle the dialog */
			if (itemHit==2) {		/* The checkbox */
				GetDialogItem(MyDialog,2, &iType, &iHndl, &iRect);
				theValue = GetControlValue((ControlHandle)iHndl);
				SetControlValue( (ControlHandle) iHndl, !theValue );
			}
		} while (itemHit!=1);
		GetDialogItem(MyDialog,2, &iType, &iHndl, &iRect);	/* Get the check box state */
		IgnoreMessage = GetControlValue((ControlHandle)iHndl);
		if (IgnoreMessage) {		/* If I should ignore... */
			SavePrefs();			/* Save the prefs file */
		}
		DisposeDialog(MyDialog);	/* Release the dialog memory */
	}
#endif
	
/* Let's set up the environment... */
	
	InitSoundMusicSystem(8,8,5,jxLowQuality);
	PurgeSongs(TRUE);			/* Allow songs to stay in memory */
	SoundListPtr = (short int *) LoadAResource(MySoundList);	/* Get the list of sounds */
	RegisterSounds(SoundListPtr,FALSE);	
	ReleaseAResource(MySoundList);			/* Release the sound list */
	
	GetTableMemory();			/* Get memory for far tables math tables */	 	
	MapListPtr = (maplist_t *) LoadAResource(rMapList);	/* Get the map list */
	SongListPtr = (unsigned short *) LoadAResource(rSongList);
	WallListPtr = (unsigned short *) LoadAResource(MyWallList);

/* Alert the user that you MUST pay! */

	if (MapListPtr->MaxMap==3) {	/* Shareware version? */
		DoMyAlert(ShareWareWin);	/* Show the shareware message */
	}
	if (FreeMem() < 3000000L) {		/* Are you low on memory? */			
		DoMyAlert(LowMemWin);
		SystemState = 0;			/* Turn off music and sound (Just in case) */
		LowOnMem = TRUE;
	}
	ShowCursor();					/* Make sure the cursor is ok */
	WaitCursor();					/* Restore the watch cursor */

/* Try to set up 8-bit color mode */

	gMainGDH = PickAMonitor(8,TRUE,512,384);	/* Pick a monitor to play on */
	if (!gMainGDH) {			/* No macs that can display or canceled */
		GoodBye();				/* Exit */
	}
	OldPixDepth = (**(**gMainGDH).gdPMap).pixelSize;		/* Get the previous mode */
	if (OldPixDepth != 8) {									/* Was it 8 bit already? */
		 if (SetDepth(gMainGDH,8, 1 << gdDevType,1)) {		/* Set to 8 bit */
			BailOut();				/* Oh oh... */
		 }
	}
	MainColorHandle = (*(*gMainGDH)->gdPMap)->pmTable;		/* Get my main device's color handle */
	BlackRect2 = (**gMainGDH).gdRect;		/* Init the black rect */
	LockPixels((**gMainGDH).gdPMap);	/* Lock down the memory FOREVER! */
	TrueVideoPointer = (Byte *) GetPixBaseAddr((**gMainGDH).gdPMap);	/* Get pointer to screen mem */
	TrueVideoWidth = (Word) ((**(**gMainGDH).gdPMap).rowBytes & 0x3FFF);	/* Get width of screen mem */

	if (!TrueVideoPointer || !TrueVideoWidth) {
		DimQD = TRUE;				/* Don't allow offscreen drawing */
		DoQuickDraw = TRUE;	/* Force QD */
	}
	MonitorHeight = BlackRect2.bottom - BlackRect2.top;	/* Get the size of the video screen */
	MonitorWidth = BlackRect2.right - BlackRect2.left;
	MouseBaseX = BlackRect2.left+256;
	MouseBaseY = BlackRect2.top+192;
	
	menuBar = GetNewMBar(MyMenuBar);	/* read menus into menu bar */
	if (!menuBar) {
		BailOut();			/* Menu bar error */
	}	
	SetMenuBar(menuBar);			/* install menus */
	DisposeHandle(menuBar);			/* Release the menu bar */
	AppendResMenu(GetMenuHandle(mApple),'DRVR');	/* add DA names to Apple menu */
	DrawMenuBar();				/* Show the menu bar */
	HideMenuBar();			/* Hide the menu bar */

	GameWindow = (CWindowPtr)NewCWindow(nil,&BlackRect2, "\p",TRUE, plainDBox, (WindowPtr) -1L, FALSE, 0);
	if (!GameWindow) {		/* No background window? */
		BailOut();
	}
	BlackRgn = NewRgn();	/* Make a region for the black area */
	SetPort((WindowPtr)GameWindow);		/* Use this grafport */
	ActivatePalette((WindowPtr)GameWindow);
	BlackRect.top = 0;				/* Init the main rect */
	BlackRect.left = 0;
	BlackRect.bottom = MonitorHeight;
	BlackRect.right = MonitorWidth;
	FillRect(&BlackRect,&qd.black);	/* Erase the screen to black */
	NewGameWindow(1);				/* Create a game window at 512x384 */
	ClearTheScreen(BLACK);			/* Force the offscreen memory blank */
	BlastScreen();
	FixTheCursor();					/* Fix the cursor */
}

/************************************

	Set the hardware palette to match my window palette for high speed
	copybits operation
	
************************************/

extern Byte CurrentPal[768];		/* Last palette the game was set to */

static void ResetPalette(void)
{	
	SetPort((WindowPtr) GameWindow);		/* Reset the game window */
	if ((**(**gMainGDH).gdPMap).pixelSize != 8) {		/* Get the previous mode */
		SetDepth(gMainGDH,8, 1 << gdDevType,1);		/* Set to 8 bit color */
	}
	SetAPalettePtr(CurrentPal);				/* Restore the palette */
}

/************************************

	Process the suspend/resume events
	
************************************/

static void DoBackground(EventRecord *event)
{
	EventRecord PauseEvent;

	if ((event->message & 0xff000000) ==0x01000000) {		/* Suspend/Resume events */
		if (!(event->message & resumeFlag)) {		/* Suspend event? */
			if (!InPause) {
				PauseSoundMusicSystem();		/* Stop the music */
				ShowMenuBar();	/* Display the menu bar */
			}
			InitCursor();	/* Restore the arrow cursor */
	
		/*	Call a new event loop while in background to let us know when we've returned. */
				
			do {
				WaitNextEvent2(everyEvent, &PauseEvent, 0xFF, nil);
				if (PauseEvent.what==updateEvt) {		/* Time for update? */
					DoUpdate((WindowPtr) PauseEvent.message);	/* Redraw the window */
				}
			} while (PauseEvent.what!=mouseDown && PauseEvent.what!=activateEvt && PauseEvent.what!=app4Evt);
	
	/*	Turn the main game window back on and update its contents. */

			ResetPalette();		/* Reset the game palette */
			FixTheCursor();		/* Make it disappear for game if needed */
			if (!InPause) {		
				HideMenuBar();			/* Make the menu bar disappear */
				ResumeSoundMusicSystem();	/* Restart the music */
			}
		}
	}
}

/************************************

	Call Wait next event if present

************************************/

Boolean WaitNextEvent2(short EventMask,EventRecord *theEvent,long sleep,RgnHandle mouseRgn)
{
	if (WaitNextOK) {
		return WaitNextEvent(EventMask,theEvent,sleep,mouseRgn);
	}
	SystemTask();
	return GetNextEvent(EventMask,theEvent);	
}

/************************************

	Execute the apple event
	
************************************/

static void HandleHighLevelEvent(EventRecord *event)
{
	AEProcessAppleEvent(event);
	if (QuitFlag) {
		GoodBye();
	}
}

/************************************

	Do the right thing for an event. Determine what kind of
	event it is, and call the appropriate routines.
	This is like TaskMaster for the IIgs

************************************/

Word DoEvent(EventRecord *event)
{
	Word part;
	WindowPtr	window;
	Word key;
	Point aPoint;
	
	switch (event->what) {	/* Process the event */
	case kHighLevelEvent:		/* Apple events? */
		HandleHighLevelEvent(event);	/* Pass it on... */
		break;
				
	case mouseDown:	/* Pressed the mouse? */
		FixMHeight();		/* Allow menu bar clicks */
		part = FindWindow(event->where, &window);	/* Choose the hit */
		ZapMHeight();		/* Don't click anymore */
		switch (part) {
		case inMenuBar:			/* process a mouse menu command (if any) */
			AdjustMenus();		/* Enable/disable menu items */
			ShowMenuBar();		/* Display the menu bar */
			DoMenuCommand(MenuSelect(event->where));	/* Handle the command */
			if (!InPause) {
				HideMenuBar();		/* Hide the menu bar */
			}
			break;
		case inSysWindow:			/* let the system handle the mouseDown */
			SystemClick(event, window);	/* Pass on the event */
			break;
		case inContent:
			if (window != FrontWindow() ) {
				SelectWindow(window);	/* Make it the front window */
			}
			if (window == (WindowPtr)GameWindow) {
				MouseHit = TRUE;
			}
			break;
		case inDrag:		/* pass screenBits.bounds to get all gDevices */
			DragWindow(window, event->where, &qd.screenBits.bounds);
			break;
		case inGoAway:
			if (TrackGoAway(window,event->where)) {	/* Handle the close box */
				DoCloseWindow(window);	/* Close my window */
			}
			break;
		case inZoomIn:
		case inZoomOut:
			if (TrackBox(window, event->where, part)) {	/* Track the zoom box */
				SetPort(window);				/* the window must be the current port... */
				EraseRect(&window->portRect);	/* because of a bug in ZoomWindow */
				ZoomWindow(window,part,TRUE);	/* note that we invalidate and erase... */
				InvalRect(&window->portRect);	/* to make things look better on-screen */
			}
			break;
		}
		break;
	case keyDown:
	case autoKey:						/* check for menukey equivalents */
		key = event->message & charCodeMask;
		if (event->modifiers & cmdKey) {			/* Command key down */
			if (event->what == keyDown) {
				AdjustMenus();		/* enable/disable/check menu items properly */
				DoMenuCommand(MenuKey(key));	/* Process the key */
			}
			return 0;	/* Ignore the event */
		}
		return key;		/* Return the key pressed */
	case updateEvt:
		DoUpdate((WindowPtr) event->message);	/* Force redraw of my window */
		break;
	case diskEvt:
		if ((event->message>>16) != noErr ) {	/* Was there an error? */
			SetPt(&aPoint, kDILeft, kDITop);
			DIBadMount(aPoint, event->message);	/* Format the disk? */
		}
		break;
	case activateEvt:
		if (event->modifiers & 1) {
			FixTheCursor();		/* Reset the cursor to the hidden state */
			SetPort((WindowPtr)GameWindow);
			ResetPalette();		/* Set my system palette */
		}
		break;
	case osEvt:	
		DoBackground(event);			/* Process suspend / resume */
		break;
	}
	if (OpenPending && JumpOK) {	
		OpenPending=FALSE;				/* Ack the event */
		if (LoadGame()) {				/* Load the game into memory */
			longjmp(ResetJmp,EX_LOADGAME);	/* Restart a loaded game */
		}
	}
	if (!InPause) {
		HideMenuBar();			/* Make SURE the menu bar is gone! */
	}
	return 0;				/* No event processed */
}

/**********************************

	Call this when I receive an update event
	
**********************************/

void DoUpdate(WindowPtr window)
{
	if (((WindowPeek) window)->windowKind >=0) {	/* Is this my window? */
		BeginUpdate(window);			/* this sets up the visRgn */
		DrawWindow(window);				/* Draw the game window */
		EndUpdate(window);				/* Fix the visRgn */
	}
}

/************************

	Update the contents of the window

************************/

void DrawWindow(WindowPtr window)
{
	Word OldQuick;				/* Previous state of the quickdraw flag */
	if (window == (WindowPtr) GameWindow) {	/* Is it my window? */
		SetPort((WindowPtr)GameWindow);
		OldQuick = DoQuickDraw;	/* Save the quickdraw flag */
		DoQuickDraw = TRUE;		/* Force quickdraw */
		BlastScreen();			/* Update the screen */
		DoQuickDraw = OldQuick;	/* Restore quickdraw flag */
		if (ValidRects) {		/* Black region valid? */
			FillRgn(BlackRgn,&qd.black);		/* Fill the black region */
		}
	}
}

/****************************

	Set an item's mark with a check mark

****************************/

static void SetAMark(MenuHandle MyHand,Word Which,Word Var)
{
	Var = (Var) ? 0x12 : 0;
	SetItemMark(MyHand,Which,Var);
}

/****************************

	Enable and disable menu items

****************************/

void AdjustMenus(void)
{
	WindowPtr window;
	MenuHandle FileMenu,EditMenu,OptionMenu;
	
	window = FrontWindow();			/* Which window is in front */
	FileMenu = GetMenuHandle(mFile);		/* Get the file menu handle */
	EditMenu = GetMenuHandle(mEdit);	/* Get the edit menu handle */
	OptionMenu = GetMenuHandle(mOptions);
	if (window!=(WindowPtr)GameWindow) {	/* Enable if there is a window */
		EnableItem(FileMenu,iClose);
		EnableItem(EditMenu,0);
	} else {
		DisableItem(FileMenu,iClose);
		DisableItem(EditMenu,0);
	}
	if (playstate == EX_STILLPLAYING) {	/* Game is in progress */
		EnableItem(FileMenu,iSaveAs);	/* Save the game */
		EnableItem(FileMenu,iSave);		/* Quicksave the game */
	} else {
		DisableItem(FileMenu,iSaveAs);	/* Can't save game when isn't in the mode */
		DisableItem(FileMenu,iSave);
	}
	if (LowOnMem) {
		DisableItem(OptionMenu,iSound);
		DisableItem(OptionMenu,iMusic);
	}
	if (DimQD) {
		DisableItem(OptionMenu,iUseQuickDraw);
	}
	if (playstate==EX_AUTOMAP || playstate==EX_DIED) {
		DisableItem(OptionMenu,iScreenSize);
	} else {
		EnableItem(OptionMenu,iScreenSize);
	}
	SetAMark(OptionMenu,iSound,SystemState&SfxActive);	/* Check the sound menu */
	SetAMark(OptionMenu,iMusic,SystemState&MusicActive);	/* Check the music menu */
	SetAMark(OptionMenu,iGovenor,SlowDown);			/* Check the Speed goveror */
	SetAMark(OptionMenu,iMouseControl,MouseEnabled);				/* Check the mouse flag */
	SetAMark(OptionMenu,iUseQuickDraw,DoQuickDraw);				/* Check the mouse flag */
}

/**********************************

	Process a menu bar event

**********************************/

void DoMenuCommand(LongWord menuResult)
{
	Word menuID;			/* the resource ID of the selected menu */
	Word menuItem;			/* the item number of the selected menu */
	Str255 daName;			/* Name of desk acc */
	Word i;					/* Temp */
	
	menuID = menuResult>>16;	
	menuItem = menuResult&0xffff;	/* get menu item number and menu number */
	switch (menuID) {
	case mApple:				/* Apple menu */
		switch (menuItem) {
		case iAbout:		/* Bring up alert for About */
			PlaySound(SND_MENU);
			DoMyAlert(rAboutAlert);		/* Show the credits */
			PlaySound(SND_OK);
			break;
		case iSpeedHint:	
			PlaySound(SND_MENU);
			DoMyAlert(SpeedTipsWin);	/* Show the hints for speed */
			PlaySound(SND_OK);
			break;
		case iShareWare:
			PlaySound(SND_MENU);
			DoMyAlert(ShareWareWin);	/* Ask about $$$ */
			PlaySound(SND_OK);
			break;
		default:			/* all non-About items in this menu are DAs */
			GetMenuItemText(GetMenuHandle(mApple), menuItem, daName);
			OpenDeskAcc(daName);
			break;
		}
		break;
	case mFile:				/* File menu */
		switch (menuItem) {
		case iNew:	
			if (ChooseGameDiff()) {		/* Choose level of difficulty */
				HiliteMenu(0);
				FixPauseMenu();
				SaveFileName = 0;		/* Zap the save game name */
				longjmp(ResetJmp,EX_NEWGAME);
			}
			break;
		case iClose:
			DoCloseWindow(FrontWindow());
			break;
		case iOpen:
			if (ChooseLoadGame()) {		/* Choose a game to load */
				if (LoadGame()) {				/* Load the game into memory */
					HiliteMenu(0);
					FixPauseMenu();
					longjmp(ResetJmp,EX_LOADGAME);	/* Restart a loaded game */
				}
			}
			break;
		case iSave:
			if (SaveFileName) {			/* Save the file automatically? */
				SaveGame();				/* Save it */
				break;
			}
		case iSaveAs:
			if (ChooseSaveGame()) {		/* Select a save game name */
				SaveGame();				/* Save it */
			}
			break;
		case iQuit:
			GoodBye();	/* Try to quit */
			break;
		}
		break;
	case mEdit:		/* call SystemEdit for DA editing & MultiFinder */
		SystemEdit(menuItem-1);	/* since we don't do any Editing */
		break;
	case mOptions:
		switch (menuItem) {
		case iSound:
			SystemState^=SfxActive;				/* Sound on/off flags */
			if (!(SystemState&SfxActive)) {
				if (InPause) {
					ResumeSoundMusicSystem();
				}
				PlaySound(0);			/* Turn off all existing sounds */
				if (InPause) {
					PauseSoundMusicSystem();
				}
			}
			break;
		case iMusic:
			SystemState^=MusicActive;			/* Music on/off flags */
			if (InPause) {
				ResumeSoundMusicSystem();
			}
			if (SystemState&MusicActive) {
				PlaySong(KilledSong);		/* Restart the music */
			} else {
				PlaySong(0);	/* Shut down the music */
			}
			if (InPause) {
				PauseSoundMusicSystem();
			}
			break;
		case iScreenSize:
			i = DoMyAlert(AskSizeWin);		/* Should I change the size? */
			if (i && i<5) {
				--i;
				if (GameViewSize!=i) {		/* Did you change the size? */
					if (!InPause) {
						HideMenuBar();
					}
					MouseHit = TRUE;		/* Get out of pause mode */
					GameViewSize = i;		/* Set the new size */
					if (playstate==EX_STILLPLAYING || playstate==EX_AUTOMAP) {
TryIt:
						GameViewSize = NewGameWindow(GameViewSize);		/* Make a new window */
						if (!StartupRendering(GameViewSize)) {	/* Set the size of the game screen */
							ReleaseScalers();
							if (!GameViewSize) {
								BailOut();
							}
							--GameViewSize;
							goto TryIt;
						}
						if (playstate==EX_STILLPLAYING) {
							RedrawStatusBar();		/* Redraw the lower area */
						}
						playstate=EX_STILLPLAYING;
						SetAPalette(rGamePal);			/* Reset the game palette */
					}
				}
			}
			break;
		case iGovenor:
			SlowDown^=1;		/* Toggle the slow down flag */
			break;
		case iMouseControl:
			MouseEnabled = (!MouseEnabled);	/* Toggle the cursor */
			FixTheCursor();
			if (MouseEnabled) {
				ReadSystemJoystick();
				mousex = 0;
				mousey = 0;
				mouseturn=0;
				ReadSystemJoystick();
				mousex =0;		/* Discard the results */
				mousey =0;
				mouseturn=0;
			} 
			break;
		case iUseQuickDraw:
			DoQuickDraw^=TRUE;		/* Toggle the quickdraw flag */
			break;
		}
		SavePrefs();
		break;
	}
	HiliteMenu(0);		/* unhighlight what MenuSelect (or MenuKey) hilited */
}

/*********************************

	Close a window, return TRUE if successful

*********************************/

Boolean DoCloseWindow(WindowPtr window)
{
	int Kind;

	if (window) {		/* Valid pointer? */
		Kind = ((WindowPeek) window)->windowKind;	/* Get the kind */
		if (Kind<0) {
			CloseDeskAcc(Kind);
		}
	}
	return TRUE;	/* I closed it! */
}

/***************************

	I can't load! Bail out now!

****************************/

void BailOut(void)
{
	DoMyAlert(rUserAlert);		/* Show the alert window */
	GoodBye();				/* Bail out! */
}

/**********************************

	Update the wolf screen as fast as possible

	Make sure that ForeColor = BLACK and Color = WHITE,
	ROWBYTES should be long aligned (4,8,12,16...)
	Source and dest should be EXACTLY the same bit depth
	ctSeed should match the WINDOW's color seed
	Both rects should be multiples of 4 to make long word transfers only

**********************************/

void BlastScreen2(Rect *BlastRect) 
{
	Rect QDRect;
	CTabHandle ColorHandle,ColorHandle2;

	QDRect = *BlastRect;
	OffsetRect(&QDRect,QDGameRect.left,QDGameRect.top);
	SetPort((WindowPtr) GameWindow);		/* Make sure the port is set */
	ColorHandle = (**(*GameGWorld).portPixMap).pmTable;	/* Get the color table */
	ColorHandle2 = (**(*GameWindow).portPixMap).pmTable;	/* Get the color table */
	PtrToXHand(*MainColorHandle,(Handle)ColorHandle,8+8*256);
	PtrToXHand(*MainColorHandle,(Handle)ColorHandle2,8+8*256);
	CopyBits((BitMap *) *((*GameGWorld).portPixMap),(BitMap *) *((*GameWindow).portPixMap),
		BlastRect,&QDRect,srcCopy,NULL);

}

void BlastScreen(void)
{
	Word i,j,k,m;
	Byte *Screenad;
	LongWord *Dest;
	union {
		LongWord * L;
		Byte *B;
	} Src;
	Point MyPoint;
	
	MyPoint.h = BlackRect2.left;
	MyPoint.v = BlackRect2.top;
	ShieldCursor(&GameRect,MyPoint);
	if (!DoQuickDraw) {	
		i = MacHeight;
		k = MacWidth/64;
		m = VideoWidth - MacWidth;
		Src.B = VideoPointer;	/* Pointer to video */
		Screenad = GameVideoPointer;		/* Get dest video address */
		do {
			j = k;		/* Init width (In Longs) */
			Dest = (LongWord *) Screenad;	/* Reset the dest pointer */
			do {
				Dest[0] = Src.L[0];
				Dest[1] = Src.L[1];
				Dest[2] = Src.L[2];
				Dest[3] = Src.L[3];
				Dest[4] = Src.L[4];
				Dest[5] = Src.L[5];
				Dest[6] = Src.L[6];
				Dest[7] = Src.L[7];
				Dest[8] = Src.L[8];
				Dest[9] = Src.L[9];
				Dest[10] = Src.L[10];
				Dest[11] = Src.L[11];
				Dest[12] = Src.L[12];
				Dest[13] = Src.L[13];
				Dest[14] = Src.L[14];
				Dest[15] = Src.L[15];
				Dest+=16;
				Src.L+=16;
			} while (--j);
			Src.B += m;
			Screenad+=TrueVideoWidth;
		} while (--i); 
	} else {
		BlastScreen2(&GameRect);
	}
	ShowCursor();
	if (!InPause) {
		ObscureCursor();
	}
}

/**********************************

	Create a new game window

**********************************/

Word NewGameWindow(Word NewVidSize)
{			
	Byte *DestPtr;
	LongWord *LongPtr;
	Word i,j;
	Boolean Pass2;
	RgnHandle TempRgn;
	
	Pass2 = FALSE;		/* Assume memory is OK */
	
	/* First, kill ALL previous permenant records */

	if (NewVidSize>=2) {	/* Is this 640 mode? */
		if (MonitorWidth<640) {	/* Can I show 640 mode? */
			NewVidSize=1;	/* Reduce to 512 mode */
		} else {
			if (MonitorHeight<480) {	/* Can I display 480 lines? */
				NewVidSize=2;	/* Show 400 instead */
			}
		}
	}
	if (NewVidSize==MacVidSize) {	/* Same size being displayed? */
		return MacVidSize;				/* Exit then... */
	}
	SetPort((WindowPtr)GameWindow);		/* Blank out the screen! */
	ForeColor(blackColor);		/* Make sure the colors are set */
	BackColor(whiteColor);
	FillRect(&BlackRect,&qd.black);	/* Blank it out! */

TryAgain:
	if (GameGWorld) {
		DisposeGWorld(GameGWorld);		/* Release the old GWorld */
		GameGWorld=0;
	}
	if (GameShapes) {
		FreeSomeMem(GameShapes);	/* All the permanent game shapes */
		GameShapes=0;
	}
	MacVidSize = NewVidSize;	/* Set the new data size */
	MacWidth = VidXs[NewVidSize];
	MacHeight = VidYs[NewVidSize];
	MacViewHeight = VidVs[NewVidSize];
	
	GameRect.bottom = MacHeight;	/* Set new video height */
	GameRect.right = MacWidth;		/* Set new video width */
	QDGameRect.top = RectY;
	QDGameRect.left = RectX;
	QDGameRect.bottom = RectY+MacHeight;
	QDGameRect.right = RectX+MacWidth;

	if (MacHeight==MonitorHeight && MacWidth==MonitorWidth) {
		ValidRects = FALSE;
	} else {
		ValidRects = TRUE;			/* The 4 bar rects are valid */
		RectRgn(BlackRgn,&BlackRect);
		TempRgn = NewRgn();
		RectRgn(TempRgn,&QDGameRect);
		DiffRgn(BlackRgn,TempRgn,BlackRgn);
		DisposeRgn(TempRgn);
	}
	
	GameVideoPointer = &TrueVideoPointer[(LongWord)QDGameRect.top*TrueVideoWidth+QDGameRect.left];
	if (NewGWorld(&GameGWorld,8,&GameRect,nil,nil,0)) {
		goto OhShit;
	}
	LockPixels(GameGWorld->portPixMap);	/* Lock down the memory FOREVER! */
	VideoPointer = (Byte *)GetPixBaseAddr(GameGWorld->portPixMap);	/* Get the video pointer */
	VideoWidth = (Word) (**(*GameGWorld).portPixMap).rowBytes & 0x3FFF;
	InitYTable();				/* Init the game's YTable */
	SetAPalette(rBlackPal);		/* Set the video palette */
	ClearTheScreen(BLACK);		/* Set the screen to black */
	BlastScreen();
	
	LongPtr = (LongWord *) LoadAResource(VidPics[MacVidSize]);
	if (!LongPtr) {
		goto OhShit;
	}
	GameShapes = (Byte **) AllocSomeMem(LongPtr[0]);	/* All the permanent game shapes */
	if (!GameShapes) {		/* Can't load in the shapes */
		ReleaseAResource(VidPics[MacVidSize]);	/* Release it NOW! */
		goto OhShit;
	}
	DLZSS((Byte *)GameShapes,(Byte *) &LongPtr[1],LongPtr[0]);
	ReleaseAResource(VidPics[MacVidSize]);
	i = 0;
	j = (MacVidSize==1) ? 47+10 : 47;		/* 512 mode has 10 shapes more */
	DestPtr = (Byte *) GameShapes;
	LongPtr = (LongWord *) GameShapes;
	do {
		GameShapes[i] = DestPtr+LongPtr[i]; 
	} while (++i<j);
	if (Pass2) {		/* Low memory? */
		if (!StartupRendering(NewVidSize)) {		/* Reset the scalers... */
			ReleaseScalers();
			goto OhShit;
		}
	}
	return MacVidSize;
	
OhShit:			/* Oh oh.... */
	if (Pass2) {
		if (!NewVidSize) {		/* At the smallest screen size? */
			BailOut();	
		}
		--NewVidSize;			/* Smaller size */
	} else {
		PlaySong(0);			/* Release song memory */
		ReleaseScalers();		/* Release the compiled scalers */
		PurgeAllSounds(1000000);	/* Force sounds to be purged */
		Pass2 = TRUE;
	}
	goto TryAgain;				/* Let's try again */
}

/**********************************

	Scale the system X coord
	
**********************************/

Word ScaleX(Word x)
{
	switch(MacVidSize) {
	case 1:
		return x*8/5;
	case 2:
	case 3:
		return x*2;
	}
	return x;
}

/**********************************

	Scale the system Y coord
	
**********************************/

Word ScaleY(Word y)
{
	switch(MacVidSize) {
	case 1:				/* 512 resolution */
		y = (y*8/5)+64;
		if (y==217) {	/* This hack will line up the gun on 512 mode */
			++y;
		}
		return y;
	case 2:				/* 640 x 400 */
		return y*2;	
	case 3:				/* 640 x 480 */
		return y*2+80;
	}
	return y;			/* 320 resolution */
}

/**********************************

	Shut down and exit
	
**********************************/

void GoodBye(void)
{
	ShowMenuBar();				/* Restore the menu bar */
	ReleaseScalers();			/* Release all my memory */
	FinisSoundMusicSystem();		/* Shut down the Halestorm driver */
	if (gMainGDH && OldPixDepth) {	/* Old pixel depth */
		SetDepth(gMainGDH,OldPixDepth, 1 << gdDevType,1);	/* Restore the depth */
	}
	InitCursor();
	ExitToShell();				/* Exit to System 7 */
}

/**********************************

	Read from the Mac's keyboard/mouse system
	
**********************************/

Word MyKey;

typedef struct MacKeys2Joy {
	Word Index;
	Word BitField;
	Word JoyValue;
} MacKeys2Joy;

MacKeys2Joy KeyMatrix[] = {
	{11,1<<1,JOYPAD_LFT|JOYPAD_UP},		/* Keypad 7 */
	{10,1<<6,JOYPAD_LFT},				/* Keypad 4 */
	{10,1<<3,JOYPAD_LFT|JOYPAD_DN},		/* Keypad 1 */
	{11,1<<3,JOYPAD_UP},				/* Keypad 8 */
	{10,1<<7,JOYPAD_DN},				/* Keypad 5 */
	{10,1<<4,JOYPAD_DN},				/* Keypad 2 */
	{11,1<<4,JOYPAD_RGT|JOYPAD_UP},		/* Keypad 9 */
	{11,1<<0,JOYPAD_RGT},				/* Keypad 6 */
	{10,1<<5,JOYPAD_RGT|JOYPAD_DN},		/* Keypad 3 */
	{15,1<<6,JOYPAD_UP},				/* Arrow up */
	{15,1<<5,JOYPAD_DN},				/* Arrow down */
	{15,1<<3,JOYPAD_LFT},				/* Arrow Left */
	{15,1<<4,JOYPAD_RGT},				/* Arrow Right */
	{ 1,1<<5,JOYPAD_UP},			/* W */
	{ 0,1<<0,JOYPAD_LFT},			/* A */
	{ 0,1<<1,JOYPAD_DN},			/* S */
	{ 0,1<<2,JOYPAD_RGT},			/* D */
	{ 4,1<<2,JOYPAD_UP},			/* I */
	{ 4,1<<6,JOYPAD_LFT},			/* J */
	{ 5,1<<0,JOYPAD_DN},			/* K */
	{ 4,1<<5,JOYPAD_RGT},			/* L */
	{ 6,1<<1,JOYPAD_A},				/* Space */
	{ 4,1<<4,JOYPAD_A},				/* Return */
	{10,1<<2,JOYPAD_A},				/* Keypad 0 */
	{ 9,1<<4,JOYPAD_A},				/* keypad enter */
	{ 7,1<<7,JOYPAD_TR},			/* Option */
	{ 7,1<<2,JOYPAD_TR},			/* Option */	
	{ 7,1<<0,JOYPAD_X},				/* Shift L */
	{ 7,1<<1,JOYPAD_X},				/* Caps Lock */
	{ 7,1<<4,JOYPAD_X},				/* Shift R */
	{ 6,1<<0,JOYPAD_SELECT},		/* Tab */
	{ 7,1<<3,JOYPAD_B},				/* Ctrl */
	{ 7,1<<6,JOYPAD_B}				/* Ctrl */
};

static char *CheatPtr[] = {		/* Cheat strings */
	"XUSCNIELPPA",	
	"IDDQD",
	"BURGER",
	"WOWZERS",
	"LEDOUX",
	"SEGER",
	"MCCALL",
	"APPLEIIGS"
};
static Word Cheat;			/* Which cheat is active */
static Word CheatIndex;	/* Index to the cheat string */

static void FixPauseMenu(void)
{
	if (PauseMenu) {	/* Is there a pause menu? */
		InPause = FALSE;
		DeleteMenu(55);	/* Remove it from the menu bar */
		DrawMenuBar();	/* Redraw the menu bar */
		HideMenuBar();	/* Hide the menu bar */
		DisposeMenu(PauseMenu);	/* Free the memory */
		PauseMenu = 0;	/* Ack the flag */
		ResumeSoundMusicSystem();	/* Restart the music */
	}
}

void ReadSystemJoystick(void)
{
	Word i;
	Word Index;
	
	MacKeys2Joy *MacPtr;
	Point MyPoint;
	union {
	unsigned char Keys[16];
	KeyMap Macy;
	} Keys;

	joystick1 = 0;			/* Assume that joystick not moved */

	i = GetAKey();				/* Allow menu events */
	if (i==0x1b) {			/* Pause key? */
		ShowMenuBar();
		InitCursor();
		PauseMenu = NewMenu(55,"\p<< Game is Paused! >>");
		InsertMenu(PauseMenu,0);
		DrawMenuBar();
		PauseSoundMusicSystem();	/* Pause the music */
		InPause = TRUE;				/* Hack to prevent the menu bar from disappearing */
		WaitTicksEvent(0);
		LastTicCount = ReadTick();	/* Reset the timer for the pause key */
		FixPauseMenu();				/* Exit pause */
		FixTheCursor();
	}
	
	/* Switch weapons like in DOOM! */
	
	if (i) {			/* Was a key hit? */
		i = toupper(i);	/* Force UPPER case */
		if (CheatIndex) {		/* Cheat in progress */
			if (CheatPtr[Cheat][CheatIndex]==i) {		/* Match the current string? */
				++CheatIndex;				/* Next char */
				if (!CheatPtr[Cheat][CheatIndex]) {	/* End of the string? */
					PlaySound(SND_BONUS);		/* I got a bonus! */
					switch (Cheat) {		/* Execute the cheat */
					case 1:
						gamestate.godmode^=TRUE;			/* I am invincible! */
						break;
					case 5:
						GiveKey(0);
						GiveKey(1);		/* Award the keys */
						break;
					case 6:
						playstate=EX_WARPED;		/* Force a jump to the next level */
						nextmap = gamestate.mapon+1;	/* Next level */
						if (MapListPtr->MaxMap<=nextmap) {	/* Too high? */
							nextmap = 0;			/* Reset to zero */
						}
						break;
					case 7:
						ShowPush ^= TRUE;
						break;
					case 0:
					case 4:
						GiveKey(0);		/* Award the keys */
						GiveKey(1);
						gamestate.godmode = TRUE;			/* I am a god */
					case 2:
						gamestate.machinegun = TRUE;
						gamestate.chaingun = TRUE;
						gamestate.flamethrower = TRUE;
						gamestate.missile = TRUE;
						GiveAmmo(gamestate.maxammo);
						GiveGas(99);
						GiveMissile(99);
						break;
					case 3:
						gamestate.maxammo = 999;
						GiveAmmo(999);
					}
				}
			} else {
				CheatIndex = 0;
				goto TryFirst;
			}
		} else {
TryFirst:
			Index = 0;				/* Init the scan routine */
			do {
				if (CheatPtr[Index][0] == i) {
					Cheat = Index;		/* This is my current cheat I am scanning */
					CheatIndex = 1;		/* Index to the second char */
					break;				/* Exit */
				}
			} while (++Index<8);		/* All words scanned? */
		}
		switch (ScanCode) {		/* Use the SCAN code to make sure I hit the right key! */
		case 0x12 :		/* 1 */
			gamestate.pendingweapon = WP_KNIFE;
			break;
		case 0x13 : 	/* 2 */
			if (gamestate.ammo) {
				gamestate.pendingweapon = WP_PISTOL;
			}
			break;
		case 0x14 :		/* 3 */
			if (gamestate.ammo && gamestate.machinegun) {
				gamestate.pendingweapon = WP_MACHINEGUN;
			}
			break;
		case 0x15 :		/* 4 */
			if (gamestate.ammo && gamestate.chaingun) {
				gamestate.pendingweapon = WP_CHAINGUN;
			}
			break;
		case 0x17 :		/* 5 */
			if (gamestate.gas && gamestate.flamethrower) {
				gamestate.pendingweapon = WP_FLAMETHROWER;
			}
			break;	
		case 0x16 :		/* 6 */
			if (gamestate.missiles && gamestate.missile) {
				gamestate.pendingweapon = WP_MISSILE;
			}
			break;
		case 0x41:		/* Keypad Period */
		case 0x2C:		/* Slash */
			joystick1 = JOYPAD_START; 
		}
	}
	
	GetKeys(Keys.Macy);		/* Get the keyboard from the mac */
	
	i = 0;					/* Init the count */
	MacPtr = KeyMatrix;
	do {
		Index = MacPtr->Index;		/* Get the byte index */
		if (Keys.Keys[Index] & MacPtr->BitField) {
			joystick1 |= MacPtr->JoyValue;	/* Set the joystick value */
		}
		MacPtr++;					/* Next index */
	} while (++i<33);				/* All done? */
	
	if (MouseEnabled) {				/* Mouse control turned on? */
		if (Button()) {				/* Get the mouse button */
			joystick1 |= JOYPAD_B;		/* Mouse button */
		}
		GetMouse(&MyPoint);			/* Get the mouse location */
		LocalToGlobal(&MyPoint);	/* Convert mouse to global coordinate system */
		if (joystick1&JOYPAD_TR) {	/* Strafing? */
			mousex += (MyPoint.h-MouseBaseX);	/* Move horizontally for strafe */
		} else {
			mouseturn += (MouseBaseX-MyPoint.h);	/* Turn left or right */
		}
		mousey += (MyPoint.v-MouseBaseY);		/* Forward motion */
		(*(unsigned short *) 0x082C) = MouseBaseY;	/* Set RawMouse */
		(*(unsigned short *) 0x082E) = MouseBaseX;
		(*(unsigned short *) 0x0828) = MouseBaseY;	/* MTemp */
		(*(unsigned short *) 0x082A) = MouseBaseX;	
		(*(Byte *) 0x08CE) = 255;				/* CrsrNew */
		(*(Byte *) 0x08CF) = 255;				/* CrsrCouple */
	}
	
	if (joystick1 & JOYPAD_TR) {		/* Handle the side scroll (Special case) */
		if (joystick1&JOYPAD_LFT) {
			joystick1 = (joystick1 & ~(JOYPAD_TR|JOYPAD_LFT)) | JOYPAD_TL;
		} else if (joystick1&JOYPAD_RGT) {
			joystick1 = joystick1 & ~JOYPAD_RGT;
		} else {
			joystick1 &= ~JOYPAD_TR;
		}
	}
}

/**********************************

	Handle GET PSYCHED!
	
**********************************/

static Rect PsychedRect;
static Word LeftMargin;
#define PSYCHEDWIDE 184
#define PSYCHEDHIGH 5
#define PSYCHEDX 20
#define PSYCHEDY 46
#define MAXINDEX (66+S_LASTONE)
		
/**********************************

	Draw the initial shape
		
**********************************/

void ShowGetPsyched(void)
{
	LongWord *PackPtr;
	Byte *ShapePtr;
	LongWord PackLength;
	Word X,Y;
	
	SetPort((WindowPtr)GameWindow);
	ClearTheScreen(BLACK);
	BlastScreen();
	PackPtr = LoadAResource(rGetPsychPic);
	PackLength = PackPtr[0];
	ShapePtr = AllocSomeMem(PackLength);
	DLZSS(ShapePtr,(Byte *) &PackPtr[1],PackLength);
	X = (MacWidth-224)/2;	
	Y = (MacViewHeight-56)/2;
	DrawShape(X,Y,ShapePtr);
	FreeSomeMem(ShapePtr);
	ReleaseAResource(rGetPsychPic);
	BlastScreen();
	SetAPalette(rGamePal);
	SetPort((WindowPtr)GameWindow);
	ForeColor(redColor);
	PsychedRect.top = Y + PSYCHEDY + QDGameRect.top;
	PsychedRect.bottom = PsychedRect.top + PSYCHEDHIGH;
	PsychedRect.left = X + PSYCHEDX + QDGameRect.left;
	PsychedRect.right = PsychedRect.left;
	LeftMargin = PsychedRect.left;
}

/**********************************

	Update the thermomitor
	
**********************************/

void DrawPsyched(Word Index)
{
	Word Factor;
	
	Factor = Index * PSYCHEDWIDE;		/* Calc the relative X pixel factor */
	Factor = Factor / MAXINDEX;
	PsychedRect.left = PsychedRect.right;	/* Adjust the left pixel */
	PsychedRect.right = Factor+LeftMargin;
	PaintRect(&PsychedRect);		/* Draw the pixel in the bar */
}

/**********************************

	Erase the Get Psyched screen
	
**********************************/

void EndGetPsyched(void)
{
	ForeColor(blackColor);		/* Reset the color to black for high speed */
	SetAPalette(rBlackPal);		/* Zap the palette */
}

/**********************************

	Choose the game difficulty
	
**********************************/

static void DrawNewRect(DialogPtr theDialog,Word Item,Word Color)
{
	short iType;
	Handle iHndl;
	Rect iRect;
	
	ForeColor(Color);
	GetDialogItem(theDialog,Item, &iType, &iHndl, &iRect);
	InsetRect(&iRect,-4,-4);
	PenSize(3,3);		/* Nice thick line */
	FrameRect(&iRect);	
	PenNormal();
}

/**********************************

	Choose the game difficulty
	
**********************************/

static Word OldVal;			/* Make semi-global so the filter can use it as input */

Word ChooseGameDiff(void)
{
	ModalFilterUPP	theDialogFilter;
	DialogPtr MyDialog;
	short itemHit;
	Word donewithdialog;
	Word RetVal;
	Word OldTick;
	unsigned short DblClick;
	Byte OldPal[768];
		
	memcpy(OldPal,CurrentPal,768);
	SetPort((WindowPtr)GameWindow);
	FillRect(&BlackRect,&qd.black);
	SetAPalette(rGamePal);
	MyDialog = GetNewDialog(NewGameWin,0L,(WindowPtr)-1);		/* Open the dialog window */
	OldVal = difficulty+1;	/* Save the current difficulty */
	PlaySound(SND_MENU);
	CenterAWindow(MyDialog);
	ShowWindow(MyDialog);	/* Display with OK button framed */
	InitCursor();
	SetPort((WindowPtr) MyDialog);
	DrawNewRect(MyDialog,OldVal,blackColor);	
	DblClick = 0;
	donewithdialog = FALSE;
	RetVal = TRUE;			/* Assume ok */

	theDialogFilter = NewModalFilterProc(StandardModalFilterProc);

	do {
		ModalDialog(theDialogFilter, &itemHit);
		switch (itemHit) {
		case 1:
		case 2:
		case 3:
		case 4:						/* The checkbox */
			if (OldVal!=itemHit) {
				DrawNewRect(MyDialog,OldVal,whiteColor);		/* Erase the old rect */
				DrawNewRect(MyDialog,itemHit,blackColor);
				OldVal = itemHit;
			}
			PlaySound(SND_GUNSHT);			/* Gun shot */
			if (DblClick==itemHit) {		/* Hit the same item? */
				if (((Word)ReadTick()-OldTick)<(Word)15) {	/* Double click time? */
					donewithdialog = TRUE;
					difficulty = OldVal-1;
					break;		/* Ok! */
				}
			}
			OldTick=ReadTick();		/* Save the tick mark */
			DblClick=itemHit;		/* Save the last item hit */
			break;
		case 5:
			RetVal = FALSE;
			donewithdialog = TRUE;
			PlaySound(SND_OK);
			break;
		case 6:				/* OK Button */
			donewithdialog = TRUE;
			difficulty = OldVal-1;
			PlaySound(SND_OK);
			break;
		}
	} while (!donewithdialog);
	DisposeRoutineDescriptor(theDialogFilter);
	DisposeDialog(MyDialog);
	SetAPalettePtr(OldPal);		/* Restore the palette */
	SetPort((WindowPtr) GameWindow);
	DrawWindow((WindowPtr)GameWindow);	/* Draw it NOW! */
	FixTheCursor();
	return RetVal;
}

static Byte BoxUp[] = {3,4,1,2};		/* Up/Down */
static Byte BoxLeft[] = {2,1,4,3};		/* Left/Right */
static Byte TabLeft[] = {2,3,4,1};		/* Tab */

pascal Boolean StandardModalFilterProc(DialogPtr theDialog, EventRecord *theEvent, short *itemHit)
{
	char	theChar;
	Handle	theHandle;
	short	theType;
	Rect	theRect;

	switch (theEvent->what) {
	case keyDown:
		theChar = toupper(theEvent->message & charCodeMask);
		switch (theChar) {
		
		case '8':		/* Up */
		case 0x1E:		/* Up */
		case '5':		/* Down */
		case '2':		/* Down */
		case 0x1F:		/* Down */
		case 'W':
		case 'S':
		case 'I':
		case 'K':
			*itemHit = BoxUp[OldVal-1];
			return TRUE;
			
		case '4':		/* Left */
		case '6':		/* Right */
		case 0x1C:		/* Left */
		case 0x1D:		/* Right */
		case 'A':
		case 'D':
		case 'J':
		case 'L':
			*itemHit = BoxLeft[OldVal-1];
			return TRUE;
		
		case 9:
			*itemHit = TabLeft[OldVal-1];
			return TRUE;
			
		case 0x0D:		/*	Return */
		case 0x03:		/*	Enter */
			*itemHit = 6;
			return (TRUE);

		case 'Q':		/* Cmd-Q */
		case '.':		/* Cmd-Period */
			if (theEvent->modifiers & cmdKey) {
		case 0x1b:		/* Esc */
				*itemHit = 5;
				return (TRUE);
			}
			break;
		}
		break;
	case updateEvt:
		GetDialogItem(theDialog, 6, &theType, &theHandle, &theRect);
		InsetRect(&theRect, -4, -4);
		PenSize(3, 3);
		FrameRoundRect(&theRect, 16, 16);
	}
	return (FALSE);		//•	Tells dialog manager to handle the event
}


/**********************************

	Load the prefs file into memory and use defaults 
	for ANY errors that could occur
	
**********************************/

typedef struct {		/* Save game data field */
	unsigned short State;		/* Game state flags */
	unsigned short Mouse;		/* Mouse control */
	unsigned short QuickDraw;	/* Quickdraw for updates */
	unsigned short Goveror;		/* Speed governor */
	unsigned short ViewSize;	/* Game screen size */
	unsigned short Difficulty;	/* Game difficulty */
	unsigned short Ignore;		/* Ignore message flag */
} Prefs_t;

void LoadPrefs(void)
{
	Prefs_t Prefs;
	
	Prefs.State = 3;			/* Assume sound & music enabled */
	Prefs.Mouse = FALSE;		/* Mouse control shut off */
	Prefs.QuickDraw = TRUE;		/* Use quickdraw for screen updates */
	Prefs.Goveror = TRUE;		/* Enable speed governor */
	Prefs.ViewSize = 0;			/* 320 mode screen */
	Prefs.Difficulty = 2;		/* Medium difficulty */
	Prefs.Ignore = FALSE;		/* Enable message */
	
	InitPrefsFile('WOLF',(Byte *)"Wolfenstein 3D Prefs");		/* Create the prefs file (If it doesn't exist) */
	LoadPrefsFile((Byte *)&Prefs,sizeof(Prefs));		/* Load in the prefs (Assume it may NOT!) */
	SystemState = Prefs.State;	/* Store the defaults from either the presets or the file */
	MouseEnabled = Prefs.Mouse;
	DoQuickDraw = Prefs.QuickDraw;
	SlowDown = Prefs.Goveror;
	GameViewSize = Prefs.ViewSize;
	difficulty = Prefs.Difficulty;
	IgnoreMessage = Prefs.Ignore;
}

/**********************************

	Save the prefs file to disk but ignore any errors
	
**********************************/

void SavePrefs(void)
{
	Prefs_t Prefs;			/* Structure to save to disk */
	Prefs.State = SystemState;	/* Init the prefs structure */
	Prefs.Mouse = MouseEnabled;
	Prefs.QuickDraw = DoQuickDraw;
	Prefs.Goveror = SlowDown;
	Prefs.ViewSize = GameViewSize;
	Prefs.Difficulty =	difficulty;
	Prefs.Ignore = IgnoreMessage;
	SavePrefsFile((Byte *)&Prefs,sizeof(Prefs));	/* Save the prefs file */
}

/**********************************

	Save the game
	
**********************************/

#ifdef __powerc
Byte MachType[4] = "PPC3";
#else
Byte MachType[4] = "68K3";
#endif

void SaveGame(void)
{
	short FileRef;
	long Count;
	Word PWallWord;
	
	HCreate(SaveFileVol,SaveFileParID,SaveFileName,'WOLF','SAVE');		/* Create the save game file */
	if (HOpen(SaveFileVol,SaveFileParID,SaveFileName,fsWrPerm,&FileRef)) {	/* Can I open it? */
		return;				/* Abort! */
	}

	Count = 4;							/* Default length */
	FSWrite(FileRef,&Count,&MachType);	/* Save a machine type ID */
	Count = sizeof(unsigned short);
	FSWrite(FileRef,&Count,&MapListPtr->MaxMap); 	/* Number of maps (ID) */			
	Count = sizeof(gamestate);			
	FSWrite(FileRef,&Count,&gamestate);		/* Save the game stats */
	Count = sizeof(PushWallRec);
	FSWrite(FileRef,&Count,&PushWallRec);	/* Save the pushwall stats */
	
	Count = sizeof(nummissiles);
	FSWrite(FileRef,&Count,&nummissiles);	/* Save missiles in motion */
	if (nummissiles) {
		Count = nummissiles*sizeof(missile_t);
		FSWrite(FileRef,&Count,&missiles[0]);
	}
	Count = sizeof(numactors);
	FSWrite(FileRef,&Count,&numactors);		/* Save actors */
	if (numactors) {
		Count = numactors*sizeof(actor_t);
		FSWrite(FileRef,&Count,&actors[0]);
	}
	Count = sizeof(numdoors);
	FSWrite(FileRef,&Count,&numdoors);		/* Save doors */
	if (numdoors) {
		Count = numdoors*sizeof(door_t);
		FSWrite(FileRef,&Count,&doors[0]);
	}
	Count = sizeof(numstatics);
	FSWrite(FileRef,&Count,&numstatics);
	if (numstatics) {
		Count = numstatics*sizeof(static_t);
		FSWrite(FileRef,&Count,&statics[0]);
	}
	Count = 64*64;
	FSWrite(FileRef,&Count,MapPtr);
	Count = sizeof(tilemap);				/* Tile map */
	FSWrite(FileRef,&Count,&tilemap);

	Count = sizeof(ConnectCount);
	FSWrite(FileRef,&Count,&ConnectCount);
	if (ConnectCount) {
		Count = ConnectCount * sizeof(connect_t);
		FSWrite(FileRef,&Count,&areaconnect[0]);
	}
	Count = sizeof(areabyplayer);
	FSWrite(FileRef,&Count,&areabyplayer[0]);
	Count = (128+5)*64;
	FSWrite(FileRef,&Count,&textures[0]);
	Count = sizeof(Word);
	PWallWord = 0;		/* Assume no pushwall pointer in progress */
	if (pwallseg) {
		PWallWord = (pwallseg-(saveseg_t*)nodes)+1;		/* Convert to number offset */
	} 
	FSWrite(FileRef,&Count,&PWallWord);
	Count = MapPtr->numnodes*sizeof(savenode_t);	/* How large is the BSP tree? */
	FSWrite(FileRef,&Count,nodes);			/* Save it to disk */
	FSClose(FileRef);						/* Close the file */
	PlaySound(SND_BONUS);
}

/**********************************

	Load the game	
	
**********************************/

void *SaveRecord;
void *SaveRecordMem;

Boolean LoadGame(void)
{
	LongWord FileSize;
	union {
		Byte *B;
		unsigned short *S;
		Word *W;
		LongWord *L;
	} FilePtr;
	void *TheMem;
	short FileRef;

	if (HOpen(SaveFileVol, SaveFileParID, SaveFileName, fsRdPerm, &FileRef)) {
		return FALSE;
	}
	GetEOF(FileRef,(long*)&FileSize);		/* Get the size of the file */
	FilePtr.B = TheMem = AllocSomeMem(FileSize);	/* Get memory for the file */
	if (!FilePtr.B) {						/* No memory! */
		return FALSE;
	}
	FSRead(FileRef,(long*)&FileSize,(Ptr)FilePtr.B);	/* Open the file */
	FSClose(FileRef);						/* Close the file */
	if (memcmp(MachType,FilePtr.B,4)) {		/* Is this the proper machine type? */
		goto Bogus;							
	}
	FilePtr.B+=4;							/* Index past signature */	
	if (MapListPtr->MaxMap!=*FilePtr.S) {	/* Map count the same? */
		goto Bogus;							/* Must be differant game */
	}
	++FilePtr.S;							/* Index past count */
	memcpy(&gamestate,FilePtr.B,sizeof(gamestate));	/* Reset the game state */
	SaveRecord = FilePtr.B;
	SaveRecordMem = TheMem;
	return TRUE;
	
Bogus:
	FreeSomeMem(TheMem);
	return FALSE;
}

void FinishLoadGame(void) 
{	
	union {
		Byte *B;
		unsigned short *S;
		Word *W;
		LongWord *L;
	} FilePtr;
	
	FilePtr.B = SaveRecord;
	
	memcpy(&gamestate,FilePtr.B,sizeof(gamestate));	/* Reset the game state */
	FilePtr.B+=sizeof(gamestate);
	
	memcpy(&PushWallRec,FilePtr.B,sizeof(PushWallRec));				/* Record for the single pushwall in progress */
	FilePtr.B+=sizeof(PushWallRec);
	
	nummissiles = *FilePtr.W;
	++FilePtr.W;
	if (nummissiles) {
		memcpy(&missiles[0],FilePtr.B,sizeof(missile_t)*nummissiles);
		FilePtr.B += sizeof(missile_t)*nummissiles;
	}

	numactors = *FilePtr.W;
	++FilePtr.W;
	if (numactors) {
		memcpy(&actors[0],FilePtr.B,sizeof(actor_t)*numactors);
		FilePtr.B += sizeof(actor_t)*numactors;
	}
	
	numdoors = *FilePtr.W;
	++FilePtr.W;
	if (numdoors) {
		memcpy(&doors[0],FilePtr.B,sizeof(door_t)*numdoors);
		FilePtr.B += sizeof(door_t)*numdoors;
	}

	numstatics = *FilePtr.W;
	++FilePtr.W;
	if (numstatics) {
		memcpy(&statics[0],FilePtr.B,sizeof(static_t)*numstatics);
		FilePtr.B += sizeof(static_t)*numstatics;
	}
	memcpy(MapPtr,FilePtr.B,64*64);
	FilePtr.B += 64*64;
	memcpy(&tilemap,FilePtr.B,sizeof(tilemap));						/* Tile map */
	FilePtr.B += sizeof(tilemap);		/* Index past data */
	
	ConnectCount = *FilePtr.W;		/* Number of valid interconnects */
	FilePtr.W++;				
	if (ConnectCount) {
		memcpy(areaconnect,FilePtr.B,sizeof(connect_t)*ConnectCount);	/* Is this area mated with another? */
		FilePtr.B+= sizeof(connect_t)*ConnectCount;
	}	
	memcpy(areabyplayer,FilePtr.B,sizeof(areabyplayer));
	FilePtr.B+=sizeof(areabyplayer);	/* Which areas can I see into? */
	
	memcpy(&textures[0],FilePtr.B,(128+5)*64);
	FilePtr.B+=((128+5)*64);				/* Texture array for pushwalls */
	
	pwallseg = 0;			/* Assume bogus */
	if (*FilePtr.W) {
		pwallseg = (saveseg_t *)nodes;
		pwallseg = &pwallseg[*FilePtr.W-1];
	}
	++FilePtr.W;
	memcpy(nodes,FilePtr.B,MapPtr->numnodes*sizeof(savenode_t)); 
/*	FilePtr.B+=(MapPtr->numnodes*sizeof(savenode_t));	/* Next entry */
	
	FreeSomeMem(SaveRecordMem);
}

/**********************************

	Process an alert and center it on my game monitor
	Note : I am cheating like a son of a bitch by modifing the
	resources directly!
	
**********************************/

static Word DoMyAlert(Word AlertNum)
{
	Word Val;
	Rect **AlertRect;		/* Handle to the alert rect */
	Rect MyRect;
	
	InitCursor();			/* Fix the cursor */
	AlertRect = (Rect **)GetResource('ALRT',AlertNum);		/* Load in the template */
	MyRect = **AlertRect;		/* Copy the rect */
	MyRect.right -= MyRect.left;	/* Get the width */
	MyRect.bottom -= MyRect.top;	/* Get the height */
	CenterSFWindow((Point *)&MyRect,MyRect.right,MyRect.bottom);	/* Center the window */
	MyRect.right += MyRect.left;	/* Fix the rect */
	MyRect.bottom += MyRect.top;
	**AlertRect = MyRect;
	ChangedResource((Handle)AlertRect);
	Val = Alert(AlertNum,nil);
	if (!InPause) {		/* If paused then don't fix the cursor */
		FixTheCursor();
	}
	return Val;
}

/**********************************

	Beg for $$$ at the end of the shareware version
	
**********************************/

void ShareWareEnd(void)
{
	SetAPalette(rGamePal);
	DoMyAlert(EndGameWin);
	SetAPalette(rBlackPal);
}

/**********************************

	Show the standard file dialog 
	for opening a new game
		
**********************************/

SFTypeList MyList = {'SAVE'};
SFReply Reply;

static void CenterSFWindow(Point *MyPoint,Word Width,Word Height)
{
	GDHandle MainDevice;
	Rect TheRect;
	Word H,W;			/* Device width of main device */
	
	if (gMainGDH) {		/* Has the video device been selected? */
		MyPoint->v = ((MonitorHeight-Height)/3)+BlackRect2.top;	/* Center the window then */
		MyPoint->h = ((MonitorWidth-Width)/2)+BlackRect2.left;
		return;
	}
	MainDevice = GetMainDevice();		/* Get the default device */
	TheRect = (**MainDevice).gdRect;	/* Get the rect of the device */
	H = TheRect.bottom - TheRect.top;	/* Get the size of the video screen */
	W = TheRect.right - TheRect.left;
	MyPoint->v = ((H-Height)/3)+TheRect.top;	/* Center the window */
	MyPoint->h = ((W-Width)/2)+TheRect.left;	
}

static void CenterAWindow(WindowPtr MyWindow)
{
	Rect MyRect;
	Point MyPoint;
	
	MyRect = ((WindowPeek)MyWindow)->port.portRect;
	CenterSFWindow(&MyPoint,MyRect.right-MyRect.left,MyRect.bottom-MyRect.top);
	MoveWindow(MyWindow,MyPoint.h,MyPoint.v,FALSE);
}


Boolean ChooseLoadGame(void)
{
	long ProcID;
	Point MyPoint;
	
	InitCursor();
	CenterSFWindow(&MyPoint,348,136);
	SFGetFile(MyPoint,0,0,1,MyList,0,&Reply);
	FixTheCursor();
	if (!Reply.good) {		/* Pressed cancel? */
		return FALSE;
	}
	SaveFileName = (Byte *)&Reply.fName;
	SaveFileVol = Reply.vRefNum;
	GetWDInfo(SaveFileVol,&SaveFileVol,&SaveFileParID,&ProcID);
	return TRUE;
}

/**********************************

	Show the standard file dialog 
	for saving a new game
		
**********************************/

Boolean ChooseSaveGame(void)
{
	Byte *SaveTemp;
	Point MyPoint;
	long ProcID;
	
	InitCursor();
	SaveTemp = SaveFileName;
	if (!SaveTemp) {
		SaveTemp = "\pUntitled";
	}
	CenterSFWindow(&MyPoint,304,104);
	SFPutFile(MyPoint,"\pSave your game as:",SaveTemp,0,&Reply);
	FixTheCursor();
	if (!Reply.good) {
		return FALSE;
	}
	SaveFileName = (Byte *)&Reply.fName;
	SaveFileVol = Reply.vRefNum;
	GetWDInfo(SaveFileVol,&SaveFileVol,&SaveFileParID,&ProcID);
	return TRUE;
}

/**********************************

	Process Apple Events
	
**********************************/
#if 1
struct AEinstalls {
    AEEventClass theClass;
    AEEventID theEvent;
    void *Code;
    AEEventHandlerUPP theProc;
};
typedef struct AEinstalls AEinstalls;


static OSErr GotRequiredParams(AppleEvent *theAppleEvent)
{
	DescType returnedType;
	Size actualSize;
	OSErr theErr;
	
	theErr = AEGetAttributePtr(theAppleEvent, keyMissedKeywordAttr,
							typeWildCard, &returnedType, nil, 0, &actualSize);
	if (theErr == errAEDescNotFound)
		return noErr;
	if (!theErr)
		return errAEParamMissed;
	return theErr;
}

/* This is the standard Open Application event.  */
static pascal OSErr AEOpenHandler(AppleEvent *theAppleEvent, AppleEvent *reply, long refIn)
{
	OSErr theErr;
	
	// This event occurs when the application is opened.  In most cases
	// nothing needs to be done, but we must make sure that we received
	// no parameters, since none are supposed to be received.
	
	theErr = GotRequiredParams(theAppleEvent);
	if (theErr)
		return theErr;
		
	// do whatever is needed (e.g., open new untitled document)
	
	return noErr;
}

/* Open Doc, opens our documents.  Remember, this can happen at application start AND */
/* anytime else.  If your app is up and running and the user goes to the desktop, hilites one */
/* of your files, and double-clicks or selects Open from the finder File menu this event */
/* handler will get called. Which means you don't do any initialization of globals here, or */
/* anything else except open then doc.  */
/* SO-- Do NOT assume that you are at app start time in this */
/* routine, or bad things will surely happen to you. */

static pascal OSErr AEOpenDocHandler(AppleEvent *theAppleEvent, AppleEvent reply, long refCon)
{
	AEDescList docList;
	OSErr theErr;
	long index, itemsInList;
	Size actualSize;
	AEKeyword keywd;
	DescType returnedType;
	FSSpec theFSS;
	
	// In response to this event, we must open all documents specified in
	// the Apple Event.

	AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList, &docList);
	
	// check for missing required parameters
	theErr = GotRequiredParams(theAppleEvent);
	if (theErr != noErr)
		return AEDisposeDesc(&docList);
	
	// count the number of descriptor records in the list
	theErr = AECountItems(&docList, &itemsInList);
	
	// get each descriptor record from the list, coerce the returned
	// data to an FSSpec record, and open the associated file
	index = 1;
	if (index <= itemsInList) {
		AEGetNthPtr(&docList, index, typeFSS, &keywd, &returnedType,
						(Ptr)&theFSS, sizeof(theFSS), &actualSize);
		// open FSSpec
		OpenPending = TRUE;
		SaveFileVol = theFSS.vRefNum;
		SaveFileParID = theFSS.parID;
		SaveFileName = (Byte *)&Reply.fName;
		memcpy(&Reply.fName,&theFSS.name,*theFSS.name+1);
	}
	
	AEDisposeDesc(&docList);
	
	return noErr;
}


static pascal OSErr AEPrintHandler(AppleEvent *theAppleEvent, AppleEvent reply, long refCon)
{
	AEDescList docList;
	OSErr theErr;
	long index, itemsInList;
	Size actualSize;
	AEKeyword keywd;
	DescType returnedType;
	FSSpec theFSS;
	
	// In response to this event, we must print all documents specified in
	// the Apple Event.

	theErr = AEGetParamDesc(theAppleEvent, keyDirectObject, typeAEList, &docList);
	
	// check for missing required parameters
	theErr = GotRequiredParams(theAppleEvent);
	if (theErr != noErr)
		return AEDisposeDesc(&docList);
	
	// count the number of descriptor records in the list
	theErr = AECountItems(&docList, &itemsInList);
	
	// get each descriptor record from the list, coerce the returned
	// data to an FSSpec record, and open the associated file
	for (index = 1; index <= itemsInList; index++)
	{
		theErr = AEGetNthPtr(&docList, index, typeFSS, &keywd, &returnedType,
						(Ptr)&theFSS, sizeof(theFSS), &actualSize);
		// print FSSpec <no printing in this application>
	}
	
	AEDisposeDesc(&docList);
	
	return noErr;
}

/* Standard Quit event handler, to handle a Quit event from the Finder, for example.
	Note that we may not exit from the AppleEvent handler!  We have to set a flag,
	and exit from within the normal application code.  */

static pascal OSErr AEQuitHandler(AppleEvent *messagein, AppleEvent *reply, long refIn)
{
	QuitFlag = 1;		/* I quit! */
	return noErr;
}

/* InitAEStuff installs my appleevent handlers */
void InitAEStuff(void);

static AEinstalls HandlersToInstall[] =  {
	{kCoreEventClass, kAEOpenApplication,AEOpenHandler,0},  
	{kCoreEventClass, kAEOpenDocuments, AEOpenDocHandler,0},  
	{kCoreEventClass, kAEQuitApplication, AEQuitHandler,0},  
	{kCoreEventClass, kAEPrintDocuments, AEPrintHandler,0}
};

void InitAEStuff(void)
{

    Word i;
    
    /* Check this machine for AppleEvents.  If they are not here (ie not 7.0)
    *   then we return. */
    
    #ifndef __powerc
   	long aLong;
	short Code,Count;
	short Index;
	AppFile Stuff;
	
	/* This is for the WONDERFUL system 6.0.7 filenames */
	/* Only execute on 680x0 macs */
	
	CountAppFiles(&Code,&Count);
	for (Index=1;Index<=Count;++Count) {
		GetAppFiles(Index,&Stuff);
		if (Index==1) {
			OpenPending = TRUE;
			SaveFileVol = Stuff.vRefNum;
			SaveFileName = (Byte *)&Reply.fName;
			memcpy(&Reply.fName,&Stuff.fName,*Stuff.fName+1);
			GetWDInfo(SaveFileVol,&SaveFileVol,&SaveFileParID,&aLong);
    	}
    	ClrAppFiles(Index);
    }
    if (Gestalt(gestaltAppleEventsAttr, &aLong)) {	/* Are Apple events present? */
    	return;					
    }
    if (!(aLong & (1<<gestaltAppleEventsPresent))) {
    	return;
    }
    #endif
    
    /* The following series of calls installs all our AppleEvent Handlers.
    *   These handlers are added to the application event handler list that 
    *   the AppleEvent manager maintains.  So, whenever an AppleEvent happens
    *   and we call AEProcessEvent, the AppleEvent manager will check our
    *   list of handlers and dispatch to it if there is one.
    */
	i = 0;
	do {
		HandlersToInstall[i].theProc = NewAEEventHandlerProc(HandlersToInstall[i].Code);
		AEInstallEventHandler(HandlersToInstall[i].theClass, HandlersToInstall[i].theEvent,
                                            HandlersToInstall[i].theProc,0,FALSE);
	} while (++i<4); 
}
#endif

/**********************************

	Make the cursor disappear if needed
	
**********************************/

static void FixTheCursor(void)
{
	InitCursor();
	if (MouseEnabled && !InPause) {
		HideCursor();
	}
}

/**********************************

	Show a watch cursor
	
**********************************/

static void WaitCursor(void)
{
	SetCursor(*WatchHandle);
}
