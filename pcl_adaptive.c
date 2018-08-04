/*
	$Header:   P:/Wtreiber/PVCS/Driver310_vs7/archives/Driver/Km/Km_Grfx/pcl_adaptive.c-arc   1.3   Jun 28 2006 13:19:18   parkhura  $
*/

#include "grfx.h"

#include <stdio.h>
#include <malloc.h>

#define zerostrip TRUE

#define MAX(A,B)	((A)>(B)?(A):(B))
#define MAX3(A,B,C)	((A)>(B)?MAX(A,C):MAX(B,C))
#define MIN(A,B)	((A)<(B)?(A):(B))
#define MIN3(A,B,C)	((A)<(B)?MIN(A,C):MIN(B,C))


/*
******************************************************************************
**
**       Compress_1() does PCL compression mode 1 on the data. 
**       This mode is run length encoding.
**
**       Given an input byte count of n, then the worst case output size
**       would be  2 * n.
**
******************************************************************************
*/


static int Compress_1( unsigned char *src, unsigned char *dest, int count )
{
	unsigned char *outptr = dest, *inptr = src;

	unsigned char data;		/* data values to match */

	unsigned char temp;

	int	repcount;		/* repeat count */


	/*
	**  "count" is the number of bytes in "src" to compress
	**  into "dest".
	*/

	while ( count )
	{
		data = *inptr++;	/* get value to work with */
		count--;

		repcount = 0;		/* no repeats yet */

		/*
		**  Look for successive bytes that match "data".
		*/

		while ( count && *inptr == data )
		{
			repcount++;
			inptr++;
			count--;
		}

		/*
		**  Now if we are out of data (count == 0), then
		**  if the repeated byte was zero, then we can ignore it
		**  unless the user disabled zero stripping.
		*/

		if ( count == 0 && data == 0 && zerostrip )
			break;					/* done */

		/*
		**  Now output the repeat count and the value.
		**
		**  If the repeat count exceeds 255, then we must send
		**  more repeat-value pairs.
		**
		**  Each time thru the loop, repcount needs to be decremented
		**  to keep in sync.  That is, if repcount == 256, then the
		**  first time thru the loop, <255> <data> are output, then
		**  repcount is now 1.  So the next time thru the loop, 
		**  repcount needs to be adjusted to 0 so that the next pair
		**  is <0> <data>.  Thus, 1 data plus 255 repeats of data
		**  plus 1 data + 0 repeats of data ==> 257 total bytes
		**  of "data", which is what a repcount of 256 means.
		*/ 

		do
		{
			temp = MIN(repcount, 255);

			*outptr++ = temp;

			repcount -= temp;

			*outptr++ = data;

		} while ( repcount-- );
	}

	return ( outptr - dest );	/* size of compressed data */
}


/*
******************************************************************************
**
**       Compress_2() does PCL compression mode 2 on the data. 
**       This mode is a combination of modes 0 and 1.
**
**       Given an input byte count of n, then the worst case output size
**       would be  n + (n + 127)/128.
**
******************************************************************************
*/


static int Compress_2( unsigned char *src, unsigned char *dest, int count)
{
	unsigned char	*outptr, *inptr;
	unsigned char	*saveptr;

	unsigned char	data;			/* data byte */
	unsigned char	lastbyte;		/* last byte */
	int		repcount;		/* repeat count */
	int		litcount;		/* literal count */

	/*
	**  src points to the input data.
	**  dest points to the output buffer.
	**  count is the number of input bytes.
	*/

	inptr = src;
	outptr = dest;

	/*
	**  Start loop thru data.  Check for possible repeat at beginning.
	*/

	while ( count )
	{
		data = *inptr++;	/* get value to work with */
		count--;

		repcount = 0;		/* no repeat count yet */


		/* 
		**  Check for repeat, since we are not in the middle
		**  of a literal run, it does not have to be more than
		**  two bytes of similar data.
		*/

		while ( count && *inptr == data )
		{
			repcount++;
			inptr++;
			count--;
		}

		/*
		**  Now, if we are out of data (count == 0), then
		**  if the repeated byte was zero, then ignore it
		**  completely (don't bother outputing the trailing zeros).
		**
		**  To always strip zero's, remove the "zerostrip"
		**  from the test.
		*/

		if ( count == 0 && data == 0 && zerostrip)
			break;			/* done */


		/*
		**  If there was a repeat (repcount > 0), then we
		**  can output the command here, otherwise, we
		**  need to go into literal run mode.
		**
		**  Note:  This is a while loop because the repeat count
		**  may actually be greater than 127.
		*/

		if ( repcount >= 1 )		/* repeat mode */
		{
			while (repcount > 127)
			{
				*outptr++ = 129;		/* count 127 */
				*outptr++ = data;		/* value */
				repcount-= 128;			/* offset */
			}

			if (repcount > 0)
			{
				*outptr++ = 256 - repcount;	/* count */
				*outptr++ = data;		/* value */

				/*
				**  Now pop to the top of the loop 
				**  looking for more repeat counts.
				*/

				continue;			/* top of loop */
			}

			/*
			**  Special case:  If we have arrived at this point,
			**  then repcount is now equal to 0.  This means
			**  that when we entered this section, repcount
			**  was a multiple of 128 (i.e. 128 :-).
			**
			**  This means that there were 129 identical bytes,
			**  so the output does a replicate of 127 which
			**  gives 128 bytes, and we now have one byte left
			**  over which should NOT be output as a repeat
			**  run, rather it should be merged into the following
			**  literal run (if it exists).
			**
			**  So, we will fall thru to the next section
			**  of code which assumes that we are working on 
			**  a literal run.
			*/

		}

		/*
		**  Literal run:  At this point, the current data byte
		**  does NOT match the following byte.  We will transfer
		**  these non-identical bytes until:
		**
		**       1)  we run out of input data (count == 0).
		**       2)  we run out of room in this output block (128)
		**       3)  we come across a value which occurs at least
		**           three times in a row.  A value occuring only
		**           twice in a row does NOT justify dropping
		**           out of a literal run.
		**
		**  Special case:  If we run out of room in the output block
		**  (which is 128 bytes), the last two values are the same,
		**  AND there is more input, it makes sense to restart
		**  the repeat detector in case the following bytes are
		**  repeats of the two.  A simple check of the following
		**  byte will determine this.
		**  (This case falls out with the test for triples below).
		**
		**  Special case:  If we run out of room in the output block
		**  (which is 128 bytes), the last value is the same as
		**  the next one on the input, then it is better to let
		**  that byte be used in a possible replicate run following
		**  the literal run.  If the last byte matches ONLY the
		**  following byte, (and not the one after that,) it is
		**  a wash, but for best results, we will test the
		**  following two bytes.
		**
		*/

		litcount = 0;
		saveptr = outptr++;	/* save location of the command byte */

		*outptr++ = data;	/* save the first byte. */

		lastbyte = data;	/* remember for testing */

		while ( count && litcount < 127 )
		{
			data = *inptr++;
			count--;
			litcount++;
			*outptr++ = data;

			/*
			**  Test to see if this byte matched the last one.
			**  If so, check the next one for a triple.
			*/

			if ( lastbyte == data && count && *inptr == data )
			{
				/*
				**  We have a triple, adjust accordingly.
				**
				**  Add two bytes back onto the input.
				*/

				count += 2;
				inptr -= 2;
				outptr -= 2;
				litcount -= 2;

				break;		/* out of loop */
			}

			lastbyte = data;	/* save data byte */
		}

		/*
		**  Check the special case number 2 above.
		*/

		if ( litcount == 127  &&  count > 1  &&  data == *inptr
		    &&  data == inptr[1] )
		{
			/*  Restore the last byte to the input stream */

			count += 1;
			inptr -= 1;
			outptr -= 1;
			litcount -= 1;
		}


		/*
		**  Save the literal run count.
		*/

		*saveptr = (unsigned char)litcount;

		/*
		**  Now go back to the top and look for repeats.
		*/
	}

	count = outptr - dest;		/* for return value */

	return ( count );
}



/*
******************************************************************************
**
**       Compress_3() does PCL compression mode 3 on the data. 
**       This mode is delta row encoding.
**
**       Given an input byte count of n, then the worst case output size
**       would be  n + (n + 7)/8
**
******************************************************************************
*/


static int Compress_3( unsigned char *seed, unsigned char *pnew, unsigned char *dest, int count)
{
	unsigned char *sptr=seed, *nptr=pnew, *dptr=dest;
	unsigned char *saveptr;

	int	delta, xfer;
	unsigned char	command;


	/*
	**  "count" is the number of bytes of data in "new" that need to
	**  be compressed into "dest" using "seed" as the basis for diffs.
	*/

	while ( count > 0 )
	{
		delta = 0;		/* position counter */

		/*
		**  Hunt through the data until the new data is different
		**  from the seed data.
		*/

		while ( *sptr == *nptr && delta < count )
		{
			delta++;
			sptr++;
			nptr++;
		}

		/*
		**  Either a difference has been found, or we ran out of data.
		*/

		if ( delta >= count )	/* too far */
			break;		/* done */

		count -= delta;
		
		/*
		**  Now set up the output with the command byte[s].
		**  (leaving the actual byte copy count till last.)
		*/

		/*
		**  The first command byte can only hold up to 31.
		**  If the delta is larger, then following bytes will
		**  be needed.
		*/

		saveptr = dptr++;	/* save the address for command byte */

		command = MIN(delta, 31);

		/*
		**  Any remaining count follows.
		**
		**  If count is 31, then a following byte must be given,
		**  even if 0.  Same holds if 255 is given in succesive bytes.
		*/

		if ( command == 31 )
		{
			delta -= command;	/* adjust for first count */

			do {
				xfer = MIN(delta,255);

				delta -= xfer;

				*dptr++ = (unsigned char)xfer;

			} while ( xfer == 255 );


		}


		/*
		**  Now transfer up to 8 bytes, stopping when the new byte
		**  matches the seed byte.  One could make a case for
		**  transfering a matching byte too (if stuck in the middle
		**  of the 8 bytes), but it does not impact the worst case,
		**  and in the long run, the compression will not be as good.
		**  Also, we need to make sure that we don't overrun count.
		**  ("count" is checked first so we don't reference past the
		**  end of the memory block).
		*/

		for ( xfer = 0; 
			count && *sptr != *nptr && xfer < 8;
				xfer++)
		{
			*dptr++ = *nptr++;	/* transfer byte */
			sptr++;			/* bump seed pointer too */
			count--;
		}

		/*
		**  Now xfer is the number of bytes transfered, but the
		**  number range is 3 bits (0-7), so decrement and merge
		**  it into the command byte and put it in the data.
		*/

		command += ((xfer - 1) << 5);

		*saveptr = command;

	}

	return ( dptr - dest );
}

//
//  Compress input using PCL adaptive compression mode 5.
//
//  Always returns TRUE
//

BOOL LibCompressPCLAdapt( 
	unsigned char *pOutBuf, 
	unsigned char *pInBuf,
	const int WidthBytes,		//  *BYTES* per row of input data.
	int Rows,					//  Number of rows of input data.
	int *pSize )				//  IN:  Size of output buffer, OUT: Number of compressed bytes
{

	unsigned char *pOut, *pIn= pInBuf;
	
	int	i,j,k;

	unsigned char *pCompBuf1, *pCompBuf2, *pCompBuf3, *pX;

	unsigned char *pSeedRow;

	unsigned char *pBlockStart, *pValueStart;

	int	BlockBytes;

	int	Empty, Duplicat, Size0, Size1, Size2, Size3, SizeX, CodeX;

	int Stats[6], StatSum4, StatSum5;

	Stats[0]=
	Stats[1]=
	Stats[2]=
	Stats[3]=
	Stats[4]=
	Stats[5]=
	StatSum4=
	StatSum5=  0;

	if( *pSize < Rows * WidthBytes )
		return FALSE;			//  Definate error in calling code.

	//
	//  Need buffers to compress into
	//

	pCompBuf1= malloc( WidthBytes * 2 );			//  Worst case is double input
	pCompBuf2= malloc( WidthBytes * 2 );			//  Worst case is double input
	pCompBuf3= malloc( WidthBytes * 2 );			//  Worst case is double input

	pSeedRow= malloc( WidthBytes );

	if( pCompBuf1 == NULL || pCompBuf2 == NULL || pCompBuf3 == NULL || pSeedRow == NULL )
	{
		//
		//  Memory failure, return what we got and bail.
		//

		if( pCompBuf1 ) free( pCompBuf1 );
		if( pCompBuf2 ) free( pCompBuf2 );
		if( pCompBuf3 ) free( pCompBuf3 );
		if( pSeedRow ) free( pSeedRow );
		return FALSE;
	}

	//
	//

	pOut= pOutBuf;	 

	i= sprintf( pCompBuf1, "\x1b*b%05dW", 0 );

	memcpy( pOut, pCompBuf1, i );

	pValueStart= pOut + 3;

	pOut+= i;

	pBlockStart= pOut;			//  Start of real data...

	BlockBytes= 32767;			//  Limit per block

	//
	//  Start by clearing the seed row.
	//

	memset( pSeedRow, 0, WidthBytes );

	//
	//  Now lets loop through the data...
	//

	while( Rows-- )
	{
		//
		//  We need at least 3 bytes remaining in the block to do anything.
		//  This was moved here because I had a problem where I started the
		//  NEXT block with a duplicat row command ;-)
		//
		//  Of course, 3 bytes is not enough if there will be data (other than
		//  modes 4 and 5, but that will be handled properly after the 4 and 5 cases.
		//

		if( BlockBytes < 3 )
		{
			//
			//  Out of room in this block.
			//

			i= sprintf( pCompBuf2, "%05d", pOut - pBlockStart );

			memcpy( pValueStart, pCompBuf2, i );

			i= sprintf( pCompBuf2, "\x1b*b%05dW", 0 );

			memcpy( pOut, pCompBuf2, i );

			pValueStart= pOut + 3;

			pOut+= i;

			pBlockStart= pOut;			//  Start of real data...

			BlockBytes= 32767;			//  Limit per block
			
			//
			//  Starting a new block clears the seed row.
			//

			memset( pSeedRow, 0, WidthBytes );
		}

		//
		//  Look for empty rows...
		//

		Empty= 0;

		Size1= Compress_1( pIn, pCompBuf1, WidthBytes );

		while( Size1 == 0 )
		{
			Empty++;
			pIn+= WidthBytes;			//  Go to next row.

			if( Rows-- == 0 )
				break;

			Size1= Compress_1( pIn, pCompBuf1, WidthBytes );
		}

		if( Empty )
		{
			//
			//  Got some empty rows...
			//

			*pOut++= 4;								//  empty row code
			*pOut++= (unsigned char)(Empty >> 8);	//  upper bytes
			*pOut++= (unsigned char)Empty;			//  lower byte	

			BlockBytes-= 3;

			Stats[4]++;
			StatSum4+= Empty;

   			//
			//  Empty rows clear the seed row.
			//

			memset( pSeedRow, 0, WidthBytes );
		}

		if( Rows == -1 )
		{
			//
			//  Ended on an empty row...
			//

			break;			//  out of loop
		}

		//
		//  Remember Size1.
		//

		//
		//  Ok, now look for duplicate rows...
		//

		Duplicat= 0;

		Size3= Compress_3( pSeedRow, pIn, pCompBuf3, WidthBytes );

		while( Size3 == 0 )
		{
			Duplicat++;
			pIn+= WidthBytes;			//  Go to next row.

			if( Rows-- == 0 )
				break;

			Size3= Compress_3( pSeedRow, pIn, pCompBuf3, WidthBytes );
		}

		if( Duplicat )
		{
			//
			//  Got some duplicate rows...
			//

			//if( BlockBytes < 3 )
			//{
			//	//
			//	//  This cannot happen because of the test at the beginning, and if the
			//	//  empty case above used the last 3 available bytes, then we cannot have
			//	//  an immediately following duplicate row. 
			//	//
			//	//  Out of room in this block.
			//	//

			//	i= sprintf( pCompBuf2, "%05d", pOut - pBlockStart );

			//	memcpy( pValueStart, pCompBuf2, i );

			//	i= sprintf( pCompBuf2, "\x1b*b%05dW", 0 );

			//	memcpy( pOut, pCompBuf2, i );

			//	pValueStart= pOut + 3;

			//	pOut+= i;

			//	pBlockStart= pOut;			//  Start of real data...

			//	BlockBytes= 32767;			//  Limit per block
			//
			//	//
			//	//  Starting a new block clears the seed row, this is here because we 
			//	//  pop up to the top anyway to re-evaluate emtpy, etc.
			//	//

			//	memset( pSeedRow, 0, WidthBytes );
			//}

			*pOut++= 5;									//  duplicat row code
			*pOut++= (unsigned char)(Duplicat >> 8);	//  upper bytes
			*pOut++= (unsigned char)Duplicat;			//  lower byte

			BlockBytes-= 3;

			Stats[5]++;
			StatSum5+= Duplicat;

			//
			//  This new row MIGHT be empty, so pop back up to the top
			//

			Rows++;	//  fix up the counter
			continue;
		}

		//
		//  Now we have a Size3 and a Size1
		//

 		Size2= Compress_2( pIn, pCompBuf2, WidthBytes );

		Size0= WidthBytes;

		//
		//  Uncompressed without trailing zeros.
		//

		while( Size0 && pIn[Size0 - 1] == 0 )
			Size0--;

		//
		//  Now output the most compressed data...
		//
		//  These are ordered in likelyhood.
		//

		CodeX= 1;
		SizeX= Size1;
		pX= pCompBuf1;

		if( Size3 < SizeX )
		{
			CodeX= 3;
			SizeX= Size3;
			pX= pCompBuf3;
		}

		if( Size2 < SizeX )
		{
			CodeX= 2;
			SizeX= Size2;
			pX= pCompBuf2;
		}

		if( Size0 < SizeX )
		{
			CodeX= 0;
			SizeX= Size0;
			pX= pIn;
		}


		//
		//  Now output...
		//

		if( BlockBytes < (SizeX + 3) )
		{
			//
			//  Out of room in this block.
			//

			i= sprintf( pSeedRow, "%05d", pOut - pBlockStart );

			memcpy( pValueStart, pSeedRow, i );

			i= sprintf( pSeedRow, "\x1b*b%05dW", 0 );

			memcpy( pOut, pSeedRow, i );

			pValueStart= pOut + 3;

			pOut+= i;

			pBlockStart= pOut;			//  Start of real data...

			BlockBytes= 32767;			//  Limit per block

			//
			//  A new block gets a new seed row, if this was a 3, then redo this row 
			//  from the top.
			//

			memset( pSeedRow, 0, WidthBytes );

			if( CodeX == 3 )
			{
				Rows++;	//  fix up the counter
				continue;
			}
		}

 		*pOut++= (unsigned char)CodeX;			//  row code
		*pOut++= (unsigned char)(SizeX >> 8);	//  upper bytes
		*pOut++= (unsigned char)SizeX;			//  lower byte
					
		BlockBytes-= (SizeX + 3);

		memcpy( pOut, pX, SizeX );

		pOut+= SizeX;

		Stats[CodeX]++;

		//
		//  Now update the seed row...
		//

		memcpy( pSeedRow, pIn, WidthBytes );

		pIn+= WidthBytes;			//  Go to next row.
	}

	//
	//  Last block size.
	//

	i= sprintf( pSeedRow, "%05d", pOut - pBlockStart );

	memcpy( pValueStart, pSeedRow, i );

	//
	//  Stick the stats in the output for now...
	//

	i= sprintf( pOut, "\x1b&p< Rows 0: %d >A", Stats[0] );

	if( i != -1 )
		pOut+= i;

	i= sprintf( pOut, "\x1b&p< Rows 1: %d >A", Stats[1] );

	if( i != -1 )
		pOut+= i;
	
	i= sprintf( pOut, "\x1b&p< Rows 2: %d >A", Stats[2] );

	if( i != -1 )
		pOut+= i;
	
	i= sprintf( pOut, "\x1b&p< Rows 3: %d >A", Stats[3] );

	if( i != -1 )
		pOut+= i;
	
	i= sprintf( pOut, "\x1b&p< Empty blocks:%d, rows:%d >A", Stats[4], StatSum4 );

	if( i != -1 )
		pOut+= i;
	
	i= sprintf( pOut, "\x1b&p< Duplicate blocks:%d, rows:%d >A", Stats[5], StatSum5 );

	if( i != -1 )
		pOut+= i;
	
	
	*pSize= pOut - pOutBuf;			//  Update with comment.

	//
	//  Free up memory used...
	//

	if( pCompBuf1 ) free( pCompBuf1 );
	if( pCompBuf2 ) free( pCompBuf2 );
	if( pCompBuf3 ) free( pCompBuf3 );
	if( pSeedRow ) free( pSeedRow );

	return TRUE;
}
