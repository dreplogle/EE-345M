//*****************************************************************************
//
// rit128x96x4.h - Prototypes for the driver for the RITEK 128x96x4 graphical
//                   OLED display.
//
// Copyright (c) 2007-2010 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// 
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.
// 
// THIS SOFTWARE IS PROVIDED "AS IS" AND WITH ALL FAULTS.
// NO WARRANTIES, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING, BUT
// NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE. TI SHALL NOT, UNDER ANY
// CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
// DAMAGES, FOR ANY REASON WHATSOEVER.
// 
// This is part of revision 6075 of the EK-LM3S8962 Firmware Package.
//
// 
//*****************************************************************************
// Additional functions added by Jonathan Valvano 10/27/2010
// added a fixed point output, line plot, bar plot, and dBfs plot

//*****************************************************************************

#ifndef __RIT128X96X4_H__
#define __RIT128X96X4_H__

//*****************************************************************************
//
// Prototypes for the driver APIs.
//
//*****************************************************************************
extern void RIT128x96x4Clear(void);
extern void RIT128x96x4StringDraw(const char *pcStr,
                                    unsigned long ulX,
                                    unsigned long ulY,
                                    unsigned char ucLevel);
extern void RIT128x96x4ImageDraw(const unsigned char *pucImage,
                                   unsigned long ulX,
                                   unsigned long ulY,
                                   unsigned long ulWidth,
                                   unsigned long ulHeight);
extern void RIT128x96x4Init(unsigned long ulFrequency);
extern void RIT128x96x4Enable(unsigned long ulFrequency);
extern void RIT128x96x4Disable(void);
extern void RIT128x96x4DisplayOn(void);
extern void RIT128x96x4DisplayOff(void);
extern void RIT128x96x4DecOut5(long num, unsigned long ulX,
                      unsigned long ulY, unsigned char ucLevel);
extern void RIT128x96x4FixOut2(long num, unsigned long ulX,
                      unsigned long ulY, unsigned char ucLevel);
extern void RIT128x96x4UDecOut3(unsigned long num, unsigned long ulX,
                      unsigned long ulY, unsigned char ucLevel);
extern void RIT128x96x4UDecOut4(unsigned long num, unsigned long ulX,
                      unsigned long ulY, unsigned char ucLevel);
extern void  RIT128x96x4FixOut22(long num, unsigned long ulX,
                      unsigned long ulY, unsigned char ucLevel);
extern void Int2Str(long const num, char *string);
extern void Fix2Str(long const num, char *string);
extern void RIT128x96x4PlotClear(long ymin, long ymax);
extern void RIT128x96x4PlotPoint(long y);
extern void RIT128x96x4PlotBar(long y);
extern void RIT128x96x4PlotdBfs(long y);
extern void RIT128x96x4PlotNext(void);
extern void RIT128x96x4ShowPlot(void);
extern void oLED_Message(int device, int line, char *string, long value);
#endif // __RIT128X96X4_H__
