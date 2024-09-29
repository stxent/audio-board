/*
 * board/audioboard_v1/tests/load/main.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include <halm/generic/work_queue.h>
#include <halm/timer.h>
#include <xcore/interface.h>
#include <assert.h>
#include <stdio.h>
/*----------------------------------------------------------------------------*/
#define SAMPLE_COUNT 8

static uint32_t samples[SAMPLE_COUNT] = {0};
/*----------------------------------------------------------------------------*/
static const struct WorkQueueConfig workQueueConfig = {
    .size = 4
};
/*----------------------------------------------------------------------------*/
static void flushAcquiredSamples(void *argument)
{
  char text[12 * SAMPLE_COUNT + 1];
  size_t length = 0;

  for (size_t i = 0; i < SAMPLE_COUNT; ++i)
    length += sprintf(text + length, "%lu\r\n", (unsigned long)samples[i]);

  ifWrite(argument, text, length);
}
/*----------------------------------------------------------------------------*/
static void onFlushTimerOverflow(void *argument)
{
  wqAdd(WQ_DEFAULT, flushAcquiredSamples, argument);
}
/*----------------------------------------------------------------------------*/
static void onLoadTimerOverflow(void *)
{
  static size_t index = 0;

  struct WqInfo info;
  wqStatistics(WQ_DEFAULT, &info);

  samples[index % SAMPLE_COUNT] = info.loops;
  ++index;
}
/*----------------------------------------------------------------------------*/
int main(void)
{
  boardSetupClock();

  struct Interface * const serial = boardMakeSerial();
  assert(serial != NULL);

  struct Timer * const baseTimer = boardMakeAdcTimer();
  assert(baseTimer != NULL);
  timerSetCallback(baseTimer, onFlushTimerOverflow, serial);
  timerSetOverflow(baseTimer, timerGetFrequency(baseTimer) * SAMPLE_COUNT);

  struct Timer * const loadTimer = boardMakeControlTimer();
  assert(loadTimer != NULL);
  timerSetCallback(loadTimer, onLoadTimerOverflow, NULL);
  timerSetOverflow(loadTimer, timerGetFrequency(loadTimer));

  timerEnable(loadTimer);
  timerEnable(baseTimer);

  /* Initialize Work Queue */
  WQ_DEFAULT = init(WorkQueue, &workQueueConfig);
  assert(WQ_DEFAULT != NULL);

  wqStart(WQ_DEFAULT);
  return 0;
}
