#include "wolfdef.h"
#include <string.h>

Word *src1,*src2,*dest;		/* Used by the sort */

/**********************************
	
	Merges src1/size1 and src2/size2 to dest
	Both Size1 and Size2 MUST be non-zero
	
**********************************/

void Merge(Word Size1, Word Size2)
{
	Word *XDest,*XSrc1,*XSrc2;
	
/* merge two parts of the unsorted array to the sorted array */

	XDest = dest;
	dest = &XDest[Size1+Size2];
	XSrc1 = src1;
	src1 = &XSrc1[Size1];
	XSrc2 = src2;
	src2 = &XSrc2[Size2];
	
	if (XSrc1[0] < XSrc2[0]) {		/* Which sort to use? */
mergefrom1:
		do {
			XDest[0] = XSrc1[0];	/* Copy one entry */
			++XDest;
			++XSrc1;
			if (!--Size1) {			/* Any more? */
				do {	/* Dump the rest */
					XDest[0] = XSrc2[0];	/* Copy the rest of data */
					++XDest;
					++XSrc2;
				} while (--Size2);
				return;
			}
		} while (XSrc1[0] < XSrc2[0]);
	}
	do {
		XDest[0] = XSrc2[0];
		++XDest;
		++XSrc2;
		if (!--Size2) {
			do {
				XDest[0] = XSrc1[0];
				++XDest;
				++XSrc1;
			} while (--Size1);
			return;
		}
	} while (XSrc1[0] >= XSrc2[0]);
	goto mergefrom1;
}

/**********************************
	
	Sorts the events from xevents[0] to xevent_p
	firstevent will be set to the first sorted event (either xevents[0] or sortbuffer[0])
	
**********************************/

void SortEvents(void)
{
	Word	count;	/* Number of members to sort */
	Word	size;	/* Entry size to sort with */
	Word	sort;	/* Sort count */
	Word	remaining;	/* Temp merge count */
	Word	*sorted,*unsorted,*temp;
    
	count = numvisspr;		/* How many entries are there? */
	if (count<2) {
		firstevent = xevents;	/* Just return the 0 or 1 entries */
		return;				/* Leave now */
	}
	
	size = 1;		/* source size		(<<1 / loop)*/
	sort = 1;		/* iteration number (+1 / loop)*/
	sorted = xevents;
	unsorted = sortbuffer;
	
	do {
		remaining = count>>sort;	/* How many times to try */
		
		/* pointers incremented by the merge */
		src1 = sorted;		/* Sorted array */
		src2 = &sorted[remaining<<(sort-1)];	/* Half point */
		dest = unsorted;	/* Dest array */
		
		/* merge paired blocks*/
		if (remaining) {	/* Any to sort? */
			do {
				Merge(size,size);	/* All groups equal size */
			} while (--remaining);
		}
		
		/* copy or merge the leftovers */
		remaining = count&((size<<1)-1);	/* Create mask (1 bit higher) */
		if (remaining > size) {	/* one complete block and one fragment */
			src1 = &src2[size];
			Merge(remaining-size,size);
		} else if (remaining) {	/* just a single sorted fragment */
			memcpy(dest,src2,remaining*sizeof(Word));	/* Copy it */
		}
		
		/* get ready to sort back to the other array */
		
		size <<= 1;		/* Double the entry size */
		++sort;			/* Increase the shift size */
		temp = sorted;	/* Swap the pointers */
		sorted = unsorted;
		unsorted = temp;
	} while (size<count);
	firstevent = sorted;
}

/**********************************
	
	Draw a single scaled sprite
	x1 = Left edge, x2 = Right edge, rs_vseg = record for sprite
	
**********************************/

void RenderSprite(Word x1,Word x2,vissprite_t *VisPtr)
{
	Word column;
	Word scaler;
		
	scaler = VisPtr->clipscale;		/* Get the size of the sprite */
	column = 0;					/* Start at the first column */
	if ((int) x1 > VisPtr->x1) {		/* Clip the left? */
		column = (x1-VisPtr->x1)*VisPtr->columnstep;	/* Start here instead */
	}

/* calculate and draw each column */

	do {
		if (xscale[x1] <= scaler) {	/* Visible? */
			IO_ScaleMaskedColumn(x1,scaler,VisPtr->pos,column>>FRACBITS);
		}
		column+=VisPtr->columnstep;		/* Next column (Fraction) */
	} while (++x1<=x2);
}

/**********************************
	
	Add a sprite entry to the render list
	
**********************************/

void AddSprite (thing_t *thing,Word actornum)
{
	fixed_t tx;			/* New X coord */
	fixed_t tz;			/* New z coord (Size) */
	Word scale;			/* Scaled size */
	int	px;				/* Center X coord */
	unsigned short *patch;	/* Pointer to sprite data */
	int	x1, x2;			/* Left,Right x */
	Word width;			/* Width of sprite */
	fixed_t trx,try;	/* x,y from the camera */
	vissprite_t *VisPtr;	/* Local pointer to visible sprite record */

/* transform the origin point */
	
	if (numvisspr>=(MAXVISSPRITES-1)) {
		return;
	}
	trx = thing->x - viewx;		/* Adjust from the camera view */
	try = viewy - thing->y;		/* Adjust from the camera view */
	tz = R_TransformZ(trx,try);	/* Get the distance */

	if (tz < MINZ) {		/* Too close? */
		return;
	}
		
	if (tz>=MAXZ) {		/* Force smallest */
		tz = MAXZ-1;
	}
	scale = scaleatzptr[tz];	/* Get the scale at the z coord */
	tx = R_TransformX(trx,try);	/* Get the screen x coord */
	px = ((tx*(long)scale)>>7) + CENTERX;	/* Use 32 bit precision! */

/* calculate edges of the shape */

	patch = SpriteArray[thing->sprite];	/* Pointer to the sprite info */
	
	width =((LongWord)patch[0]*scale)>>6; 	/* Get the width of the shape */
	if (!width) {
		return;		/* too far away*/
	}
	x1 = px - (width>>1);	/* Get the left edge */
	if (x1 >= (int) SCREENWIDTH) {
		return;		/* off the right side */
	}
	x2 = x1 + width - 1;			/* Get the right edge */
	if (x2 < 0) {
		return;		/* off the left side*/
	}
	VisPtr = &vissprites[numvisspr];
	VisPtr->pos = &patch[0];	/* Sprite info offset */
	VisPtr->x1 = x1;			/* Min x */
	VisPtr->x2 = x2;			/* Max x */
	VisPtr->clipscale = scale;	/* Size to draw */
	VisPtr->columnstep = (patch[0]<<8)/width; /* Step for width scale */
	VisPtr->actornum = actornum;	/* Actor who this is (0 for static) */

/* pack the vissprite number into the low 6 bits of the scale for sorting */

	xevents[numvisspr] = (scale<<6) | numvisspr;		/* Pass the scale in the upper 10 bits */
	++numvisspr;		/* 1 more valid record */
}

/**********************************
	
	Draw a scaling game over sprite on top of everything
	
**********************************/

void DrawTopSprite(void)
{
	unsigned short *patch;
	int x1, x2;
	Word width;
	vissprite_t VisRecord;

	if (topspritescale) {		/* Is there a top sprite? */

/* calculate edges of the shape */

		patch = SpriteArray[topspritenum];		/* Get the info on the shape */
		
		width = (patch[0]*topspritescale)>>7;		/* Adjust the width */
		if (!width) {	
			return;		/* Too far away */
		}
		x1 = CENTERX - (width>>1);		/* Use the center to get the left edge */
		if (x1 >= SCREENWIDTH) {		
			return;		/* off the right side*/
		}
		x2 = x1 + width - 1;		/* Get the right edge */
		if (x2 < 0) {
			return;		/* off the left side*/
		}
		VisRecord.pos = patch;	/* Index to the shape record */
		VisRecord.x1 = x1;			/* Left edge */
		VisRecord.x2 = x2;			/* Right edge */
		VisRecord.clipscale = topspritescale;	/* Size */
		VisRecord.columnstep = (patch[0]<<8)/(x2-x1+1);	/* Width step */

/* Make sure it is sorted to be drawn last */

		memset(xscale,0,sizeof(xscale));		/* don't clip behind anything */
		if (x1<0) {
			x1 = 0;		/* Clip the left */
		}
		if (x2>=SCREENWIDTH) {
			x2 = SCREENWIDTH-1;	/* Clip the right */
		}
		RenderSprite(x1,x2,&VisRecord);		/* Draw the sprite */
	}
}

/**********************************
	
	Draw all the character sprites
	
**********************************/

void DrawSprites(void)
{
	vissprite_t	*dseg;		/* Pointer to visible sprite record */
	int x1,x2;				/* Left x, Right x */
	Word i;					/* Index */
	static_t *stat;			/* Pointer to static sprite record */
	actor_t	*actor;			/* Pointer to active actor record */
	missile_t *MissilePtr;	/* Pointer to active missile record */
	Word *xe;				/* Pointer to sort value */

	numvisspr = 0;			/* Init the sprite count */
	
/* add all sprites in visareas*/

	if (numstatics) {		/* Any statics? */
		i = numstatics;
		stat = statics;			/* Init my pointer */
		do {
			if (areavis[stat->areanumber]) {	/* Is it in a visible area? */
				AddSprite((thing_t *) stat,0);	/* Add to my list */
			}
			++stat;		/* Next index */
		} while (--i);	/* Count down */
	}
	
	if (numactors>1) {		/* Any actors? */
		i = 1;				/* Index to the first NON-PLAYER actor */
		actor = &actors[1];	/* Init pointer */
		do {
			if (areavis[actor->areanumber]) {	/* Visible? */
				AddSprite ((thing_t *)actor, i);	/* Add it */
			}
			++actor;		/* Next actor */
		} while (++i<numactors);	/* Count up */
	}
	
	if (nummissiles) {		/* Any missiles? */
		i = nummissiles;	/* Get the missile count */
		MissilePtr = missiles;	/* Get the pointer to the first missile */
		do {
			if (areavis[MissilePtr->areanumber]) {	/* Visible? */
				AddSprite((thing_t *)MissilePtr,0);	/* Show it */
			}
			++MissilePtr;	/* Next missile */
		} while (--i);		/* Count down */
	}

	i = numvisspr;
	if (i) {			/* Any sprites? */

/* sort sprites from back to front*/

		SortEvents();

/* draw from smallest scale to largest */

		xe=firstevent;
		do {
			dseg = &vissprites[xe[0]&(MAXVISSPRITES-1)];	/* Which one? */
			x1 = dseg->x1;
			if (x1<0) {		/* Clip the left? */
				x1 = 0;
			}
			x2 = dseg->x2;
			if (x2>= (int)SCREENWIDTH) {	/* Clip the right? */
				x2 = SCREENWIDTH-1;
			}
			RenderSprite(x1,x2,dseg);	/* Draw the sprite */
			++xe;
		} while (--i);
	}
}



