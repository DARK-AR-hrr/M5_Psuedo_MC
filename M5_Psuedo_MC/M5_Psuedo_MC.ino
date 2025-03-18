/*
    Please install FastLED library first.
    In arduino library manage search FastLED

    PSRAMをdisable にすること！
*/
// #include <M5StickCPlus.h>
#define M5STACK_MPU6886
#include <M5Stack.h>

#include "FastLED.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>


FASTLED_USING_NAMESPACE

// FastLED "100-lines-of-code" demo reel, showing just a few 
// of the kinds of animation patterns you can quickly and easily 
// compose using FastLED.  
//
// This example also shows one easy way to define multiple 
// animations patterns and have them automatically rotate.
//
// -Mark Kriegsman, December 2014

#if defined(FASTLED_VERSION) && (FASTLED_VERSION < 3001000)
#warning "Requires FastLED 3.1 or later; check github for latest code."
#endif

//  white -> green
#define DATA_PIN 17
//  yellow -> yello
#define CLK_PIN  16
#define LED_TYPE    APA102
#define COLOR_ORDER BGR
#define NUM_LEDS    144
CRGB leds[NUM_LEDS];


#define M5STACKFIRE_MICROPHONE_PIN 34

#define BRIGHTNESS          30
#define FRAMES_PER_SECOND  480

// -- The core to run FastLED.show()
#define FASTLED_SHOW_CORE 0

// -- Task handles for use in the notifications
static TaskHandle_t FastLEDshowTaskHandle = 0;
static TaskHandle_t userTaskHandle = 0;

static uint16_t dtime;

float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;

int micValue;
int curBrightness;

// color for special regcognition
//CRGB c = CRGB(64,128,0);
CRGB c = CRGB(16,64,0);
//CRGB c = CRGB(24,48,0);
CRGB b = CRGB(0, 0, 0);
CRGB o = CRGB(16,8,0);
CRGB p = CRGB(93,0,128);
CRGB y = CRGB(128,64,0);


/*
int line[8][8] = {{1,0, 0,0, 0,1, 1,0}
      , {1,1, 0,0, 0,0, 1,0}
      , {1,1, 1,0, 0,0, 0,1}
      , {1,0, 1,1, 0,0, 0,1}
      , {1,0, 0,1, 1,0, 0,1}
      , {1,0, 0,0, 1,1, 1,1}
      , {1,0, 0,0, 0,1, 0,0}
      , {1,0, 0,0, 0,0, 0,0}};
*/


// image array
int height =  32;
int width =  32;

//#include "ledpattern.h"
//#include "linearray.h"


/** show() for ESP32
 *  Call this function instead of FastLED.show(). It signals core 0 to issue a show, 
 *  then waits for a notification that it is done.
 */
void FastLEDshowESP32()
{
    if (userTaskHandle == 0) {
        // -- Store the handle of the current task, so that the show task can
        //    notify it when it's done
        userTaskHandle = xTaskGetCurrentTaskHandle();

        // -- Trigger the show task
        xTaskNotifyGive(FastLEDshowTaskHandle);

        // -- Wait to be notified that it's done
        // const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 200 );
        const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 50 );
        ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
        userTaskHandle = 0;
    }
}

/** show Task
 *  This function runs on core 0 and just waits for requests to call FastLED.show()
 */
void FastLEDshowTask(void *pvParameters)
{
    // -- Run forever...
    for(;;) {
        // -- Wait for the trigger
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // -- Do the show (synchronously)
        FastLED.show();

        // -- Notify the calling task
        xTaskNotifyGive(userTaskHandle);
    }
}

void setup() {
  M5.begin();
  Wire.begin();
  delay(3000); // 3 second delay for recovery
Serial.begin(115200);

  pinMode(DATA_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);

  M5.IMU.Init();
  M5.Lcd.setRotation(3);
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextSize(2);

  initBLE();


  // M5.IMU.getAccelData(&accX, &accY, &accZ);

  dtime = 0;
  
  // tell FastLED about the LED strip configuration
  //FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.addLeds<LED_TYPE,DATA_PIN,CLK_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // set master brightness control
  FastLED.setBrightness(BRIGHTNESS);
  curBrightness = BRIGHTNESS;

  int core = xPortGetCoreID();
  Serial.print("Main code running on core ");
  Serial.println(core);

    // -- Create the FastLED show task
    xTaskCreatePinnedToCore(FastLEDshowTask, "FastLEDshowTask", 2048, NULL, 2, &FastLEDshowTaskHandle, FASTLED_SHOW_CORE);
}


// List of patterns to cycle through.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
//SimplePatternList gPatterns = { rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm };
//SimplePatternList gPatterns = {showImgN1, showImgN2,showImgN3, showImgN4, showImgN5, redBlack};
SimplePatternList gPatterns = {rainbow, encManchesterByteA, rainbowWithGlitter, encManchesterByteY, confetti, encManchesterByteV};

uint8_t gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns
uint8_t chg = 0;
uint8_t pchg =0;

uint8_t updatePatternNumber = 0;
bool updatePattern = false;

void loop()
{

  M5.update();
  if(updatePattern){
    gCurrentPatternNumber = updatePatternNumber;
    EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically
    updatePattern = false;
  }
  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPatternNumber]();

  int mic = analogRead(M5STACKFIRE_MICROPHONE_PIN);
  micValue = (mic - 1900) / 2 + 50; 

  // M5.IMU.getAccelData(&accX, &accY, &accZ);
  M5.Lcd.setCursor(10, 0);
  M5.Lcd.setTextColor(TFT_LIGHTGREY);

 
 // M5.Lcd.print("FastLED IMU\r\n");
 // M5.Lcd.setCursor(30, 30);
  //M5.Lcd.setTextColor(WHITE);
 // M5.Lcd.println(" X    Y    Z");
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(30,40);
  M5.Lcd.printf("                                    ");
  M5.Lcd.setCursor(30,40);
  M5.Lcd.printf("%6d %6.2f %6.2f        ", micValue, accY, accZ);

  loopBLE();

  // send the 'leds' array out to the actual LED strip
  FastLEDshowESP32();
  // FastLED.show();
  // insert a delay to keep the framerate modest
  FastLED.delay(1000/FRAMES_PER_SECOND); 

  // do some periodic updates
  EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow
  EVERY_N_SECONDS( 10 ) { nextPattern(); } // change patterns periodically
  EVERY_N_MILLISECONDS(500) { chg++; }
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

int interval = 20;
int blanktime = 800;
int offset = 100;
int usedLed = 32;


void nextPattern()
{
  // add one to the current pattern number, and wrap around at the end
  gCurrentPatternNumber = (gCurrentPatternNumber + 1) % ARRAY_SIZE( gPatterns);
}

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random16(NUM_LEDS);
//  
  leds[pos] += CHSV( gHue + random8(64), 200, micValue);
}


void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );

  micValue = analogRead(M5STACKFIRE_MICROPHONE_PIN);
  leds[pos] += CHSV( gHue, micValue%255, 192);
}


byte dataToSend = 'A';
int bitDuration = 66; // duration for 30fps

void showManchesterBit(bool bit)
{
  if (bit) {
    for (int i=0; i < NUM_LEDS; i++){
      if(i %5 == 0) {leds[i] = c;} 
      else {leds[i] = 0;}
    }
    FastLED.delay(bitDuration);
    for (int i=0; i < NUM_LEDS; i++){
      if(i %5 == 0) {leds[i] = p;} 
      else {leds[i] = 0;}
    }
    FastLED.delay(bitDuration);
  } else {
    for (int i=0; i < NUM_LEDS; i++){
      if(i%5 == 0) {leds[i] = c;}
      else {leds[i] = 0;}
    }
    FastLED.delay(bitDuration);
    for (int i=0; i < NUM_LEDS; i++){
      if(i %5 == 0) {leds[i] = o;} 
      else {leds[i] = 0;}
    }
    FastLED.delay(bitDuration);
  }  
  
}


void encManchesterByteA()
{
  dataToSend = 'A';
  for (int i=0; i < NUM_LEDS; i++){
     if(i %5 == 0) {leds[i] = c;} 
      else {leds[i] = 0;}
  }
  FastLED.delay(bitDuration);
  for (int i = 7; i >= 0; i--) {
    bool bit = (dataToSend >> i) & 1;
    showManchesterBit(bit);
  }
  for (int i=0; i < NUM_LEDS; i++){
       if(i %5 == 0) {leds[i] = c;} 
      else {leds[i] = 0;}
  }
  FastLED.delay(bitDuration);
}

void encManchesterByteY()
{
  dataToSend = 'Y';

  for (int i=0; i < NUM_LEDS; i++){
      if(i %5 == 0) {leds[i] = c;} 
      else {leds[i] = 0;}
  }
  FastLED.delay(bitDuration);
  for (int i = 7; i >= 0; i--) {
    bool bit = (dataToSend >> i) & 1;
    showManchesterBit(bit);
  }
  for (int i=0; i < NUM_LEDS; i++){
      if(i %5 == 0) {leds[i] = c;} 
      else {leds[i] = 0;}
  }
  FastLED.delay(bitDuration);
}

void encManchesterByteV()
{
  dataToSend = 'V';

  for (int i=0; i < NUM_LEDS; i++){
      if(i %5 == 0) {leds[i] = c;} 
      else {leds[i] = 0;}
  }
  FastLED.delay(bitDuration);
  for (int i = 7; i >= 0; i--) {
    bool bit = (dataToSend >> i) & 1;
    showManchesterBit(bit);
  }
  for (int i=0; i < NUM_LEDS; i++){
      if(i %5 == 0) {leds[i] = c;} 
      else {leds[i] = 0;}
  }
  FastLED.delay(bitDuration);
}


void redWhite()
{

  // fill_noise8 (CRGB *leds, int num_leds, uint8_t octaves, uint16_t x, int scale, uint8_t hue_octaves, uint16_t hue_x, int hue_scale, uint16_t time)
// fill_noise8(leds, NUM_LEDS, 3,dtime*3, 4, 4, 1, 3, dtime);
// dtime+=1; 
  M5.IMU.getAccelData(&accX, &accY, &accZ);

  int x = abs(accX * 100);
  int y = abs(accY * 100);
  int z = abs(accZ * 100);
  if(chg != pchg){
    for (int i = 0; i < NUM_LEDS; i++){
      leds[i] = CRGB(random(20,128), 0,0);
    }
    pchg = chg;
  }
  for(int j = 0; j < NUM_LEDS/3; j++){
    uint32_t whitedots = random(y, z);
    leds[random(0,NUM_LEDS)] = CRGB(whitedots,0,0);
  }
  for(int j = 0; j < NUM_LEDS/4; j++){
    //uint32_t whitedots = random(x, z)%128;
//    leds[random(0,NUM_LEDS)] = CRGB(0,0,0);
  }
  
}

void redBlack()
{

  // fill_noise8 (CRGB *leds, int num_leds, uint8_t octaves, uint16_t x, int scale, uint8_t hue_octaves, uint16_t hue_x, int hue_scale, uint16_t time)
// fill_noise8(leds, NUM_LEDS, 3,dtime*3, 4, 4, 1, 3, dtime);
// dtime+=1; 
  M5.IMU.getAccelData(&accX, &accY, &accZ);

  uint32_t x = (uint32_t)(accX * 100);
  uint32_t y = (uint32_t)(accY * 100);
  uint32_t z = (uint32_t)(accZ * 100);
  for (int i = 0; i < usedLed; i++){
    leds[offset + i] = CRGB(random(20,x%128), 0,0);
  }
  for(int j = 0; j < usedLed; j++){
    //uint32_t whitedots = random(x, z)%128;
    leds[offset + random(0,NUM_LEDS)] = CRGB(0,0,0);
  }
  
}


void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}


// BLE 
BLEServer *pServer = NULL;
BLECharacteristic * pRxCharacteristic;\
BLECharacteristic * pNotifyCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

#define LOCAL_NAME                  "M5Stack-AR"
// See the following for generating UUIDs:
// https://www.uuidgenerator.net/


#define SERVICE_UUID                "050de1f2-2199-43ab-8bdd-41ce8ea808a6"
#define CHARACTERISTIC_UUID_RX      "dc50b05e-d109-49d8-84a0-d33db79cb3b0"
#define CHARACTERISTIC_UUID_NOTIFY  "ad349df6-5197-416c-bfdb-de2b8d89fd5d"

// Bluetooth LE Change Connect State
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

// Bluetooth LE Recive
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        String cmd = String(rxValue.c_str());
        Serial.print("Received Value: ");
        Serial.println(cmd);
        //M5.Lcd.setCursor(50,80);
        //M5.Lcd.printf("%s", rxValue.c_str());

        if (cmd == "A")
        {
          updatePatternNumber =0;
          updatePattern = true;
        }
        if (cmd == "Y")
        {
          updatePatternNumber = 3;
          updatePattern = true;
        }
        if (cmd == "V")
        {
          updatePatternNumber = 5;
          updatePattern = true;
          // BLUE
          //lastColor = "BLUE";
          //updateColor = true;
        }
        if (cmd =="+"){
          curBrightness += 1;
          FastLED.setBrightness(curBrightness);
        }
        if (cmd == "-"){
          curBrightness -= 1;
          FastLED.setBrightness(curBrightness);
        }
      }
    }
};

// Bluetooth LE initialize
void initBLE() {
  // Create the BLE Device
  BLEDevice::init(LOCAL_NAME);

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  // pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pNotifyCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID_NOTIFY,
                        BLECharacteristic::PROPERTY_NOTIFY
                        );
  
  pNotifyCharacteristic->addDescriptor(new BLE2902());

  pRxCharacteristic = pService->createCharacteristic(
                       CHARACTERISTIC_UUID_RX,
                      BLECharacteristic::PROPERTY_WRITE
                    );

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // iOS互換のための設定
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("BLE advertising");

  //pServer->getAdvertising()->start();
}

// Bluetooth LE loop
void loopBLE() {
    // disconnecting
    /*
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("startAdvertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
    // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
    */
    //delay(500); 
}
