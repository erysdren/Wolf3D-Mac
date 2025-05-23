#include <Folders.h>
#include <Gestalt.h>
#include <Processes.h>
#include "Wolfdef.h"
#include <String.h>
#include "prefs.h"

typedef struct {
	Byte fileName[34];		/* Enough space for filename */
	OSType creator;			/* Creator TYPE */
	OSType fileType;		/* File type */
	OSType resType;			/* Resource type */
	short resID;			/* Open resource file ID */
} PrefsInfo;

static PrefsInfo prefsInfo;	/* My internal prefs record */
static Boolean prefsInited = FALSE;	/* Is the struct valid? */

/**********************************

	I miss the Apple IIgs where all you need to set the prefs directory
	was to pass a filename of "@:Prefs file" and it will automatically place
	the prefs file in either the proper Network folder or system prefs folder...
	
	Instead I have to do this bullshit to scan the volumes to find the prefs
	folder and volume...
	
	If the file was not found, then create it.
	
	return TRUE if the file was found and could not be created
	
**********************************/

static Boolean FindPrefsFile(short *prefVRefNum, long *prefDirID)
{
	OSErr theErr;
	long response;
	CInfoPBRec infoPB;

	if (!prefsInited) {		/* Only look if the prefs structure is valid */
		return FALSE;		/* Exit NOW! */
	}
		
	/* First, try it the easy way... */
	
	if ( !Gestalt(gestaltFindFolderAttr, &response) && /* Is the easy way available? */
		( (1<<gestaltFindFolderPresent) & response)) {	
		/* Call the OS to do the dirty work */
		theErr = FindFolder(kOnSystemDisk, kPreferencesFolderType, kCreateFolder,
							prefVRefNum, prefDirID);
	
	/* OK, try it the hard way... :( */
	
	} else {
		SysEnvRec theSysEnv;
		StringPtr prefFolderName = "\pPreferences";
		
		/* yeachh -- we have to do it all by hand! */
		if (SysEnvirons(1, &theSysEnv))  {		/* Is the system disk present? */
			return FALSE;						/* Forget it! */
		}
		*prefVRefNum = theSysEnv.sysVRefNum;	/* Save off the boot volume ID */
		
		/* check whether Preferences folder already exists */
		infoPB.hFileInfo.ioCompletion = 0;		/* Wait for completion */
		infoPB.hFileInfo.ioNamePtr    = prefFolderName;	/* Get folder name */
		infoPB.hFileInfo.ioVRefNum    = *prefVRefNum;	/* Pass the volume # */
		infoPB.hFileInfo.ioFDirIndex  = 0;		/* Scan directories */
		infoPB.hFileInfo.ioDirID      = 0;		/* Init dir id */
		theErr = PBGetCatInfo(&infoPB, FALSE);	/* Get the catalog info */
		if (!theErr) {
			*prefDirID = infoPB.hFileInfo.ioDirID;	/* Return the folder id */
		} else if (theErr == fnfErr) {		/* Preferences doesn't already exist */
	
			HParamBlockRec dirPB;
			/* create "Preferences" folder */
			dirPB.fileParam.ioCompletion = 0;
			dirPB.fileParam.ioVRefNum    = *prefVRefNum;
			dirPB.fileParam.ioNamePtr    = prefFolderName;
			dirPB.fileParam.ioDirID      = 0;
			theErr = PBDirCreate(&dirPB, FALSE);		/* Create the folder */
			if (!theErr) {
				*prefDirID = dirPB.fileParam.ioDirID;	/* Save the ID */
			}
		}
	}
	
	/* if we make it here OK, create Preferences file if necessary */
	
	if (!theErr) {
		infoPB.hFileInfo.ioCompletion = 0;
		infoPB.hFileInfo.ioNamePtr    = prefsInfo.fileName;
		infoPB.hFileInfo.ioVRefNum    = *prefVRefNum;
		infoPB.hFileInfo.ioFDirIndex  = 0;
		infoPB.hFileInfo.ioDirID      = *prefDirID;
		theErr = PBGetCatInfo(&infoPB, FALSE);		/* Get the file info */
		if (theErr == fnfErr) {				/* Not present? */
			theErr = HCreate(*prefVRefNum, *prefDirID, prefsInfo.fileName,
								prefsInfo.creator, prefsInfo.fileType);
			if (!theErr) {
				HCreateResFile(*prefVRefNum, *prefDirID, prefsInfo.fileName);
				theErr = ResError();		/* Was there an error? */
			}
		}
	}
	return (!theErr);
}

/**********************************

	Init the record for the "Prefs" file
	All this does is preset the prefsfile structure
	for the filename of the prefs file.
	
**********************************/

void InitPrefsFile(OSType creator,Byte *PrefsName)
{
	Word FLen;

	FLen = strlen((char *)PrefsName);		/* How long is the string? */
	prefsInfo.fileName[0] = FLen;			/* Make a PASCAL string */
	BlockMove(PrefsName,&prefsInfo.fileName[1],FLen);	/* Copy the bulk */
	prefsInfo.creator = creator;	/* 4 Letter creator filetype */
	prefsInfo.fileType = 'PREF';	/* Pref's filetype */
	prefsInfo.resType = 'PREF';		/* Pref's resource */
	prefsInfo.resID = 0;			/* No resource ID assigned yet */
	prefsInited = TRUE;				/* All numbers are set... */
}

/**********************************

	Load in a prefs file and store it 
	into a passed pointer IF the file was
	loaded properly.
	
	Return any error codes.
		
**********************************/

OSErr LoadPrefsFile(Byte *PrefsPtr,Word PrefsLen)
{
	short prefVRefNum, prefRefNum;
	long prefDirID;
	Handle	origHdl;
	LongWord origSize;

	if (!FindPrefsFile(&prefVRefNum, &prefDirID)) {	/* Search for the file */
		return fnfErr;		/* File was NOT found error */
	}
	
	prefRefNum = HOpenResFile(prefVRefNum, prefDirID, prefsInfo.fileName, fsRdWrPerm);
	if (prefRefNum == -1) {		/* Resource error? */
		return ResError();		/* Return the resource manager error */
	}
	/* Not finding the resource is not an error -- caller will use default data */
	origHdl = Get1Resource(prefsInfo.resType, prefsInfo.resID);
	if (origHdl) {							/* Valid handle? */
		origSize = GetHandleSize(origHdl);	/* How much data is here? */
		if (origSize < PrefsLen) {			/* Less data than expected? */
			PrefsLen = origSize;			/* Use the smaller size */
		}
		BlockMove(*origHdl,PrefsPtr,PrefsLen);	/* Copy the NEW prefs data */
		ReleaseResource(origHdl);			/* Release the data */
	}
	CloseResFile(prefRefNum);		/* Close the resource file */
	return ResError();				/* Return any errors */
}

/**********************************

	Save data for a new prefs file
	
**********************************/

OSErr SavePrefsFile(Byte *PrefsPtr,Word PrefsLen)
{
	short prefVRefNum, prefRefNum;
	long prefDirID;
	Handle origHdl;
	LongWord origSize;
	OSErr theErr = noErr;
	
	if (!FindPrefsFile(&prefVRefNum, &prefDirID)) {	/* File not present? */
		return fnfErr;		/* File not found error */
	}
	
	prefRefNum = HOpenResFile(prefVRefNum, prefDirID, prefsInfo.fileName, fsRdWrPerm);
	if (prefRefNum == -1) {	/* Bad resource fork? */
		return ResError();		/* Return the error */
	}
	
	origHdl = Get1Resource(prefsInfo.resType, prefsInfo.resID);	/* Get the resource */
	if (origHdl) {		/* Overwrite the existing resource */
		origSize = GetHandleSize(origHdl);		/* How large is it? */
		if (PrefsLen != origSize) {				/* Different size? */
			SetHandleSize(origHdl, PrefsLen);	/* Set the new size */
		}
		BlockMove(PrefsPtr,*origHdl,PrefsLen);	/* Copy the data */
		ChangedResource(origHdl);			/* Mark as changed */
		WriteResource(origHdl);				/* Save to disk */
		ReleaseResource(origHdl);			/* Release it */
	} else {
		/* store specified preferences for the first time */
		origHdl = NewHandle(PrefsLen);		/* Make some temp memory */
		if (origHdl) {
			BlockMove(PrefsPtr,*origHdl,PrefsLen);
			AddResource(origHdl, prefsInfo.resType, prefsInfo.resID, "\p");
			WriteResource(origHdl);			/* Write to disk */
			ReleaseResource(origHdl);		/* Release it */
		}
	}
	
	CloseResFile(prefRefNum);		/* Close the resource file */
	if (!theErr) {					/* No errors? */
		return ResError();			/* Return any resource error */
	}
	return theErr;					/* Return the last error */
}
