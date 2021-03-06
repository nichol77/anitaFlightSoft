
#include "../carrier/carrier.h"
#include "ip408.h"

/*
{+D}
    SYSTEM:	    Library Software

    FILENAME:	    cnfg408.c

    MODULE NAME:    cnfg408 - configure 408 board

    VERSION:	    B

    CREATION DATE:  01/04/95

    CODED BY:	    FJM

    ABSTRACT:       This module is used to perform the configure function
		    for the IP408 board.

    CALLING
	SEQUENCE:   cnfg408(ptr);
		    where:
			ptr (pointer to structure)
			    Pointer to the configuration block structure.

    MODULE TYPE:    void

    I/O RESOURCES:

    SYSTEM
	RESOURCES:

    MODULES
	CALLED:

    REVISIONS:

  DATE	     BY	    PURPOSE
  --------  ----    ------------------------------------------------
  04/01/11   FJM    Changed carrier include to carrier.h

{-D}
*/


/*
    MODULES FUNCTIONAL DETAILS:

    This module is used to perform the "configure board" function
    for the IP408 board.  A pointer to the Configuration Block will
    be passed to this routine.  The routine will use a pointer
    within the Configuration Block to reference the registers
    on the Board.  Based on flag bits in the Attribute and
    Parameter Flag words in the Configuration Block, the board
    will be configured and various registers will be updated with
    new information which will be transfered from the Configuration
    Block to registers on the Board.
*/



void cnfg408(c_blk)

struct cblk408 *c_blk;

{

/*
    declare local storage
*/

    struct map408 *map_ptr;	/* pointer to board memory map */

/*
    ENTRY POINT OF ROUTINE:
    Initialize local storage.  The board memory map pointer is initialized.
*/

    map_ptr = (struct map408 *)c_blk->brd_ptr;

/*
    Check to see if the Interrupt Vector Register is to be updated.
    Update the Vector Register before enabling Global Interrupts so that
    the board will always output the correct vectors upon interrupt.
*/

    if(c_blk->param & VECT )
	output_byte(c_blk->enable, (byte*)&map_ptr->int_vect, c_blk->vector);

/*
    If the Type Select Register is to be updated, then update it.
*/

    if(c_blk->param & INT_TYPE)
    	output_byte(c_blk->nHandle, (byte*)&map_ptr->type_reg, c_blk->type);


/*
    If the Interrupt Polarity Register is to be updated . . .
*/

    if(c_blk->param & INT_POLARITY)
		output_byte(c_blk->nHandle, (byte*)&map_ptr->pol_reg, c_blk->polarity);

/*
    If the Interrupt Enable Register is to be updated then do so.
*/

    if(c_blk->param & INT_ENAB)
		output_byte(c_blk->nHandle, (byte*)&map_ptr->en_reg, c_blk->enable);


/*
    If the Interrupt Status Register is to be updated, then update it.
*/

    if(c_blk->param & INT_STATUS)
	   	output_byte(c_blk->nHandle, (byte*)&map_ptr->sts_reg, c_blk->int_status);
}



