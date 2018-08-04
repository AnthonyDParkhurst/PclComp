/*
	$Header:   P:/Wtreiber/PVCS/Driver310_vs7/archives/Driver/Km/Km_Grfx/pcl_adaptive_decompress.c-arc   1.1   Jun 28 2006 13:20:18   parkhura  $
*/

#include <stdio.h>
#include <malloc.h>

#define zerostrip TRUE

#define MAX(A,B)	((A)>(B)?(A):(B))
#define MAX3(A,B,C)	((A)>(B)?MAX(A,C):MAX(B,C))
#define MIN(A,B)	((A)<(B)?(A):(B))
#define MIN3(A,B,C)	((A)<(B)?MIN(A,C):MIN(B,C))



/*-----------------------------------------------------------------------*\
 |                                                                       |
 |  Function Name: Uncompress_1                                          |
 |                                                                       |
 |  Description:                                                         |
 |                                                                       |
 |  This is the routine that handles a Mode 1 graphics                   |
 |  Mode 1 graphics is a compacted mode.                                 |
 |  The data in Mode 1 is in pairs.  The first byte is a replicate       |
 |  count and the second byte is the data.  The data byte is stored      |
 |  then replicated the replicate count.  Therefore a replicate count    |
 |  of 0 means the data byte is stored once.  The input byte count       |
 |  must be an even amount for the data to be in byte pairs.             |
 |                                                                       |
\*-----------------------------------------------------------------------*/


static void Uncompress_1( unsigned char *pOut, unsigned char *pIn, int input_bytes, int output_bytes )
{

   unsigned char
      *store_ptr,               /* Pointer to where to store the byte. */
      input_char;               /* Byte to be replicated. */

   int
      read_bytes,               /* Local copy of input_bytes. */
      write_bytes;              /* Local copy of output_bytes. */

   int
      replicate_count;          /* Number of times to replicate data. */


   /* Initialize the local variables. */

   read_bytes = input_bytes;
   write_bytes= output_bytes;
   store_ptr = pOut;
   
   /* Check for an even input count. */

   if ((read_bytes % 2) == 0)
   {
      /* Even so input data is in pairs as required. So store the data. */
   
      while ((read_bytes != 0) && (write_bytes != 0))
      {
         /* First get the replicate count and the byte to store. */

         replicate_count = *pIn++;
         input_char = *pIn++;
         read_bytes -= 2;
      
         /* Since write_bytes was not 0 there is room to store the byte. */

         *store_ptr++ = input_char;
         write_bytes--;
         
         /* Now make sure there is room for the replicated data. */

         if (replicate_count > write_bytes)
         {
            /* Too much so limit to the room available. */

            replicate_count = write_bytes;
         }
            
         /* Update the amount to be written. */

         write_bytes -= replicate_count;

         /* Then replicate it. */

         while (replicate_count != 0)
         {
            /* Store the byte the decrement the count. */

            *store_ptr++ = input_char;
         
            replicate_count--;
         }
      }

   }
}


/*-----------------------------------------------------------------------*\
 |                                                                       |
 |  Function Name: Uncompress_2                                          |
 |                                                                       |
 |  Description:                                                         |
 |                                                                       |
 |  This is the routine that handles a Mode 2 graphics                   |
 |  Mode 2 graphics is a compacted mode.                                 |
 |  The data in Mode 2 is of one of two types.  The first type is a      |
 |  class type and the second type is a data type.  The class type is    |
 |  a single byte which is a combination of replicate count and a sub    |
 |  mode.  There are two sub modes within mode 2, sub mode 0 and sub     |
 |  mode 1.  These sub modes are flagged by the MSB of the class type    |
 |  byte.  If the MSB = 0 then the replicate count is the value of the   |
 |  class type byte.  In sub mode 0 the replicate count ranges from 1    |
 |  to 127.  In sub mode 0 the next byte and then the replicate count    |
 |  of bytes are of the data type and stored.  If the MSB = 1 then the   |
 |  sub mode is 1 and the replicate count is the negative value of the   |
 |  class type.  In sub mode 1 the replicate count is stored in 2s       |
 |  compliment form and ranges from -1 to -127.  In sub mode 1 the       |
 |  next byte is of the data type and is stored.  That data byte is      |
 |  then replicated and stored the replicate count.  If the class type   |
 |  byte is 128 then there is no data type byte.                         |
 |                                                                       |
\*-----------------------------------------------------------------------*/


static void Uncompress_2( unsigned char *pOut, unsigned char *pIn, int input_bytes, int output_bytes )
{

   unsigned char
      *store_ptr,               /* Pointer to where to store the byte. */
      input_char,               /* Byte to be replicated. */
      sub_mode;                 /* Flag if sub mode is 0 or 1. */

   int
      read_bytes,               /* Local copy of input_bytes. */
      write_bytes;              /* Local copy of output_bytes. */

   int
      replicate_count;          /* Number of times to replicate data. */


   /* Initialize the local variables. */

   read_bytes = input_bytes;
   write_bytes = output_bytes;
   store_ptr = pOut;
   
   while ((read_bytes > 1) && (write_bytes != 0))
   {
      /* First get the class type byte and the first data type byte. */

      replicate_count = *pIn++;

      /* First check that this not an ignore class type. */

      if (replicate_count != 128)
      {
         /* Not ignore so get the data class byte. */

         input_char = *pIn++;
         read_bytes -= 2;
         
        /* Since write_bytes wasn't 0 there is room to store the byte. */

         *store_ptr++ = input_char;
         write_bytes--;

         /* Determine the sub mode. */
   
         if (replicate_count > 128)
         {
            /* Sub mode 1. */
   
            sub_mode = 1;
            /* replicate count was unsigned char */
            replicate_count = 256 - replicate_count;
         }
         else
         {
            /* Sub mode 0. */
   
            sub_mode = 0;

            /* See if there is enoungh input left for the data byte count. */

            if (replicate_count > read_bytes)
            {
               /* Too many data bytes so limit to the input left. */

               replicate_count = read_bytes;
            }
         }
            
         /* Now make sure there is room for the replicated data. */
   
         if (replicate_count > write_bytes)
         {
            /* Too much so limit to the room available. */
   
            replicate_count = write_bytes;
         }
               
         /* Update the amount to be written. */
   
         write_bytes -= replicate_count;
   
         /* Then replicate it. */
   
         if (sub_mode == 0)
         {
            /* Sub mode 0 so get the replicate count of data bytes. */
   
            memcpy( store_ptr, pIn, replicate_count );

            read_bytes -= replicate_count;

			pIn+= replicate_count;
            
            /* Find the last byte stored. */
   
            store_ptr += replicate_count;
         }
         else
         {
            /* Sub mode 1 so just duplicate the original byte. */
   
            while (replicate_count != 0)
            {
               /* Store the byte the decrement the count. */
   
               *store_ptr++ = input_char;
            
               replicate_count--;
            }
         }
      }
      else
      {
         /* Ignore class so don't get the data class byte. */

         read_bytes--;
      }
   }

   return;
}


/*-----------------------------------------------------------------------*\
 |                                                                       |
 |  Function Name: Uncompress_3                                          |
 |                                                                       |
 |  Description:                                                         |
 |                                                                       |
 |  This is the routine that handles a Mode 3 graphics                   |
 |  Mode 3 graphics is a compacted mode.                                 |
 |  Mode 3 data is a difference from one row to the next.  In order to   |
 |  work, each row must be saved to be a seed for the next.  This        |
 |  mode is used in conjuction with other compaction modes when the      |
 |  data remains fairly constant between pairs of rows.                  |
 |  The data is in the form:                                             |
 |  <command byte>[<optional offset bytes>]<1 to 8 replacement bytes>    |
 |  The command byte is in the form:                                     |
 |    Bits 5-7: Number of bytes to replace (1 - 8)                       |
 |    Bits 0-4: Relative offset from last byte.                          |
 |       (If the offset is 31, then add the following bytes for offset   |
 |       until an offset byte of less then 255 (but inclusive)           |
 |                                                                       |
\*-----------------------------------------------------------------------*/


static void Uncompress_3( unsigned char *pOut, unsigned char *pIn, unsigned int input_bytes, unsigned int output_bytes )
{

	unsigned char
	*store_ptr;             /* Pointer to where to store the byte. */

	unsigned int
	read_bytes,             /* Local copy of input_bytes. */
	write_bytes;            /* Local copy of output_bytes. */

	unsigned int
	replace,		/* number of bytes to replace, 1-8 */
	offset;			/* relative offset */

	unsigned char	command;


	/* Initialize the local variables. */

	read_bytes = input_bytes;
	write_bytes = output_bytes;
	store_ptr = pOut;

	/* 
	**  read_bytes has to be at least 2 to be valid
	*/

	while ( read_bytes > 1 && write_bytes > 0 )
	{

		/* start by getting the command byte */

		command = *pIn++;

			replace = (command >> 5) + 1;
			offset = command & 0x1f;

		read_bytes--;

		/*
		**  Sometimes offsets go past the end.  If so, bail.
		*/

		if ( offset >= write_bytes )
			break;

		write_bytes -= offset;
		store_ptr += offset;

		/*
		**  If the first offset value is 31, then we must
		**  get more offsets until we encounter a byte value
		**  less than 255 (even if it's 0).
		*/

		if ( offset == 31 )		/* get more offsets */

			do{
				offset = *pIn++;

				read_bytes--;

				/*
				**  Check for pre-mature finish
				*/

				if ( read_bytes == 0 )
					return;

				/*
				**  Check for excessive offset.
				*/

				if ( offset >= write_bytes )
				{
					/*
					**  Resetting write_bytes is needed
					**  to get out of outer loop so
					**  that the call to Discard_Block()
					**  is made at the end.
					*/

					write_bytes = 0;
					break;
				}

				write_bytes -= offset;
				store_ptr += offset;

			} while (offset == 255);	/* 255 = keep going */

		/* now do the byte replacement */

		while ( replace-- && write_bytes > 0 && read_bytes > 0 )
		{
			*store_ptr++ = *pIn++;
			read_bytes--;
			write_bytes--;
		}
	}
   
	return;
}



//
//  Decompress input using PCL adaptive compression mode 5.
//  
//  Returns the actual number of rows from input data, might be less than valued passed in.
//

int DecompressPCLAdapt( 
	unsigned char *pOutBuf, 
	unsigned char *pInData,
	const int WidthBytes,		//  *BYTES* per row in output buffer.
	int Rows,					//  Number of rows in output buffer.
	int InBytes )				//  Number of bytes of compressed data
{

	unsigned char *pOut= pOutBuf, *pIn= pInData;
	
	int	i,j,k;

	unsigned char *pSeedRow;

	unsigned char Code;

	long Count;

	int	ActualRows= 0;


	//
	//  Need buffer to decompress into
	//

	pSeedRow= malloc( WidthBytes );

	if( pSeedRow == NULL )
	{
		//
		//  Memory failure, bail.
		//

		return ActualRows;
	}

	//
	//  Always start with an empty seed row...
	//

	memset( pSeedRow, 0, WidthBytes );	// Clear seed row

	while( Rows && InBytes >= 3 )	//  Need at least 3 input bytes per operation
	{
		//
		//  Get next code and count
		//

		Code= *pIn++;

		Count= *pIn++;

		Count <<= 8;

		Count += *pIn++;

		InBytes-= 3;

		switch( Code )
		{
		case 0:
			//
			//  Uncompressed, almost never happens.
			//

			if( Count > InBytes )
			{
				//
				//  This is a failure of some sort...
				//

				Count= InBytes;
			}

			if( Count > WidthBytes )
			{
				//
				//  This is a failure of some sort...
				//

				Count= WidthBytes;
			}

			memset( pSeedRow, 0, WidthBytes );		//  Start with a cleared seed row

			memcpy( pSeedRow, pIn, Count );			//  Copy just the first count bytes over.

			memcpy( pOut, pSeedRow, WidthBytes );	//  Copy whole row over...

			pIn+= Count;
			InBytes-= Count;
			pOut+= WidthBytes;
			Rows--;
			ActualRows++;

			break;

		case 1:
			//
			//  Mode 1.
			//

			if( Count > InBytes )
			{
				//
				//  This is a failure of some sort...
				//

				Count= InBytes;
			}

			memset( pSeedRow, 0, WidthBytes );		//  Start with a cleared seed row

			Uncompress_1( pSeedRow, pIn, Count, WidthBytes );	

			memcpy( pOut, pSeedRow, WidthBytes );	//  Copy whole row over...

			pIn+= Count;
			InBytes-= Count;
			pOut+= WidthBytes;
			Rows--;
 			ActualRows++;

			break;

		case 2:
			//
			//  Mode 2.
			//

			if( Count > InBytes )
			{
				//
				//  This is a failure of some sort...
				//

				Count= InBytes;
			}

			memset( pSeedRow, 0, WidthBytes );		//  Start with a cleared seed row

			Uncompress_2( pSeedRow, pIn, Count, WidthBytes );	

			memcpy( pOut, pSeedRow, WidthBytes );	//  Copy whole row over...

			pIn+= Count;
			InBytes-= Count;
			pOut+= WidthBytes;
			Rows--;
 			ActualRows++;

			break;

		case 3:
			//
			//  Mode 3.
			//

			if( Count > InBytes )
			{
				//
				//  This is a failure of some sort...
				//

				Count= InBytes;
			}

			//
			//  Do NOT clear the seed row for this one...
			//

			Uncompress_3( pSeedRow, pIn, Count, WidthBytes );	

			memcpy( pOut, pSeedRow, WidthBytes );	//  Copy whole row over...

			pIn+= Count;
			InBytes-= Count;
			pOut+= WidthBytes;
			Rows--;
 			ActualRows++;

			break;

		case 4:
			//
			//  Empty rows...
			//

			if( Count > Rows )
			{
				//
				//  This is a failure of some sort...
				//

				Count= Rows;
			}

			memset( pOut, 0, Count * WidthBytes );

			pOut+= Count * WidthBytes;

			Rows-= Count;
			ActualRows+= Count;

			memset( pSeedRow, 0, WidthBytes );	// Clear seed row

			break;

		case 5:
			//
			//  Duplicate rows...
			//

			if( Count > Rows )
			{
				//
				//  This is a failure of some sort...
				//

				Count= Rows;
			}

			for( i= Count; i > 0; i-- )
			{
				memcpy( pOut, pSeedRow, WidthBytes );

				pOut+= WidthBytes;
 			}

			Rows-= Count;
			ActualRows+= Count;

			break;

		case 27:
			//
			//  New block of data???
			//

			pIn+= 6;		//  skip it
			InBytes-= 6;

			memset( pSeedRow, 0, WidthBytes );	// Clear seed row

			break;

		default:
			break;

		}

	}

	//
	//  Return used memory.
	//

	if( pSeedRow ) free( pSeedRow );

	return ActualRows;
}
