#include "AnimatedGIF.h"

#include <ESP32-Chimera-Core.h> // https://github.com/tobozo/ESP32-Chimera-Core or regular M5Stack Core
#define tft M5.Lcd // syntax sugar

#ifndef M5STACK_SD
 // for custom ESP32 builds
 #define M5STACK_SD SD
#endif


AnimatedGIF gif;

int xOffset = 0;
int yOffset = 0;

File FSGifFile;

static void * GIFOpenFile(char *fname, int32_t *pSize) {
  Serial.printf("GIFOpenFile( %s, %d)\n", fname, pSize );
  FSGifFile = M5STACK_SD.open(fname);
  if (FSGifFile) {
    *pSize = FSGifFile.size();
    return (void *)&FSGifFile;
  }
  return NULL;
}

static void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
     f->close();
}


static int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
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
}

static int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
}

static void TFTDraw(int x, int y, int w, int h, uint16_t* lBuf ) {
  tft.pushRect( x, y, w, h, lBuf );
}


int gifPlay( const char* gifPath ) { // 0=infinite

  gif.setFSCallbacks( GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, TFTDraw );

  if( ! gif.open((char* )gifPath ) ) {
    Serial.printf("Could not open gif %s\n", gifPath );
    while(1);
    return 0;
  }
  // center the GIF !!
  xOffset = ( tft.width()  - gif.getCanvasWidth() )  /2 ;
  yOffset = ( tft.height() - gif.getCanvasHeight() ) /2;

  int *frameDelay;
  int then = 0;

  while (gif.playFrame(true, frameDelay)) {
    then += *frameDelay;
  }
  gif.close();
  return then;
}



File root;

void walk_dir( const char* basePath ) {

  root = M5STACK_SD.open(basePath);

  if(!root){
    log_e("Failed to open directory");
    return;
  }

  if(!root.isDirectory()){
    log_e("Not a directory");
    return;
  }

  File file = root.openNextFile();

  while( file ) {
    if(!file.isDirectory()) {

      const char* fileName = file.name();
      file.close();
      tft.clear();
      int loops = 5; // max 5 loops
      int durationControl = 3000; // stop loops after 3s

      while(loops-->0 && durationControl > 0 ) {
        durationControl -= gifPlay( (char*)fileName );
      }
    } else file.close();

    file = root.openNextFile();
  }
  root.close();
}


void setup() {
  M5.begin(115200);
  //while (!Serial);

  if(!M5STACK_SD.begin()) {
    Serial.println("SD Card mount failed!");
    return;
  } else {
    Serial.println("SD Card mount succeeded!");
  }

  // put your setup code here, to run once:
  tft.begin();
  tft.fillScreen(ILI9341_BLACK);
  tft.setSwapBytes( true );
  gif.begin();
  gif.setFSCallbacks( GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, TFTDraw );
}


void loop() {

  // from the SD example
  Serial.println("About to call gif.open");
  if (gif.open((char *)"/gif/wrong_ronnies.gif")) {
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    while (gif.playFrame())
    {
    }
    gif.close();
  } else {
    Serial.println("Error opening file");
    while (1)
    {};
  }
  // open dir "/gif" and play all files in there
  walk_dir( "/gif");

}
