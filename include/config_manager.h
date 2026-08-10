#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>

extern Preferences preferences;

extern int panelWidth;
extern int panelHeight;
extern bool useSlowClock;

void loadConfiguration();
void saveConfiguration(int width, int height, bool slowClock);
bool checkAndRunConfigMode(int buttonPin, int ledPin);

#endif
