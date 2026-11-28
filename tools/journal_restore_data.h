// Written by tools/journal_restore_gen.py from docs/calibration/journal_2026-11-28.txt: do not edit.
// reason, phase, target, preinfusion, wait, duration, inTenths, outTenths, loss (ml), cup, delay100, tempStart, tempEnd, cal
static const RestoredEntry RESTORED_ENTRIES[] = {
  {END_USER, EXTRACTION_PREINFUSION, 60, 40, 5, 2, 0, 0, 58, 0, 37, 17, 17, 0},   // #0
  {END_NO_WATER, EXTRACTION_PREINFUSION, 60, 40, 5, 5, 0, 0, 58, 0, 37, 17, 17, 0},   // #1
  {END_TARGET, EXTRACTION_RUNNING, 60, 40, 5, 42, 1592, 421, 58, 59, 37, 90, 85, 62},   // #2
  {END_TARGET, EXTRACTION_RUNNING, 60, 40, 5, 29, 1179, 21, 56, 60, 36, 90, 83, 49},   // #3
  {END_USER, EXTRACTION_PREINFUSION, 60, 40, 5, 1, 40, 0, 64, 0, 36, 88, 89, 0},   // #4
  {END_USER, EXTRACTION_PREINFUSION_WAIT, 60, 40, 5, 10, 427, 22, 64, 0, 36, 128, 119, 0},   // #5
  {END_USER, EXTRACTION_PREINFUSION, 60, 40, 5, 1, 0, 0, 64, 0, 36, 88, 88, 0},   // #6
  {END_USER, EXTRACTION_PREINFUSION, 60, 40, 5, 0, 0, 0, 64, 0, 36, 88, 88, 0},   // #7
  {END_NO_WATER, EXTRACTION_PREINFUSION, 60, 40, 5, 5, 0, 0, 64, 0, 36, 88, 87, 0},   // #8
  {END_NO_WATER, EXTRACTION_PREINFUSION, 60, 40, 5, 5, 0, 9, 64, 0, 36, 113, 112, 0},   // #9
  {END_USER, EXTRACTION_PREINFUSION, 60, 40, 5, 0, 0, 1, 64, 0, 36, 98, 98, 0},   // #10
  {END_TARGET, EXTRACTION_RUNNING, 60, 40, 5, 38, 1479, 235, 64, 60, 39, 90, 85, 49},   // #11
};

static const uint8_t RESTORED_CURVE_1[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const uint8_t RESTORED_CURVE_2[] = {2,0,23,0,24,0,26,0,26,0,26,0,23,0,25,0,25,0,24,0,25,0,25,0,25,0,24,0,25,1,25,0,24,0,23,2,6,14,1,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,3,4,21,2,24,4,24,7,24,8,23,3,22,10,21,5,20,7,20,9,18,8,19,8,17,8,17,9,17,9,18,9,17,9,17,8,18,9,18,8,17,8,19,8,18,8,18,7,19,7,18,7,19,7,19,7,19,6,19,7,20,7,19,6,20,6,19,6,20,6,20,6,20,6,20,6,21,6,20,6,20,6,20,5,20,6,21,6,21,5,20,6,20,6,21,5,21,6,20,5,21,6,20,5,21,6,20,6,21,5,21,6,20,5,21,6,21,6,10,4,3,4,0,3};
static const uint8_t RESTORED_CURVE_3[] = {3,0,23,0,26,0,26,0,26,0,25,0,26,0,26,0,25,0,26,0,26,0,25,0,26,0,25,0,25,1,26,0,35,0,4,0,3,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,14,0,25,0,24,0,24,1,24,0,23,0,23,0,22,0,22,0,21,4,22,2,22,6,22,6,21,1,23,0,21,0,22,0,22,0,22,0,22,0,22,0,22,0,22,0,22,0,22,0,22,0,22,0,22,0,22,0,21,0,23,0,22,0,22,0,18,0,5,0,1,0};
static const uint8_t RESTORED_CURVE_4[] = {4,0,23,0};
static const uint8_t RESTORED_CURVE_5[] = {4,0,21,0,24,0,24,0,24,0,25,0,24,0,25,0,25,0,25,0,24,0,26,0,24,0,25,0,26,0,24,0,26,0,22,1,6,0,2,1,1,5};
static const uint8_t RESTORED_CURVE_6[] = {0,0,0,0};
static const uint8_t RESTORED_CURVE_7[] = {0,0};
static const uint8_t RESTORED_CURVE_8[] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static const uint8_t RESTORED_CURVE_9[] = {0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,8};
static const uint8_t RESTORED_CURVE_10[] = {0,0};
static const uint8_t RESTORED_CURVE_11[] = {3,0,23,0,26,0,25,0,27,0,26,0,25,0,26,0,26,0,25,0,26,0,25,0,26,0,25,0,25,0,25,0,24,0,15,0,2,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,13,0,25,0,25,0,24,0,24,0,24,1,22,2,22,2,21,3,20,4,20,5,20,5,19,6,20,6,18,7,19,7,20,8,19,7,19,8,20,7,20,7,20,6,20,7,20,6,20,7,20,6,21,6,21,6,20,6,22,6,20,5,21,6,22,5,20,5,22,5,21,5,21,6,22,5,21,5,22,4,21,5,22,5,21,5,22,4,21,5,22,5,21,4,22,5,22,4,21,1,7,0,1,0,0,0};

static const RestoredCurve RESTORED_CURVES[] = {
  {1, 10, RESTORED_CURVE_1},
  {2, 89, RESTORED_CURVE_2},
  {3, 63, RESTORED_CURVE_3},
  {4, 2, RESTORED_CURVE_4},
  {5, 21, RESTORED_CURVE_5},
  {6, 2, RESTORED_CURVE_6},
  {7, 1, RESTORED_CURVE_7},
  {8, 10, RESTORED_CURVE_8},
  {9, 10, RESTORED_CURVE_9},
  {10, 1, RESTORED_CURVE_10},
  {11, 80, RESTORED_CURVE_11},
};
