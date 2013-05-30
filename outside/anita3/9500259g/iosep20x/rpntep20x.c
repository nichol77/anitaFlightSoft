
#include "../ioscarrier/ioscarrier.h"
#include "iosep20x.h"

/*
{+D}
    SYSTEM:             Library Software
        
    MODULE NAME:        rpntep20x() - read input point
                            
    VERSION:            A
    
    CREATION DATE:      10/01/01
    
    CODED BY:           FJM
    
    ABSTRACT:           The module reads a input value from a single I/O point.

    CALLING SEQUENCE:   status=rpntep20x(ptr,port,point);
                          where:
                            status (long)
                              The returned value of the I/O point
                              or error flag.
							ptr (pointer to structure)
							  Pointer to the configuration block structure.
                            port (unsigned)
                              The target I/O port number.
                            point (unsigned)
                              The target I/O point.
                            
    MODULE TYPE:        long
                        
    I/O RESOURCES:
    
    SYSTEM RESOURCES:   
    
    MODULES CALLED:
    
    REVISIONS:
    
      DATE      BY      PURPOSE
    --------   -----    ---------------------------------------------------
    
{-D}
*/


/*  
    MODULES FUNCTION DETAILS

        This module reads a value from an I/O point of an I/O port, and 
        returns the current value of the point. The valid values of 
        "port" are 0 to 2 and "point" are from 0 to 15; otherwise,
        a -1 is returned.  If no error, either 0 or 1 is returned.
*/

long rpntep20x(c_blk, port, point)

struct cblkep20x *c_blk;
unsigned port;              /* the I/O port number */
unsigned point;             /* the I/O point of a port */

{
    
/*
    ENTRY POINT OF ROUTINE
*/

    if (port > 2 || point > 15 )	/* error checking */
       return(-1);

    if ( input_word( c_blk->nHandle, (word*)&c_blk->brd_ptr->io_port[port] ) & (1 << point) )
         return(1);
    else 
         return(0);
}
