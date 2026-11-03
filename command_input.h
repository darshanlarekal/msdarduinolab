#pragma once

bool readCommand();
bool isCommand(const char* word, char letter);
bool isNumberToken();

// Value of the current token when it is only digits.
int commandNumber();
