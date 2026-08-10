#include <Arduino.h>
#include "tests.h"
#include "ble_ota.h"

uint16_t COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW, COLOR_CYAN, COLOR_MAGENTA, COLOR_WHITE, COLOR_BLACK;
uint16_t customTestColor; // Active color for pattern tests

static int colorIndex = 0; // 0=White, 1=Red, 2=Green, 3=Blue, 4=Cyan, 5=Magenta, 6=Yellow

static unsigned long lastPatternUpdate = 0;
static int currentBlock = 0;
static int currentColorStep = 0;
static bool oddRowsActive = false;

static int ghostX = 0;
static int ghostY = 0;
static int ghostDirectionX = 1; // 1 for right, -1 for left
static int ghostDirectionY = 1; // 1 for down, -1 for up

static int hunterX = 0;
static int hunterY = 0;

static int sweepY = 0;
static int sweepX = 0;

// --- ANIMATION OFFSETS FOR SCROLLING PATTERNS ---
static int hOffset = 0;
static int vOffset = 0;
static int diagOffset = 0;

void initColors(MatrixPanel_I2S_DMA &matrix) {
    COLOR_RED = matrix.color565(255, 0, 0);
    COLOR_GREEN = matrix.color565(0, 255, 0);
    COLOR_BLUE = matrix.color565(0, 0, 255);
    COLOR_YELLOW = matrix.color565(255, 255, 0);
    COLOR_CYAN = matrix.color565(0, 255, 255);
    COLOR_MAGENTA = matrix.color565(255, 0, 255);
    COLOR_WHITE = matrix.color565(255, 255, 255);
    COLOR_BLACK = matrix.color565(0, 0, 0);

    customTestColor = COLOR_WHITE; // Default start color
}

// Cycles through R, G, B, Cyan, Magenta, Yellow, White on button hold
void cycleTestColor(MatrixPanel_I2S_DMA &matrix) {
    colorIndex = (colorIndex + 1) % 7;
    switch (colorIndex) {
        case 0: customTestColor = COLOR_WHITE;   Serial.println("Color: White");   break;
        case 1: customTestColor = COLOR_RED;     Serial.println("Color: Red");     break;
        case 2: customTestColor = COLOR_GREEN;   Serial.println("Color: Green");   break;
        case 3: customTestColor = COLOR_BLUE;    Serial.println("Color: Blue");    break;
        case 4: customTestColor = COLOR_CYAN;    Serial.println("Color: Cyan");    break;
        case 5: customTestColor = COLOR_MAGENTA; Serial.println("Color: Magenta"); break;
        case 6: customTestColor = COLOR_YELLOW;  Serial.println("Color: Yellow");  break;
    }
}

void setCustomColor(uint16_t color) {
    customTestColor = color;
}

void setPixelHunterXY(int x, int y) {
    hunterX = x;
    hunterY = y;
}

// --- FIXED SCROLLING & SPACED PATTERNS ---

void drawInterleavedRows(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    int startRow = oddRowsActive ? 1 : 0; 
    for (int y = startRow; y < matrix.height(); y += 2) {
        matrix.drawFastHLine(0, y, matrix.width(), customTestColor);
    }
    oddRowsActive = !oddRowsActive; 
}

void drawDiagonalPattern(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    int spacing = 8; // Distance between diagonal stripes
    for (int i = -matrix.height(); i < matrix.width() + matrix.height(); i += spacing) {
        int currentI = i + diagOffset;
        for (int x = 0; x < matrix.width(); x++) {
            int y = x - currentI;
            if (y >= 0 && y < matrix.height()) {
                matrix.drawPixel(x, y, customTestColor);
            }
        }
    }
    diagOffset = (diagOffset + 1) % spacing; // Smooth diagonal scroll
}

void drawHorizontalPattern(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    int spacing = 4; // Distance between horizontal lines
    for (int y = hOffset; y < matrix.height(); y += spacing) {
        matrix.drawFastHLine(0, y, matrix.width(), customTestColor);
    }
    hOffset = (hOffset + 1) % spacing; // Smooth vertical scroll
}

void drawVerticalPattern(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    int spacing = 4; // Distance between vertical lines
    for (int x = vOffset; x < matrix.width(); x += spacing) {
        matrix.drawFastVLine(x, 0, matrix.height(), customTestColor);
    }
    vOffset = (vOffset + 1) % spacing; // Smooth horizontal scroll
}

void drawCheckerboardPattern(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    int blockSize = 4;
    for (int y = 0; y < matrix.height(); y += blockSize) {
        for (int x = 0; x < matrix.width(); x += blockSize) {
            if (((x / blockSize) + (y / blockSize)) % 2 == 0) {
                matrix.fillRect(x, y, blockSize, blockSize, customTestColor);
            }
        }
    }
}

void drawTopHalf(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    matrix.fillRect(0, 0, matrix.width(), matrix.height() / 2, customTestColor);
}

void drawBottomHalf(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    matrix.fillRect(0, matrix.height() / 2, matrix.width(), matrix.height() / 2, customTestColor);
}

void drawAddressIsolator(MatrixPanel_I2S_DMA &matrix, int bitPos) {
    matrix.fillScreen(COLOR_BLACK);
    for (int y = 0; y < matrix.height(); y++) {
        int rowAddress = y % 16; 
        if ((rowAddress & (1 << bitPos)) != 0) {
            matrix.drawFastHLine(0, y, matrix.width(), customTestColor);
        }
    }
}

void drawGhostingTest(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    matrix.fillRect(ghostX, ghostY, 4, 4, customTestColor);
    ghostX += ghostDirectionX;
    ghostY += ghostDirectionY;
    if (ghostX <= 0 || ghostX >= matrix.width() - 4) ghostDirectionX *= -1;
    if (ghostY <= 0 || ghostY >= matrix.height() - 4) ghostDirectionY *= -1;
}

void drawPixelHunter(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    matrix.fillRect(hunterX, hunterY, 2, 2, customTestColor);
}

void drawRowSweep(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    matrix.drawFastHLine(0, sweepY, matrix.width(), customTestColor);
    sweepY++;
    if (sweepY >= matrix.height()) sweepY = 0;
}

void drawColumnSweep(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    matrix.drawFastVLine(sweepX, 0, matrix.height(), customTestColor);
    sweepX++;
    if (sweepX >= matrix.width()) sweepX = 0;
}

void drawICBlockChase(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    int x = (currentBlock % 2) * 16;
    int y = (currentBlock / 2) * 16;
    matrix.fillRect(x, y, 16, 16, customTestColor);
    currentBlock = (currentBlock + 1) % 4;
}

static int currentQuadrantStep = 0;

void drawQuadrantChase(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);

    int halfW = matrix.width() / 2;
    int halfH = matrix.height() / 2;

    switch (currentQuadrantStep) {
        case 0: matrix.fillRect(0, 0, halfW, halfH, customTestColor); break;
        case 1: matrix.fillRect(halfW, 0, matrix.width() - halfW, halfH, customTestColor); break;
        case 2: matrix.fillRect(halfW, halfH, matrix.width() - halfW, matrix.height() - halfH, customTestColor); break;
        case 3: matrix.fillRect(0, halfH, halfW, matrix.height() - halfH, customTestColor); break;
    }

    currentQuadrantStep = (currentQuadrantStep + 1) % 4;
}

void drawOtaScreen(MatrixPanel_I2S_DMA &matrix) {
    matrix.fillScreen(COLOR_BLACK);
    // Draw "OTA" visual icon / border for the update screen
    int w = matrix.width();
    int h = matrix.height();
    
    // Outer cyan border
    matrix.drawRect(0, 0, w, h, COLOR_CYAN);
    // Central "U" / Arrow up pattern
    int cx = w / 2;
    int cy = h / 2;
    matrix.drawFastVLine(cx, cy - 4, 9, COLOR_YELLOW);
    matrix.drawPixel(cx - 1, cy - 3, COLOR_YELLOW);
    matrix.drawPixel(cx + 1, cy - 3, COLOR_YELLOW);
    matrix.drawPixel(cx - 2, cy - 2, COLOR_YELLOW);
    matrix.drawPixel(cx + 2, cy - 2, COLOR_YELLOW);
}

void runStateEngine(MatrixPanel_I2S_DMA &matrix, TestState state, bool &stateNeedsInit) {
    static TestState lastState = static_cast<TestState>(-1);
    if (state != lastState) {
        customTestColor = COLOR_WHITE; 
        lastState = state;
    }

    switch (state) {
        case STATE_WHITE: if (stateNeedsInit) { matrix.fillScreen(COLOR_WHITE); stateNeedsInit = false; } break;
        case STATE_RED: if (stateNeedsInit) { matrix.fillScreen(COLOR_RED); stateNeedsInit = false; } break;
        case STATE_GREEN: if (stateNeedsInit) { matrix.fillScreen(COLOR_GREEN); stateNeedsInit = false; } break;
        case STATE_BLUE: if (stateNeedsInit) { matrix.fillScreen(COLOR_BLUE); stateNeedsInit = false; } break;
        case STATE_YELLOW: if (stateNeedsInit) { matrix.fillScreen(COLOR_YELLOW); stateNeedsInit = false; } break;
        case STATE_CYAN: if (stateNeedsInit) { matrix.fillScreen(COLOR_CYAN); stateNeedsInit = false; } break;
        case STATE_MAGENTA: if (stateNeedsInit) { matrix.fillScreen(COLOR_MAGENTA); stateNeedsInit = false; } break;
        
        case STATE_CHECKERBOARD:
            if (stateNeedsInit) { drawCheckerboardPattern(matrix); stateNeedsInit = false; }
            break;
        case STATE_QUADRANT:
            if (stateNeedsInit || (millis() - lastPatternUpdate >= 300)) {
                lastPatternUpdate = millis();
                drawQuadrantChase(matrix);
                stateNeedsInit = false;
            }
            break;
        case STATE_DIAGONAL:
            if (stateNeedsInit || (millis() - lastPatternUpdate >= 100)) {
                lastPatternUpdate = millis();
                drawDiagonalPattern(matrix);
                stateNeedsInit = false;
            }
            break;
        case STATE_HORIZONTAL:
            if (stateNeedsInit || (millis() - lastPatternUpdate >= 100)) {
                lastPatternUpdate = millis();
                drawHorizontalPattern(matrix);
                stateNeedsInit = false;
            }
            break;
        case STATE_VERTICAL:
            if (stateNeedsInit || (millis() - lastPatternUpdate >= 100)) {
                lastPatternUpdate = millis();
                drawVerticalPattern(matrix);
                stateNeedsInit = false;
            }
            break;
        case STATE_IC_CHASE:
            if (stateNeedsInit || (millis() - lastPatternUpdate >= 400)) {
                lastPatternUpdate = millis();
                drawICBlockChase(matrix);
                stateNeedsInit = false;
            }
            break;
        case STATE_INTERLEAVED_ROWS:
            if (stateNeedsInit || (millis() - lastPatternUpdate >= 500)) {
                lastPatternUpdate = millis();
                drawInterleavedRows(matrix);
                stateNeedsInit = false;
            }
            break;
        case STATE_OFF:
            if (stateNeedsInit) { matrix.fillScreen(COLOR_BLACK); stateNeedsInit = false; }
            break;
        case STATE_TOP_HALF:
            if (stateNeedsInit) { drawTopHalf(matrix); stateNeedsInit = false; }
            break;
        case STATE_BOTTOM_HALF:
            if (stateNeedsInit) { drawBottomHalf(matrix); stateNeedsInit = false; }
            break;
        case STATE_ADDR_A:
            if (stateNeedsInit) { drawAddressIsolator(matrix, 0); stateNeedsInit = false; }
            break;
        case STATE_ADDR_B:
            if (stateNeedsInit) { drawAddressIsolator(matrix, 1); stateNeedsInit = false; }
            break;
        case STATE_ADDR_C:
            if (stateNeedsInit) { drawAddressIsolator(matrix, 2); stateNeedsInit = false; }
            break;
        case STATE_ADDR_D:
            if (stateNeedsInit) { drawAddressIsolator(matrix, 3); stateNeedsInit = false; }
            break;
        case STATE_ROW_SWEEP:
            if (stateNeedsInit || (millis() - lastPatternUpdate >= 100)) {
                lastPatternUpdate = millis();
                drawRowSweep(matrix);
                stateNeedsInit = false;
            }
            break;
        case STATE_COLUMN_SWEEP:
            if (stateNeedsInit || (millis() - lastPatternUpdate >= 100)) {
                lastPatternUpdate = millis();
                drawColumnSweep(matrix);
                stateNeedsInit = false;
            }
            break;
        case STATE_GHOSTING:
            if (stateNeedsInit || (millis() - lastPatternUpdate >= 15)) {
                lastPatternUpdate = millis();
                drawGhostingTest(matrix);
                stateNeedsInit = false;
            }
            break;
        case STATE_PIXEL_HUNTER:
            if (stateNeedsInit) { drawPixelHunter(matrix); stateNeedsInit = false; }
            break;
        case STATE_OTA_MODE:
            if (stateNeedsInit) { drawOtaScreen(matrix); stateNeedsInit = false; }
            break;
        default:
            break;
    }
}