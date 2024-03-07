#ifndef RGB_H
#define RGB_H

void rgb_init(uint16_t pin);
void rgb_set_color(uint8_t red, uint8_t green, uint8_t blue);
void rgb_set_led(uint8_t num, uint8_t red, uint8_t green, uint8_t blue);

#endif