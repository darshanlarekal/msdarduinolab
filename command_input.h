#pragma once

bool readCommand();
bool isCommand(const char* word, char letter);
bool isNumberToken();

// The token most recently returned by readCommand(). Valid until the
// next readCommand() call.
const char* currentCommand();

// Queue a command from a non-serial source. Returns false when the
// previous queued command has not been consumed yet or the token is
// empty/too long/contains whitespace.
bool submitCommand(const char* token);

// Value of the current token when it is only digits.
int commandNumber();
