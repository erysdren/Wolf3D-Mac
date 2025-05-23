#include "wolfdef.h"
#include <Palettes.h>
#include <gestalt.h>
#include "PickAMonitor.h"

#define	PAMStrings		2001
#define	NoColorString		1
#define	NoDepthString		2
#define	NoSizeString		3
#define	NoProblemString	4

void CheckMonitor(DialogPtr theWindow, int bitDepth, Boolean colorRequired, short minWidth, short minHeight);
void OutlineOK(DialogPtr theDialog, Boolean enabled);

/**********************************

	Take a window pointer and center it onto the
	current video device
		
**********************************/

void CenterWindowOnMonitor(WindowPtr theWindow, GDHandle theMonitor)
{
	short	newH, newV;
	Rect	mRect;

	mRect = (**theMonitor).gdRect;		/* Get the rect of the monitor */

	/*	Find the difference between the two monitors' sizes */
	newH = (mRect.right - mRect.left) - (theWindow->portRect.right - theWindow->portRect.left);
	newV = (mRect.bottom - mRect.top) - (theWindow->portRect.bottom - theWindow->portRect.top);

	/*	Half the difference so that it's centered top-to-bottom and left-to-right */
	/*	Add that offset to the upper-left of the monitor */

	MoveWindow(theWindow,(newH>>1)+mRect.left,(newV>>1)+mRect.top,TRUE); /* Move and bring to front */
}

/**********************************

	Returns true if the device supports color
		
**********************************/

Boolean SupportsColor(GDHandle theMonitor)
{
	return TestDeviceAttribute(theMonitor,gdDevType);		/* Is it color? */
}

/**********************************

	Returns true if the device supports a specific color depth
		
**********************************/

short SupportsDepth(GDHandle theMonitor, int theDepth, Boolean needsColor)
{
	return HasDepth(theMonitor,theDepth,1<<gdDevType,needsColor);
}

/**********************************

	Returns true if the device supports a specific video size
		
**********************************/

Boolean SupportsSize(GDHandle theMonitor, short theWidth, short theHeight)
{
	Rect theRect;

	/* Grab the dimensions of the monitor */
	theRect = (**theMonitor).gdRect;

	/* Offset it to (0, 0) references */
	OffsetRect(&theRect, -theRect.left, -theRect.top);
	
	/* Check the dimensions to see if they are large enough */
	if ((theRect.right < theWidth) || (theRect.bottom < theHeight)) {
		return FALSE;		/* No good! */
	}
	return TRUE;
}

/**********************************

	Return the GDevice from a specific quickdraw point
		
**********************************/

GDHandle MonitorFromPoint(Point *thePoint)
{
	GDHandle theMonitor;

	/*	Loop through the list of monitors to see one encompasses the point */
	theMonitor = GetDeviceList();	/* Get the first monitor */
	do {
		if (PtInRect(*thePoint,&(**theMonitor).gdRect)) {	/* Is this it? */
			break;			/* Exit now */
		}
		theMonitor = GetNextDevice(theMonitor);		/* Get the next device in list */
	} while (theMonitor);		/* All done now? */

	/*	Just in case some weird evil happened, return a fail value (THEORETICALLY can't happen) */
	return theMonitor;
}



//•	----------------------------------------	PickAMonitor

GDHandle PickAMonitor(int theDepth, Boolean colorRequired, short minWidth, short minHeight)
{
	GDHandle	theMonitor;
	GDHandle	tempMonitor;
	Point		thePoint;
	EventRecord	theEvent;
	DialogPtr	theDialog;
	short	itemHit;
	char	theChar;
	Word	validCount;
	GrafPtr	savedPort;

	/*	Loop through the monitor list once to make sure there is at least one good monitor */
	theMonitor = GetDeviceList();		/* Get the first monitor */
	tempMonitor = 0;					/* No monitor found */
	validCount = 0;						/* None found */
	do {
		if (colorRequired && !SupportsColor(theMonitor)) {	/* Check for color? */
			continue;
		}

		if (!SupportsDepth(theMonitor, theDepth, colorRequired)) { /* Check for bit depth */
			continue;
		}

		if (!SupportsSize(theMonitor, minWidth, minHeight))	{	/* Check for monitor size */
			continue;
		}
		tempMonitor = theMonitor;		/* Save the valid record */
		++validCount;					/* Inc the count */
	} while ((theMonitor = GetNextDevice(theMonitor)) != 0);

	/*	If there was only one valid monitor, goodMonitor will be referencing it.  Return it immediately. */
	if (validCount == 1) {
		return (tempMonitor);		/* Exit now */
	}

	/* If there are no valid monitors then put up a dialog saying so.  Then return nil. */

	if (!validCount) {
		StopAlert(2001, nil);
		return 0;
	}

	//•	Start an event loop going.  Rather than using modalDialog with a gnarly filter, we'll just display the dialog
	//•	then use a gnarly event loop to accommodate it.  This will go on until the user selects either "This Monitor"
	//•	(ok) or "Quit" (cancel).
	
	theDialog = GetNewDialog(2000, nil, (WindowPtr) -1L);
	CenterWindowOnMonitor((WindowPtr) theDialog, GetMainDevice());
	InitCursor();
	GetPort(&savedPort);
	SetPort(theDialog);
	
	ShowWindow(theDialog);
	CheckMonitor(theDialog, theDepth, colorRequired, minWidth, minHeight);

	do {
		itemHit = 0;
		
		/*	Get next event from the event queue */
		if (WaitNextEvent2(everyEvent&(~highLevelEventMask), &theEvent, 30, nil)) {
			switch (theEvent.what) {
				case keyDown:
					theChar = theEvent.message & charCodeMask;
					if ((theEvent.modifiers & cmdKey) && ((theChar == '.') || (theChar == 'q') || (theChar == 'Q'))) {
						itemHit = cancel;
						break;
					}
					
					if (theChar == 0x1B) {
						itemHit = cancel;
						break;
					}
					
					if ((theChar == 0x0D) || (theChar == 0x03)) {
						itemHit = ok;
						break;
					}
					
					break;
			
				//•	Did we mouse-down in the dialogs drag-bar?
				case mouseDown:
					if (FindWindow(theEvent.where, (WindowPtr *) &theDialog) == inDrag)
					{
						DragWindow((WindowPtr) theDialog, theEvent.where, &qd.screenBits.bounds);
						GetMouse(&thePoint);
						LocalToGlobal(&thePoint);
						tempMonitor = MonitorFromPoint(&thePoint);
						CenterWindowOnMonitor((WindowPtr) theDialog, tempMonitor);
						CheckMonitor(theDialog, theDepth, colorRequired, minWidth, minHeight);
						break;
					}

				default:
					//•	If not, perform regular dialog event processing
					DialogSelect(&theEvent, &theDialog, &itemHit);
			}
		}
	} while ((itemHit != ok) && (itemHit != cancel));

	//•	Ok, the dialog is still the active grafport so all coordinates are in its local system.  If I define a point
	//•	as (0, 0) now it will be the upper left corner of the dialog's drawing area.  I then turn this to a global screen
	//•	coordinate and call GetMonitorFromPoint.
	
	SetPt(&thePoint, 0, 0);
	LocalToGlobal(&thePoint);
	theMonitor = MonitorFromPoint(&thePoint);

	//•	Restore our current graphics environment and return the monitor reference.
	SetPort(savedPort);
	DisposeDialog(theDialog);

	if (itemHit == ok) {
		return (theMonitor);
	}
	return (nil);
}




//•	----------------------------------------	CheckMonitor

void CheckMonitor(DialogPtr theDialog, int bitDepth, Boolean colorRequired, short minWidth, short minHeight)
{
GDHandle	theMonitor;
Point		thePoint;
short	theType;
Handle	theHandle;
Rect		theRect;
Str255	message;
Boolean	badMonitor = FALSE;

GrafPtr	savedPort;

	GetPort(&savedPort);
	SetPort((WindowPtr) theDialog);
	SetPt(&thePoint, 0, 0);
	LocalToGlobal(&thePoint);
	theMonitor = MonitorFromPoint(&thePoint);

	GetDialogItem(theDialog, 4, &theType, &theHandle, &theRect);
	
	if (colorRequired && ! SupportsColor(theMonitor)) {
		GetIndString(message, PAMStrings, NoColorString);
		SetDialogItemText(theHandle, message);
		badMonitor = TRUE;
	}
	
	if (!SupportsDepth(theMonitor, bitDepth, colorRequired)) {
		GetIndString(message, PAMStrings, NoDepthString);
		SetDialogItemText(theHandle, message);
		badMonitor = TRUE;
	}

	if (! SupportsSize(theMonitor, minWidth, minHeight)) {
		GetIndString(message, PAMStrings, NoDepthString);
		SetDialogItemText(theHandle, message);
		badMonitor = TRUE;
	}

	SetPort(savedPort);
	
	if (badMonitor) {
		//•	Gray-out the "This Monitor" button
		GetDialogItem(theDialog, ok, &theType, &theHandle, &theRect);
		HiliteControl((ControlHandle) theHandle, 255);
		OutlineOK(theDialog, FALSE);
		BeginUpdate((WindowPtr) theDialog);
		UpdateDialog(theDialog, theDialog->visRgn);
		EndUpdate((WindowPtr) theDialog);
		return;
	}

	//•	Activate the "This Monitor" button and put up the no problem text.
	GetIndString(message, PAMStrings, NoProblemString);
	SetDialogItemText(theHandle, message);
	GetDialogItem(theDialog, ok, &theType, &theHandle, &theRect);
	HiliteControl((ControlHandle) theHandle, 0);
	OutlineOK(theDialog, TRUE);
	BeginUpdate((WindowPtr) theDialog);
	UpdateDialog(theDialog, theDialog->visRgn);
	EndUpdate((WindowPtr) theDialog);
}

//•	----------------------------------------	OutlineOK

void OutlineOK(DialogPtr theDialog, Boolean enabled)
{
GrafPtr	savedPort;

ColorSpec	saveColor;
PenState	savedState;

short	theType;
Handle	theHandle;
Rect		theRect;

	GetPenState(&savedState);
	PenMode(patCopy);
	PenSize(2, 2);

	SaveFore(&saveColor);
	ForeColor(blackColor);

	GetPort(&savedPort);
	SetPort((WindowPtr) theDialog);

	GetDialogItem(theDialog, ok, &theType, &theHandle, &theRect);
	InsetRect(&theRect, -4, -4);
	
	if (enabled)
		PenPat(&qd.black);
	else
		PenPat(&qd.gray);
	
	FrameRoundRect(&theRect, 16, 16);
	
	SetPort(savedPort);
	RestoreFore(&saveColor);
	SetPenState(&savedState);
}
