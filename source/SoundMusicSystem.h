/*****************************************************/
/*
**	SoundMusicSys.h
**
**		Structures for handling the sounds used by system
**
**  		(c) 1989-1994 by Halestorm, Inc, All Rights Reserved
**  
**
**	History	-
** 	7/20/89	Created
** 	7/24/89	Created Asyncronous Sample List Player.
** 	12/3/89	Modified all tools to work with the Sound Driver instead of the
**			Sound Manager (He got fired.)
**	12/21/89 Added PlayTheSample.
**	2/1/90	Added Sound Manager (he got hired as a consultant) and use it
**			when system 6.0 or greater is around. (Better sound.   !!?!??)
**	4/10/90	Fixed pause/resume sound to allocate/deallocate sound channel.
**	7/3/90	Added a REST mechanisim to the SampleList player
**	7/28/90	Added IsSoundListFinished() to the SampleList player
**	7/31/90	Added IsSoundFXFinished()
**	8/13/90	Moved example and StandAlone code to a seperate project.
**  12/12/90	Added GetSoundWaveform() & GetSoundLength() & GetSoundDefaultRate().
**	1/9/91	Added sound done call back function.
**	3/4/91	Completed work on Sound Driver's call back.
**	4/10/91	Fixed a bug in CalcPlaybackLength that calculated the wrong length.
**	4/10/91	Added purgability of the samples.
**	4/23/91	Added better compatibility to Think C version 3.0
**	4/30/91	Put in support for snd resource type 1 and type 2.
**	5/18/91	Changed to zone calls instead of globals.
**	5/20/91	Mucking around for bug that Mick is having.
**	5/20/91	Added csnd compressed resource based upon the LZSS algo.
**	5/25/91	Added a FinisSoundChannel/InitSoundChannel inside of EndSound() for Mick
**	5/27/91	Fixed potential bug with BeginSoundList() in which the sound may be
**			loaded during a VBL!
**	5/28/91	Modified to new Think C MPW like header names.
**	6/18/91	Added delay to the starting of a new sound with BeginSound (4 Ticks).
**	7/24/91	Added delay to the starting of a new sound with PlayTheSample (4 Ticks).
**	8/5/91	Removed duplicate code in BeginSound & PlayTheSample.
**	8/5/91	Added 2 tick delay to soundLength. Used in IsSoundFXFinished.
**	8/5/91	Added BeginSoundReverse & BeginSoundSection.
**	8/6/91	Fixed some memory management problems with SoundMemorySize.
**	8/6/91	Added a semaphore to the vbl task.
**	9/9/91	Cleaned up for Think C 5.0
**	9/9/91	Added PLAY_ALL_SAMPLE
**	9/9/91	Changed all 'int's to 'short int'. Sorry guys. This is because I want
**			16 bit integers.
**	10/8/91	Added sound looping features. BeginSoundLoop(), BeginSoundEnvelope(),
**			and BeginSoundEnvelopeProc().
**	11/11/91 Added SoundLock().
**	1/22/92	Put in buffer SndDoCommands for looped & enveloped sounds
**	3/9/92	Modifed for MPW and removed all THINK C 3 & 4 compiler stuff
**	3/23/92	Added access to"Private-Sound/Music.h"
**	5/1/92	Added the SoundChannelRestart() function
**	7/10/92	Combined SoundSys.h & MusicSys.h into new and improved version
**	7/10/92	Added new calls to initilize & clean up the system
**	7/10/92	Heavily modified for Rev 3 of the sound system.
**	7/10/92	Now calling jim's sound mixer
**	7/13/92	Added BeginMasterFadeOut & FadeLevel
**	7/18/92	Fully intergrated the SoundManager out and replaced with "SoundGod"
**	7/26/92	Added ChangeOuputQuality() and fixed bug with BeginSongLooped
**	7/31/92	Fixed weird bug with MPW passing of 32-bit value "shorts"!!
**	7/31/92	Fixed BeginMasterFadeOut code.
**	8/4/92	Change playback rate command to 16.16 fixed value. Sorry guys.
**	8/6/92	Built a mult-voice tracker, to work with sound effects.
**	8/23/92	Added 2 pt intrepolation	
**	8/24/92	Added ChangeSystemVoices
**	12/4/92	Added LoadSound
**	12/9/92	Added FreeSound
**	12/29/92 ResumeSoundMusicSystem and PauseSoundMusicSystem now return
**			errors
**	3/6/93	Fixed the SoundDone callback system.
**	3/6/93	Elimated the playback decay govenour.
**	3/16/93	Added BeginMasterFadeIn
**	3/19/93	Added SetSongDoneCallBack
**	5/31/93	Made LoadSong/FreeSong work
**	6/14/93	Changed VBL task to be out of the system heap
**	6/24/93	Added StartFilePlayback, EndFilePlayback, and ChangeFilePlaybackRate
**	6/24/93	Added IsThisSoundFXFinished
**	7/2/93	Added EndSoundList
**	7/13/93	Fixed File streaming stop bug
**	7/14/93	Worked on stopping callback bug
**	7/15/93	Think I fixed the callback bug
**	7/21/93	Forced callbacks to use a long for MPW
**	7/25/93	Fixed bug with resume while file playback is in progress
**	7/26/93	Changed all callbacks to be pascal based
**	9/2/93	Fixed macro bug with FreeSong
**	9/15/93	Fixed a looping bug with BeginSongLoop
**	9/19/93	Exposed DeltaDecompressHandle for Larry, for those who want to decompress a 'csnd' resource
**	10/27/93 Fixed bug with ReleaseRegisteredSounds that caused a crash
**	11/18/93 Added ServiceFilePlayback to the API for StartFilePlayback
**	11/18/93 Added GetSoundLoopStart & GetSoundLoopEnd
**	11/18/93 Added SetMasterVolume & GetMasterVolume
**	12/4/93	Added code/data cache flush to callback
**	12/4/93	Added GetCurrentMidiClock() & SetCurrentMidiClock()
**	1/11/94	Cleaned header up a bit.
**	2/28/94	Added PlayTheSampleWithID
**	3/15/94	Found a nasty callback problem and fixed it.
**	4/5/94	Fixed memory leak with instrument loading
**	4/29/94	Removed external div and mul function references for A4 land.
**	6/23/94	Updated to CodeWarrior
**	9/2/94	Added mixedmode manager support
**
*/
/*****************************************************/


#ifndef HALESTORM_DRIVER
#define HALESTORM_DRIVER

#ifdef __cplusplus
extern "C" {
#endif

// SOUND DRIVER EQUATES

/* Definitions for Sound resources */
#define SOUND_END				-1					/* End Sound for list */

/* Defines for sound functions */
#define SOUND_RATE_DEFAULT		0L					/* Default rate */
#define SOUND_RATE_FAST		0x56EE8BA3L			/* Fast */
#define SOUND_RATE_MEDIUM		0x2B7745D1L			/* Medium */
#define SOUND_RATE_MOSEY		SOUND_RATE_FAST/3	/* Mosey */
#define SOUND_RATE_SLOW		SOUND_RATE_MEDIUM/2	/* Slow */

#define SOUND_RATE_22k		SOUND_RATE_FAST
#define SOUND_RATE_11k		SOUND_RATE_MEDIUM
#define SOUND_RATE_7k			SOUND_RATE_MOSEY
#define SOUND_RATE_5k			SOUND_RATE_SLOW

#define REST_SAMPLE			-1L				/* When using the SampleList mechanisim, this will
											** rest (no sound) for the duration of the length
											*/
#define PLAY_ALL_SAMPLE		-2L				/* When using the SampleList mechanisim, this will
											** play the entire sound if this is put into the SampleList
											** theLength parameter.
											*/

#define CUSTOM_PLAY_ID	((short int)0x8000)		/* ID value of samples played with PlayTheSample */
#define FILE_PLAY_1_ID		((short int)0x8001)		/* ID value of samples played with StartFilePlayback (Buffer 1) */
#define FILE_PLAY_2_ID		((short int)0x8002)		/* ID value of samples played with StartFilePlayback (Buffer 1) */

/* Ranges for SetMasterFade() and FadeLevel() */
#define MAX_VOLUME		0x100
#define MIN_VOLUME		0

#define FULL_VOLUME		MAX_VOLUME
#define NO_VOLUME			MIN_VOLUME

/* Structure for a list of samples to play asynchronously
*/
#if _POWERPC_
#pragma options align=mac68k
#endif
struct SampleList
{
	short int	theSampleID;		/* Sample resource ID or REST_SAMPLE for no sound */
	long		theRate;			/* Sample playback rate */
	short int	theLength;			/* Length of note in 1/60ths of a second */
};
typedef struct SampleList SampleList;
#if _POWERPC_
#pragma options align=reset
#endif


// SoundQuality
#define jxNoDriver				0				// no driver
#define jxAnalyzeQuality			1				// best driver for speed of mac
#define jxLowQuality			2				// lowest quality (11khz) no intrepolation
#define jxHighQuality			3				// high quality (22khz) no intrepolation
#define jxIntrepLowQuality		4				// lowest quality with 2 pt intrepolation
#define jxIntrepHighQuality		5				// high quality with 2 pt intrepolation
#define jxIntrepBestLowQuality	6				// low quality with 128K memory intrepolation
#define jxIntrepBestHighQuality	7				// high quality with 128K memory intrepolation

// Support for hystrical reasons. Bad spellings
#define jxAnalyizeQuality			jxAnalyzeQuality

/* NOTE:
**	For future revisions, the numbers will change into bit fields for features as we add
**	support for 16 bit, stereo, etc. Use the defines to pass the values you want. If you 
**	don't now, you may get in trouble in the future.
*/
typedef short int SoundQuality;

/* FUNCTIONS
*/
OSErr	InitSoundMusicSystem(short int maxSongVoices, 
							short int maxNormalizedVoices, 
							short int maxEffectVoices,
							SoundQuality quality);
void		FinisSoundMusicSystem(void);

OSErr	ChangeSystemVoices	(short int maxSongVoices, 
							short int maxNormalizedVoices, 
							short int maxEffectVoices);

OSErr	PauseSoundMusicSystem(void);
OSErr	ResumeSoundMusicSystem(void);


Handle	DeltaDecompressHandle(Handle theData);

OSErr	RegisterSounds(short int *pSoundID, Boolean prePurge);
void		ReleaseRegisteredSounds(void);

long		SoundMemorySize(short int *pSoundID);

void		ChangeSoundPitch(short int theID, long theRate);
void		BeginSoundList(SampleList * sampleList, short int totalSamples);
Boolean	IsSoundListFinished(void);
void		BeginSound(short int theID, long theRate);
void		LoadSound(short int theID);
void		FreeSound(short int theID);
void		BeginSoundReverse(short int theID, long theRate);
void		BeginSoundSection(short int theID, long theRate, long sectionStart, long sectionEnd);
void		BeginSoundLoop(short int theID, long theRate, long loopStart, long loopEnd);
void		BeginSoundEnvelope(short int theID, long theRate, short int loopTime);
void		BeginSoundEnvelopeProc(short int theID, long theRate, pascal Boolean (*theLoopProc)(short int sampleID));
Boolean	IsSoundFXFinished(void);
Boolean	IsThisSoundFXFinished(short theID);

void		PlayTheSample(Ptr pSamp, long sampSize, long sampRate);
void		PlayTheSampleWithID(Ptr pSamp, long sampSize, long sampRate, short int theID);

void		SetSoundDoneCallBack(pascal void (*theProcPtr)(short int theID));
void		SetSongDoneCallBack(pascal void (*theProcPtr)(short int theID));

void		SetMasterVolume(short int theVolume);
short int	GetMasterVolume(void);

void		SetSoundVBCallBack(pascal void (*theProc)(void));

Byte *	GetSoundWaveform(short int theID);
long		GetSoundLength(short int theID);
long		GetSoundDefaultRate(short int theID);
long		GetSoundLoopStart(short int theID);
long		GetSoundLoopEnd(short int theID);
short int	GetSoundTime(short int theID, long sampRate);
short int	CalcPlaybackLength(long theRate, long theLength);
void		EndAllSound(void);
void		EndSound(short int theID);
void		EndSoundList(void);
void		PurgeAllSounds(unsigned long minMemory);

OSErr	StartFilePlayback(FSSpec *theFile, long theRate, long bufferSize);
Boolean	ServiceFilePlayback(void);
void		ChangeFilePlaybackRate(long theNewRate);
void		EndFilePlayback(void);


#define kBeginSong					(short int)0	// kBeginSong, theSongID
#define kBeginSongLooped				(short int)1	// kBeginSongLooped, theSongID
#define kEndSong					(short int)2	// kEndSong, 0
#define kIsSongDone					(short int)3	// kIsSongDone, 0
#define kTicksSinceStart				(short int)4	// kTicksSinceStart, 0
#define kPurgeFlag					(short int)5	// kPurgeFlag, Boolean
#define kLockFlag					(short int)6	// kLockFlag, Boolean
#define kLoopSongFlag				(short int)7	// kLoopSongFlag, Boolean
#define kFadeOutput					(short int)8	// kFadeOutput, level (0 to 256)
#define kAutoFadeSoundOut			(short int)10	// kAutoFadeSoundOut, ticks for how fast
#define kAutoFadeSoundIn				(short int)11	// kAutoFadeSoundIn, ticks for how fast
#define kFadeLevel					(short int)12	// kFadeLevel, short int
#define kSetMasterFade				(short int)13	// kSetMasterFade, short int
#define kChangeOutputQuality			(short int)14	// kChangeOutputQuality, SoundQuality
#define kLoadSong					(short int)15	// kLoadSong, theSongID
#define kFreeSong					(short int)16	// kFreeSong
#define kGetErrors					(short int)17	// kGetErrors, char *
#define kGetMidiClock				(short int)18	// kGetMidiClock
#define kGetMidiBeat				(short int)19	// kGetMidiBeat
#define kGetNextMidiBeat				(short int)20	// kGetNextMidiBeat
#define kSetMidiClock				(short int)21	// kSetMidiClock, pos
#define kGetTempo					(short int)22	// kGetTempo
#define kGetSongLength				(short int)23	// kGetSongLength

#if !THINK_C
#define MUSICDRIVERHANDLER MusicDriverHandler
#endif
pascal unsigned long MusicDriverHandler(short int theMessage, long theData);

// Function call interface:

#define	BeginSong(theSong)			MusicDriverHandler(kBeginSong, (long)(theSong))
#define	BeginSongLooped(theSong)		MusicDriverHandler(kBeginSongLooped, (long)(theSong))
#define	LoadSong(theSong)			MusicDriverHandler(kLoadSong, (long)(theSong))
#define	FreeSong()				MusicDriverHandler(kFreeSong, 0L)
#define	SetLoopSongFlag(theFlag)		MusicDriverHandler(kLoopSongFlag, (long)(theFlag))
#define	EndSong()					MusicDriverHandler(kEndSong, 0L)
#define	IsSongDone()				MusicDriverHandler(kIsSongDone, 0L)
#define	SongTicks()				MusicDriverHandler(kTicksSinceStart, 0L)
#define	PurgeSongs(purge)			MusicDriverHandler(kPurgeFlag, (long)(purge))
#define	LockSongs(lock)			MusicDriverHandler(kLockFlag, (long)(lock))
#define	BeginMasterFadeOut(time)		MusicDriverHandler(kAutoFadeSoundOut, (long)(time))
#define	BeginMasterFadeIn(time)		MusicDriverHandler(kAutoFadeSoundIn, (long)(time))
#define	SetMasterFade(level)		MusicDriverHandler(kSetMasterFade, (long)(level))
#define	FadeLevel()				MusicDriverHandler(kFadeLevel, 0L)
#define	ChangeOuputQuality(qual)		MusicDriverHandler(kChangeOutputQuality, (long)(qual))
#define	GetErrors(buffer)			MusicDriverHandler(kGetErrors, (long)(buffer))
#define	GetCurrentMidiClock()		MusicDriverHandler(kGetMidiClock, 0L)
#define	GetCurrentMidiBeat()		MusicDriverHandler(kGetMidiBeat, 0L)
#define	GetNextBeatMidiClock()		MusicDriverHandler(kGetNextMidiBeat, 0L);
#define	SetCurrentMidiClock(clock)	MusicDriverHandler(kSetMidiClock, (long)(clock))
#define	GetCurrentTempo()			MusicDriverHandler(kGetTempo, 0L)
#define	GetSongLength()			MusicDriverHandler(kGetSongLength, 0L)

OSErr	BeginSongFromMemory(short int theID, Handle theSongResource, Handle theMidiResource, Boolean loopSong);
long		MaxVoiceLoad(void);

#ifdef __cplusplus
}
#endif


#endif	// HALESTORM_DRIVER
