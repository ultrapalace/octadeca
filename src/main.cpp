#include <Arduino.h>
#include <wvr_pins.h>
#include <button_struct.h>
#include <ws_log.h>
#include <WVR.h>
#include <midiXparser.h>
#include <midi_in.h>
#include <wav_player.h>
#include <button.h>
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

U8G2_SH1106_128X64_NONAME_F_4W_SW_SPI u8g2(U8G2_R2, /* clock=*/ SCK, /* data=*/ SDA, /* cs=*/ CS, /* dc=*/ DC,  /* reset=*/ RST);

WVR wvr;

Button *Left;
Button *Right;
Button *Center;

#define NUM_SCREENS 2
#define NUM_ITEMS_HOME 2
#define NUM_ITEMS_SETTINGS 2

uint8_t screen = 0;
uint8_t item = 0;
uint8_t channel = 0;

char *get_voice_name(uint8_t voice)
{
  switch(voice)
  {
    case 0:
      return get_metadata()->voice_name_1;
    case 1:
      return get_metadata()->voice_name_2;
    case 2:
      return get_metadata()->voice_name_3;
    case 3:
      return get_metadata()->voice_name_4;
    case 4:
      return get_metadata()->voice_name_5;
    case 5:
      return get_metadata()->voice_name_6;
    case 6:
      return get_metadata()->voice_name_7;
    case 7:
      return get_metadata()->voice_name_8;
    case 8:
      return get_metadata()->voice_name_9;
    case 9:
      return get_metadata()->voice_name_10;
    case 10:
      return get_metadata()->voice_name_11;
    case 11:
      return get_metadata()->voice_name_12;
    case 12:
      return get_metadata()->voice_name_13;
    case 13:
      return get_metadata()->voice_name_14;
    case 14:
      return get_metadata()->voice_name_15;
    case 15:
      return get_metadata()->voice_name_16;
    default:
      return get_metadata()->voice_name_1;
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
    uint8_t voice = wvr.getVoice(0);
    char program_number[12];
    sprintf(program_number, "voice #%d", voice + 1);
    u8g2.drawStr(5, 15, program_number);

    // voice name
    u8g2.drawStr(5, 27, get_voice_name(voice));

    // volume number
    uint8_t volume = wvr.getGlobalVolume();
    char volume_str[10];
    sprintf(volume_str, "%d", volume);
    u8g2.drawStr(5, 53, volume_str);

    // volume bar
    float vol_f = (float) volume / 128.0;
    float width_f = 90.0 * vol_f;
    uint8_t width = (uint8_t)width_f;
    u8g2.drawBox(30, 45, width, 10);

    // selection box
    if(item == 0) // voice
    {
      u8g2.drawFrame(1, 1, 127, 32);
    }
    else if(item == 1) // volume
    {
      u8g2.drawFrame(1 , 43, 127, 15);
    }
  }
  else if (screen == 1) // settings
  {
    if(item == 0) // channel
    {
      if(channel == 0)
      {
        u8g2.drawStr(10, 35, "Midi Channel: OMNI");
      }
      else
      {
        char channel_string[20];
        sprintf(channel_string, "Midi Channel: %d", channel);
        u8g2.drawStr(10, 35, channel_string);
      }
    }
    else if(item ==1) // wifi
    {
      u8g2.drawStr(10, 35, "Press enter to start WiFi config mode");      
    }
  }
  else if (screen == 2) // wifi
  {
      u8g2.drawFrame(1 ,1 , 127, 63);
      u8g2.drawFrame(3 ,4 , 125, 61);
      u8g2.drawStr(10, 15, "    * WIFI MODE ENABLED *    ");
      u8g2.drawStr(10, 25, "    NETWORK: SAMPLE_BANKR    ");
      u8g2.drawStr(10, 35, "    PASSWORD: ultrapalace    ");
      u8g2.drawStr(10, 45, "     http://192.168.5.18     ");
      u8g2.drawStr(10, 60, "press enter to exit WiFi mode");
  }

  u8g2.sendBuffer();	
}

void handleLeft(void){
  if(screen == 0) // home
  {
    if(item == 0) // program
    {
      uint8_t voice = wvr.getVoice(0);
      voice = voice > 0 ? voice - 1 : 15;
      wvr.setVoice(0, voice);
    }
    else if(item == 1) // volume
    {
      uint8_t volume = wvr.getGlobalVolume();
      volume -= (volume > 0);
      wvr.setGlobalVolume(volume);
    }
  }
  else if(screen == 1)
  {
    item -= (item > 0);
  }
  draw_screen();
}

void handleCenter(void){
  if(screen == 0) // home screen
  {
    item = item == 0 ? 1 : 0;
  }
  else if(screen == 1) // settings screen
  {
    switch(item){
      case 0: // midi channel
        channel = channel < 16 ? channel + 1 : 0;
        break;
      case 1: // wifi
        wvr.wifiOn();
        screen = 2; // show special screen
        break;
      default: break;
    }
  }
  else if(screen == 2){
    wvr.wifiOff() ;
    screen = 1; // exit wifi screen
  }
  draw_screen();
}

void handleRight(void){
  if(screen == 0) // home screen
  {
    if(item == 0) // program
    {
      uint8_t voice = wvr.getVoice(0);
      voice = voice < 15 ? voice + 1 : 0;
      wvr.setVoice(0, voice);
    }
    else if(item == 1) // volume
    {
      uint8_t volume = wvr.getGlobalVolume();
      volume += (volume < 127);
      wvr.setGlobalVolume(volume);
    }
  }
  else if(screen == 1) // settings screen
  {
    item += (item < (NUM_ITEMS_SETTINGS - 1));
  }
  draw_screen();
}

void setup() {
  wvr.useFTDI = true;
  wvr.useUsbMidi = true;
  wvr.begin();
  // wvr.wifiOff();

  // screen
  wvr.resetPin(SCK);
  wvr.resetPin(SDA);
  wvr.resetPin(RST);
  wvr.resetPin(CS);
  wvr.resetPin(DC);

  // leds
  wvr.resetPin(D9); // bodged together with D5 RGB
  wvr.resetPin(RGB);
  rgb_init(RGB);

  u8g2.begin();
  draw_screen();

  // switches
  wvr.resetPin(BUTTON_1);
  wvr.resetPin(BUTTON_2);
  wvr.resetPin(BUTTON_3);
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
  pinMode(BUTTON_3, INPUT_PULLUP);

  Left = new Button(BUTTON_1, FALLING, 60);
  Center = new Button(BUTTON_2, FALLING, 60);
  Right = new Button(BUTTON_3, FALLING, 60);
  Left->onPress(handleLeft);
  Center->onPress(handleCenter);
  Right->onPress(handleRight);

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
}

void loop() {
  vTaskDelete(NULL);
}