/*
 * board/audioboard_v1/tests/load/main.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include <halm/generic/timer_factory.h>
#include <halm/wq.h>
#include <xcore/interface.h>
#include <assert.h>
#include <stdio.h>
/*----------------------------------------------------------------------------*/
#define SAMPLE_COUNT 8
static uint32_t samples[SAMPLE_COUNT] = {0};
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
  boardSetupDefaultWQ();

  struct ChronoPackage chronoPackage = boardSetupChronoPackage();
  struct Timer * const eventTimer = chronoPackage.load;
  struct Timer * const flushTimer = boardMakeAdcTimer();
  struct Interface * const serial = boardMakeSerial();

  timerSetCallback(flushTimer, onFlushTimerOverflow, serial);
  timerSetOverflow(flushTimer, timerGetFrequency(flushTimer) * SAMPLE_COUNT);

  timerSetCallback(eventTimer, onLoadTimerOverflow, NULL);
  timerSetOverflow(eventTimer, timerGetFrequency(eventTimer));

  timerEnable(eventTimer);
  timerEnable(flushTimer);

  wqStart(WQ_DEFAULT);
  return 0;
}
