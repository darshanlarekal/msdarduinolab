#include "command_input.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

static char commandBuffer[CMD_BUF_SIZE];
static int commandLength = 0;
static unsigned long lastCharMs = 0;

static bool finishCommand() {
  commandBuffer[commandLength] = '\0';

  bool ready = commandLength > 0;

  commandLength = 0;

  return ready;
}

bool readCommand() {
  while (Serial.available() > 0) {
    int character = Serial.read();

    lastCharMs = millis();

    if (character == '\n' || character == '\r') {
      if (commandLength == 0) continue;

      return finishCommand();
    }

    if (character == ' ') continue;

    if (commandLength < CMD_BUF_SIZE - 1) {
      commandBuffer[commandLength++] = (char)tolower(character);
    }
  }

  if (commandLength > 0 && (millis() - lastCharMs) > CMD_IDLE_MS) {
    return finishCommand();
  }

  return false;
}

// True when the token is either the full word or its single letter.
bool isCommand(const char* word, char letter) {
  if (strcmp(commandBuffer, word) == 0) return true;

  return commandBuffer[0] == letter && commandBuffer[1] == '\0';
}

// True when the token is only digits.
bool isNumberToken() {
  if (commandBuffer[0] == '\0') return false;

  for (int i = 0; commandBuffer[i] != '\0'; i++) {
    if (commandBuffer[i] < '0' || commandBuffer[i] > '9') return false;
  }

  return true;
}

int commandNumber() {
  return atoi(commandBuffer);
}
