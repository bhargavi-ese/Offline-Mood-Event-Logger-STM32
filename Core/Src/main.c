#include "stm32f4xx.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ========== I2C LCD Pins =============
#define SDA_PIN 9
#define SCL_PIN 8
#define SDA_HIGH() (GPIOA->BSRR = (1 << SDA_PIN))
#define SDA_LOW()  (GPIOA->BSRR = (1 << (SDA_PIN + 16)))
#define SCL_HIGH() (GPIOA->BSRR = (1 << SCL_PIN))
#define SCL_LOW()  (GPIOA->BSRR = (1 << (SCL_PIN + 16)))
#define SDA_READ   ((GPIOA->IDR >> SDA_PIN) & 1)

#define LCD_ADDR 0x4E

// ========== Moods =============
const char* moods[] = {"Happy", "Sad", "Neutral"};
uint8_t current_mood = 0;

// ========== Time (Software RTC) ==========
volatile uint8_t seconds = 0, minutes = 0, hours = 10;

// ========== Delay ==========
void delay() {
    for (volatile int i = 0; i < 500; i++);
}
void delay_ms(uint32_t ms) {
    for (uint32_t i = 0; i < ms * 3000; i++) __NOP();
}

// ========== I2C Bit-Bang ==========
void I2C_Start() {
    SDA_HIGH(); SCL_HIGH(); delay();
    SDA_LOW(); delay(); SCL_LOW(); delay();
}
void I2C_Stop() {
    SDA_LOW(); SCL_HIGH(); delay(); SDA_HIGH(); delay();
}
void I2C_WriteBit(uint8_t bit) {
    if (bit) SDA_HIGH(); else SDA_LOW();
    delay(); SCL_HIGH(); delay(); SCL_LOW(); delay();
}
uint8_t I2C_ReadBit() {
    SDA_HIGH(); delay(); SCL_HIGH(); delay();
    uint8_t bit = SDA_READ;
    SCL_LOW(); delay();
    return bit;
}
uint8_t I2C_WriteByte(uint8_t byte) {
    for (int i = 7; i >= 0; i--) I2C_WriteBit((byte >> i) & 1);
    return I2C_ReadBit(); // ACK
}

// ========== LCD I2C ==========
void LCD_Enable(uint8_t data) {
    I2C_Start(); I2C_WriteByte(LCD_ADDR);
    I2C_WriteByte(data | 0x04); delay();
    I2C_WriteByte(data & ~0x04); delay();
    I2C_Stop();
}
void LCD_Send4Bits(uint8_t data) {
    LCD_Enable(data | 0x08);  // backlight
}
void LCD_SendCmd(uint8_t cmd) {
    LCD_Send4Bits(cmd & 0xF0);
    LCD_Send4Bits((cmd << 4) & 0xF0);
}
void LCD_SendData(uint8_t data) {
    LCD_Enable((data & 0xF0) | 0x09);
    LCD_Enable(((data << 4) & 0xF0) | 0x09);
}
void LCD_Clear() {
    LCD_SendCmd(0x01);
    for (volatile int i = 0; i < 30000; i++);
}
void LCD_Init() {
    delay_ms(50);
    LCD_SendCmd(0x33);
    LCD_SendCmd(0x32);
    LCD_SendCmd(0x28);
    LCD_SendCmd(0x0C);
    LCD_SendCmd(0x06);
    LCD_SendCmd(0x01);
}
void LCD_SetCursor(uint8_t row, uint8_t col) {
    LCD_SendCmd((row == 0 ? 0x80 : 0xC0) + col);
}
void LCD_Print(char* str) {
    while (*str) LCD_SendData(*str++);
}

// ========== Mood Functions ==========
void Display_Mood() {
    LCD_Clear();
    LCD_Print("Mood: ");
    LCD_Print((char*)moods[current_mood]);
}
void Toggle_Mood() {
    current_mood = (current_mood + 1) % 3;
    Display_Mood();
}
void Log_Event() {
    char line[32];
    LCD_Clear();
    LCD_Print("Event Logged:");
    LCD_SetCursor(1, 0);
    sprintf(line, "%02d:%02d:%02d %s", hours, minutes, seconds, moods[current_mood]);
    LCD_Print(line);
}

// ========== Time Update ==========
void Update_Time() {
    seconds++;
    if (seconds >= 60) {
        seconds = 0;
        minutes++;
        if (minutes >= 60) {
            minutes = 0;
            hours = (hours + 1) % 24;
        }
    }
}
void Display_Time() {
    char timeStr[16];
    sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
    LCD_Clear();
    LCD_Print("Time:");
    LCD_SetCursor(1, 0);
    LCD_Print(timeStr);
}

// ========== GPIO Init ==========
void GPIO_Init() {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

    // PA8, PA9 as output for I2C
    GPIOA->MODER &= ~((3 << (8*2)) | (3 << (9*2)));
    GPIOA->MODER |=  (1 << (8*2)) | (1 << (9*2));

    // PA5, PA6 as input with pull-up
    GPIOA->MODER &= ~((3 << (5*2)) | (3 << (6*2)));
    GPIOA->PUPDR &= ~((3 << (5*2)) | (3 << (6*2)));
    GPIOA->PUPDR |=  (1 << (5*2)) | (1 << (6*2));
}

// ========== Main ==========
int main(void) {
    GPIO_Init();
    LCD_Init();
    Display_Mood();

    uint8_t prev_sw1 = 1, prev_sw2 = 1;

    while (1) {
        // Read switches
        uint8_t sw1 = (GPIOA->IDR >> 5) & 1;
        uint8_t sw2 = (GPIOA->IDR >> 6) & 1;

        // Log event on PA5 press
        if (prev_sw1 == 1 && sw1 == 0) {
            Log_Event();
            delay_ms(1000);
        }

        // Toggle mood on PA6 press
        if (prev_sw2 == 1 && sw2 == 0) {
            Toggle_Mood();
            delay_ms(1000);
        }

        // Update software time every second
        Display_Time();
        delay_ms(1000);
        Update_Time();

        prev_sw1 = sw1;
        prev_sw2 = sw2;
    }
}
