#pragma once

// Start with white illumination.
// The LED remains on in the currently displayed color.
extern int displayedColor;  // WHITE
extern int candidateColor;
extern int stableCount;

// Most recent detector reading, for status reporting.
extern float lastLdrReading;

// Switch into detection mode and reset the detector state.
void startDetection(const char* source);

// One detection pass, including command handling and servo seeking.
void runDetection();
