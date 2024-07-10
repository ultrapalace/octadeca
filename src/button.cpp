#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "Arduino.h"
#include "button.h"
#include "wvr_pins.h"
#include "ws_log.h"
#include "file_system.h"
#include "wav_player.h"
#include "midi_in.h"
#include "WVR.h"

#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_THRESH_PERCENT  (80)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (10)

xQueueHandle gpio_queue_handle;
xTaskHandle gpio_task_handle;

int cnt = 0;

static void gpioTask(void* x) {
    button_event_t *event;
    uint32_t touch_reg = 0;
    for(;;) {
        if(xQueueReceive(gpio_queue_handle, &event, portMAX_DELAY)) {
            log_d("%d -> gpio task pin:%d val:%d",cnt++, event->pin, event->val);
            event->button->handleChange(event->val);
        }
    };
}

void isr(void *e){
    button_event_t *event = (button_event_t*)e;
    event->val = digitalRead(event->pin);
    // isr_log_d("read %d",event->val);
    int pxHigherPriorityTaskWoken;
    xQueueSendFromISR(gpio_queue_handle, &event, &pxHigherPriorityTaskWoken);
    if(pxHigherPriorityTaskWoken){
        portYIELD_FROM_ISR();
    }
}

void touch_isr(void *e){
    return;
}

Button::Button(int pin, int mode, int dbnc){
    this->pin = pin;
    this->mode = mode;
    this->dbnc = dbnc;
    this->last = 0;
    this->touch = false;
    this->handlePress = NULL;
    this->handleRelease = NULL;
    this->pressed = false;
    event.pin = pin;
    event.button = this;
}

Button::Button(int pin, int mode, int dbnc, bool touch){
    this->pin = pin;
    this->mode = mode;
    this->dbnc = dbnc;
    this->last = 0;
    this->touch = true;
    this->pressed = false;
    this->handlePress = NULL;
    this->handleRelease = NULL;
    event.pin = pin;
    event.button = this;
}

Button::~Button(){
    log_d("destructor button on %u",pin);
    detachInterrupt(pin);
}

void Button::onPress(void(*handlePress)()){
    this->handlePress = handlePress;
    attachInterruptArg((uint8_t)pin, isr, (void*)&event, CHANGE);
}

void Button::onRelease(void(*handleRelease)()){
    this->handleRelease=handleRelease;
    attachInterruptArg(pin, isr, (void*)&event, CHANGE);
}

void Button::handleChange(int val){
    // log_d("pin:%d val:%d",pin,val);
    // if EDGE_NONE then ignore
    if(mode != RISING && mode != FALLING) return;
    int now = millis();
    if((now - last) > dbnc){
        // last = now;
        if(
            (val==0 && (mode == FALLING) && !pressed) ||
            (val==1 && (mode == RISING)  && !pressed)  
        ){
            if(handlePress != NULL)
            {
                handlePress();
            }
            last = now;
            pressed = true;
        }
        else if(
            (val==1 && mode == FALLING && pressed) ||
            (val==0 && mode == RISING && pressed)  
        ){
            if(handleRelease != NULL && pressed)
            {
                handleRelease();
            }
            last = now;
            pressed = false;
        }
    }
    // make sure the pressed state updates even when debounced (this isr logic is annoying)
    // touch pins have no such concept
    // pressed = (val==0 && mode==FALLING) || (val==1 && mode==RISING);
}

static bool touch_initialized = false;

void init_touch(void)
{
    return;    
}

void init_touch_pad(int pin, void *event)
{
    return;
}

void button_init(){
    gpio_queue_handle = xQueueCreate(10, sizeof(button_event_t));
    xTaskCreate(gpioTask, "gpio_task", 2048, NULL, 3, &gpio_task_handle);
}