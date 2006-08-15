/*
 * Copyright (c) 2003, 2006 Matteo Frigo
 * Copyright (c) 2003, 2006 Massachusetts Institute of Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

/* This file was automatically generated --- DO NOT EDIT */
/* Generated on Sun Jul  2 16:09:01 EDT 2006 */

#include "codelet-rdft.h"

#ifdef HAVE_FMA

/* Generated by: ../../../genfft/gen_r2hc -fma -reorder-insns -schedule-for-pipeline -compact -variables 4 -pipeline-latency 4 -n 10 -name r2hcII_10 -dft-II -include r2hcII.h */

/*
 * This function contains 32 FP additions, 18 FP multiplications,
 * (or, 14 additions, 0 multiplications, 18 fused multiply/add),
 * 37 stack variables, and 20 memory accesses
 */
/*
 * Generator Id's : 
 * $Id: algsimp.ml,v 1.9 2006-02-12 23:34:12 athena Exp $
 * $Id: fft.ml,v 1.4 2006-01-05 03:04:27 stevenj Exp $
 * $Id: gen_r2hc.ml,v 1.18 2006-02-12 23:34:12 athena Exp $
 */

#include "r2hcII.h"

static void r2hcII_10(const R *I, R *ro, R *io, stride is, stride ros, stride ios, INT v, INT ivs, INT ovs)
{
     DK(KP951056516, +0.951056516295153572116439333379382143405698634);
     DK(KP559016994, +0.559016994374947424102293417182819058860154590);
     DK(KP250000000, +0.250000000000000000000000000000000000000000000);
     DK(KP618033988, +0.618033988749894848204586834365638117720309180);
     INT i;
     for (i = v; i > 0; i = i - 1, I = I + ivs, ro = ro + ovs, io = io + ovs, MAKE_VOLATILE_STRIDE(is), MAKE_VOLATILE_STRIDE(ros), MAKE_VOLATILE_STRIDE(ios)) {
	  E Tq, Ti, Tk, Tu, Tw, Tp, Tb, Tj, Tr, Tv;
	  {
	       E T1, To, Ts, Tt, T8, Ta, Te, Tm, Tl, Th, Tn, T9;
	       T1 = I[0];
	       To = I[WS(is, 5)];
	       {
		    E T2, T3, T5, T6;
		    T2 = I[WS(is, 4)];
		    T3 = I[WS(is, 6)];
		    T5 = I[WS(is, 8)];
		    T6 = I[WS(is, 2)];
		    {
			 E Tc, T4, T7, Td, Tf, Tg;
			 Tc = I[WS(is, 1)];
			 Ts = T2 + T3;
			 T4 = T2 - T3;
			 Tt = T5 + T6;
			 T7 = T5 - T6;
			 Td = I[WS(is, 9)];
			 Tf = I[WS(is, 3)];
			 Tg = I[WS(is, 7)];
			 T8 = T4 + T7;
			 Ta = T4 - T7;
			 Te = Tc - Td;
			 Tm = Tc + Td;
			 Tl = Tf + Tg;
			 Th = Tf - Tg;
		    }
	       }
	       ro[WS(ros, 2)] = T1 + T8;
	       Tn = Tl - Tm;
	       Tq = Tm + Tl;
	       Ti = FMA(KP618033988, Th, Te);
	       Tk = FNMS(KP618033988, Te, Th);
	       io[WS(ios, 2)] = Tn - To;
	       T9 = FNMS(KP250000000, T8, T1);
	       Tu = FMA(KP618033988, Tt, Ts);
	       Tw = FNMS(KP618033988, Ts, Tt);
	       Tp = FMA(KP250000000, Tn, To);
	       Tb = FMA(KP559016994, Ta, T9);
	       Tj = FNMS(KP559016994, Ta, T9);
	  }
	  Tr = FMA(KP559016994, Tq, Tp);
	  Tv = FNMS(KP559016994, Tq, Tp);
	  ro[WS(ros, 1)] = FNMS(KP951056516, Tk, Tj);
	  ro[WS(ros, 3)] = FMA(KP951056516, Tk, Tj);
	  ro[0] = FMA(KP951056516, Ti, Tb);
	  ro[WS(ros, 4)] = FNMS(KP951056516, Ti, Tb);
	  io[WS(ios, 1)] = FNMS(KP951056516, Tw, Tv);
	  io[WS(ios, 3)] = FMA(KP951056516, Tw, Tv);
	  io[WS(ios, 4)] = FMS(KP951056516, Tu, Tr);
	  io[0] = -(FMA(KP951056516, Tu, Tr));
     }
}

static const kr2hc_desc desc = { 10, "r2hcII_10", {14, 0, 18, 0}, &GENUS, 0, 0, 0, 0, 0 };

void X(codelet_r2hcII_10) (planner *p) {
     X(kr2hcII_register) (p, r2hcII_10, &desc);
}

#else				/* HAVE_FMA */

/* Generated by: ../../../genfft/gen_r2hc -compact -variables 4 -pipeline-latency 4 -n 10 -name r2hcII_10 -dft-II -include r2hcII.h */

/*
 * This function contains 32 FP additions, 12 FP multiplications,
 * (or, 26 additions, 6 multiplications, 6 fused multiply/add),
 * 21 stack variables, and 20 memory accesses
 */
/*
 * Generator Id's : 
 * $Id: algsimp.ml,v 1.9 2006-02-12 23:34:12 athena Exp $
 * $Id: fft.ml,v 1.4 2006-01-05 03:04:27 stevenj Exp $
 * $Id: gen_r2hc.ml,v 1.18 2006-02-12 23:34:12 athena Exp $
 */

#include "r2hcII.h"

static void r2hcII_10(const R *I, R *ro, R *io, stride is, stride ros, stride ios, INT v, INT ivs, INT ovs)
{
     DK(KP250000000, +0.250000000000000000000000000000000000000000000);
     DK(KP587785252, +0.587785252292473129168705954639072768597652438);
     DK(KP951056516, +0.951056516295153572116439333379382143405698634);
     DK(KP559016994, +0.559016994374947424102293417182819058860154590);
     INT i;
     for (i = v; i > 0; i = i - 1, I = I + ivs, ro = ro + ovs, io = io + ovs, MAKE_VOLATILE_STRIDE(is), MAKE_VOLATILE_STRIDE(ros), MAKE_VOLATILE_STRIDE(ios)) {
	  E T1, To, T8, Tq, T9, Tp, Te, Ts, Th, Tn;
	  T1 = I[0];
	  To = I[WS(is, 5)];
	  {
	       E T2, T3, T4, T5, T6, T7;
	       T2 = I[WS(is, 4)];
	       T3 = I[WS(is, 6)];
	       T4 = T2 - T3;
	       T5 = I[WS(is, 8)];
	       T6 = I[WS(is, 2)];
	       T7 = T5 - T6;
	       T8 = T4 + T7;
	       Tq = T5 + T6;
	       T9 = KP559016994 * (T4 - T7);
	       Tp = T2 + T3;
	  }
	  {
	       E Tc, Td, Tm, Tf, Tg, Tl;
	       Tc = I[WS(is, 1)];
	       Td = I[WS(is, 9)];
	       Tm = Tc + Td;
	       Tf = I[WS(is, 3)];
	       Tg = I[WS(is, 7)];
	       Tl = Tf + Tg;
	       Te = Tc - Td;
	       Ts = KP559016994 * (Tm + Tl);
	       Th = Tf - Tg;
	       Tn = Tl - Tm;
	  }
	  ro[WS(ros, 2)] = T1 + T8;
	  io[WS(ios, 2)] = Tn - To;
	  {
	       E Ti, Tk, Tb, Tj, Ta;
	       Ti = FMA(KP951056516, Te, KP587785252 * Th);
	       Tk = FNMS(KP587785252, Te, KP951056516 * Th);
	       Ta = FNMS(KP250000000, T8, T1);
	       Tb = T9 + Ta;
	       Tj = Ta - T9;
	       ro[WS(ros, 4)] = Tb - Ti;
	       ro[WS(ros, 3)] = Tj + Tk;
	       ro[0] = Tb + Ti;
	       ro[WS(ros, 1)] = Tj - Tk;
	  }
	  {
	       E Tr, Tw, Tu, Tv, Tt;
	       Tr = FMA(KP951056516, Tp, KP587785252 * Tq);
	       Tw = FNMS(KP587785252, Tp, KP951056516 * Tq);
	       Tt = FMA(KP250000000, Tn, To);
	       Tu = Ts + Tt;
	       Tv = Tt - Ts;
	       io[0] = -(Tr + Tu);
	       io[WS(ios, 3)] = Tw + Tv;
	       io[WS(ios, 4)] = Tr - Tu;
	       io[WS(ios, 1)] = Tv - Tw;
	  }
     }
}

static const kr2hc_desc desc = { 10, "r2hcII_10", {26, 6, 6, 0}, &GENUS, 0, 0, 0, 0, 0 };

void X(codelet_r2hcII_10) (planner *p) {
     X(kr2hcII_register) (p, r2hcII_10, &desc);
}

#endif				/* HAVE_FMA */
