extern void CenterWindowOnMonitor(WindowPtr theWindow, GDHandle theMonitor);
extern Boolean SupportsColor(GDHandle theMonitor);
extern short SupportsDepth(GDHandle theMonitor, int theDepth, Boolean needsColor);
extern Boolean SupportsSize(GDHandle theMonitor, short theWidth, short theHeight);
extern GDHandle MonitorFromPoint(Point *thePoint);
extern GDHandle PickAMonitor(int bitDepth, Boolean colorRequired, short minWidth, short minHeight);
extern Boolean WaitNextEvent2(short EventMask,EventRecord *theEvent,long sleep,RgnHandle mouseRgn);