/*
 * Copyright © 2006 Anthony Parkhurst
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders and/or authors
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  The copyright holders
 * and/or authors make no representations about the suitability of this
 * software for any purpose.  It is provided "as is" without express or
 * implied warranty.
 *
 * THE COPYRIGHT HOLDERS AND/OR AUTHORS DISCLAIM ALL WARRANTIES WITH REGARD
 * TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS, IN NO EVENT SHALL THE COPYRIGHT HOLDERS AND/OR AUTHORS BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


//
//  Pclcomp -- PCL compression filter.
//
//  If you have any problems or errors to report, please send them to me:
//
//  Anthony Parkhurst  
// 
//  Email address:  Anthony.Parkhurst@gmail.com
//
//  Please send a copy of the file that is causing the problem, and the
//  version of pclcomp you are using.
//
//  All suggestions and requests are welcome.
//
//  However, my ability to support new things with this program has
//  been greatly limited over the years.
//

//
//  This is an old C program that was released long ago on the net to help driver
//  writers by providing compression code.
//
//  Here is a newer (but still old) version.
//
//  I've written newer and better pcl parsers, so perhaps I will update this program
//  one day.
//

 
static const char copyr[]= "Copyright © 2006 Anthony Parkhurst";

static const char author[]="Anthony Parkhurst (Anthony.Parkhurst@gmail.com)";

static const char rcs_id[]="$Header:   P:/Wtreiber/PVCS/Projects/archives/pclcomp/pclcomp.c-arc   1.1   Jun 26 2006 13:05:18   parkhura  $";

static const char rev_id[]="Version: 1.63";
 

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef MSDOS
#include <fcntl.h>
#endif


#define Get_Character() getchar()

#define MIN(x,y)	( ((x) < (y)) ? (x) : (y) )

#define TRUE 1
#define FALSE 0

#define ESC	27

#define DEFAULT 2400		/* default width in pixels (multiple of 8) */

#define MAXMODES  10
#define MAXPLANES 8
#define MAXBYTES 60000		/* now mostly meaningless, just a big number */

unsigned char	*seed_row[MAXPLANES];
unsigned char	*new_row;
unsigned char	*out_row[MAXMODES];
unsigned int	out_size[MAXMODES];

char	memflag = FALSE;	/* set when memory has been allocated */


char	mode0=FALSE,
	mode1=FALSE,
	mode2=FALSE,
	mode3=FALSE,
	mode9=FALSE;

unsigned char	num_planes=1;
unsigned char	curr_plane=0;

char	imaging = FALSE;		/* not imaging, so no lockout */

char	verbose = FALSE;

unsigned char	inmode = 0;		/* input compression mode */
unsigned char	outmode = 0;		/* output compression mode */

unsigned int	rasterwidth=DEFAULT/8;	/* width of picture, in bytes */
unsigned int	rpix = DEFAULT;		/* width of picture, in pixels */

unsigned char	zerostrip= TRUE;	/* strip trailing zeros */

unsigned int	inuse[10]={0,0,0,0,0,0,0,0,0,0}, 
		outuse[10] = {0,0,0,0,0,0,0,0,0,0};

char	widthwarning = FALSE;	/* for trucation warning */


struct {			/* this will hold the data for the  */
     unsigned char model;	/* configuring of image processing   */
     unsigned char pix_mode;
     unsigned char inx_bits;
     unsigned char red;
     unsigned char green;
     unsigned char blue;
     short wr;
     short wg;
     short wb;
     short br;
     short bg;
     short bb; 
} imdata;

extern	unsigned char *malloc();

char	*filein = NULL, *fileout = NULL;

/*
**  These variables are for the new parser.
**  The new parser handles more sequences, and also deals with combined
**  escape sequences better.
*/

int	parameter;
int	group_char;
int	terminator;
int	old_terminator=0;
int	value;
float	fvalue;			/* fractional value */
int	scanf_count;

/* indicates whether we're in the middle of escape sequence interpretation */
char	in_sequence = FALSE;

/* 
	indicates whether we just want to pass this sequence through without
	performing any special processing
*/
char	pass_seq;
char	plus_sign;		/* for relative values */


/* dummy buffer */
char buf[BUFSIZ];

/*
**  If the printer is a DeskJet, then we must handle <esc>*rB differently
**  Option '-d' will turn on this mode.
**
**  Note:  This is only for DeskJet, DeskJet+, DeskJet 300 and 500 series.
**         Do NOT use with DeskJets with numbers > 1000 (i.e. 1200C).
*/

char	deskjet = FALSE;


/*
**  Many drivers it seems put <esc>*rB<esc>*rA between each and every row
**  of data.  This defeats compression mode 3 on a DeskJet, and also
**  makes the PaintJet (not XL) quite slow.  This next flag "-s" on the
**  command line, will attempt to do a reasonable job of stripping
**  out the excess commands.
**
**  The in_graphics flag will be used to strip unwanted control chars from
**  the file.  It will also be used to spot implicit end of graphics.
*/

char	strip_seq = FALSE;
char	in_graphics = FALSE;


/*
**  Just for certain special cases, it would be nice to append an <esc>E reset
**  to the end of the job.  Specify with "-r".
*/

char	reset_seq = FALSE;


char	*progname;		/* to hold the program name for verbose */

/*
**  Even though the horizontal offset command <esc>*b#X is obsolete, many
**  drivers still use it, and it causes some interesting problems with
**  mode 3 compression, so pclcomp needs to deal with it in some hopefully
**  intelligent fashion, and they will get stripped if -x is used.
*/

int	horiz_offset = 0;

char	strip_offsets = FALSE;


static float	Get_Frac();	/* instead of scanf */

/*
**  seed_row_source supports the <esc>*b#S command used by DeskJet drivers.
**  It basically controls which seed plane is used as input to compression
**  methods 3 and 9.  Since pclcomp did not support method 9 for output,
**  we will strip these sequences (none will go into the output
**  file, unless the -p option is given).
*/

int	seed_row_source = 0;
char	pass_seed_row_source = FALSE;

/*
**  Now that pclcomp does method 9, we will attempt to generate seed
**  row source commands.  Note, the only real use for seed row source
**  commands is to colapse monocrome data residing in a color (cmy)
**  raster.  Although we could put a lot of effort in compressing data
**  in referernce to all the previous convenient planes, we will only
**  use it to colapse black data (into essentially one plane of data).
**
**  Note:  It would not be a good idea to generate seed row source
**  commands and pass through seed row source commands (-p option)
**  at the same time.
**
**  "srs" == "seed row source"
*
**  Note:  This flag only gets true if the "deskjet" option is specified
**  and either modes 3 or 9 are used.
*/

int	out_srs = 0;
char	generate_srs = FALSE;


/*
**  Configure Raster Data (CRD).  Now heavily used by DeskJet drivers.
**  Pclcomp will attempt to understand a common and simple version used
**  for the 660/690 series drivers.  (It is only important to determine
**  the number of planes, and the size (which would be greater than
**  expected in the usual case).
**
**  Only format 2 is supported.
*/

typedef struct crd_comp_str_tag
{
    unsigned short  horiz_res;
    unsigned short  vert_res;
    unsigned short  num_levels;

}   crd_comp_type;


struct crd_str_tag
{
    unsigned char   format;
    unsigned char   num_components;
    crd_comp_type   component[8];	/* 4 is max for format 2 */

}   crd_info;

int crd_max_horiz_res = 0;



/*
******************************************************************************
**
**				Main program.
**
******************************************************************************
*/

main(argc, argv)
int argc;
char *argv[];
{
  int	c,j;
  extern char *optarg;
  extern int   optind;

	progname = argv[0];

	/*
	**  Just in case a user typed "pclcomp" to get a usage message:
	*/

	if ( argc == 1 && isatty(0) )
	{
		fprintf(stderr, 
	"Usage: %s [-01239dprsvxz] [-n###] [infile [outfile]]\n",argv[0]);
		exit(-1);
	}

#ifdef MSDOS
	setmode(fileno(stdin), O_BINARY);	/* Place stdin and stdout in */
	setmode(fileno(stdout), O_BINARY);	/* binary mode. (Mike Slomin)*/
#endif

	/* parse up the args here */

  while ((c = getopt(argc, argv, "01239di:n:o:prsvxz")) != EOF )
	switch(c){
	case '0':
			mode0 = TRUE;
			break;
	case '1':
			mode1 = TRUE;
			break;
	case '2':
			mode2 = TRUE;
			break;
	case '3':
			mode3 = TRUE;
			break;
	case '9':
			mode9 = TRUE;
			break;
	case 'd':
			/*
			**  This option is only for original DJ, DJ+ or ones
			**  with numbers below 1000, (i.e. 300 or 500 series)
			*/
			deskjet = TRUE;
			break;
	case 'p':
			pass_seed_row_source = TRUE;
			break;
	case 'r':
			reset_seq = TRUE;
			break;
	case 's':
			strip_seq = TRUE;
			break;
	case 'v':
			verbose = TRUE;
			break;
	case 'x':
			strip_offsets = TRUE;
			break;
	case 'z':
			zerostrip = FALSE;
			break;
	case 'n':
			rpix = atoi(optarg);	/* new default */
			rasterwidth = (rpix + 7) / 8;	/* round up */
			break;

	case 'i':
			filein = optarg;
			break;
	case 'o':
			fileout = optarg;
			break;

	case '?':
	default:
			fprintf(stderr, 
			"Usage: %s [-01239dprsvxz] [-n###] [infile [outfile]]\n",
				argv[0]);
			exit(-1);
	};

	if ( verbose )
	{
		fprintf(stderr, "%s: %s\n", argv[0], rev_id);
	}


	/*
	**  If no modes were specified, then select some.
	*/

	if ( !(mode0||mode1||mode2||mode3||mode9) )	/* any modes on? */
		mode0 = mode2 = mode3 = TRUE;		/* 3 modes by default */

	/*
	**  Check to see if any file args were given on the command line.
	**  Ones that were not preceded by a "-i" or "-o".
	*/

	if ( filein == NULL && optind < argc && argv[optind] != NULL )
		filein = argv[optind++];

	if ( fileout == NULL && optind < argc && argv[optind] != NULL )
		fileout = argv[optind++];

	/*
	**  Now open files for stdin and stdout if provided by the user.
	*/

	if ( filein != NULL )		/* new input file */

		if ( freopen( filein, "rb", stdin ) == NULL )
		{
			fprintf(stderr,"Unable to open %s for input.\n",filein);
			exit(-42);
		}

	if ( fileout != NULL )		/* new output file */

		if ( freopen( fileout, "wb", stdout ) == NULL )
		{
			fprintf(stderr, "Unable to open %s for output.\n",
				fileout);
			exit(-43);
		}


	/*
	**  Determine if we should be generating seed row source commands
	**  using the -d, -9, -p or perhaps -3 options.
	**
	**  But only if "deskjet" is specified.
	*/

	if ( !pass_seed_row_source && (deskjet && (mode3||mode9)) )

		generate_srs = TRUE;

	/*
	**
	**		This is the pcl input parsing loop.
	**
	*/

	while( ( c = getchar() ) != EOF )
	{

		/*  Ignore all chars until an escape char  */

		/*
		**  If we are in graphics, toss it if strip_seq is set.
		*/

		if ( c != ESC )
		{
			/*
			**  Pass thru the character if not stripping
			**  or not in graphics.
			*/

			/* if ( !strip_seq || !in_graphics ) */
				putchar(c);	/* pass it thru */
			
			/*
			**  If we are in graphics and we are not stripping,
			**  then this character implies an end raster graphics,
			**  and the seed rows need to be zeroed.
			*/

			if ( in_graphics /* && !strip_seq */ )
			{
				zero_seeds();
				in_graphics = FALSE;	/* fell out */

				if ( strip_seq )
					outmode = 4;
			}

			continue;	/* pop to the top of the loop */
		}

		/*
		**  Now we have an escape sequence, get the parameter char.
		*/

		parameter = getchar();

		if ( parameter == EOF )		/* oops */
		{
			putchar ( ESC );
			fprintf(stderr, "Warning:  File ended with <esc>.\n");
			break;			/* unexpected end of input */
		}

		/*
		**  Now check to see if it is a two character sequence.
		*/

		if ( parameter >= '0' && parameter <= '~' )
		{
			putchar ( ESC );
			putchar ( parameter );		/* pass it thru */

			/*
			**  If the second character is an E, then we
			**  and the printer do a reset.
			*/

			if ( parameter == 'E' )
			{
				free_mem();
				curr_plane = 0;
				num_planes = 1;
				imaging = FALSE;
				inmode = 0;
				outmode = 0;
				in_graphics = FALSE;
				crd_max_horiz_res = 0;

				/* can't do this if user gave value with -n.
				rasterwidth = DEFAULT/8;
				rpix = DEFAULT;
				*/
			}

			continue;		/* return to the top */
		}

		/*
		**  Now check to make sure that the parameter character is
		**  within range.
		*/

		if ( parameter < '!' || parameter > '/' )
		{
			putchar ( ESC );
			putchar ( parameter );

			fprintf(stderr, "Warning:  Invalid escape sequence.\n");

			continue;
		}

		/*
		**  We are only interested in certain parameters, so pass
		**  the rest of the sequences.
		*/

		/*
		**  For the moment, we are only interested in '*' (graphics)
		**  '(' and ')' (downloads).  Although we do not do anything
		**  with downloads, we need to pass the binary data thru
		**  untouched.
		**  Oops, '&' is handled too.
		*/

		if ( parameter != '*' && parameter != '(' 
			&& parameter != ')' && parameter != '&' )
		{

			/*
			**  If the "stripper" is active, we need to suspend
			**  it till graphics are re-started.
			*/

			if ( strip_seq && !in_graphics )
			{
				curr_plane = 0;
				free_mem();		/* force re-start */
			}

			/*
			**  Pass thru the sequence intact.
			*/

			putchar ( ESC );
			putchar ( parameter );
			Flush_To_Term();		/* flush rest of seq. */
			continue;
		}


		/*
		**  Parameter character is in range, look for a valid group char
		*/

		group_char = getchar();

		if ( group_char == EOF )	/* oops, ran out of input */
		{
			putchar ( ESC );
			putchar ( parameter );

			fprintf(stderr, "Warning:  Incomplete escape sequence.\n");
			break;
		}

		/*
		**  See if in proper range.  If it isn't, it is not an error
		**  because the group character is optional for some sequences.
		**  For the moment, we are not interested in those sequences,
		**  so pass them thru.
		*/

		if ( group_char < '`' || group_char > '~' )
		{

			/*
			**  If the "stripper" is active, we need to suspend
			**  it till graphics are re-started.
			*/

			if ( strip_seq && !in_graphics )
			{
				curr_plane = 0;
				free_mem();		/* force re-start */
			}

			/*
			**  Pass thru the sequence intact.
			*/

			putchar ( ESC );
			putchar ( parameter );
			putchar ( group_char );
			if ( group_char < '@' || group_char > '^' )
				Flush_To_Term();	/* pass rest of seq. */
			continue;
		}

		/*
		**  Now we have a valid group character, decide if we want
		**  to deal with this escape sequence.
		**
		**  Sequences we want do deal with include:
		**
		**    <esc>*r	** graphics
		**    <esc>*b	** graphics
		**    <esc>*v	** graphics
		**    <esc>*g	** CRD
		**
		**  Sequences we must pass thru binary data:
		**
		**    <esc>*c	** pattern
		**    <esc>*o	** specials (for DeskJet)
		**    <esc>*m	** download dither
		**    <esc>*t	** obsolete
		**    <esc>(f	** download char set
		**    <esc>(s	** download char
		**    <esc>)s	** download font
		**    <esc>&a	** logical page
		**    <esc>&b	** AppleTalk stuff
		**    <esc>&l	** obsolete
		**
		*/

		if (  ( parameter == '*'
			&& group_char != 'r' && group_char != 'b' 
			&& group_char != 'v' && group_char != 'c' 
			&& group_char != 't' && group_char != 'm' 
			&& group_char != 'g'
			&& group_char != 'o' )
		   || ( parameter == '&'
			&& group_char != 'a' && group_char != 'l' 
			&& group_char != 'b' )
		   || ( parameter == '(' 
			&& group_char != 'f' && group_char != 's' )
		   || ( parameter == ')' && group_char != 's' ) )
		{
			/*
			**  Definately not interested in the sequence.
			*/

			/*
			**  If the "stripper" is active, we need to suspend
			**  it till graphics are re-started.
			*/

			if ( strip_seq && !in_graphics )
			{
				curr_plane = 0;
				free_mem();		/* force re-start */
			}

			/*
			**  Pass thru the sequence intact.
			*/

			putchar ( ESC );
			putchar ( parameter );
			putchar ( group_char );
			Flush_To_Term();
			continue;
		}


		/*
		**  If the sequence is <esc>&a#H, it will have gotten past
		**  the above, but we need to suspend the "stripper" if
		**  it is active, because the CAP is getting moved.
		**
		**  The <esc>*p#X/Y sequences will have been filtered
		**  thru just above (<esc>*p is not a needed group).
		*/

		if ( strip_seq && parameter != '*' && !in_graphics )
		{
			curr_plane = 0;
			free_mem();		/* force re-start */
		}


		/*
		**  Now set up a pass thru flag so we can ignore the entire
		**  sequences of some of these.
		*/

		if ( parameter != '*' )
			pass_seq = TRUE;

		else if ( group_char == 'c' || group_char == 't' 
		       || group_char == 'm' || group_char == 'o' )

			pass_seq = TRUE;
		else
			pass_seq = FALSE;


		/*
		**  Now we have a sequence that we are definately interested in.
		**
		**  Get the value field and terminator, and loop until final
		**  terminator is found.
		*/

		do
		{
			/* first see if the value has a plus sign */

			scanf_count = scanf(" + %d ", &value );

			if ( scanf_count == 1 )

				plus_sign = TRUE;
			else
			{
				plus_sign = FALSE;

				scanf_count = scanf(" %d ", &value );

				if ( scanf_count == 0 )
					value = 0;		/* by default */
			}

			/*
			**  Fixed problem with trailing spaces in above scanf.
			*/

			terminator = getchar();

			/*
			**  Check for a fractional component.
			*/

			fvalue = 0.0;

			if ( terminator == '.' )
			{
				fvalue = Get_Frac();

				/*
				**  Now get real terminator.
				*/

				terminator = getchar();
			}


			if ( terminator == EOF )	/* barf */
			{
				fprintf(stderr, 
				"Warning:  Incomplete sequence at EOF.\n");
				break;
			}

			/*
			**  If the pass_seq flag is set, then just pass
			**  it thru to stdout until a 'W' is found.
			*/

			if ( pass_seq )
			{
				/*
				**  If not in sequence, then we output esc
				**  otherwise, output the saved terminator.
				*/

				if ( !in_sequence )
				{
					in_sequence = TRUE;
					putchar ( ESC );
					putchar ( parameter );
					putchar ( group_char );
				} else
				{
					putchar ( old_terminator );
				}

				/* now pass the value */

				if ( plus_sign )
					putchar('+');

				/*
				**  See if there was a non-zero fraction.
				*/

				if ( fvalue != 0.0 )
				{
					if ( value < 0 )
					{
						putchar('-');
						value = -value;
					}

					fvalue += value;

					printf("%g", fvalue);

				} else if ( scanf_count )
					printf("%0d", value);
				
				/*
				**  We save the terminator, because we may
				**  need to change it to upper case.
				*/

				old_terminator = terminator;

				/* if binary data, pass it thru */

				if ( terminator == 'W' )	/* aha */
				{
					putchar ( terminator );
					in_sequence = FALSE;	/* terminates */
					Flush_Bytes ( value );	/* pass data */
				}

				continue;
			}

			/*
			**  Ok, this is a sequence we want to pay attention to.
			**
			**  Do_Graphics returns TRUE if we need to pass seq.
			**
			**  Note:  Do_Graphics modifies the parser vars such
			**         as in_sequence.  This is because it may
			**         have to output stuff directly.
			*/

			if ( Do_Graphics ( group_char, value, terminator ) )
			{
				/*
				**  If not in sequence, then we output esc
				**  otherwise, output the saved terminator.
				*/

				if ( !in_sequence )
				{
					in_sequence = TRUE;
					putchar ( ESC );
					putchar ( parameter );
					putchar ( group_char );
				} else
				{
					if ( old_terminator )
					    putchar ( old_terminator );
				}

				/* now pass the value */

				if ( plus_sign )
					putchar('+');

				/*
				**  See if there was a non-zero fraction.
				*/

				if ( fvalue != 0.0 )
				{
					if ( value < 0 )
					{
						putchar('-');
						value = -value;
					}

					fvalue += value;

					printf("%g", fvalue);

				} else if ( scanf_count )
					printf("%0d", value);

				/*
				**  We save the terminator, because we may
				**  need to change it to upper case.
				*/

				old_terminator = terminator;
			}

		} while ( terminator >= '`' && terminator <= '~' );

		/*
		** The oppsite test (above) may be more appropriate.  That is, 
		** !(terminator >= '@' && terminator <= '^').
		*/
		
		/*
		**  If we were in a sequence, then we must terminate it.
		**  If it was lower case, then it must be uppered.
		*/

		if ( in_sequence )
		{
			putchar ( terminator & 0xdf );		/* a ==> A */
			in_sequence = FALSE;
		}
	}
	

	/*
	**  If the user wants a reset, give him one.
	*/

	if ( reset_seq )
	{
		putchar ( ESC );
		putchar ( 'E' );
	}


	/*
	**  Finished up, so print stats and close output file.
	*/



	if ( verbose )
	{
		long	inpos, outpos;

		for(j = 0; j < 4; j++)
			fprintf(stderr,"Rows in mode %1d: %d\n", j, inuse[j]);

		if ( inuse[9] )
			fprintf(stderr,"Rows in mode %1d: %d\n", 9, inuse[9]);

		for(j = 0; j < 4; j++)
			fprintf(stderr,"Rows out mode %1d: %d\n", j, outuse[j]);

		if ( outuse[9] )
			fprintf(stderr,"Rows out mode %1d: %d\n", 9, outuse[9]);

		inpos = ftell(stdin);
		outpos = ftell(stdout);

		/*
		**  If the input or output is a pipe, then ftell returns a
		**  -1.  Don't bother telling the user about it.
		*/

		if ( inpos > 0 && outpos > 0 )
		{

			fprintf(stderr, "Input size:  %ld bytes\n", inpos );
			fprintf(stderr, "Output size: %ld bytes\n", outpos );

			if ( inpos > outpos )
				fprintf(stderr, "Compression: %ld%%\n",
					(long)(99L - 100L*outpos/inpos));
			else if ( outpos > inpos )
				fprintf(stderr, "Expansion: %ld%%\n",
					(long)(100L*outpos/inpos - 100L));
			else
				fprintf(stderr, "No compression.\n");
		}
	}

	fclose(stdout);

	exit(0);
}


/*
**  Do_Graphics() takes the graphics escape sequence and performs the
**  necessary functions.
**  TRUE is returned if the escape sequence needs to be passed to the output.
*/

/* perform necessary functions on graphics escape sequence */

int	Do_Graphics( group, num, terminator )
int	group, num, terminator;
{
	/*  first look at vW  */

	if ( group == 'v' )

		if ( terminator != 'W' )
			
			return ( TRUE );	/* pass it thru */
		else
		{
			if ( !in_sequence )
			{
				putchar ( ESC );
				putchar ( parameter );
				putchar ( group );
			} else
				putchar ( old_terminator );

			in_sequence = FALSE;		/* terminating */

			printf("%0d", num);
			putchar ( terminator );

			free_mem();	/* reset memory */

			imaging++;

			fread(&imdata, MIN(num, 18), 1, stdin);
			fwrite(&imdata, MIN(num, 18), 1, stdout);

			num -= MIN(num, 18);

			/* copy rest of unknown data */

			if ( num > 0 )
				Flush_Bytes(num);


			switch(imdata.pix_mode){
				case 0x00:
					rasterwidth = (rpix + 7)/8;
					num_planes = imdata.inx_bits;
					break;
				case 0x01:
					rasterwidth = rpix*imdata.inx_bits/8;
					break;
				case 0x02:
					rasterwidth = (rpix + 7)/8;
					num_planes =imdata.red + imdata.green +
					            imdata.blue;
					break;
				case 0x03:
					rasterwidth = (imdata.red +
					               imdata.green +
					               imdata.blue)*rpix/8;
					break;
			}

			return ( FALSE );
		}


	/*
	**  Now CRD *gW
	*/

	if ( group == 'g' )

		if ( terminator != 'W' )
			
			return ( TRUE );	/* pass it thru */
		else
		{
			if ( !in_sequence )
			{
				putchar ( ESC );
				putchar ( parameter );
				putchar ( group );
			} else
				putchar ( old_terminator );

			in_sequence = FALSE;		/* terminating */

			printf("%0d", num);
			putchar ( terminator );

			free_mem();	/* reset memory */

			fread(&crd_info, MIN(num, sizeof(crd_info) ), 1, stdin);
			fwrite(&crd_info, MIN(num, sizeof(crd_info) ),1,stdout);

			num -= MIN(num, sizeof(crd_info) );

			/* copy rest of unknown data */

			if ( num > 0 )
				Flush_Bytes(num);


			/*
			**  Only format 2 is supported.
			**
			**  Note:  This code ASSUMES that enough bytes 
			**  were sent (that is, it is a valid sequence).
			**  No checking is done to ensure this.
			*/

			if ( crd_info.format == 2 )
			{
			    int	index;

			    fprintf( stderr, "CRD format 2:\n" );
			    fprintf( stderr, "CRD number of components: %d\n",
				crd_info.num_components );


			    num_planes = 0;

			    for ( index = 0; index < crd_info.num_components;
				index++ )
			    {
				fprintf( stderr, "CRD comp %d:\n", index + 1 );
				fprintf( stderr, "  horiz_res: %d\n",
				    crd_info.component[index].horiz_res );
				fprintf( stderr, "  vert_res: %d\n",
				    crd_info.component[index].vert_res );
				fprintf( stderr, "  num_levels: %d\n",
				    crd_info.component[index].num_levels );

				/*
				**  At least one plane per component.
				*/

				num_planes++;

				/*
				**  Another plane for 600 dpi vertical.
				*/

				if ( crd_info.component[index].vert_res > 300 )
				    num_planes++;

				/*
				**  What about multiple levels?
				*/

				switch( crd_info.component[index].num_levels )
				{

				    case 8:
				    case 7:
				    case 6:
				    case 5:
					num_planes++;
				    case 4:
				    case 3:
					num_planes++;
				    case 2:
					break;

				    case 1:
				    case 0:
				    default:
					fprintf( stderr, "Unsupported CRD.\n");
					break;
				}
				    

				/*
				**  Find max res for rasterwidth.
				*/

				if ( crd_info.component[index].horiz_res
				    > crd_max_horiz_res )
				{
				    crd_max_horiz_res = 
					crd_info.component[index].horiz_res;
				}
			    }

			    fprintf( stderr, "crd max horiz res: %d\n",
				crd_max_horiz_res );
			    fprintf( stderr, "num_planes: %d\n", num_planes );

			    if ( crd_max_horiz_res > 300 )
			    {
				rasterwidth *= 2;
			    }

			}

			return ( FALSE );
		}

	/*
	**  Now deal with <esc>*r stuff
	*/

	if ( group == 'r' )
	{
		switch ( terminator )
		{
			case 'A':
			case 'a':

				/* if user wants to strip redundant seq */
				if ( strip_seq && memflag )
					return( FALSE );

				/* Enter graphics mode, enable stripping */

				in_graphics = TRUE;
				
				if ( strip_seq )
				{
					printf("\033*rB");

					if ( deskjet )	/* B resets modes on DJ */
					{
						inmode = 0;
						outmode = 0;
					}
				}


				curr_plane=0;
				zero_seeds();	/* may allocate mem */

				break;

			case 'C':
			case 'c':

				/* Exit graphics, disable stripping */

				in_graphics = FALSE;

				if ( strip_seq )
					return( FALSE );

				inmode = 0;
				outmode = 0;

				free_mem();
				curr_plane=0;
				break;

			case 'B':
			case 'b':

				/* Exit graphics, disable stripping */

				in_graphics = FALSE;

				if ( strip_seq )
					return( FALSE );

				if ( deskjet )	/* B resets modes on DJ */
				{
					inmode = 0;
					outmode = 0;
				}
				free_mem();
				curr_plane=0;
				break;

			case 'S':
			case 's':

				/* free mem in case widths changed */
				free_mem();

				rpix = num;

				if (imaging){
					switch(imdata.pix_mode)
					{
						case 0x00:
							rasterwidth=(rpix+7)/8;
							break;
						case 0x01:
							rasterwidth = 
							 rpix*imdata.inx_bits/8;
							break;
						case 0x02:
							rasterwidth=(rpix+7)/8;
							break;
						case 0x03:
							rasterwidth = 
							  (imdata.red 
							  + imdata.green
							  + imdata.blue)*rpix/8;
							break;
					}
				} else
					rasterwidth = (num + 7) / 8;

				/*
				**  If crd is on, may need to bump up width
				*/

				if ( crd_max_horiz_res > 300 )
				{
				    /*
				    **  NOTE:  This increases the size of
				    **  ALL planes, not just the ones
				    **  that need it.
				    */

				    rasterwidth *= 2;
				}

				break;

			case 'T':
			case 't':
				break;

			case 'U':
			case 'u':
				curr_plane=0;
				free_mem();	/* if ESC*rA came first */

				/*  num can be negative */

				if ( num < 0 )
					num_planes= -num;
				else
					num_planes = num;

				/*
				**  This turns off imaging mode,
				**  so we must recalculate rasterwidth,
				**  (which is number of bytes needed),
				**  based on normal raster transfer.
				*/

				imaging = FALSE;	/* goes off */
				rasterwidth = (rpix + 7) / 8;

				break;

			default:
				break;
		}

		return ( TRUE );		/* pass sequence on */

	}	/* group r */

	/*
	**  Last and final group 'b'.  All the graphics data comes thru here.
	*/


	switch ( terminator )
	{
		/*
		**  Compression Method command.
		*/

		case 'm':
		case 'M':
			inmode = num;
			return ( FALSE );	/* we do NOT pass this */
			break;

		/*
		**  Seed row source command.
		*/

		case 's':
		case 'S':

			/* a negative number could crash us */
			if ( num < 0 )
				num = (-num);

			seed_row_source = num;

			if ( pass_seed_row_source )
				return ( TRUE );	/* pass this thru */
			else
				return ( FALSE );	/* do not pass thru */
			break;

	       /*
	       **  <esc>*b#X is obsolete, but I need to use it.
	       **  In addition, they will not get passed thru.
	       */

	       case 'x':
	       case 'X':
			/*
			**  Compute in bytes, rounding down.
			*/

			horiz_offset = num / 8;

			if ( horiz_offset < 0 )		/* just in case */
				horiz_offset = 0;

			if ( strip_offsets || horiz_offset == 0 )
				return ( FALSE );	/* do not pass seq */

			break;

	       case 'y':
	       case 'Y':
			/* zero only if allocated */
			if ( memflag )
				zero_seeds();
			break;

	       case 'W':
	       case 'w':
			if(!memflag)
				zero_seeds();		/* get memory */

			/* fire up sequence */

			if ( !in_sequence )
			{
				in_sequence = TRUE;
				putchar ( ESC );
				putchar ( parameter );
				putchar ( group );
			} else
			{
				if ( old_terminator )
				    putchar ( old_terminator );
			}
			
			/* only terminate if big W */
			if (terminator == 'W')
				in_sequence = FALSE;	/* terminating seq */
			else
			    old_terminator = 0;		/* kludge */

			/*
			**  Check to see if we are expecting another plane.
			*/

			if(curr_plane < num_planes) 
			{
				/*
				**  If the input file does not have all the
				**  expected planes  (i.e., <esc>*b0W instead
				**  of <esc>*b0V<esc>*b0V<esc>*b0W), then
				**  special handling is needed.
				*/

				if( curr_plane + 1 < num_planes )
				{
					Process_Gap ( num, terminator );

				} else		/* don't worry, be happy */

					Process(num, terminator );

			} else		/* oops, too many planes of data */

				Process_extra(num, terminator );   

			curr_plane=0;

			/*
			**  If we were not already in graphics, then we are
			**  now (implied start of raster graphics).
			*/

			in_graphics = TRUE;

			return ( FALSE );

			break;

	       case 'V':
	       case 'v':
			if(!memflag)
				zero_seeds();		/* get memory */
			
			/*
			**  If curr_plane is the last plane, this should
			**  be a 'W', not a 'V'.  I could change it,
			**  then I would fix Process_extra() to not output
			**  anything as the 'W' was already sent.
			*/

			if( curr_plane < num_planes ) 
			{
				/* fire up sequence */

				if ( !in_sequence )
				{
					in_sequence = TRUE;
					putchar ( ESC );
					putchar ( parameter );
					putchar ( group );
				} else
				{
				    if ( old_terminator )
					putchar ( old_terminator );
				}

				/* only terminate if big V */
				if (terminator == 'V')
					in_sequence = FALSE;	/* terminate */
				else
				    old_terminator = 0;		/* kludge */
			

				Process(num, terminator );
				curr_plane++;
			} else
				Process_extra(num, terminator );

			/*
			**  If we were not already in graphics, then we are
			**  now (implied start of raster graphics).
			*/

			in_graphics = TRUE;

			return ( FALSE );

			break;

		default:
			break;
	}

	return ( TRUE );		/* pass sequence */
}


/*
**  Flush_To_Term() passes thru input until a valid terminator
**  character is found.  This is for unwanted escape sequences.
*/

/* pass through input until valid terminator character is found */

Flush_To_Term()
{
	int	c;

	do
	{
		c = getchar();

		if ( c == EOF )			/* this is a problem */
			return;
		
		putchar ( c );

	} while ( c < '@' || c > '^' );
}


/*
**  Flush_Bytes() transfers so many bytes directly from input to output.
**  This is used to pass thru binary data that we are not interested in so that
**  it will not confuse the parser.  I.e. downloads.
*/

/* transfer 'num' bytes directly from input to output */

Flush_Bytes( num )
unsigned int	num;
{
	int	bnum;

	while ( num > 0 )
	{
		bnum = MIN ( BUFSIZ, num );

		fread( buf, 1, bnum, stdin );

		if ( fwrite( buf, 1, bnum, stdout ) < bnum )

			/* check for error and exit */

			if ( ferror(stdout) )
			{
				perror("Output error");
				exit(-2);
			}

		num -= bnum;
	}
}




/*----------------------------------------*/

/*
**	Zero_seeds() will allocate and initialize memory.
**	If memory has already been allocated, then it will just initialize it.
*/

/* allocate and initialize memory */

zero_seeds()
{
	int r;

	/* first allocate and init seed_rows for number of planes. */

	for ( r = 0; r < num_planes ; r++)
	{
		if(!memflag)
		{
			seed_row[r] = (unsigned char *) malloc(rasterwidth);

			if ( seed_row[r] == NULL )
			{
				fprintf(stderr, "Out of memory.\n");
				exit(-3);
			}
		}

		/* zero seeds for mode 3 */

		memset(seed_row[r], 0, rasterwidth);
	}

	/*
	**  Now allocate the input and output compression buffers.
	*/

	if(!memflag)
	{
		new_row = (unsigned char *) malloc(rasterwidth);

		if ( new_row == NULL )
		{
			fprintf(stderr, "Out of memory.\n");
			exit(-3);
		}

		for(r=0; r<MAXMODES; r++)
		{

			/*
			**  Given input size (uncompressed) of n bytes, 
			**  the worst case output size for each mode is:
			**
			**  Mode 0:  n
			**
			**  Mode 1:  n * 2
			**
			**  Mode 2:  n + (n + 127)/128
			**
			**  Mode 3:  n + (n + 7)/8
			**
			**  Mode 9:  n + (n + 254)/255 + 1 (approx.)
			**
			**  So, the worst would be mode 1 at 2*n, so I
			**  make all the output sizes be 2*n.
			**
			**  This way, the compression routines will not
			**  overwrite the output buffers.
			*/

			/*
			**  Only allocate for modes 0-3 and 9
			*/

			if ( r < 4 || r == 9 )
			{
				out_row[r] = (unsigned char *) 
						malloc(2 * rasterwidth);

				if ( out_row[r] == NULL )
				{
					fprintf(stderr, "Out of memory.\n");
					exit(-3);
				}
			} else
				out_row[r] = NULL;
		}

	}

	memset(new_row, 0, rasterwidth);

	memflag = TRUE;			/* memory is in place */
}


/* this routine if for incomplete transfers of data */

zero_upper(plane)
int	plane;
{
	int i;

	/* assume memory already present */

	for ( i = plane; i < num_planes; i++)
		memset(seed_row[i], 0, rasterwidth);
}


/*
**  Process() manages the decompression and re-compression of data.
*/

Process(inbytes, terminator)
int inbytes, terminator;
{

	int minmode = 0;
	unsigned char	*seed_ptr;
	int	scratch;

	/*
	**  Select the seed_row_source if requested.
	**  A value of 0 is essentially a NOP.
	*/

	scratch =  (num_planes + curr_plane - seed_row_source) % num_planes;

	if ( scratch < 0 )
	{
		scratch = 0;
		fprintf(stderr, 
			"Warning:  Seed Row Source > number of planes.\n");
	}

	seed_ptr = seed_row[ scratch ];

	inuse[inmode]++;

	/*
	**  Clamp horizontal offset to the rasterwidth for safety.
	*/

	if ( horiz_offset > rasterwidth )

		horiz_offset = rasterwidth;

	/*
	**  Zero out horiz_offset bytes in new_row.
	*/

	if ( horiz_offset )

		memset ( new_row, 0, horiz_offset );


	switch ( inmode ) {

	case 0:
		if ( !widthwarning && inbytes > rasterwidth )
		{
			/* This is likely to result in data truncation. */
			widthwarning = TRUE;
			fprintf(stderr,"Warning: Input pixel width exceeds expected width.\n");
		}

		Uncompress_0( inbytes, rasterwidth - horiz_offset,
				new_row + horiz_offset);
		break;

	case 1:
		Uncompress_1( inbytes, rasterwidth - horiz_offset,
				new_row + horiz_offset);
		break;

	case 2:
		Uncompress_2( inbytes, rasterwidth - horiz_offset,
				new_row + horiz_offset);
		break;

	case 3:
		memcpy(new_row, seed_ptr, rasterwidth);

		if ( horiz_offset )
			memset ( new_row, 0, MIN( horiz_offset, rasterwidth ) );

		Uncompress_3(inbytes, rasterwidth - horiz_offset,
				new_row + horiz_offset);
		break;

	case 9:
		memcpy(new_row, seed_ptr, rasterwidth);

		if ( horiz_offset )
			memset ( new_row, 0, MIN( horiz_offset, rasterwidth ) );

		Uncompress_9(inbytes, rasterwidth - horiz_offset,
				new_row + horiz_offset);
		break;

	default:		/* unknown mode? */

		/*  Don't know what to do about seed rows, pass stuff thru */

		fprintf(stderr, "%s: Unsupported compression mode %d.\n",
			progname, inmode );

		ChangeMode(inmode);	/* go to that mode */

		/* <esc>*b has already been output */

		printf("%1d%c", inbytes, terminator);

		Flush_Bytes( inbytes );

		/*  Go ahead and clear the seed rows if present  */
		if ( memflag )
			zero_seeds();

		return;

	}

	/*
	**  If we are generating seed row source commands for DeskJets, then
	**  we can interrupt the flow at this point, do the work then
	**  bypass the rest.
	**
	**  After we decide that we are generating seed row source commands
	**  then we must see if we can take advantage of them.  If not, then
	**  we pass through to the regular compression stuff.
	**
	**  Taking advantage of the feature for us requires a multi-plane
	**  file (CMY or RGB), a single plane file is a nop for us.
	*/

	if ( generate_srs && num_planes > 1 )
	{
		/*
		**  Ok, right kind of file, lets doit.
		*/

		/*
		**  If this is not the first plane, and it matches
		**  the previous plane, then we can colapse it.
		*/

		if ( curr_plane && memcmp( seed_row[curr_plane - 1],
						new_row, rasterwidth) == 0 )
		{
			/*
			**  Make seed row source point to previous plane.
			*/

			if ( out_srs != 1 )
				ChangeSRS(1);
			
			/*
			**  Output mode must be 3 or 9.
			*/

			if ( outmode != 3 && outmode != 9 )

				/*
				**  Prefer mode 9, but if not available,
				**  use 3, and we don't test 'cause we know
				**  at this point that it is available.
				*/

				if ( mode9 )
					ChangeMode(9);
				else
					ChangeMode(3);

			/*
			**  Now, output the data :-)
			*/

			printf("0%c", terminator);

			/*
			**  Now copy the new row to the current seed.
			*/

			memcpy( seed_row[curr_plane], new_row, rasterwidth );

			/*
			**  As below, clear offset. (just in case)
			*/

			horiz_offset = 0;

			return;				/* done here */
		}

		/*
		**  Ok, now we have either the first plane (which may or
		**  may not be for black data), or the current plane does
		**  not match the previous plane (color), so seed row
		**  source should be reset to 0 and control dropped through
		**  to the normal stuff below.
		*/

		if ( curr_plane == 0 )
		{
			/*
			**  Ok, here is the first plane, what do we do?
			**
			**  Well, the easiest thing to do is to put
			**  seed row source to 0 and continue, but this
			**  would mean a seed row source command for
			**  every row (not plane).  So, let's check the
			**  current seed row source, and if it is 0, we
			**  can just fall through to the regular code,
			**  however, if it is something else (i.e. 1)
			**  then the previous plane was black, so we should
			**  just check the seed row for the current plane,
			**  and the seed row specified by the current srs,
			**  and if they match, then we can just fall through,
			**  and the code below won't care because the two
			**  two seed rows are the same, and it will just use
			**  the normal one.  However, if they do not match,
			**  then we will forcibly reset seed row, and the
			**  compression will be done with the correct plane.
			**
			**  Theoretically, this may increase the size of the
			**  data, but we are assuming that seed row source
			**  is only used (and to be used) for colapsing
			**  black data in color raster, so then it is mostly
			**  a wash, and not worth the extra cpu cycles it
			**  would take to achieve the diminishing returns.
			*/

			if ( out_srs )
			{
				/*
				**  Ok, at this point, we are probably in
				**  black data, so compare seed rows.
				**
				**  This computation is taken from above.
				*/

				scratch =  (num_planes - out_srs) % num_planes;

				if ( memcmp( 	seed_row[curr_plane],
						seed_row[scratch],
						rasterwidth ) )
					/*
					**  They don't match, so reset srs
					**  which effectively means go back
					**  to color data.
					*/

					ChangeSRS( 0 );	
			}

		} else			/* curr_plane is NOT 0 (first) */
		{
			/*
			**  Here we have color data, so fall through, but
			**  make sure that seed row source is reset, in
			**  case the previous row was black data.
			*/

			if ( out_srs )

				ChangeSRS ( 0 );
		}
	}


	/*
	**  We need to account for the horizontal offset, but if strip_offsets
	**  is on, then assume that zero is white.
	*/

	if ( strip_offsets )
		horiz_offset = 0;


	out_size[4] = MAXBYTES+1;	/* kludge */

	if ( mode0 )
		/* actually, this is redundant since new_row is mode 0 */
		out_size[0] = Compress_0( new_row + horiz_offset, out_row[0], 
				rasterwidth - horiz_offset );
	else
		out_size[0] = MAXBYTES+1;

	if ( mode1 )
		out_size[1] = Compress_1( new_row + horiz_offset, out_row[1], 
				rasterwidth - horiz_offset );
	else
		out_size[1] = MAXBYTES+1;

	if ( mode2 )
		out_size[2] = Compress_2( new_row + horiz_offset, out_row[2], 
				rasterwidth - horiz_offset );
	else
		out_size[2] = MAXBYTES+1;

	/*
	**  Note:  the pass_seed_row_source stuff will not interact with
	**  the generate seed row source stuff because the generate_srs
	**  flag will not be turned on if pass_seed_row source was 
	**  specified by the user.
	*/

	if ( mode3 )
		if ( pass_seed_row_source )
			out_size[3] = Compress_3( seed_ptr + horiz_offset, 
				new_row + horiz_offset, out_row[3], 
				rasterwidth - horiz_offset );
		else
			out_size[3] = Compress_3( seed_row[curr_plane] 
				+ horiz_offset, 
				new_row + horiz_offset, out_row[3], 
				rasterwidth - horiz_offset );
	else
		out_size[3] = MAXBYTES+1;

	if ( mode9 )
		if ( pass_seed_row_source )
			out_size[9] = Compress_9( seed_ptr + horiz_offset, 
				new_row + horiz_offset, out_row[9], 
				rasterwidth - horiz_offset );
		else
			out_size[9] = Compress_9( seed_row[curr_plane] 
				+ horiz_offset, 
				new_row + horiz_offset, out_row[9], 
				rasterwidth - horiz_offset );
	else
		out_size[9] = MAXBYTES+1;
	

	/*
	**  Obsolete comment:
	**
	**  Now determine which mode will give the best output.  Note that it
	**  takes 5 bytes to change modes, so we penalize all modes that are
	**  not the current output by 5 bytes.  This is to discourage changing
	**  unless the benifit is worth it.  The exception to this rule is
	**  mode 3.  We want to encourage going to mode 3 because of the seed
	**  row behaviour.  That is, if we have a simple picture that does
	**  not change much, and say each of the sizes for modes 1 and 2 always
	**  comes out to 4 bytes of data, then if we add 5 to mode 3 each time,
	**  it would never get selected.  But, we remove the penalty, and if
	**  mode 3 is selected (0 bytes of data needed for mode 3), then each
	**  succesive row only needs 0 bytes of data.  For a 300 dpi A size
	**  picture with 3 data planes, this could be a savings of 37k bytes.
	*/

	/*
	**  With the new parser, the output to change modes is now only
	**  2 bytes, since it gets combined with the *b#W sequence.
	**  So, I decided to ignore the switching penalty.
	*/

	minmode = 9;

	if ( out_size[3] < out_size[minmode] )
		minmode = 3;

	if ( out_size[2] < out_size[minmode] )
		minmode = 2;

	if ( out_size[1] < out_size[minmode] )
		minmode = 1;

	if ( out_size[0] < out_size[minmode] )
		minmode = 0;


	/*
	**  Check sizes, and don't change modes unless we save space.
	*/

	if ( minmode != outmode )
		if ( out_size[minmode] == out_size[outmode] )
			minmode = outmode;


	outuse[minmode]++;

	if ( outmode != minmode )
		ChangeMode( minmode );
	
	/* <esc>*b has already been output */

	printf("%1d%c", out_size[minmode], terminator);

	if ( fwrite( out_row[minmode], 1, out_size[minmode], stdout) < 
							out_size[minmode] )

		/* check for error and exit */

		if ( ferror(stdout) )
		{
			perror("Output error");
			exit(-2);
		}


	memcpy(seed_row[curr_plane], new_row, rasterwidth);

	/*
	**  Now clear horizontal offset for next plane.
	*/

	horiz_offset = 0;

}


/*
**  Process_Gap() is to handle the case where less planes are sent for a
**  row than we are expecting.  For example, if we are expecting 3 planes
**  per row, and the driver decides to take a short cut for blank areas and
**  send only the final 'W'  ( <esc>*b0W instead of the complete <esc>*b0V
**  <esc>*b0V <esc>*b0W), then we have to do some special handling for mode
**  3 seed rows.
**
**  Update for mode 9:  Any place that says "3" also applies to 9.
*/

Process_Gap( bytes, terminator )
int	bytes, terminator;
{
	char	save0, save1, save2, save3, save9;
	int	vterminator;

	/*
	**  If the input file does not have all the expected planes  
	**  (i.e., <esc>*b0W instead **  of <esc>*b0V<esc>*b0V<esc>*b0W), 
	**  then special handling is needed.
	**
	**  4 cases are handled:
	**
	**  input mode  output mode   extra action
	**  ----------  -----------   ------------
	**
	**    non-3       non-3       zero seeds
	**
	**      3           3         do nothing
	**
	**    non-3         3         zero seeds & extra output
	**
	**      3         non-3       extra output
	**
	**  Note:  We don't know what the output
	**  mode will be before we call Process(),
	**  so we must force the modes.
	*/

	/*
	**  Save output modes in case we need to manipulate them.
	*/

	save0 = mode0;
	save1 = mode1;
	save2 = mode2;
	save3 = mode3;
	save9 = mode9;

	/*
	**  Save upper or lower case...
	*/

	if ( terminator == 'W' )
	{
	    vterminator = 'V';
	} 
	else
	{
	    vterminator = 'v';
	}


	if ( inmode != 3 && inmode != 9 )
	{
		/*
		**  Force output to non-3
		**  to do as little as possible.
		*/

		if ( mode0 || mode1 || mode2 )
		{
			mode3 = FALSE;
			mode9 = FALSE;

			Process(bytes, terminator );

			mode3 = save3;		/* restore mode 3 */
			mode9 = save9;		/* restore mode 9 */

			zero_upper( curr_plane + 1);

		} else	/* mode 3 or 9 are only ones allowed for output */
		{
			/*
			**  We must output more info.
			*/

			Process( bytes, vterminator );	/* convert to plane */

			curr_plane++;

			while ( curr_plane < num_planes )
			{
				/*
				**  Restart graphics data sequence.
				*/

				if ( terminator == 'W' )
				{
				    putchar ( ESC );
				    putchar ( '*' );
				    putchar ( 'b' );
				}

				/*
				**  Call Process() with 0 bytes instead
				**  of just doing output because we
				**  need Process() to zero the appropriate
				**  seed rows, and to use mode 3 to clear
				**  the seed rows in the output (printer).
				*/

				if ( curr_plane + 1 == num_planes )

					/* last plane */
					Process(0, terminator );	
				else
					Process(0, vterminator );

				curr_plane++;
			}
		}
	} else		/* inmode == 3 || inmode == 9 */
	{
		/*
		**  Inmode is 3 or 9, so make outmode be 3 or 9 so we 
		**  can do nothing.
		*/

		if ( mode3 || mode9 )		/* is mode 3 or 9 allowed? */
		{
			mode0 =
			mode1 =
			mode2 = FALSE;

			Process( bytes, terminator );

			mode0 = save0;		/* restore modes */
			mode1 = save1;
			mode2 = save2;

		} else				/* ooops, no mode 3 */
		{
			/*
			**  We must output more info.
			*/

			Process( bytes, vterminator );	/* convert to plane */

			curr_plane++;

			while ( curr_plane < num_planes )
			{
				/*
				**  Restart graphics data sequence.
				*/

				if ( terminator == 'W' )
				{
				    putchar ( ESC );
				    putchar ( '*' );
				    putchar ( 'b' );
				}

				/*
				**  Call Process() with 0 bytes instead
				**  of just doing output because we
				**  need Process() to use the seed rows
				**  to create non-mode3 data.
				*/

				if ( curr_plane + 1 == num_planes )

					/* last plane */
					Process(0, terminator );
				else
					Process(0, vterminator );

				curr_plane++;
			}
		}
	}
}


/*
**  Process_extra() is to handle the extra planes.  That is, for PaintJets,
**  when sending 3 planes of data using <esc>*b#V, many drivers send a
**  fourth plane as <esc>*b0W to terminate the row, instead of the recommended
**  'W' as the 3rd plane.  This routine handles the extra without disturbing
**  the mode 3 seed rows.
**
**  In the future, this routine could be used to strip out the 4th plane.
*/

/* handle extra planes */

Process_extra(bytes, terminator)
int	bytes;
char	terminator;
{
	int  i;

	/* toss any excess data */

	for(i = 0; i < bytes; i++)
	   getchar();

	/* last plane? force move down to next row */

	if(terminator == 'W')
	{
		/* <esc>*b has already been output */
		printf("0W");
	}

}

/*
**  ChangeMode() outputs the sequence to change compression modes.
*/

/* output the sequence to change compression modes */

ChangeMode(newmode)
int newmode;
{
	/*
	**  <esc>*b have already been output.
	**  terminator is 'm' instead of 'M' since it will be followed by 'W'
	*/
	printf("%1dm", newmode);
	outmode = newmode;
}


/*____________________________________________________________________________
 |             |                                                              |
 | ChangeSRS() | outputs the sequence to change Seed Row Source        |
 |_____________|______________________________________________________________|
*/

/* output the sequence to change Seed Row Source */

ChangeSRS(newsrs)
int newsrs;
{
	/*
	**  <esc>*b has already been output.
	**  terminator is 's' instead of 'S' since it will be followed by 'W'
	*/

	printf("%1ds", newsrs);
	out_srs = newsrs;
}




Uncompress_0(input_bytes, output_bytes, address)

unsigned int
   input_bytes,                 /* Count of bytes to be read. */
   output_bytes;                /* Count of bytes to be stored. */

unsigned char
   *address;                    /* Pointer to where to store bytes. */

{

   unsigned char
      *store_ptr;               /* Pointer to where to store the byte. */

   unsigned int
      read_bytes,               /* Local copy of input_bytes. */
      write_bytes;              /* Local copy of output_bytes. */


   /* Initialize the local variables. */

   read_bytes = input_bytes;
   write_bytes = output_bytes;
   store_ptr = address;
   

   /* transfer the lesser of available bytes or available room */

     Transfer_Block( MIN(write_bytes,read_bytes), store_ptr);

   /* now zero fill or throw excess data away */

   if ( read_bytes > write_bytes )
      Discard_Block(read_bytes - write_bytes);		/* throw excess */
   else {
      store_ptr += read_bytes;				/* adjust pointer */
      write_bytes -= read_bytes;			/* zero fill count */

      memset(store_ptr, 0, write_bytes);
   }

   return ( input_bytes );
}


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


Uncompress_1(input_bytes, output_bytes, address)

unsigned int
   input_bytes,                 /* Count of bytes to be read. */
   output_bytes;                /* Count of bytes to be stored. */

unsigned char
   *address;                    /* Pointer to where to store bytes. */

{

   unsigned char
      *store_ptr,               /* Pointer to where to store the byte. */
      input_char;               /* Byte to be replicated. */

   unsigned int
      read_bytes,               /* Local copy of input_bytes. */
      write_bytes;              /* Local copy of output_bytes. */

   int
      replicate_count;          /* Number of times to replicate data. */


   /* Initialize the local variables. */

   read_bytes = input_bytes;
   write_bytes = output_bytes;
   store_ptr = address;
   
   /* Check for an even input count. */

   if ((read_bytes % 2) == 0)
   {
      /* Even so input data is in pairs as required. So store the data. */
   
      while ((read_bytes != 0) && (write_bytes != 0))
      {
         /* First get the replicate count and the byte to store. */

         replicate_count = (unsigned char) Get_Character();
         input_char = Get_Character();
         read_bytes -= 2;
      
         /* Since write_bytes was 0 there is room to store the byte. */

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
   /* Discard any left over input. */
	/* OR */
   /* Discard all of the input data as odd byte count. */

   Discard_Block(read_bytes);

   read_bytes = store_ptr - address;  /* how much was done? */

   /* zero fill if needed */
   memset(store_ptr, 0, write_bytes);
         
   return(read_bytes);
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


Uncompress_2(input_bytes, output_bytes, address)

unsigned int
   input_bytes,                 /* Count of bytes to be read. */
   output_bytes;                /* Count of bytes to be stored. */

unsigned char
   *address;                    /* Pointer to where to store bytes. */

{

   unsigned char
      *store_ptr,               /* Pointer to where to store the byte. */
      input_char,               /* Byte to be replicated. */
      sub_mode;                 /* Flag if sub mode is 0 or 1. */

   unsigned int
      read_bytes,               /* Local copy of input_bytes. */
      write_bytes;              /* Local copy of output_bytes. */

   int
      replicate_count;          /* Number of times to replicate data. */


   /* Initialize the local variables. */

   read_bytes = input_bytes;
   write_bytes = output_bytes;
   store_ptr = address;
   
   while ((read_bytes > 1) && (write_bytes != 0))
   {
      /* First get the class type byte and the first data type byte. */

      replicate_count = Get_Character();

      /* First check that this not an ignore class type. */

      if (replicate_count != 128)
      {
         /* Not ignore so get the data class byte. */

         input_char = Get_Character();
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
   
              Transfer_Block(replicate_count, store_ptr);

            read_bytes -= replicate_count;
            
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

   /* Now discard any left over input. */

   Discard_Block(read_bytes);

   read_bytes = store_ptr - address;

   /* zero fill if needed */
   memset(store_ptr, 0, write_bytes);
         
   
   return(read_bytes);
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


Uncompress_3(input_bytes, output_bytes, address)

unsigned int
   input_bytes,                 /* Count of bytes to be read. */
   output_bytes;                /* Count of bytes to be stored. */

unsigned char
   *address;                    /* Pointer to where to store bytes. */

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
	store_ptr = address;

	/* 
	**  read_bytes has to be at least 2 to be valid
	*/

	while ( read_bytes > 1 && write_bytes > 0 )
	{

		/* start by getting the command byte */

		command = Get_Character();

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
				offset = Get_Character();

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
			*store_ptr++ = Get_Character();
			read_bytes--;
			write_bytes--;
		}
	}
   
	/*
	**  Don't do any zero fill with mode 3,
	**  and discard any leftover input.
	*/

	Discard_Block(read_bytes);

	return;
}


/*
***************************************************************************
**
**  Mode 9 Uncompress.
**
**  Mode 9 is similar to mode 3, however, the delta bytes may also be
**  compressed.
**
***************************************************************************
*/

Uncompress_9 (input_num, output_num, dest_ptr)

unsigned int
	input_num,		/* number of bytes on input stream */
	output_num;		/* number of bytes in destination area */

unsigned char
	*dest_ptr;		/* pointer to destination area */

{

	/*
	**  Local copies of parameters for possible compiler optimizations.
	*/

	unsigned int
		read_count = input_num,
		write_count = output_num;

	unsigned char
		*store_ptr = dest_ptr;

	/*
	**  Truely local vars
	*/

	unsigned int
		offset,		/* relative offset */
		replace;	/* replacement byte count */

	unsigned char
		command;	/* command byte in stream */

	unsigned char
		control_bit;	/* determines fields and compression */

	unsigned char
		value;		/* value to be repeated in run-length mode */

	int
		scratch;


	/*
	**  Now do the uncompress algorithm (similar to mode 3).
	*/

	while ( read_count && write_count )
	{
		command = Get_Character();

		read_count--;

		control_bit = command & 0x80;		/* top bit */

		/*
		**  Control bit determines field sizes and compression.
		*/

		if ( control_bit )
		{
			/*
			**  Control_bit on means run length encoded data.
			**  Offset field is bits 5-6.
			**  Replacement field is bits 0-4.
			*/

			offset = (command & 0x60) >> 5;
			replace = (command & 0x1f);

			/*
			**  Check for and get optional offset bytes.
			*/

			if ( offset == 3 && read_count )
			{
				/*
				**  Get more offset bytes.
				**  These bytes will terminate when a value
				**  of less than 255 is encountered.
				*/

				do
				{
					scratch = Get_Character();

					read_count--;

					if ( scratch == EOF )
					{
						read_count = 0;
						break;
					}

					offset += scratch;

				} while ( read_count && scratch == 255 );
			}

			/*
			**  Now we have total offset, see if it still fits.
			*/

			if ( offset > write_count )
			{
				write_count = 0;
				break;
			}

			/*
			**  Adjust pointers and counters.
			*/

			write_count -= offset;
			store_ptr += offset;
				
			/*
			**  Check for and get optional replace count bytes.
			*/

			if ( replace == 0x1f && read_count )
			{
				/*
				**  Get more replace count bytes.
				**  These bytes will terminate when a value
				**  of less than 255 is encountered.
				*/

				do
				{
					scratch = Get_Character();

					read_count--;

					if ( scratch == EOF )
					{
						read_count = 0;
						break;
					}

					replace += scratch;

				} while ( read_count && scratch == 255 );
			}

			/*
			**  The value in the replacement field is incremented
			**  to adjust the range, then to convert it from 
			**  a "repeat" count to a "total occurance" count, it
			**  is incremented again.
			*/

			replace += 2;

			/*
			**  Now do run-length encoded transfer.
			*/

			if ( read_count && write_count )
			{
				value  = Get_Character();

				read_count--;

				/*
				**  We don't want to overwrite by the repeat.
				*/

				scratch = MIN(write_count, replace);

				memset(store_ptr, value, scratch);

				store_ptr += scratch;
				write_count -= scratch;
			}

			/*
			**  Done with this command byte, go to next.
			*/
		}
		else
		{
			/*
			**  Control_bit off means unencoded data.
			**  Offset field is bits 3-6.
			**  Replacement field is bits 0-2.
			*/

			offset = (command & 0x78) >> 3;
			replace = (command & 0x7);

			/*
			**  Check for and get optional offset bytes.
			*/

			if ( offset == 15 && read_count )
			{
				/*
				**  Get more offset bytes.
				**  These bytes will terminate when a value
				**  of less than 255 is encountered.
				*/

				do
				{
					scratch = Get_Character();

					read_count--;

					if ( scratch == EOF )
					{
						read_count = 0;
						break;
					}

					offset += scratch;

				} while ( read_count && scratch == 255 );
			}

			/*
			**  Now we have total offset, see if it still fits.
			*/

			if ( offset > write_count )
			{
				write_count = 0;
				break;
			}

			/*
			**  Adjust pointers and counters.
			*/

			write_count -= offset;
			store_ptr += offset;
				
			/*
			**  Check for and get optional replace count bytes.
			*/

			if ( replace == 0x7 && read_count )
			{
				/*
				**  Get more replace count bytes.
				**  These bytes will terminate when a value
				**  of less than 255 is encountered.
				*/

				do
				{
					scratch = Get_Character();

					read_count--;

					if ( scratch == EOF )
					{
						read_count = 0;
						break;
					}

					replace += scratch;

				} while ( read_count && scratch == 255 );
			}

			/*
			**  Now do unencoded transfer.
			*/

			/*
			**  By definition, 1 is added to the replace count
			**  to adjust the range.
			*/

			replace++;

			if ( read_count && write_count )
			{
				/*
				**  Do a MIN3 to get the safe value.
				*/

				scratch = MIN ( replace, read_count );
				scratch = MIN ( scratch, write_count );

				Transfer_Block( scratch, store_ptr );
				store_ptr += scratch;

				/*
				**  Update counters
				*/

				read_count -= scratch;
				write_count -= scratch;

			}

			/*
			**  Done with this command byte, go to next.
			*/
		}
	}

	
	/*
	**  If we have left over read_count bytes, then we must discard them.
	*/

	Discard_Block( read_count );

	return;				/* done */

}



/* discard a block of characters */

Discard_Block(count)
unsigned int count;
{
	while ( count-- )
		getchar();
}

/* read a block of characters from input to a destination buffer */

Transfer_Block( count, dest )
unsigned int count;
unsigned char *dest;
{
	fread(dest, 1, count, stdin);
}


/*
**  Compress_0() does mode 0 compression (which is a no compression).
*/

Compress_0(src, dest, count)
unsigned char *src, *dest;
int count;
{
	memcpy(dest, src, count);

	if ( zerostrip )
		while ( count && dest[count-1] == 0 )
			count--;

	return(count);

}



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


Compress_1(src, dest, count)
unsigned char *src, *dest;
register int count;
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


Compress_2(src, dest, count)
unsigned char *src, *dest;
register int count;
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

		*saveptr = litcount;

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


Compress_3(seed, new, dest, count)
unsigned char *seed, *new, *dest;
register int count;
{
	unsigned char *sptr=seed, *nptr=new, *dptr=dest;
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

				*dptr++ = xfer;

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


/*____________________________________________________________________________
 |              |                                                             |
 | Compress_9() | does PCL compression mode 9 on the data.                    |
 |______________|_____________________________________________________________|
 |                                                                            |
 | This mode is compressed delta row encoding.                                |
 | Given an input byte count of n, then the worst case output size            |
 | would be  n + (n + 255)/256 + 2.                                           |
 |____________________________________________________________________________|
*/

/* perform PCL compression mode 9 on the data */

Compress_9(seed, new, dest, count)
unsigned char *seed, *new, *dest;
register int count;
{
	/*
	**  This code is similar to the mode 3 code.
	*/

	/*
	**  seed is the seed row for comparison.
	**  new is the new row of data (the desired result)
	**  Both seed and new are uncompressed graphics data.
	**  dest is the data that will convert the seed row to the new row.
	**  count is the number of bytes in the row.
	*/

	unsigned char *sptr=seed, *nptr=new, *dptr=dest;
	unsigned char *saveptr;

	int	offset, replace, repeat, scratch;

	unsigned char	command;
	unsigned char	data;
	unsigned char	lastbyte;

	unsigned char	offflag, repflag;


	/*
	**  "count" is the number of bytes of data in "new" that need to
	**  be compressed into "dest" using "seed" as the basis for diffs.
	*/

	while ( count > 0 )
	{
		offset = 0;		/* position counter */

		/*
		**  Hunt through the data until the new data is different
		**  from the seed data.
		*/

		while ( *sptr == *nptr && offset < count )
		{
			offset++;
			sptr++;
			nptr++;
		}

		/*
		**  Either a difference has been found, or we ran out of data.
		*/

		if ( offset >= count )	/* too far? */
			break;		/* done */

		count -= offset;
		

		/*
		**  Now that we have located a difference, we need to
		**  do two things:
		**
		**  1)  Determine whether the different data should
		**      be run-length encoded, or simply replaced.
		**
		**  2)  Determine how many bytes need to be either
		**      duplicated or replaced.
		*/

		saveptr = nptr;			/* save for later */

		/*
		**  First hunt for repeats...
		*/

		data = *nptr++;
		sptr++;				/* sptr must track nptr */
		count--;
		repeat = 0;

		/*
		**  I removed the part of the test where the loop stops
		**  when the new data is the same as the old data, because
		**  in a repeat run, just because one byte of the seed
		**  is the same, it does not justify the likely 2 byte
		**  penalty every time we come accross it.  (repeats
		**  are pratically free).  The only time it would not work
		**  out is if the repeat count went up enough to need another
		**  byte for the count, but this is only a problem if the
		**  matching bytes end a repeat run (which I do not test for
		**  for here).
		**
		**  [This compression method seems to have a lot of corner
		**  cases.]
		*/

		while ( count && *nptr == data /* && *nptr != *sptr */ )
		{
			repeat++;
			count--;
			nptr++;
			sptr++;
		}

		/*
		**  If we had any repeats, then do a repeat command in the
		**  output, otherwise, we hunt for literal run.
		**
		**  We cannot use a repeat run for a single byte because the
		**  repeat count is adjusted to use zero in the range:
		**
		**  0-31 ==> 1-32
		**
		**  And since a single byte has a repeat count of zero, then
		**  in decompression, it would get repeated.
		*/

		if ( repeat )
		{
			/*
			**  Create the command byte with the msb set.
			**
			**  Also, the repeat count must be adjusted to start
			**  at zero.  (During de-compression, this count is
			**  adjusted back.)
			*/

			repeat--;

			/*
			**  First offset field...
			*/

			command = MIN(offset, 3);

			offflag = ( command == 3 );	/* need more bytes */

			offset -= command;

			command <<= 5;			/* shift into place */

			/*
			**  Now the repeat field...
			*/

			scratch = MIN(repeat, 31);

			command |= scratch;

			command |= 0x80;		/* control bit */

			repeat -= scratch;

			repflag = ( scratch == 31 );	/* need more bytes */

			/*
			**  Now, output command byte.
			*/

			*dptr++ = command;

			/*
			**  Output optional offset bytes.
			**
			**  Note:  This loop must output a non-255 value
			**         to terminate.
			*/

			if ( offflag )
				do
				{
					scratch = MIN(offset, 255);

					*dptr++ = scratch;

					offset -= scratch;

				} while ( scratch == 255 );

			/*
			**  Output optional repeat count bytes.
			**
			**  Note:  This loop must output a non-255 value
			**         to terminate.
			*/

			if ( repflag )
				do
				{
					scratch = MIN(repeat, 255);

					*dptr++ = scratch;

					repeat -= scratch;

				} while ( scratch == 255 );

			/*
			**  Last, but not least, output the data byte
			**  to be repeated.
			*/

			*dptr++ = data;

			continue;		/* go to top of loop */
		}

		/*
		**  This is not a repeat run, so count how many literal
		**  bytes of data to replace.
		**
		**  The counting will continue until we run out of input,
		**  or we detect matching data, or we detect a repeat run.
		**  As in method 2, the repeat run has to have 3 in a row
		**  to justify dropping out of a literal run.
		**
		**  We drop out with a byte in the new row matches a byte
		**  in the seed row because we 1) do not lose if we do so,
		**  and 2) we could gain a count byte over the long run.
		*/

		replace = 0;

		lastbyte = data;		/* remember for testing */

		while ( count && *nptr != *sptr )
		{
			data = *nptr++;
			sptr++;			/* sptr must track nptr */
			count--;
			replace++;

			/*
			**  Triplet test:  Test this data against the last
			**  data byte.  If it matches check the next byte
			**  on the input.
			*/

			if ( lastbyte == data && count && *nptr == data )
			{
				/*
				**  Triplet, drop out of literal run.
				**  Adjust pointers and stuff.  Don't
				**  need to worry about immediate 
				**  success, as the very first data byte
				**  in this run is guaranteed not to be
				**  part of a repeat.
				**
				**  Restore last two matching bytes to input.
				*/

				count += 2;
				nptr -= 2;
				sptr -= 2;
				replace -= 2;

				break;		/* pop out of loop */
			}

			lastbyte = data;	/* save this data byte */
		}

		/*
		**  Ok, we now have a literal replace run (even if it is
		**  just one byte).  Go ahead and create the command bytes
		**  and copy over the data bytes.
		*/


		/*
		**  Create the command byte with the msb clear.
		*/

		/*
		**  First offset field...
		*/

		command = MIN(offset, 15);

		offflag = ( command == 15 );	/* need more bytes */

		offset -= command;

		command <<= 3;			/* shift into place */

		/*
		**  Now the replace field...
		*/

		scratch = MIN(replace, 7);

		command |= scratch;

		repeat = replace;		/* save this count */

		replace -= scratch;

		repflag = ( scratch == 7 );	/* need more bytes */

		/*
		**  Now, output command byte.
		*/

		*dptr++ = command;

		/*
		**  Output optional offset bytes.
		**
		**  Note:  This loop must output a non-255 value
		**         to terminate.
		*/

		if ( offflag )
			do
			{
				scratch = MIN(offset, 255);

				*dptr++ = scratch;

				offset -= scratch;

			} while ( scratch == 255 );

		/*
		**  Output optional replace count bytes.
		**
		**  Note:  This loop must output a non-255 value
		**         to terminate.
		*/

		if ( repflag )
			do
			{
				scratch = MIN(replace, 255);

				*dptr++ = scratch;

				replace -= scratch;

			} while ( scratch == 255 );

		/*
		**  Last, but not least, output the data bytes
		**  to replace with.  "repeat" is a scratch variable here.
		*/

		memcpy( dptr, saveptr, repeat + 1);

		dptr += (repeat + 1);

	}	/* while count */

	return ( dptr - dest );		/* number of bytes in output */
}


/*----------------------------------------------------------------------*\
 * This is here in case <ESC>*rU is sent after <ESC>*r#A, in which case *
 * we must deallocate the memory to provide for a different amount of   *
 * planes when graphics are sent.                                       *
\*----------------------------------------------------------------------*/

/* deallocate memory to provide for different amount of planes */

free_mem()	
{
	int r;


	if ( !memflag )
		return;		/* no memory to free */

	free(new_row);

	for(r = MAXMODES -1; r >= 0; r--)
		if ( out_row[r] )		/* only if memory there */
			free(out_row[r]);

	for(r = num_planes - 1; r >= 0; r--)
		free(seed_row[r]);

	memflag = FALSE;
}

/*
**  Get_Frac() gets the fractional part of a value.  This is here
**  because scanf() will consume a trailing 'e' or 'E', which is a problem
**  in PCL.
*/

static float	Get_Frac()
{
	float	result = 0.0;
	int	c;
	float	position = 10.0;

	while ( (c = getchar()) != EOF )
	{
		/*
		**  Do we have a digit?
		*/

		if ( !isdigit(c) )		/* not a digit */
		{
			ungetc( c, stdin );	/* put it back */
			break;			/* quit */
		}

		result += ((c - '0') / position);

		position *= 10.0;
	}

	return ( result );
}
