// AnimatedGIF example for Adafruit 2.4" TFT FeatherWing
// using DMA for screen updates, uses SdFat on M4, SD otherwise

#include <AnimatedGIF.h>

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9341.h"

#if defined(__SAMD51__)
 #include <SdFat.h>
 #if defined(ENABLE_EXTENDED_TRANSFER_CLASS) // Do want, much faster
  SdFatEX filesys;
 #else
  SdFat filesys;
 #endif
#else
 #include <SD.h>
 #define filesys SD
#endif

#define SD_CS  5 // SD card ON TFT FEATHERWING, not on Feather
#define TFT_CS 9
#define TFT_DC 10

Adafruit_ILI9341 tft(TFT_CS, TFT_DC);
AnimatedGIF gif;
File f;

void * GIFOpenFile(char *fname, int32_t *pSize)
{
  Serial.printf("Filename is '%s'\n", fname);
  f = filesys.open(fname);
  if (f)
  {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    // Note: If you read a file all the way to the last byte, seek() stops working
    if ((pFile->iSize - pFile->iPos) < iLen)
       iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
    if (iBytesRead <= 0)
       return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{ 
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

// Draw a line of image directly on the LCD
void GIFDraw(GIFDRAW *pDraw)
{
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y;

    tft.startWrite(); // TFT on SPI bus plz

    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line

    s = pDraw->pPixels;
    // Apply the new pixels to the main image
    if (pDraw->ucHasTransparency) // if transparency used
    {
      uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
      int x, iCount;
      pEnd = s + pDraw->iWidth;
      x = 0;
      iCount = 0; // count non-transparent pixels
      while(x < pDraw->iWidth)
      {
        c = ucTransparent-1;
        d = usTemp;
        while (c != ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent) // done, stop
          {
            s--; // back up to treat it like transparent
          }
          else // opaque
          {
             *d++ = usPalette[c];
             iCount++;
          }
        } // while looking for opaque pixels
        if (iCount) // any opaque pixels?
        {
          tft.dmaWait(); // Wait for prior writePixels() to finish
          tft.setAddrWindow(pDraw->iX+x, y, iCount, 1);
          tft.writePixels(usTemp, iCount, true, true); // Use DMA, big-endian
          x += iCount;
          iCount = 0;
        }
        // no, look for a run of transparent pixels
        c = ucTransparent;
        while (c == ucTransparent && s < pEnd)
        {
          c = *s++;
          if (c == ucTransparent)
             iCount++;
          else
             s--; 
        }
        if (iCount)
        {
          x += iCount; // skip these
          iCount = 0;
        }
      }
    }
    else
    {
      s = pDraw->pPixels;
      // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
      for (x=0; x<pDraw->iWidth; x++)
        usTemp[x] = usPalette[*s++];
      tft.dmaWait(); // Wait for prior writePixels() to finish
      tft.setAddrWindow(pDraw->iX, y, pDraw->iWidth, 1);
      tft.writePixels(usTemp, pDraw->iWidth, true, true); // Use DMA, big-endian
    }

    tft.dmaWait();  // Wait for last writePixels() to finish
    tft.endWrite(); // SD can have SPI bus back, thx

} /* GIFDraw() */


void setup() {
  Serial.begin(115200);
  while (!Serial);

// Note - some systems (ESP32?) require an SPI.begin() before calling SD.begin()
// this code was tested on a Teensy 4.1 board

  // Do this before SD begin to make sure select pins are in proper state
  tft.begin(32000000);
  tft.setRotation(1); // Feather orientation w USB at top
  tft.fillScreen(ILI9341_BLACK);

  if(!filesys.begin(SD_CS))
  {
    Serial.println("SD Card mount failed!");
    return;
  }
  else
  {
    Serial.println("SD Card mount succeeded!");
  }

  gif.begin(BIG_ENDIAN_PIXELS); // TFT is big-endian, faster if no byte swaps
}

void loop() {
  Serial.println("About to call gif.open");
  // Some test files on SD (in gifs folder): beast.gif bigbuck2.gif dragons.gif krampus-anim.gif
  if (gif.open((char *)"/gifs/dragons.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    while (gif.playFrame(true, NULL))
    {
    }
    gif.close();
  }
  else
  {
    Serial.println("Error opening file");
    while (1)
    {};
  }
}
