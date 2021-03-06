/*
 * This file is part of Espruino, a JavaScript interpreter for Microcontrollers
 *
 * Copyright (C) 2013 Gordon Williams <gw@pur3.co.uk>
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * ----------------------------------------------------------------------------
 * Graphics Draw Functions
 * ----------------------------------------------------------------------------
 */

#include "graphics.h"
#include "bitmap_font_4x6.h"
#include "vector_font.h"
#include "jsutils.h"
#include "jsvar.h"
#include "jsparse.h"

#include "lcd_arraybuffer.h"
#include "lcd_js.h"
#ifdef USE_LCD_SDL
#include "lcd_sdl.h"
#endif
#ifdef USE_LCD_FSMC
#include "lcd_fsmc.h"
#endif

// ----------------------------------------------------------------------------------------------


void graphicsFallbackSetPixel(JsGraphics *gfx, short x, short y, unsigned int col) {
  NOT_USED(gfx);
  NOT_USED(x);
  NOT_USED(y);
  NOT_USED(col);
}

unsigned int graphicsFallbackGetPixel(JsGraphics *gfx, short x, short y) {
  NOT_USED(gfx);
  NOT_USED(x);
  NOT_USED(y);
  return 0;
}

void graphicsFallbackFillRect(JsGraphics *gfx, short x1, short y1, short x2, short y2) {
  // Software emulation
  if (x1>x2) {
    short l=x1; x1 = x2; x2 = l;
  }
  if (y1>y2) {
    short l=y1; y1 = y2; y2 = l;
  }
  short x,y;
  for (y=y1;y<=y2;y++)
    for (x=x1;x<=x2;x++)
      graphicsSetPixel(gfx,x,y, gfx->data.fgColor);
}

void graphicsFallbackBitmap1bit(JsGraphics *gfx, short x1, short y1, unsigned short width, unsigned short height, unsigned char *data) {
  unsigned int x,y;
  for(x=0;x<width;x++) {
    for(y=0;y<height;y++) {
      int bitOffset = x1+(int)x+(((int)y+y1)*width);
      graphicsSetPixel(gfx,(short)x,(short)y, (unsigned int)(((data[bitOffset>>3]>>(bitOffset&7))&1) ? gfx->data.fgColor : gfx->data.bgColor));
    }
  }
}

// ----------------------------------------------------------------------------------------------

bool graphicsGetFromVar(JsGraphics *gfx, JsVar *parent) {
  gfx->graphicsVar = parent;
  JsVar *data = jsvObjectGetChild(parent, JS_HIDDEN_CHAR_STR"gfx", 0);
  assert(data);
  if (data) {
    jsvGetString(data, (char*)&gfx->data, sizeof(JsGraphicsData)+1/*trailing zero*/);
    jsvUnLock(data);
    gfx->setPixel = graphicsFallbackSetPixel;
    gfx->getPixel = graphicsFallbackGetPixel;
    gfx->fillRect = graphicsFallbackFillRect;
    gfx->bitmap1bit = graphicsFallbackBitmap1bit;
#ifdef USE_LCD_SDL
    if (gfx->data.type == JSGRAPHICSTYPE_SDL) {
      lcdSetCallbacks_SDL(gfx);
    } else
#endif
#ifdef USE_LCD_FSMC
    if (gfx->data.type == JSGRAPHICSTYPE_FSMC) {
      lcdSetCallbacks_FSMC(gfx);
    } else
#endif
    if (gfx->data.type == JSGRAPHICSTYPE_ARRAYBUFFER) {
      lcdSetCallbacks_ArrayBuffer(gfx);
    } else if (gfx->data.type == JSGRAPHICSTYPE_JS) {
      lcdSetCallbacks_JS(gfx);
    } else {
      jsErrorInternal("Unknown graphics type\n");
      assert(0);
    }

    return true;
  } else
    return false;
}

void graphicsSetVar(JsGraphics *gfx) {
  JsVar *dataname = jsvFindChildFromString(gfx->graphicsVar, JS_HIDDEN_CHAR_STR"gfx", true);
  JsVar *data = jsvSkipName(dataname);
  if (!data) {
    data = jsvNewStringOfLength(sizeof(JsGraphicsData));
    jsvSetValueOfName(dataname, data);
  }
  jsvUnLock(dataname);
  assert(data);
  jsvSetString(data, (char*)&gfx->data, sizeof(JsGraphicsData));
  jsvUnLock(data);
}

// ----------------------------------------------------------------------------------------------

void graphicsSetPixel(JsGraphics *gfx, short x, short y, unsigned int col) {
  if (x<0 || y<0 || x>=gfx->data.width || y>=gfx->data.height) return;
  gfx->setPixel(gfx,x,y,col & (unsigned int)((1L<<gfx->data.bpp)-1));
}

unsigned int graphicsGetPixel(JsGraphics *gfx, short x, short y) {
  return gfx->getPixel(gfx, x, y);
}

void graphicsFillRect(JsGraphics *gfx, short x1, short y1, short x2, short y2) {
  return gfx->fillRect(gfx, x1, y1, x2, y2);
}


void graphicsClear(JsGraphics *gfx) {
  unsigned int c = gfx->data.fgColor;
  gfx->data.fgColor = gfx->data.bgColor;
  graphicsFillRect(gfx,0,0,(short)(gfx->data.width-1),(short)(gfx->data.height-1));
  gfx->data.fgColor = c;
}

// ----------------------------------------------------------------------------------------------


void graphicsDrawRect(JsGraphics *gfx, short x1, short y1, short x2, short y2) {
  // rather than writing pixels, we use fillrect - as it is faster
  graphicsFillRect(gfx,x1,y1,x2,y1);
  graphicsFillRect(gfx,x2,y1,x2,y2);
  graphicsFillRect(gfx,x1,y2,x2,y2);
  graphicsFillRect(gfx,x1,y2,x1,y1);
}

void graphicsDrawString(JsGraphics *gfx, short x1, short y1, const char *str) {
  while (*str) {
    graphicsDrawChar4x6(gfx,x1,y1,*(str++));
    x1 = (short)(x1 + 4);
  }
}

void graphicsDrawLine(JsGraphics *gfx, short x1, short y1, short x2, short y2) {
  int xl = x2-x1;
  int yl = y2-y1;
  if (xl<0) xl=-xl; else if (xl==0) xl=1;
  if (yl<0) yl=-yl; else if (yl==0) yl=1;
  if (xl > yl) { // longer in X - scan in X
    if (x1>x2) {
      short t;
      t = x1; x1 = x2; x2 = t;
      t = y1; y1 = y2; y2 = t;
    }
    int pos = (y1<<8) + 128; // rounding!
    int step = ((y2-y1)<<8) / xl;
    short x;
    for (x=x1;x<=x2;x++) {
      graphicsSetPixel(gfx, x, (short)(pos>>8), gfx->data.fgColor);
      pos += step;
    }
  } else {
    if (y1>y2) {
      short t;
      t = x1; x1 = x2; x2 = t;
      t = y1; y1 = y2; y2 = t;
    }
    int pos = (x1<<8) + 128; // rounding!
    int step = ((x2-x1)<<8) / yl;
    short y;
    for (y=y1;y<=y2;y++) {
      graphicsSetPixel(gfx, (short)(pos>>8), y, gfx->data.fgColor);
      pos += step;
    }
  }
}

#ifdef HORIZONTAL_SCANLINE
static inline void graphicsFillPolyCreateHorizScanLines(JsGraphics *gfx, short *minx, short *maxx, short x1, short y1,short x2, short y2) {
    if (y2 < y1) {
        short t;
        t=x1;x1=x2;x2=t;
        t=y1;y1=y2;y2=t;
    }
    int xh = x1*256;
    int yl = y2-y1;
    if (yl==0) yl=1;
    int stepx = (x2-x1)*256 / yl;
    short y;
    for (y=y1;y<=y2;y++) {
        int x = xh>>8;
        if (x<-32768) x=-32768;
        if (x>32767) x=32767;
        if (y>=0 && y<gfx->data.height) {
            if (x<minx[y]) {
                minx[y] = (short)x;
            }
            if (x>maxx[y]) {
                maxx[y] = (short)x;
            }
        }
        xh += stepx;
    }
}
#else
static inline void graphicsFillPolyCreateVertScanLines(JsGraphics *gfx, short *miny, short *maxy, short x1, short y1,short x2, short y2) {
    if (x2 < x1) {
        short t;
        t=x1;x1=x2;x2=t;
        t=y1;y1=y2;y2=t;
    }
    int yh = y1*256;
    int xl = x2-x1;
    if (xl==0) xl=1;
    int stepy = (y2-y1)*256 / xl;
    short x;
    for (x=x1;x<=x2;x++) {
        int y = yh>>8;
        if (y<-32768) y=-32768;
        if (y>32767) y=32767;
        if (x>=0 && x<gfx->data.width) {
            if (y<miny[x]) {
                miny[x] = (short)y;
            }
            if (y>maxy[x]) {
                maxy[x] = (short)y;
            }
        }
        yh += stepy;
    }
}
#endif

void graphicsFillPoly(JsGraphics *gfx, int points, const short *vertices) {
#ifdef HORIZONTAL_SCANLINE
  int i;
  short miny = gfx->data.height-1;
  short maxy = 0;
  for (i=0;i<points*2;i+=2) {
    short y = vertices[i+1];
    if (y<miny) miny=y;
    if (y>maxy) maxy=y;
  }
  if (miny<0) miny=0;
  if (maxy>=gfx->data.height) maxy=gfx->data.height-1;
  short minx[gfx->data.height];
  short maxx[gfx->data.height];
  short y;
  for (y=miny;y<=maxy;y++) {
    minx[y] = gfx->data.width-1;
    maxx[y] = 0;
  }
  int j = (points-1)*2;
  for (i=0;i<points*2;i+=2) {
    graphicsFillPolyCreateVertScanLines(gfx,minx,maxx, vertices[j+0], vertices[j+1], vertices[i+0], vertices[i+1]);
    j = i;
  }
  for (y=miny;y<=maxy;y++) {
    if (maxx[y]>=minx[y]) {
      // clip
      if (minx[y]<0) minx[y]=0;
      if (maxx[y]>=gfx->data.width) maxx[y]=gfx->data.width-1;
      // try and expand the rect that we fill
      int oldy = y;
      while (y<maxy && minx[y+1]==minx[oldy] && maxx[y+1]==maxx[oldy])
        y++;
      // actually fill
      graphicsFillRect(gfx,minx[y],oldy,maxx[y],y);
      if (jspIsInterrupted()) break;
    }
  }
#else
  int i;
  short minx = (short)(gfx->data.width-1);
  short maxx = 0;
  for (i=0;i<points*2;i+=2) {
    short x = vertices[i];
    if (x<minx) minx=x;
    if (x>maxx) maxx=x;
  }
  if (minx<0) minx=0;
  if (maxx>=gfx->data.width) maxx=(short)(gfx->data.width-1);
  short miny[gfx->data.width];
  short maxy[gfx->data.width];
  short x;
  for (x=minx;x<=maxx;x++) {
    miny[x] = (short)(gfx->data.height-1);
    maxy[x] = 0;
  }
  int j = (points-1)*2;
  for (i=0;i<points*2;i+=2) {
    graphicsFillPolyCreateVertScanLines(gfx,miny,maxy, vertices[j+0], vertices[j+1], vertices[i+0], vertices[i+1]);
    j = i;
  }
  for (x=minx;x<=maxx;x++) {
    if (maxy[x]>=miny[x]) {
      // clip
      if (miny[x]<0) miny[x]=0;
      if (maxy[x]>=gfx->data.height) maxy[x]=(short)(gfx->data.height-1);
      // try and expand the rect that we fill
      int oldx = x;
      while (x<maxx && miny[x+1]==miny[oldx] && maxy[x+1]==maxy[oldx])
        x++;
      // actually fill
      graphicsFillRect(gfx,oldx,miny[x],x,maxy[x]);
      if (jspIsInterrupted()) break;
    }
  }
#endif
}



// prints character, returns width
unsigned int graphicsFillVectorChar(JsGraphics *gfx, short x1, short y1, short size, char ch) {
  if (size<0) return 0;
  if (ch<vectorFontOffset || ch-vectorFontOffset>=vectorFontCount) return 0;
  int vertOffset = 0;
  int i;
  /* compute offset (I figure a ~50 iteration FOR loop is preferable to
   * a 200 byte array) */
  int fontOffset = ch-vectorFontOffset;
  for (i=0;i<fontOffset;i++)
    vertOffset += vectorFonts[i].vertCount;
  VectorFontChar vector = vectorFonts[fontOffset];
  short verts[VECTOR_FONT_MAX_POLY_SIZE*2];
  int idx=0;
  for (i=0;i<vector.vertCount;i+=2) {
    verts[idx+0] = (short)(x1+(((vectorFontPolys[vertOffset+i+0]&0x7F)*size+(VECTOR_FONT_POLY_SIZE/2))/VECTOR_FONT_POLY_SIZE));
    verts[idx+1] = (short)(y1+(((vectorFontPolys[vertOffset+i+1]&0x7F)*size+(VECTOR_FONT_POLY_SIZE/2))/VECTOR_FONT_POLY_SIZE));
    idx+=2;
    if (vectorFontPolys[vertOffset+i+1] & VECTOR_FONT_POLY_SEPARATOR) {
      graphicsFillPoly(gfx,idx/2, verts);

      if (jspIsInterrupted()) break;
      idx=0;
    }
  }
  return (vector.width * (unsigned int)size)/(VECTOR_FONT_POLY_SIZE*2);
}

// returns the width of a character
unsigned int graphicsVectorCharWidth(JsGraphics *gfx, short size, char ch) {
  NOT_USED(gfx);
  if (size<0) return 0;
  if (ch<vectorFontOffset || ch-vectorFontOffset>=vectorFontCount) return 0;
  VectorFontChar vector = vectorFonts[ch-vectorFontOffset];
  return (vector.width * (unsigned int)size)/(VECTOR_FONT_POLY_SIZE*2);
}

// Splash screen
void graphicsSplash(JsGraphics *gfx) {
  graphicsDrawString(gfx,0,0,"Espruino "JS_VERSION);
  graphicsDrawString(gfx,0,6,"  Embedded JavaScript");
  graphicsDrawString(gfx,0,12,"  www.espruino.com");
}

void graphicsIdle() {
#ifdef USE_LCD_SDL
  lcdIdle_SDL();
#endif
}

