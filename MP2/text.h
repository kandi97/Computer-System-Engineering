/* tab:4
 *
 * text.h - font data and text to mode X conversion utility header file
 *
 * "Copyright (c) 2004-2009 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:        Steve Lumetta
 * Version:       2
 * Creation Date: Thu Sep 9 22:08:16 2004
 * Filename:      text.h
 * History:
 *    SL    1    Thu Sep 9 22:08:16 2004
 *        First written.
 *    SL    2    Sat Sep 12 13:40:11 2009
 *        Integrated original release back into main code base.
 */

#ifndef TEXT_H
#define TEXT_H

/* The default VGA text mode font is 8x16 pixels. */
#define FONT_WIDTH   8
#define FONT_HEIGHT 16

// added by Sean 10/07/2017
#define X_DIM     				320   					/* pixels; must be divisible by 4  */
#define Y_DIM     				200   					/* pixels                          */
#define X_WIDTH	  				X_DIM/4 
#define MSG_LEN	  				X_DIM/8 				/* 8 pixels width */
#define INPUT_TEXT_MAX			MSG_LEN/2				/* input text can only up to 20 characters */
#define STATUSBAR_ADDR_SIZE		X_DIM/4*(FONT_HEIGHT+2)
#define BACKGROUND_COLOR		0x03					/* Blue */
#define FONT_COLOR				0x3C					/* Yellow */

/* Standard VGA text font. */
extern unsigned char font_data[256][16];

/* Convert the received text into graph which will be stored at the buffer from the input */
extern void text_to_graph(unsigned char *buf_statusBar, char *statusMsg, const char *inputText, const char *placeName);

#endif /* TEXT_H */
