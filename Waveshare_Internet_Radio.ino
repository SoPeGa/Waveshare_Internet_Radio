#include <WiFiManager.h>
#include <HTTPClient.h>
#include <Audio.h>
#include <TFT_eSPI.h>   // TFT display library
#include "Touch_CST328.h"
#include <Preferences.h>  // For non-volatile storage
#include "wave_image.h"

// WiFi and radio setup
WiFiManager myWiFi;

// Radio channel URLs
const char* radioURLs[] = {
  "http://stream.otvoreni.hr/otvoreni",
  "http://live.electricfm.com/electricfm",
  "http://stream2.radiotransilvania.ro/Oradea",
  "http://stream2.radiotransilvania.ro/Nagyvarad",
  "http://icecast.luxnet.ua/lux",
  "http://nl.ah.fm/live",
  "http://s4-webradio.rockantenne.de/90er-rock/stream/mp3",
  "https://punk.stream.laut.fm/punk",
  "http://edge76.rdsnet.ro:84/digifm/digifm.mp3",
  "http://edge126.rdsnet.ro:84/profm/profm.mp3",
  "http://edge126.rdsnet.ro:84/profm/dancefm.mp3",
  "http://144.76.106.52:7000/chillout.mp3",
  "https://streaming.radiostreamlive.com/radionylive_devices",
  "http://stream.104.6rtl.com/dance-hits/mp3-192/",
  "http://stream.radioparadise.com/mp3-192",
  "http://icecast.omroep.nl/radio2-bb-mp3",
  "http://streaming.exclusive.radio/er/scorpions/icecast.audio"

  
};

const int numberOfChannels = sizeof(radioURLs) / sizeof(radioURLs[0]);
int currentChannel = 0;
int volume = 4;

// ---------------------
// Scrolling Text Setup
// ---------------------
String streamTitle = "Unknown";
String stationName = "Unknown";
int titleScrollX = 0;
unsigned long lastScrollTime = 0;
int titleScrollDelay = 40;
String scrollTitle = "";

// ---------------------
// Non-Volatile Storage
// ---------------------
Preferences preferences;

// ---------------------
// I2S / Audio Pin Setup
// ---------------------
#define I2S_DOUT 47
#define I2S_BCLK 48
#define I2S_LRC 38

// ---------------------
// Touch Screen Setup
// ---------------------
#define I2C_SCL_PIN       10
#define I2C_SDA_PIN       11
TwoWire I2C = TwoWire(1); 

void I2C_Init(void) {
  I2C.begin( I2C_SDA_PIN, I2C_SCL_PIN);                       
}

// ---------------------
// TFT and Touch Objects
// ---------------------
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite scrollSprite = TFT_eSprite(&tft);


// ---------------------
// Color Defines
// ---------------------
#define TFT_DARKBLUE   0x0000
#define TFT_DARKGREEN  0x03E0
#define TFT_DARKGREY   0x7BEF

// ---------------------
// Audio Object
// ---------------------
Audio audio;

// ---------------------
// Touch Timing
// ---------------------
unsigned long lastTouchCheck = 0;
const unsigned long touchCheckInterval = 50; // ms

// ---------------------
// Slider Definitions
// ---------------------
const int SLIDER_X = 10;
const int SLIDER_HEIGHT = 20;
const int MAX_VOLUME = 21;

// ---------------------
// UI Layout Constants
// ---------------------
#define WAVE_IMAGE_Y 115
#define CHANNEL_BUTTONS_Y 180
#define CHANNEL_BUTTON_WIDTH 20
#define CHANNEL_BUTTON_HEIGHT 50
#define PREV_BUTTON_X 10
#define NEXT_BUTTON_X (tft.width() - 10 - CHANNEL_BUTTON_WIDTH)
#define VOLUME_TEXT_Y_OFFSET 25
#define UI_VOL_TEXT_CLEAR_H 30
#define STATION_INFO_Y 15
#define STATION_INFO_H 70
#define STATION_NAME_CURSOR_Y 30
#define SCROLL_TEXT_Y 80
#define SCROLL_TEXT_H 30
#define SCROLL_TEXT_SPEED 2
#define SCROLL_TEXT_SPACING 50
#define TOUCH_DEBOUNCE_MS 200


// The slider is placed at Y = 280.
int getSliderWidth() {
  return tft.width() - 20;  // 10 pixels padding on each side
}

int getSliderY() {
  return 290;
}

// ---------------------
// Function Prototypes
// ---------------------
void drawWaveImage();  // We'll define this to draw your wave image
void drawVolumeSlider();
void drawChannelTriangles();
void displayVolume();
void audio_showstation(const char* info);
void audio_showstreamtitle(const char* info);
void scrollStreamTitle();
void connectToRadio(const char* url);
void changeChannel(int direction);
void show_Message_No_Connection(WiFiManager* myWiFi);
void checkTouch();

// ---------------------
// Draw the static image
// ---------------------
void drawWaveImage() {
  // Example: place the image so it sits above the channel buttons (Y ~ 150–160).
  // Adjust (x, y) as needed. 
  int x = (tft.width() - waveImageWidth) / 2;  // center horizontally
  int y = WAVE_IMAGE_Y;                                 // place above the buttons (which start at ~200)
  
  // Push the raw image data to the screen
  // pushImage(x, y, width, height, dataPointer);
  tft.pushImage(x, y, waveImageWidth, waveImageHeight, waveImage);
}

// ---------------------
// UI Drawing Functions
// ---------------------
void drawVolumeSlider() {
  int sliderWidth = getSliderWidth();
  int sliderY = getSliderY();
  
  // Slider background
  tft.fillRect(SLIDER_X, sliderY, sliderWidth, SLIDER_HEIGHT, TFT_DARKGREY);
  
  // Filled portion
  int filledWidth = map(volume, 0, MAX_VOLUME, 0, sliderWidth);
  tft.fillRect(SLIDER_X, sliderY, filledWidth, SLIDER_HEIGHT, TFT_GREEN);
  
  // Border
  tft.drawRect(SLIDER_X, sliderY, sliderWidth, SLIDER_HEIGHT, TFT_WHITE);
}

// Draw the "Prev" / "Next" triangles
void drawChannelTriangles() {
  // "Prev" triangle (points left)
  tft.fillTriangle(
    PREV_BUTTON_X, CHANNEL_BUTTONS_Y,  // Tip
    PREV_BUTTON_X + CHANNEL_BUTTON_WIDTH, CHANNEL_BUTTONS_Y - CHANNEL_BUTTON_HEIGHT / 2,  // Top-right
    PREV_BUTTON_X + CHANNEL_BUTTON_WIDTH, CHANNEL_BUTTONS_Y + CHANNEL_BUTTON_HEIGHT / 2,  // Bottom-right
    TFT_BLUE
  );
  // "Next" triangle (points right)
  tft.fillTriangle(
    NEXT_BUTTON_X + CHANNEL_BUTTON_WIDTH, CHANNEL_BUTTONS_Y, // Tip
    NEXT_BUTTON_X, CHANNEL_BUTTONS_Y - CHANNEL_BUTTON_HEIGHT / 2,
    NEXT_BUTTON_X, CHANNEL_BUTTONS_Y + CHANNEL_BUTTON_HEIGHT / 2,
    TFT_BLUE
  );
}

// Display volume number above the slider
void displayVolume() {
  int textY = getSliderY() - VOLUME_TEXT_Y_OFFSET; // 25 px above slider
  tft.fillRect(0, textY, tft.width(), UI_VOL_TEXT_CLEAR_H, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setFreeFont(&FreeMonoBold9pt7b);
  String volStr = "Volume: " + String(volume);
  tft.drawCentreString(volStr, tft.width() / 2, textY, 1);
}

// ---------------------
// Audio Callbacks
// ---------------------
void audio_showstation(const char* info) {
  Serial.print("Station: ");
  Serial.println(info);
  tft.fillRect(0, STATION_INFO_Y, tft.width(), STATION_INFO_H, TFT_BLACK);
  tft.setCursor(0, STATION_NAME_CURSOR_Y);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setFreeFont(&FreeMonoBold9pt7b);
  tft.println(info);
}

void audio_showstreamtitle(const char* info) {
  Serial.print("Stream Title: ");
  Serial.println(info);
  scrollTitle = String(info);
  titleScrollX = tft.width();
  lastScrollTime = millis();
  
  scrollSprite.createSprite(tft.width(), SCROLL_TEXT_H);
  scrollSprite.fillSprite(TFT_BLACK);
  scrollSprite.setTextColor(TFT_YELLOW, TFT_BLACK);
  scrollSprite.setTextSize(1);
  scrollSprite.setFreeFont(&FreeMono9pt7b);
  
  scrollSprite.pushSprite(0, SCROLL_TEXT_Y);
}

// Smoothly scroll the current song title
void scrollStreamTitle() {
    if (scrollTitle.length() == 0) return;

    if (millis() - lastScrollTime > titleScrollDelay) {
        lastScrollTime = millis();

        // Calculate the width of the text
        int16_t textWidth = scrollSprite.textWidth(scrollTitle);

        // If the text is shorter than the screen, just display it centered.
        if (textWidth <= tft.width()) {
            scrollSprite.fillSprite(TFT_BLACK);
            scrollSprite.drawCentreString(scrollTitle, tft.width() / 2, 0, 1);
            scrollSprite.pushSprite(0, SCROLL_TEXT_Y);
            return;
        }

        // The text is wider than the screen, so we scroll it
        titleScrollX -= SCROLL_TEXT_SPEED;
        if (titleScrollX < -textWidth) {
            titleScrollX = tft.width();
        }

        scrollSprite.fillSprite(TFT_BLACK);
        scrollSprite.drawString(scrollTitle, titleScrollX, 0);
        // Draw the text again, shifted by its width, to create a continuous loop
        scrollSprite.drawString(scrollTitle, titleScrollX + textWidth + SCROLL_TEXT_SPACING, 0); // 50 pixels space

        scrollSprite.pushSprite(0, SCROLL_TEXT_Y);
    }
}

// ---------------------
// Radio Control
// ---------------------
void connectToRadio(const char* url) {
  Serial.print("Connecting to radio: ");
  Serial.println(url);
  audio.connecttohost(url);
}

void changeChannel(int direction) {
  currentChannel = (currentChannel + direction + numberOfChannels) % numberOfChannels;
  
  preferences.begin("radio", false);
  preferences.putInt("lastStation", currentChannel);
  preferences.end();
  
  connectToRadio(radioURLs[currentChannel]);
}

// Show message if Wi-Fi is not connected
void show_Message_No_Connection(WiFiManager* myWiFi) {
  tft.fillScreen(TFT_NAVY);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(10, 10);
  tft.setFreeFont(&FreeMonoBold9pt7b);
  tft.println(F("WiFi: no connection.\nConnect to hotspot 'My_Radio'\nand open a browser\nat 192.168.4.1\nto enter network credentials."));
}

// ---------------------
// Touch Handling
// ---------------------
void checkTouch() {
  uint16_t touchpad_x[5] = {0};
  uint16_t touchpad_y[5] = {0};
  uint16_t strength[5] = {0};
  uint8_t touchpad_cnt = 0;

  static unsigned long lastTouchTime = 0;
  const unsigned long debounceDelay = TOUCH_DEBOUNCE_MS; // 200 ms delay for debounce

  // Citire date de la touchpad
  Touch_Read_Data();
  uint8_t touch_detected = Touch_Get_XY(touchpad_x, touchpad_y, strength, &touchpad_cnt, CST328_LCD_TOUCH_MAX_POINTS);

  if (touch_detected && touchpad_cnt > 0) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastTouchTime < debounceDelay) {
      return; // Debounce: Ignore if within delay time
    }
    lastTouchTime = currentMillis;

    uint16_t touchX = touchpad_x[0]; // Coordonata X a primei atingeri detectate
    uint16_t touchY = touchpad_y[0]; // Coordonata Y a primei atingeri detectate
    
    // Check slider area
    int sliderWidth = getSliderWidth();
    int sliderY = getSliderY();
    if (touchY >= sliderY && touchY <= sliderY + SLIDER_HEIGHT &&
        touchX >= SLIDER_X && touchX <= SLIDER_X + sliderWidth) {
      
      int newVolume = map(touchX, SLIDER_X, SLIDER_X + sliderWidth, 0, MAX_VOLUME);
      if (newVolume != volume) {
        volume = newVolume;
        audio.setVolume(volume);
        displayVolume();
        drawVolumeSlider();
      }
      return;
    }
    
    // Check "Prev" bounding box
    if (touchX >= PREV_BUTTON_X && touchX <= (PREV_BUTTON_X + CHANNEL_BUTTON_WIDTH) &&
        touchY >= (CHANNEL_BUTTONS_Y - CHANNEL_BUTTON_HEIGHT / 2) && touchY <= (CHANNEL_BUTTONS_Y + CHANNEL_BUTTON_HEIGHT / 2)) {
      changeChannel(-1);
      return;
    }
    
    // Check "Next" bounding box
    if (touchX >= NEXT_BUTTON_X && touchX <= (NEXT_BUTTON_X + CHANNEL_BUTTON_WIDTH) &&
        touchY >= (CHANNEL_BUTTONS_Y - CHANNEL_BUTTON_HEIGHT / 2) && touchY <= (CHANNEL_BUTTONS_Y + CHANNEL_BUTTON_HEIGHT / 2)) {
      changeChannel(1);
      return;
    }
  }
}

// ---------------------
// Setup & Main Loop
// ---------------------
void setup() {
  Serial.begin(115200);
  
  // TFT setup
  tft.init();
  tft.setSwapBytes (true);  // swap the byte order for pushImage() - corrects endianness
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setFreeFont(&FreeMonoBold9pt7b);
  
  // Touch init
  Touch_Init();
  
  // Wi-Fi setup
  myWiFi.setAPCallback(show_Message_No_Connection);
  myWiFi.autoConnect("My_Radio");
  
  // Load last station from NVS
  preferences.begin("radio", true);
  currentChannel = preferences.getInt("lastStation", 0);
  preferences.end();
  
  // Audio init
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(volume);
  connectToRadio(radioURLs[currentChannel]);
  
  // Draw initial UI
  displayVolume();
  drawVolumeSlider();
  drawChannelTriangles();
  
  // Draw your static wave image
  drawWaveImage();
}

void loop() {
  unsigned long currentMillis = millis();
  
  // Touch check at intervals
  if (currentMillis - lastTouchCheck >= touchCheckInterval) {
    checkTouch();
    lastTouchCheck = currentMillis;
  }
  
  // Audio streaming + scroll the song title
  audio.loop();
  scrollStreamTitle();
}