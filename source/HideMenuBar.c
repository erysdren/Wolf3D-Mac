#include "hidemenubar.h"
											
static Boolean MenuBarHidden = FALSE;		/* Current state of the menu bar. */
static Rect OldeMBarRect;		/* Saves rectangle enclosing real menu bar. */
static RgnHandle OldeGrayRgn;	/* Saves the region defining the desktop */
static short OldMBarHeight;		/* Previous menu bar height */
extern GDHandle gMainGDH;

/********************************

	Hide the menu bar (If visible)

********************************/

void HideMenuBar(void)
{
	RgnHandle menuRgn;
	WindowPeek theWindow;
	GDHandle TempHand;
	
	TempHand = GetMainDevice();
	if (TempHand!=gMainGDH) {	/* Main graphics handle */
		return;
	}				
	if (!MenuBarHidden) {		/* Already hidden? */
		OldMBarHeight = LMGetMBarHeight();	/* Get the height in pixels */
		OldeGrayRgn = NewRgn();		/* Make a new region */
		CopyRgn(GetGrayRgn(), OldeGrayRgn);	/* Copy the background region */
		OldeMBarRect = qd.screenBits.bounds;	/* Make a region from the rect or the monitor width */
		OldeMBarRect.bottom = OldeMBarRect.top + OldMBarHeight;	/* Top to bottom of menu */
		menuRgn = NewRgn();			/* Convert to region */
		RectRgn(menuRgn, &OldeMBarRect);
		
		UnionRgn(GetGrayRgn(), menuRgn, GetGrayRgn());	/* Add the menu area to background */
		theWindow = (WindowPeek)FrontWindow();
		PaintOne((WindowPtr)theWindow, menuRgn);			/* Redraw the front window */
		PaintBehind((WindowPtr)theWindow, menuRgn);		/* Redraw all other windows */
		CalcVis((WindowPtr)theWindow);						/* Resize the visible region */
		CalcVisBehind((WindowPtr)theWindow, menuRgn);		/* Resize the visible regions for all others */
		DisposeRgn(menuRgn);				/* Release the menu region */
		MenuBarHidden = TRUE;				/* It is now hidden */
		ZapMHeight();						/* Zap the height */
	}
}

/********************************

	Show the menu bar (If hidden)

********************************/

void ShowMenuBar(void)
{
	WindowPeek theWindow;
	
	if (MenuBarHidden) {		/* Hidden? */
		FixMHeight();
		MenuBarHidden = FALSE;		/* The bar is here */
		CopyRgn(OldeGrayRgn, GetGrayRgn());	/* Get the region */
		RectRgn(OldeGrayRgn, &OldeMBarRect);	/* Convert menu rect to region */
		theWindow = (WindowPeek)FrontWindow();	/* Get the front window */
		CalcVis((WindowPtr)theWindow);				/* Reset the visible region */
		CalcVisBehind((WindowPtr)theWindow,OldeGrayRgn);	/* Remove the menu bar from windows */
		DisposeRgn(OldeGrayRgn);		/* Bye bye region */
		OldeGrayRgn = 0;			/* Zap the handle */
		HiliteMenu(0);				/* Don't hilite any menu options */
		DrawMenuBar();				/* Redraw the menu bar */
	}
}

/********************************

	Restore the menu bar height so that
	the OS can handle menu bar events

********************************/

void FixMHeight(void)
{
	if (MenuBarHidden) {		/* Hidden? */
		LMSetMBarHeight(OldMBarHeight);		/* Reset the height */
	}
}

/********************************

	Zap the menu bar height so that things
	like MenuClock won't draw

********************************/

void ZapMHeight(void)
{
	if (MenuBarHidden) {		/* Hidden? */
		LMSetMBarHeight(0);		/* Zap the height */
	}
}
