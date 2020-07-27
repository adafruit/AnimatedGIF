#include <ESP32-Chimera-Core.h> // https://github.com/tobozo/ESP32-Chimera-Core or regular M5Stack Core
#define tft M5.Lcd // syntax sugar

#ifndef M5STACK_SD
 // for custom ESP32 builds
 #define M5STACK_SD SD
#endif

#include "AnimatedGIF.h"

AnimatedGIF gif;

// used to center image based on GIF dimensions
static int xOffset = 0;
static int yOffset = 0;

static int totalFiles = 0; // GIF files count

static File FSGifFile; // temp gif file holder
static File GifRootFolder; // directory listing

std::vector<std::string> GifFiles; // GIF files path


static void MyCustomDelay( unsigned long ms ) {
  delay( ms );
  //Serial.printf("delay %d\n", ms);
}


static void * GIFOpenFile(char *fname, int32_t *pSize) {
  //Serial.printf("GIFOpenFile( %s )\n", fname );
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
  else
     Serial.println("Can't close file");
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
  //Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
}


static void TFTDraw(int x, int y, int w, int h, uint16_t* lBuf ) {
  tft.pushRect( x+xOffset, y+yOffset, w, h, lBuf );
}


int gifPlay( char* gifPath ) { // 0=infinite

  gif.begin();
  gif.setFSCallbacks( GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, TFTDraw, MyCustomDelay );
  //pfnDelay = MyCustomDelay;

  if( ! gif.open( gifPath ) ) {
    Serial.printf("Could not open gif %s\n", gifPath );
    return 0;
  }
  // center the GIF !!
  int w = gif.getCanvasWidth();
  int h = gif.getCanvasHeight();
  xOffset = ( tft.width()  - w )  /2;
  yOffset = ( tft.height() - h ) /2;

  Serial.printf("%s : w/h: [%d,%d] = offset [%d,%d]\n", gifPath, w, h, xOffset, yOffset );

  int *frameDelay;
  int then = 0;

  while (gif.playFrame(true, frameDelay)) {
    then += *frameDelay;
  }

  gif.close();

  return then;
}


int getGifInventory( const char* basePath ) {
  int amount = 0;
  GifRootFolder = M5STACK_SD.open(basePath);
  if(!GifRootFolder){
    log_e("Failed to open directory");
    return 0;
  }

  if(!GifRootFolder.isDirectory()){
    log_e("Not a directory");
    return 0;
  }

  File file = GifRootFolder.openNextFile();

  tft.setTextColor( TFT_WHITE, TFT_BLACK );
  tft.setTextSize( 2 );

  int textPosX = tft.width()/2 - 16;
  int textPosY = tft.height()/2 - 10;

  tft.drawString("GIF Files:", textPosX-40, textPosY-20 );

  while( file ) {
    if(!file.isDirectory()) {
      GifFiles.push_back( file.name() );
      amount++;
      tft.drawString(String(amount), textPosX, textPosY );
      file.close();
    }
    file = GifRootFolder.openNextFile();
  }
  GifRootFolder.close();

  return amount;
}




void setup() {
  M5.begin();

  if(!M5STACK_SD.begin()) {
    Serial.println("SD Card mount failed!");
    return;
  } else {
    Serial.println("SD Card mounted!");
  }

  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setSwapBytes( true ); // compensate for default big endian format

  totalFiles = getGifInventory( "/gif" ); // scan the SD card GIF folder

}


int currentFile = 0;

void loop() {

  tft.clear();

  const char * fileName = GifFiles[currentFile++%totalFiles].c_str();

  int loops = 5; // max 5 loops
  int durationControl = 3000; // stop loops after 3s

  while(loops-->0 && durationControl > 0 ) {
    durationControl -= gifPlay( (char*)fileName );
  }

}
