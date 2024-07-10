#include <Arduino.h>
#include "InterruptButton.h"
#include <Preferences.h>
#include <midiXparser.h>

#include <WVR.h>
#include <wvr_pins.h>
#include <ws_log.h>
#include <midi_in.h>
#include <wav_player.h>
#include <rpc.h>
#include <rgb.h>
#include <file_system.h>
#include <wvr_0.3.h>
#include <gpio.h>

#include <U8g2lib.h>

// #define RGB D9
#define RGB D5

#define SCK D11
#define SDA D12
#define RST D13
#define CS D3
#define DC D4

#define BUTTON_1 D8
#define BUTTON_2 D7
#define BUTTON_3 D10

WVR wvr;

U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R2, /* clock=*/ SCK, /* data=*/ SDA, /* cs=*/ CS, /* dc=*/ DC,  /* reset=*/ RST);

// Preferences preferences;

const char* prefs_key = "wvr";

const char* prefs_wifi_on_key = "wifi";
const char* prefs_bank_key = "bank";
const char* prefs_volume_key = "vol";

bool prefs_wifi_on = true;
int prefs_bank = 1;
int prefs_volume = 100;

bool should_update_wifi_on = false;
bool should_update_bank = false;
bool should_update_volume = false;

uint8_t volume = 100;

InterruptButton *Left;
InterruptButton *Right;
InterruptButton *Center;

#define NUM_SCREENS 2
#define NUM_ITEMS_HOME 2
#define NUM_ITEMS_SETTINGS 2

uint8_t screen = 0;
uint8_t item = 0;
uint8_t channel = 0;

QueueHandle_t rgb_flasher_h;

#define RGB_EVT_MIDI 0
#define RGB_EVT_PRESS_LEFT 1
#define RGB_EVT_PRESS_CENTER 2
#define RGB_EVT_PRESS_RIGHT 3
#define FLASHER_PERIOD 40

static const uint8_t note_on_evt = RGB_EVT_MIDI;
static const uint8_t press_left_evt = RGB_EVT_PRESS_LEFT;
static const uint8_t press_center_evt = RGB_EVT_PRESS_CENTER;
static const uint8_t press_right_evt = RGB_EVT_PRESS_RIGHT;

uint32_t last_midi = 0;
uint32_t last_left = 0;
uint32_t last_center = 0;
uint32_t last_right = 0;

static void rgb_unflasher(void *arg){
  while(1){
    if((last_midi != 0) && (xTaskGetTickCount() > (last_midi + FLASHER_PERIOD))){
        rgb_set_led(1, 0, 0, 0);
        last_midi = 0;
    }
    if((last_left != 0) && (xTaskGetTickCount() > (last_left + FLASHER_PERIOD))){
        rgb_set_led(2, 0, 0, 0);
        last_left = 0;
    }
    if((last_center != 0) && (xTaskGetTickCount() > (last_center + FLASHER_PERIOD))){
        rgb_set_led(1, 0, 0, 0);
        last_center = 0;
    }
    if((last_right != 0) && (xTaskGetTickCount() > (last_right + FLASHER_PERIOD))){
        rgb_set_led(0, 0, 0, 0);
        last_right = 0;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

static void rgb_flasher(void *arg){
  while(1){
    uint8_t evt;
    if(xQueueReceive(rgb_flasher_h, &evt, portMAX_DELAY)){
      if(evt == RGB_EVT_MIDI){
        rgb_set_led(1, 0x01, 0x01, 0x01);
        last_midi = xTaskGetTickCount();
      } else if(evt == RGB_EVT_PRESS_LEFT){
        rgb_set_led(2, 0x66, 0x44, 0x11);
        last_left = xTaskGetTickCount();
      } else if(evt == RGB_EVT_PRESS_CENTER){
        rgb_set_led(1, 0x66, 0x44, 0x11);
        last_center = xTaskGetTickCount();
      } else if(evt == RGB_EVT_PRESS_RIGHT){
        rgb_set_led(0, 0x66, 0x44, 0x11);
        last_right = xTaskGetTickCount();
      }
    }
  }
}

/*

SETUP THE BUTTON LIBRARY SAVED IN BOOKMARKS to detect held presses and long presses
https://github.com/rwmingis/InterruptButton

*/

void draw_screen(void)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);

  if(screen == 0) // home screen
  {
    // voice num
    uint8_t voice = wvr.getBank();
    char program_number[12];
    sprintf(program_number, "bank #%d", voice + 1);
    u8g2.drawStr(5, 15, program_number);

    // voice name
    u8g2.drawStr(5, 27, wvr.getBankName(voice));

    // volume number
    char volume_str[10];
    sprintf(volume_str, "%d", volume);
    u8g2.drawStr(5, 53, volume_str);

    // volume bar
    float vol_f = (float) volume / 100.0;
    float width_f = 90.0 * vol_f;
    uint8_t width = (uint8_t)width_f;
    u8g2.drawBox(30, 45, width, 10);

    // midi channel
    char channel_string[3];
    sprintf(channel_string, "%d", channel + 1);
    u8g2.drawStr(110, 34, channel_string);

    // wifi state
    u8g2.drawStr(110, 15, wvr.wifiIsOn ? "W" : "---");


    // selection box
    if(item == 0) // voice
    {
      u8g2.drawFrame(1, 1, 105, 32);
    }
    else if(item == 1) // volume
    {
      u8g2.drawFrame(1, 43, 127, 15);
    }
    else if(item == 2) // volume
    {
      u8g2.drawFrame(105, 1 ,22, 20);
    }
    else if(item == 3) // volume
    {
      u8g2.drawFrame(105, 21, 22, 20);
    }
  }
  else if (screen == 1) // wifi
  {
      u8g2.drawFrame(1 ,1 , 127, 63);
      // u8g2.drawFrame(3 ,4 , 125, 61);
      if(wvr.wifiIsOn){
        u8g2.drawStr(5, 15, "   * WiFi is on *    ");
      } else {
        u8g2.drawStr(5, 15, "   * WiFi is off *   ");
      }
      u8g2.drawStr(5, 25,   "network: octadeca    ");
      u8g2.drawStr(5, 35,   "password: ultrapalace");
      u8g2.drawStr(5, 45,   "http://192.168.4.1   ");
      // if(wvr.wifiIsOn){
      //   u8g2.drawStr(5, 60,   "press ▶ for WiFi on ");
      // } else {
      //   u8g2.drawStr(5, 60,   "press ◀ for WiFi off ");
      // }    
  }

  u8g2.sendBuffer();	
}

void handleLeft(void){
  if(screen == 0) // home
  {
    if(item == 0) // program
    {
      uint8_t bank = wvr.getBank();
      bank = bank > 0 ? bank - 1 : 0;
      wvr.setBank(bank);
      should_update_bank = true;
    }
    else if(item == 1) // volume
    {
      float vol_a = (100.0 / 127.0) * volume;
      if(volume > 0){
        volume -= 1;
        wvr.setGlobalVolumePercent(volume);
        should_update_volume = true;
      }
    }
    else if(item == 2)
    {
      wvr.toggleWifi();
    }
    else if(item == 3)
    {
      channel -= (channel > 0);
    }
  }
  xQueueSendToBack(rgb_flasher_h, &press_left_evt, 0);
  draw_screen();
}

void handleCenter(void){
  if(screen == 0) // home screen
  {
    item++;
    item = item > 3 ? 0 : item;
  }
  xQueueSendToBack(rgb_flasher_h, &press_center_evt, 0);
  draw_screen();
}

void handleRight(void){
  if(screen == 0) // home screen
  {
    if(item == 0) // program
    {
      uint8_t bank = wvr.getBank();
      bank = bank < (NUM_BANKS - 1) ? bank + 1 : 0;
      wvr.setBank(bank);
      should_update_bank = true;
    }
    else if(item == 1) // volume
    {
      if(volume < 100){
        volume += 1;
        wvr.setGlobalVolumePercent(volume);
        should_update_volume = true;
      }
    }
    else if(item == 2) // wifi
    {
      wvr.toggleWifi();
    }
    else if(item == 3) // channel
    {
      channel += (channel < 15);
    }
  }
  // else if(screen == 1) // settings screen
  // {
  //   item += (item < (NUM_ITEMS_SETTINGS - 1));
  // }
  xQueueSendToBack(rgb_flasher_h, &press_right_evt, 0);
  draw_screen();
}

void handleLongPress(void){
  screen = screen == 1 ? 0 : 1;
  draw_screen();
}

void onMidi(uint8_t *msg){
  uint8_t code = (msg[0] >> 4) & 0b00001111;
  if(code == MIDI_NOTE_ON){
    xQueueSendToBack(rgb_flasher_h, &note_on_evt, 0);
  }
}

void setup() {
  // preferences.begin(prefs_key, false);

  // prefs_wifi_on = preferences.getBool(prefs_wifi_on_key, false);
  // prefs_bank = preferences.getInt(prefs_bank_key, 0);
  // prefs_volume = preferences.getInt(prefs_volume_key, 100);

  // log_i("main", "\nprefs\nwifi-on: %s\nbank: %d\nvolume: %d", prefs_wifi_on?"true":"false", prefs_bank, prefs_volume);

  rgb_flasher_h = xQueueCreate(1024, 1);
  wvr.useFTDI = true;
  wvr.useUsbMidi = true;
  wvr.begin();
  // wvr.wifiOn();

  // if(prefs_wifi_on){
  //   wvr.wifiOn();
  //   wvr.wifiIsOn = true;
  //   should_update_wifi_on = true;
  // } else {
  //   wvr.wifiOff();
  //   wvr.wifiIsOn = false;
  //   should_update_wifi_on = true;
  // }
  volume = prefs_volume;
  wvr.setGlobalVolumePercent(volume);
  wvr.setBank(prefs_bank);

  // screen
  wvr.resetPin(SCK);
  detachInterrupt(SCK);
  wvr.resetPin(SDA);
  detachInterrupt(SDA);
  wvr.resetPin(RST);
  detachInterrupt(RST);
  wvr.resetPin(CS);
  detachInterrupt(CS);
  wvr.resetPin(DC);
  detachInterrupt(DC);

  // leds
  wvr.resetPin(D9); // bodged together with D5 RGB
  detachInterrupt(D9);
  wvr.resetPin(RGB);
  detachInterrupt(RGB);
  rgb_init(RGB);

  u8g2.begin();
  draw_screen();

  // switches
  wvr.resetPin(BUTTON_1);
  detachInterrupt(BUTTON_1);
  wvr.resetPin(BUTTON_2);
  detachInterrupt(BUTTON_2);
  wvr.resetPin(BUTTON_3);
  detachInterrupt(BUTTON_3);
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);

  Left = new InterruptButton(BUTTON_3, LOW);
  Center = new InterruptButton(BUTTON_2, LOW);
  Right = new InterruptButton(BUTTON_1, LOW);

  Left->bind(Event_KeyPress, handleLeft);
  Center->bind(Event_KeyPress, handleCenter);
  Right->bind(Event_KeyPress, handleRight);

  Center->bind(Event_LongKeyPress, handleLongPress);

  Left->bind(Event_AutoRepeatPress, handleLeft);
  Left->bind(Event_LongKeyPress, [](){return;});
  Left->setAutoRepeatInterval(50);
  Left->setLongPressInterval(500);

  Right->bind(Event_AutoRepeatPress, handleRight);
  Right->bind(Event_LongKeyPress, [](){return;});
  Right->setAutoRepeatInterval(50);
  Right->setLongPressInterval(500);

  wvr.setMidiHook(onMidi);

  uint8_t w_volume = wvr.getGlobalVolume();
  float f_volume = (100.0 / 127.0) * w_volume;
  volume = round(f_volume);

  rgb_set_led(0, 100, 0, 0);
  vTaskDelay(100);
  rgb_set_led(1, 100, 0, 0);
  vTaskDelay(100);
  rgb_set_led(2, 100, 0, 0);
  vTaskDelay(100);

  rgb_set_led(0, 0, 100, 0);
  vTaskDelay(100);
  rgb_set_led(1, 0, 100, 0);
  vTaskDelay(100);
  rgb_set_led(2, 0, 100, 0);
  vTaskDelay(100);

  rgb_set_led(0, 0, 0, 100);
  vTaskDelay(100);
  rgb_set_led(1, 0, 0, 100);
  vTaskDelay(100);
  rgb_set_led(2, 0, 0, 100);

  vTaskDelay(100);
  rgb_set_led(0, 0, 0, 0);
  rgb_set_led(1, 0, 0, 0);
  rgb_set_led(2, 0, 0, 0);

  xTaskCreatePinnedToCore(rgb_flasher, "rgb_flasher", 1024, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(rgb_unflasher, "rgb_unflasher", 1024 * 2, NULL, 1, NULL, 1);
}

void loop() {
  // vTaskDelete(NULL);
    // if(should_update_wifi_on){
    //   should_update_wifi_on = false;
    //   preferences.putBool(prefs_wifi_on_key, wvr.wifiIsOn);
    // }
    // if(should_update_bank){
    //   should_update_bank = false;
    //   preferences.putInt(prefs_bank_key, wvr.getBank());
    // }
    // if(should_update_volume){
    //   should_update_volume = false;
    //   preferences.putInt(prefs_volume_key, volume);
    // }
    vTaskDelay(3000 / portTICK_PERIOD_MS);
}