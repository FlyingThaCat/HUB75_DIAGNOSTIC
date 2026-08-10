#ifndef TESTS_H
#define TESTS_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

enum TestState {
    STATE_WHITE = 0,
    STATE_RED,
    STATE_GREEN,
    STATE_BLUE,
    STATE_YELLOW,
    STATE_CYAN,
    STATE_MAGENTA,
    STATE_CHECKERBOARD,
    STATE_QUADRANT,
    STATE_DIAGONAL,
    STATE_HORIZONTAL,
    STATE_VERTICAL,
    STATE_IC_CHASE,
    STATE_INTERLEAVED_ROWS,
    STATE_OFF,
    BUTTON_STATE_COUNT,

    STATE_TOP_HALF,
    STATE_BOTTOM_HALF,
    STATE_ADDR_A,
    STATE_ADDR_B,
    STATE_ADDR_C,
    STATE_ADDR_D,
    STATE_ROW_SWEEP,
    STATE_COLUMN_SWEEP,
    STATE_GHOSTING,
    STATE_PIXEL_HUNTER,
    STATE_OTA_MODE
};


extern TestState currentState;
extern bool stateNeedsInit;

extern uint16_t customTestColor;
void cycleTestColor(MatrixPanel_I2S_DMA &matrix);
void setCustomColor(uint16_t color);

void setPixelHunterXY(int x, int y);
void initColors(MatrixPanel_I2S_DMA &matrix);
void runStateEngine(MatrixPanel_I2S_DMA &matrix, TestState state, bool &stateNeedsInit);

#endif
