

     This is pclcomp -- An HP-PCL compression filter.  It reads in
PCL graphics files, and outputs compressed PCL files which may be sent
directly to printers that support the compressions.  A partial list of
printer support is included.

     Why use pclcomp?

	1)  PCL files are much smaller ( up to 90% ).

	2)  Graphics printing on a LaserJet (IIP or III) is faster.


     If you have a LaserJet II that does not support compression, you can
still compress the files for storage, and decompress them while printing.

     I wrote this program for testing.  This is NOT an HP product.  It will
NOT be supported by HP, but rather myself, in my spare time, if need be.

     If you need real support for driver development, then call Hewlett-
Packard directly, preferably the ISV support group at the Boise Division.

     You may use parts of this code within your drivers to support compression
if you wish.

     I did what I think is a reasonable job to make the program work for
most possible PCL files.  Please feel free to send comments, complaints 
or suggestions to me at adp@sdd.hp.com.  If you have a file that does
not survive the filter intact, please e-mail me the file and describe the
problem.

     This filter runs under UNIX and MS-DOS and hopefully anything else that
supports ANSI-C.  To compile under HP-UX:

cc -Aa -O pclcomp.c -o pclcomp


     Please direct all compliments and praise to:  adp@sdd.hp.com
     (or Anthony_Parkhurst@hp.com)


     -- Anthony Parkhurst
