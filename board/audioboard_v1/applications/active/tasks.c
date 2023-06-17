/*
 * board/audioboard_v1/applications/active/tasks.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board.h"
#include "controls.h"
#include "settings.h"
#include "slave.h"
#include "tasks.h"
#include <halm/core/cortex/nvic.h>
#include <halm/generic/work_queue.h>
#include <halm/interrupt.h>
#include <halm/pm.h>
#include <halm/timer.h>
#include <halm/watchdog.h>
#include <xcore/interface.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/*----------------------------------------------------------------------------*/
#define CONTROL_UPDATE_RATE   10

#define AUTO_SUSPEND_TIMEOUT  (5 * CONTROL_UPDATE_RATE)
#define MODE_ACTIVE_TIMEOUT   (3 * CONTROL_UPDATE_RATE)

#define MIN_LEVEL             0
#define MAX_LEVEL             7

#define FLASH_OFFSET          (28 * 1024)
/*----------------------------------------------------------------------------*/
static void codecLoadDefaultSettings(struct Board *);
static void codecLoadSettings(struct Board *, const struct Settings *);
static inline uint8_t gainToLevel(uint8_t gain);
static inline uint8_t levelToBar(uint8_t);
static inline uint8_t levelToGain(uint8_t);
static uint8_t readSwitchState(struct Board *);
static void slaveLoadSettings(struct SlaveRegOverlay *,
    const struct Settings *);
static void slaveStoreSettings(struct Settings *,
    const struct SlaveRegOverlay *);
static void writeLedState(struct Board *, uint8_t);

static void onControlUpdateEvent(void *);
static void onConversionCompleted(void *);
static void onMicPressed(void *);
static void onSlaveUpdateEvent(void *);
static void onSpkPressed(void *);
static void onVolMPressed(void *);
static void onVolPPressed(void *);

static void autoSuspendTask(void *);
static void ledUpdateTask(void *);
static void micUpdateTask(void *);
static void slaveUpdateTask(void *);
static void spkUpdateTask(void *);
static void startupTask(void *);
static void switchReadTask(void *);
static void volumeUpdateTask(void *);

#ifdef ENABLE_DBG
static void debugInfoTask(void *);
static void onLoadTimerOverflow(void *);
#endif
/*----------------------------------------------------------------------------*/
static void codecLoadDefaultSettings(struct Board *board)
{
  board->config.inputLevel = MAX_LEVEL >> 1;
  board->config.inputPath = BOARD_AUDIO_INPUT_A;
  board->config.outputLevel = MAX_LEVEL >> 1;
  board->config.outputPath = BOARD_AUDIO_OUTPUT_A;
}
/*----------------------------------------------------------------------------*/
static void codecLoadSettings(struct Board *board,
    const struct Settings *settings)
{
  board->config.inputLevel = gainToLevel(settings->codecInputLevel);
  board->config.inputPath = settings->codecInputPath;
  board->config.outputLevel = gainToLevel(settings->codecOutputLevel);
  board->config.outputPath = settings->codecOutputPath;
}
/*----------------------------------------------------------------------------*/
static inline uint8_t gainToLevel(uint8_t gain)
{
  return gain * MAX_LEVEL / 255;
}
/*----------------------------------------------------------------------------*/
static inline uint8_t levelToBar(uint8_t level)
{
  uint8_t result = 0;

  switch ((level + 1) / 2)
  {
    case 4:
      result |= 0x01;
      /* Fall through */
    case 3:
      result |= 0x02;
      /* Fall through */
    case 2:
      result |= 0x04;
      /* Fall through */
    case 1:
      result |= 0x08;
      break;

    default:
      break;
  }

  return result;
}
/*----------------------------------------------------------------------------*/
static inline uint8_t levelToGain(uint8_t level)
{
  return level * 255 / MAX_LEVEL;
}
/*----------------------------------------------------------------------------*/
static uint8_t readSwitchState(struct Board *board)
{
  uint8_t state;

#ifdef CONFIG_OVERRIDE_SW
  (void)board;
  state = CONFIG_OVERRIDE_SW;
#else
  pinReset(board->controlPackage.enR);
  pinSet(board->controlPackage.csR);
  pinSet(board->controlPackage.enR);
  ifRead(board->controlPackage.spi, &state, sizeof(state));
  pinReset(board->controlPackage.csR);
#endif

  return state & SW_MASK;
}
/*----------------------------------------------------------------------------*/
static void slaveLoadSettings(struct SlaveRegOverlay *overlay,
    const struct Settings *settings)
{
  /* Amplifier control */
  overlay->ctl &= ~SLAVE_CTL_MASK;

  if (settings->ampEnabled)
    overlay->ctl |= SLAVE_CTL_POWER;
  if (settings->ampGain & 0x01)
    overlay->ctl |= SLAVE_CTL_GAIN0;
  if (settings->ampGain & 0x02)
    overlay->ctl |= SLAVE_CTL_GAIN1;

  /* Path control */
  overlay->path &= ~SLAVE_PATH_MASK;

  switch (settings->codecInputPath)
  {
    case BOARD_AUDIO_INPUT_A:
      overlay->path |= SLAVE_PATH_INPUT(1);
      break;

    case BOARD_AUDIO_INPUT_B:
      overlay->path |= SLAVE_PATH_INPUT(2);
      break;

    default:
      break;
  }

  switch (settings->codecOutputPath)
  {
    case BOARD_AUDIO_OUTPUT_A:
      overlay->path |= SLAVE_PATH_OUTPUT(1);
      break;

    case BOARD_AUDIO_OUTPUT_B:
      overlay->path |= SLAVE_PATH_OUTPUT(2);
      break;

    default:
      break;
  }

  /* Initial input and output levels */
  overlay->mic = settings->codecInputLevel;
  overlay->spk = settings->codecOutputLevel;
}
/*----------------------------------------------------------------------------*/
static void slaveStoreSettings(struct Settings *settings,
    const struct SlaveRegOverlay *overlay)
{
  settings->ampEnabled = (overlay->ctl & SLAVE_CTL_POWER) != 0;

  settings->ampGain = 0;
  if (overlay->ctl & SLAVE_CTL_GAIN0)
    settings->ampGain |= 0x01;
  if (overlay->ctl & SLAVE_CTL_GAIN1)
    settings->ampGain |= 0x02;

  switch (SLAVE_PATH_INPUT_VALUE(overlay->path))
  {
    case 1:
      settings->codecInputPath = BOARD_AUDIO_INPUT_A;
      break;

    case 2:
      settings->codecInputPath = BOARD_AUDIO_INPUT_B;
      break;

    default:
      settings->codecInputPath = AIC3X_NONE;
      break;
  }

  switch (SLAVE_PATH_OUTPUT_VALUE(overlay->path))
  {
    case 1:
      settings->codecOutputPath = BOARD_AUDIO_OUTPUT_A;
      break;

    case 2:
      settings->codecOutputPath = BOARD_AUDIO_OUTPUT_B;
      break;

    default:
      settings->codecOutputPath = AIC3X_NONE;
      break;
  }

  settings->codecInputLevel = overlay->mic;
  settings->codecOutputLevel = overlay->spk;
}
/*----------------------------------------------------------------------------*/
static void writeLedState(struct Board *board, uint8_t state)
{
  pinReset(board->controlPackage.csW);
  ifWrite(board->controlPackage.spi, &state, sizeof(state));
  pinSet(board->controlPackage.csW);
}
/*----------------------------------------------------------------------------*/
static void onControlUpdateEvent(void *argument)
{
  struct Board * const board = argument;

  if (board->system.slave == NULL)
  {
    if (++board->indication.blink == CONTROL_UPDATE_RATE)
      board->indication.blink = 0;

    if (board->indication.active)
    {
      if (!--board->indication.active)
        board->config.mode = MODE_NONE;
    }
  }

  if (!board->event.read)
  {
    if (wqAdd(WQ_DEFAULT, switchReadTask, board) == E_OK)
      board->event.read = true;
  }

  if (board->system.autosuspend)
  {
    if (!board->system.timeout)
    {
      if (!board->event.suspend)
      {
        if (wqAdd(WQ_DEFAULT, autoSuspendTask, board) == E_OK)
        {
          board->system.timeout = AUTO_SUSPEND_TIMEOUT;
          board->event.suspend = true;
        }
      }
    }
    else
      --board->system.timeout;
  }
}
/*----------------------------------------------------------------------------*/
static void onConversionCompleted(void *argument)
{
  /* R1 = 20 kOhm, R2 = 10 kOhm, Vref = 3300 mV */
  static const uint32_t R1 = 20;
  static const uint32_t R2 = 10;
  static const uint32_t V_REF = 3300;

  struct Board * const board = argument;
  uint16_t sample;
  uint32_t voltage;
  bool powered;

  ifRead(board->adcPackage.adc, &sample, sizeof(sample));

  voltage = ((sample * V_REF * (R1 + R2)) / R2) >> 16;
  powered = voltage >= VOLTAGE_THRESHOLD;

  if (board->system.powered != powered)
  {
    board->system.powered = powered;

    if (board->system.slave != NULL && !board->event.slave)
    {
      if (wqAdd(WQ_DEFAULT, slaveUpdateTask, board) == E_OK)
        board->event.slave = true;
    }
  }

  if (board->system.watchdog != NULL)
    watchdogReload(board->system.watchdog);
}
/*----------------------------------------------------------------------------*/
static void onMicPressed(void *argument)
{
  struct Board * const board = argument;

  // TODO Long press to disable

  switch (board->config.inputPath)
  {
    case BOARD_AUDIO_INPUT_A:
      board->config.inputPath = BOARD_AUDIO_INPUT_B;
      break;

    case BOARD_AUDIO_INPUT_B:
      board->config.inputPath = BOARD_AUDIO_INPUT_A;
      break;

    default:
      board->config.inputPath = BOARD_AUDIO_INPUT_A;
      break;
  }

  if (!board->event.codec)
  {
    if (wqAdd(WQ_DEFAULT, micUpdateTask, board) == E_OK)
      board->event.codec = true;
  }
}
/*----------------------------------------------------------------------------*/
static void onSlaveUpdateEvent(void *argument)
{
  struct Board * const board = argument;

  if (!board->event.slave)
  {
    if (wqAdd(WQ_DEFAULT, slaveUpdateTask, board) == E_OK)
      board->event.slave = true;
  }
}
/*----------------------------------------------------------------------------*/
static void onSpkPressed(void *argument)
{
  struct Board * const board = argument;

  // TODO Long press to disable

  switch (board->config.outputPath)
  {
    case BOARD_AUDIO_OUTPUT_A:
      board->config.outputPath = BOARD_AUDIO_OUTPUT_B;
      break;

    case BOARD_AUDIO_OUTPUT_B:
      board->config.outputPath = BOARD_AUDIO_OUTPUT_A;
      break;

    default:
      board->config.outputPath = AIC3X_NONE;
      break;
  }

  if (!board->event.codec)
  {
    if (wqAdd(WQ_DEFAULT, spkUpdateTask, board) == E_OK)
      board->event.codec = true;
  }
}
/*----------------------------------------------------------------------------*/
static void onVolMPressed(void *argument)
{
  struct Board * const board = argument;

  switch (board->config.mode)
  {
    case MODE_NONE:
      board->config.mode = MODE_SPK;
      break;

    case MODE_MIC:
      if (!board->event.codec && board->config.inputLevel > MIN_LEVEL)
      {
        if (wqAdd(WQ_DEFAULT, volumeUpdateTask, board) == E_OK)
        {
          --board->config.inputLevel;
          board->event.codec = true;
        }
      }
      break;

    case MODE_SPK:
      if (!board->event.codec && board->config.outputLevel > MIN_LEVEL)
      {
        if (wqAdd(WQ_DEFAULT, volumeUpdateTask, board) == E_OK)
        {
          --board->config.outputLevel;
          board->event.codec = true;
        }
      }
      break;
  }

  board->indication.active = MODE_ACTIVE_TIMEOUT;
}
/*----------------------------------------------------------------------------*/
static void onVolPPressed(void *argument)
{
  struct Board * const board = argument;

  switch (board->config.mode)
  {
    case MODE_NONE:
      board->config.mode = MODE_MIC;
      break;

    case MODE_MIC:
      if (!board->event.codec && board->config.inputLevel < MAX_LEVEL)
      {
        if (wqAdd(WQ_DEFAULT, volumeUpdateTask, board) == E_OK)
        {
          ++board->config.inputLevel;
          board->event.codec = true;
        }
      }
      break;

    case MODE_SPK:
      if (!board->event.codec && board->config.outputLevel < MAX_LEVEL)
      {
        if (wqAdd(WQ_DEFAULT, volumeUpdateTask, board) == E_OK)
        {
          ++board->config.outputLevel;
          board->event.codec = true;
        }
      }
      break;
  }

  board->indication.active = MODE_ACTIVE_TIMEOUT;
}
/*----------------------------------------------------------------------------*/
static void autoSuspendTask(void *argument)
{
  struct Board * const board = argument;

  board->event.suspend = false;

  pinReset(board->indication.red);
  boardResetClock();
  interruptEnable(board->system.wakeup);

  /* Wait for activity on I2C lines */
  pmChangeState(PM_SUSPEND);

  interruptDisable(board->system.wakeup);
  boardSetupClock();
  pinSet(board->indication.red);
}
/*----------------------------------------------------------------------------*/
static void ledUpdateTask(void *argument)
{
  struct Board * const board = argument;
  uint8_t value = 0;

  board->event.show = false;

  if (board->system.slave == NULL)
  {
    const bool enabled = board->indication.blink < CONTROL_UPDATE_RATE / 2;

    switch (board->config.mode)
    {
      case MODE_MIC:
        value |= levelToBar(board->config.inputLevel);
        break;

      case MODE_SPK:
        value |= levelToBar(board->config.outputLevel);
        break;

      default:
        break;
    }

    if (board->config.mode != MODE_MIC)
    {
      if (board->config.inputPath == BOARD_AUDIO_INPUT_A)
        value |= 0x80;
      else if (board->config.inputPath == BOARD_AUDIO_INPUT_B)
        value |= 0x40;
    }
    else if (enabled)
    {
      value |= 0xC0;
    }

    if (board->config.mode != MODE_SPK)
    {
      if (board->config.outputPath == BOARD_AUDIO_OUTPUT_A)
        value |= 0x10;
      else if (board->config.outputPath == BOARD_AUDIO_OUTPUT_B)
        value |= 0x20;
    }
    else if (enabled)
    {
      value |= 0x30;
    }
  }
  else
  {
    value = board->indication.state;
  }

  writeLedState(board, value);
}
/*----------------------------------------------------------------------------*/
static void micUpdateTask(void *argument)
{
  struct Board * const board = argument;

  board->event.codec = false;

  codecSetInputPath(board->codecPackage.codec, board->config.inputPath);

  if (!board->event.show)
  {
    if (wqAdd(WQ_DEFAULT, ledUpdateTask, board) == E_OK)
      board->event.show = true;
  }
}
/*----------------------------------------------------------------------------*/
static void slaveUpdateTask(void *argument)
{
  static_assert(sizeof(struct SlaveRegOverlay) == SLAVE_REG_COUNT,
      "Incorrect slave structure");

  struct Board * const board = argument;
  struct SlaveRegOverlay overlay;

  board->event.slave = false;

  ifRead(board->system.slave, &overlay, sizeof(overlay));

  /* Software reset control */
  if (overlay.reset & SLAVE_RESET_RESET)
  {
    nvicResetCore();
    /* Unreachable code */
  }

  /* System control */
  if (overlay.sys & SLAVE_SYS_EXT_CLOCK)
  {
    if ((overlay.sys & SLAVE_SYS_SUSPEND) && !board->event.suspend)
    {
      if (wqAdd(WQ_DEFAULT, autoSuspendTask, board) == E_OK)
      {
        overlay.sys &= ~SLAVE_SYS_SUSPEND;
        board->event.suspend = true;
      }
    }

    board->system.autosuspend = (overlay.sys & SLAVE_SYS_SUSPEND_AUTO) != 0;
    pinReset(board->codecPackage.mux);
  }
  else
  {
    board->system.autosuspend = false;
    pinSet(board->codecPackage.mux);
  }

  if (overlay.sys & SLAVE_SYS_SAVE_CONFIG)
  {
    struct Settings settings;

    memset(&settings, 0, sizeof(settings));
    slaveStoreSettings(&settings, &overlay);
    saveSettings(board->config.memory, FLASH_OFFSET, &settings);

    overlay.sys &= ~SLAVE_SYS_SAVE_CONFIG;
  }

  /* Amplifier control */
  pinWrite(board->ampPackage.power, (overlay.ctl & SLAVE_CTL_POWER) != 0);
  pinWrite(board->ampPackage.gain0, (overlay.ctl & SLAVE_CTL_GAIN0) != 0);
  pinWrite(board->ampPackage.gain1, (overlay.ctl & SLAVE_CTL_GAIN1) != 0);

  /* External voltage status */
  overlay.status = board->system.powered ? SLAVE_STATUS_POWER_READY : 0;

  /* LED */
  if (board->indication.state != overlay.led)
  {
    board->indication.state = overlay.led;

    if (!board->event.show)
    {
      if (wqAdd(WQ_DEFAULT, ledUpdateTask, board) == E_OK)
        board->event.show = true;
    }
  }

  /* Switches */
  overlay.sw = board->system.sw;

  /* Clear unused bits */
  overlay.sys &= SLAVE_SYS_MASK;
  overlay.ctl &= SLAVE_CTL_MASK;

  ifWrite(board->system.slave, &overlay, sizeof(overlay));

  board->system.timeout = board->system.autosuspend ?
      AUTO_SUSPEND_TIMEOUT : 0;
}
/*----------------------------------------------------------------------------*/
static void spkUpdateTask(void *argument)
{
  struct Board * const board = argument;

  board->event.codec = false;

  if (board->config.outputPath == BOARD_AUDIO_OUTPUT_B)
    pinSet(board->ampPackage.power);
  else
    pinReset(board->ampPackage.power);

  codecSetOutputPath(board->codecPackage.codec, board->config.outputPath);

  if (!board->event.show)
  {
    if (wqAdd(WQ_DEFAULT, ledUpdateTask, board) == E_OK)
      board->event.show = true;
  }
}
/*----------------------------------------------------------------------------*/
static void startupTask(void *argument)
{
  struct Board * const board = argument;
  const uint8_t sw = readSwitchState(board);

  struct Settings settings;
  bool ready = true;

  writeLedState(board, 0);

  const bool valid = loadSettings(board->config.memory, FLASH_OFFSET,
      &settings);

  if ((sw & SW_LOAD_CONFIG) && valid)
    codecLoadSettings(board, &settings);
  else
    codecLoadDefaultSettings(board);

  if (sw & SW_ACTIVE)
  {
    const bool pll = (sw & SW_EXT_CLOCK) == 0;

    if (!boardSetupCodecPackage(&board->codecPackage, WQ_DEFAULT, true, pll))
      ready = false;

    switchReadTask(board);
    micUpdateTask(board);
    spkUpdateTask(board);
    volumeUpdateTask(board);
  }
  else
  {
    board->system.sw = sw;

    if ((board->system.slave = boardMakeI2CSlave()) != NULL)
    {
      struct SlaveRegOverlay overlay;
      memset(&overlay, 0, sizeof(overlay));

      overlay.sw = sw;

      if (sw & SW_EXT_CLOCK)
        overlay.sys |= SLAVE_SYS_EXT_CLOCK;
      if (!(sw & SW_LOAD_CONFIG))
        overlay.ctl |= SLAVE_CTL_POWER;
      if (sw & SW_OUTPUT_GAIN_BOOST)
        overlay.ctl |= SLAVE_CTL_GAIN0 | SLAVE_CTL_GAIN1;

      if ((sw & SW_LOAD_CONFIG) && valid)
        slaveLoadSettings(&overlay, &settings);

      ifWrite(board->system.slave, &overlay, sizeof(overlay));
      ifSetCallback(board->system.slave, onSlaveUpdateEvent, board);
    }
    else
      ready = false;

    if (!boardSetupCodecPackage(&board->codecPackage, NULL, false, false))
      ready = false;

    slaveUpdateTask(board);
  }

  assert(ready);
  (void)ready;

  if (board->system.slave == NULL)
  {
    interruptSetCallback(board->buttonPackage.buttons[0], onMicPressed, board);
    interruptEnable(board->buttonPackage.buttons[0]);
    interruptSetCallback(board->buttonPackage.buttons[1], onSpkPressed, board);
    interruptEnable(board->buttonPackage.buttons[1]);
    interruptSetCallback(board->buttonPackage.buttons[2], onVolMPressed, board);
    interruptEnable(board->buttonPackage.buttons[2]);
    interruptSetCallback(board->buttonPackage.buttons[3], onVolPPressed, board);
    interruptEnable(board->buttonPackage.buttons[3]);
    timerEnable(board->buttonPackage.base);
  }

  timerSetOverflow(board->controlPackage.timer,
      timerGetFrequency(board->controlPackage.timer) / CONTROL_UPDATE_RATE);
  timerSetCallback(board->controlPackage.timer, onControlUpdateEvent, board);
  timerEnable(board->controlPackage.timer);

  /* CONTROL_UPDATE_RATE * 2 Hz ADC trigger rate, start ADC sampling */
  ifSetParam(board->adcPackage.adc, IF_ENABLE, NULL);
  ifSetCallback(board->adcPackage.adc, onConversionCompleted, board);
  timerSetOverflow(board->adcPackage.timer,
      timerGetFrequency(board->adcPackage.timer) / (CONTROL_UPDATE_RATE * 2));
  timerEnable(board->adcPackage.timer);

#ifdef ENABLE_DBG
  /* 24-bit SysTick timer is used for debug purposes */
  timerSetCallback(board->debug.timer, onLoadTimerOverflow, board);
  timerSetOverflow(board->debug.timer, timerGetFrequency(board->debug.timer));
  timerEnable(board->debug.timer);
#endif
}
/*----------------------------------------------------------------------------*/
static void switchReadTask(void *argument)
{
  struct Board * const board = argument;
  const uint8_t state = readSwitchState(board);

  board->event.read = false;

  if (state != board->system.sw)
  {
    if (board->system.slave == NULL)
    {
      const bool boost = (state & SW_OUTPUT_GAIN_BOOST) != 0;

      pinWrite(board->codecPackage.mux, (state & SW_EXT_CLOCK) == 0);
      pinWrite(board->ampPackage.gain0, boost);
      pinWrite(board->ampPackage.gain1, boost);

      codecSetSampleRate(board->codecPackage.codec,
          (state & SW_SAMPLE_RATE) ? 48000 : 44100);
      codecSetAGCEnabled(board->codecPackage.codec,
          (state & SW_INPUT_GAIN_AUTO) != 0);

      board->system.sw = state;
    }
    else if (!board->event.slave)
    {
      if (wqAdd(WQ_DEFAULT, slaveUpdateTask, board) == E_OK)
      {
        board->system.sw = state;
        board->event.slave = true;
      }
    }
  }

  if (board->system.slave == NULL && !board->event.show)
  {
    if (wqAdd(WQ_DEFAULT, ledUpdateTask, board) == E_OK)
      board->event.show = true;
  }
}
/*----------------------------------------------------------------------------*/
static void volumeUpdateTask(void *argument)
{
  struct Board * const board = argument;

  board->event.codec = false;

  if (board->config.inputPath != AIC3X_NONE)
  {
    const uint8_t gain = levelToGain(board->config.inputLevel);

    if (board->config.inputPath == BOARD_AUDIO_INPUT_A)
    {
      codecSetInputGain(board->codecPackage.codec, CHANNEL_LEFT, gain);
      codecSetInputGain(board->codecPackage.codec, CHANNEL_RIGHT, 0);
    }
    else
      codecSetInputGain(board->codecPackage.codec, CHANNEL_BOTH, gain);
  }
  if (board->config.outputPath != AIC3X_NONE)
  {
    codecSetOutputGain(board->codecPackage.codec, CHANNEL_BOTH,
        levelToGain(board->config.outputLevel));
  }

  if (!board->event.show)
  {
    if (wqAdd(WQ_DEFAULT, ledUpdateTask, board) == E_OK)
      board->event.show = true;
  }
}
/*----------------------------------------------------------------------------*/
#ifdef ENABLE_DBG
static void debugInfoTask(void *argument)
{
  struct Board * const board = argument;

  /* Heap used */
  void * const stub = malloc(0);
  const unsigned int used = (unsigned int)((uintptr_t)stub - (uintptr_t)board);
  free(stub);

  /* CPU used */
  const unsigned int loops = board->debug.loops <= board->debug.idle ?
      board->debug.idle - board->debug.loops : 0;
  const unsigned int load = board->debug.idle > 100 ?
      loops / (board->debug.idle / 100) : 100;

  size_t count;
  char text[64];

  count = sprintf(text, "Heap %u ticks %u cpu %u%%\r\n", used, loops, load);
  ifWrite(board->debug.serial, text, count);
}
#endif
/*----------------------------------------------------------------------------*/
#ifdef ENABLE_DBG
static void onLoadTimerOverflow(void *argument)
{
  struct Board * const board = argument;

  struct WqInfo info;
  wqStatistics(WQ_DEFAULT, &info);

  board->debug.loops = info.loops;
  wqAdd(WQ_DEFAULT, debugInfoTask, board);
}
#endif
/*----------------------------------------------------------------------------*/
void invokeStartupTask(struct Board *board)
{
  wqAdd(WQ_DEFAULT, startupTask, board);
}
