/*
 * board/audioboard_v1/shared/board_shared.c
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#include "board_shared.h"
#include "slave.h"
#include <dpm/audio/tlv320aic3x.h>
#include <dpm/button.h>
#include <halm/core/cortex/systick.h>
#include <halm/generic/timer_factory.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/lpc/adc.h>
#include <halm/platform/lpc/backup_domain.h>
#include <halm/platform/lpc/clocking.h>
#include <halm/platform/lpc/flash.h>
#include <halm/platform/lpc/gptimer.h>
#include <halm/platform/lpc/i2c.h>
#include <halm/platform/lpc/i2c_slave.h>
#include <halm/platform/lpc/pin_int.h>
#include <halm/platform/lpc/serial.h>
#include <halm/platform/lpc/spi.h>
#include <halm/platform/lpc/wakeup_int.h>
#include <halm/platform/lpc/wdt.h>
#include <assert.h>
/*----------------------------------------------------------------------------*/
#define BACKUP_MAGIC_WORD 0xB6A617A5UL
/*----------------------------------------------------------------------------*/
#define PRI_TIMER_DBG 3

#define PRI_I2C       2

#define PRI_SERIAL    1
#define PRI_SPI       1
#define PRI_TIMER_SYS 1

#define PRI_ADC       0
#define PRI_WAKEUP    0
/*----------------------------------------------------------------------------*/
void boardResetClock(void)
{
  static const struct GenericClockConfig mainClockConfigInt = {
      .divisor = 3,
      .source = CLOCK_INTERNAL
  };

  clockEnable(MainClock, &mainClockConfigInt);
  clockDisable(ClockOutput);
  clockDisable(ExternalOsc);
}
/*----------------------------------------------------------------------------*/
bool boardSetupClock(void)
{
  static const struct ExternalOscConfig extOscConfig = {
      .frequency = 12000000
  };
  static const struct GenericClockConfig mainClockConfigExt = {
      .divisor = 1,
      .source = CLOCK_EXTERNAL
  };
  static const struct ClockOutputConfig outputClockConfig = {
      .divisor = 1,
      .pin = BOARD_I2S_CLK_PIN,
      .source = CLOCK_EXTERNAL
  };

  [[maybe_unused]] enum Result res;

  if (clockEnable(ExternalOsc, &extOscConfig) != E_OK)
    return false;
  while (!clockReady(ExternalOsc));

  clockEnable(MainClock, &mainClockConfigExt);

  res = clockEnable(ClockOutput, &outputClockConfig);
  assert(res == E_OK);
  while (!clockReady(ClockOutput));

  return true;
}
/*----------------------------------------------------------------------------*/
void boardSetupDefaultWQ(void)
{
  static const struct WorkQueueConfig wqConfig = {
      .size = 8
  };

  WQ_DEFAULT = init(WorkQueue, &wqConfig);
  assert(WQ_DEFAULT != NULL);
}
/*----------------------------------------------------------------------------*/
bool boardRecoverState(uint32_t *state)
{
  const uint32_t * const backup = backupDomainAddress();

  if (backup[0] == BACKUP_MAGIC_WORD)
  {
    *state = backup[1];
    return true;
  }
  else
    return false;
}
/*----------------------------------------------------------------------------*/
void boardSaveState(uint32_t state)
{
  uint32_t * const backup = backupDomainAddress();

  backup[0] = BACKUP_MAGIC_WORD;
  backup[1] = state;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeAdc(void)
{
  static const PinNumber adcPins[] = {
      BOARD_ADC_PIN,
      0
  };
  static const struct AdcConfig adcConfig = {
      .pins = adcPins,
      .event = ADC_CT32B0_MAT0,
      .priority = PRI_ADC,
      .channel = 0
  };

  struct Interface * const interface = init(Adc, &adcConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeAdcTimer(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .channel = GPTIMER_CT32B0
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeCodecTimer(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000,
      .priority = PRI_I2C,
      .channel = GPTIMER_CT16B1
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeLoadTimer(void)
{
  static const struct GpTimerConfig timerConfig = {
      .frequency = 1000000,
      .priority = PRI_TIMER_DBG,
      .channel = GPTIMER_CT32B1
  };

  struct Timer * const timer = init(GpTimer, &timerConfig);
  assert(timer != NULL);
  return timer;
}
/*----------------------------------------------------------------------------*/
struct Entity *boardMakeCodec(struct WorkQueue *wq, struct Interface *i2c,
    struct Timer *timer, uint16_t prescaler)
{
  const struct TLV320AIC3xConfig codecConfig = {
      .bus = i2c,
      .timer = timer,
      .address = 0x18,
      .rate = 0,
      .samplerate = 44100,
      .prescaler = prescaler,
      .reset = BOARD_I2S_RST_PIN,
      .type = AIC3X_TYPE_3104
  };

  struct TLV320AIC3x * const codec = init(TLV320AIC3x, &codecConfig);
  assert(codec != NULL);
  codecSetUpdateWorkQueue(codec, wq);
  codecReset(codec);

  return (struct Entity *)codec;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2CMaster(void)
{
  static const struct I2CConfig i2cMasterConfig = {
      .rate = 400000, /* Initial rate */
      .scl = PIN(0, 4),
      .sda = PIN(0, 5),
      .priority = PRI_I2C,
      .channel = 0
  };

  struct Interface * const interface = init(I2C, &i2cMasterConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2CSlave(void)
{
  static const struct I2CSlaveConfig i2cSlaveConfig = {
      .size = SLAVE_REG_COUNT,
      .scl = PIN(0, 4),
      .sda = PIN(0, 5),
      .priority = PRI_I2C,
      .channel = 0
  };

  struct Interface * const interface = init(I2CSlave, &i2cSlaveConfig);
  assert(interface != NULL);

  [[maybe_unused]] const enum Result res = ifSetParam(interface, IF_ADDRESS,
      &(uint32_t){SLAVE_ADDRESS});
  assert(res == E_OK);

  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeMemory(void)
{
  struct Interface * const interface = init(Flash, NULL);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSerial(void)
{
  static const struct SerialConfig serialConfig = {
      .rxLength = 16,
      .txLength = 128,
      .rate = 19200,
      .rx = PIN(1, 6),
      .tx = PIN(1, 7),
      .priority = PRI_SERIAL,
      .channel = 0
  };

  struct Interface * const interface = init(Serial, &serialConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSpi(void)
{
  static const struct SpiConfig spiConfig = {
      .rate = 1000000,
      .miso = PIN(0, 8),
      .mosi = PIN(0, 9),
      .sck = PIN(0, 6),
      .priority = PRI_SPI,
      .channel = 0,
      .mode = 0
  };

  struct Interface * const interface = init(Spi, &spiConfig);
  assert(interface != NULL);
  return interface;
}
/*----------------------------------------------------------------------------*/
struct Interrupt *boardMakeWakeupInt(void)
{
  static const struct WakeupIntConfig wakeupIntConfig = {
      .pin = PIN(0, 5),
      .priority = PRI_WAKEUP,
      .event = INPUT_FALLING,
      .pull = PIN_NOPULL
  };

  struct Interrupt * const interrupt = init(WakeupInt, &wakeupIntConfig);
  assert(interrupt != NULL);
  return interrupt;
}
/*----------------------------------------------------------------------------*/
struct Watchdog *boardMakeWatchdog(void)
{
  /* Clocks */
  static const struct WdtOscConfig wdtOscConfig = {
      .frequency = WDT_FREQ_600
  };
  static const struct GenericClockConfig wdtClockConfig = {
      .source = CLOCK_WDT
  };

  /* Objects */
  static const struct WdtConfig wdtConfig = {
      .period = 1000
  };

  [[maybe_unused]] enum Result res;

  res = clockEnable(WdtOsc, &wdtOscConfig);
  assert(res == E_OK);
  while (!clockReady(WdtOsc));

  clockEnable(WdtClock, &wdtClockConfig);
  while (!clockReady(WdtClock));

  struct Watchdog * const watchdog = init(Wdt, &wdtConfig);
  assert(watchdog != NULL);
  return watchdog;
}
/*----------------------------------------------------------------------------*/
struct AdcPackage boardSetupAdcPackage(void)
{
  struct AdcPackage package;

  package.timer = boardMakeAdcTimer();
  assert(package.timer != NULL);

  package.adc = boardMakeAdc();
  assert(package.adc != NULL);

  return package;
}
/*----------------------------------------------------------------------------*/
struct AmpPackage boardSetupAmpPackage(void)
{
  struct AmpPackage package;

  package.power = pinInit(BOARD_AMP_POWER_PIN);
  pinOutput(package.power, false);

  package.gain0 = pinInit(BOARD_AMP_GAIN0_PIN);
  pinOutput(package.gain0, false);

  package.gain1 = pinInit(BOARD_AMP_GAIN1_PIN);
  pinOutput(package.gain1, false);

  return package;
}
/*----------------------------------------------------------------------------*/
struct ButtonPackage boardSetupButtonPackage(struct TimerFactory *factory)
{
  static const struct PinIntConfig buttonIntConfigs[] = {
      /* MIC */
      {
          .pin = BOARD_BUTTON_MIC_PIN,
          .event = INPUT_FALLING,
          .pull = PIN_PULLUP
      },
      /* SPK */
      {
          .pin = BOARD_BUTTON_SPK_PIN,
          .event = INPUT_FALLING,
          .pull = PIN_PULLUP
      },
      /* VOL- */
      {
          .pin = BOARD_BUTTON_VOL_M_PIN,
          .event = INPUT_FALLING,
          .pull = PIN_PULLUP
      },
      /* VOL+ */
      {
          .pin = BOARD_BUTTON_VOL_P_PIN,
          .event = INPUT_FALLING,
          .pull = PIN_PULLUP
      }
  };

  struct ButtonPackage package;

  static_assert(ARRAY_SIZE(buttonIntConfigs) == ARRAY_SIZE(package.buttons),
      "Incorrect button count");
  static_assert(ARRAY_SIZE(buttonIntConfigs) == ARRAY_SIZE(package.events),
      "Incorrect event count");
  static_assert(ARRAY_SIZE(buttonIntConfigs) == ARRAY_SIZE(package.timers),
      "Incorrect timer count");

  for (size_t i = 0; i < ARRAY_SIZE(buttonIntConfigs); ++i)
  {
    package.events[i] = init(PinInt, &buttonIntConfigs[i]);
    assert(package.events[i] != NULL);

    package.timers[i] = timerFactoryCreate(factory);
    assert(package.timers[i] != NULL);

    const struct ButtonConfig buttonConfig = {
        .interrupt = package.events[i],
        .timer = package.timers[i],
        .pin = buttonIntConfigs[i].pin,
        .delay = 2,
        .level = false
    };
    package.buttons[i] = init(Button, &buttonConfig);
    assert(package.buttons[i] != NULL);
  }

  return package;
}
/*----------------------------------------------------------------------------*/
struct ChronoPackage boardSetupChronoPackage(void)
{
  struct ChronoPackage package;

  package.load = boardMakeLoadTimer();

  package.base = init(SysTick, &(struct SysTickConfig){PRI_TIMER_SYS});
  assert(package.base != NULL);
  timerSetOverflow(package.base, timerGetFrequency(package.base) / 200);

  package.factory = init(TimerFactory,
      &(struct TimerFactoryConfig){package.base});
  assert(package.factory != NULL);

  return package;
}
/*----------------------------------------------------------------------------*/
struct CodecPackage boardSetupCodecPackage(struct WorkQueue *wq, bool active,
    bool pll)
{
  struct CodecPackage package;

  package.mux = pinInit(BOARD_I2S_MUX_PIN);
  pinOutput(package.mux, true);

  if (active)
  {
    package.i2c = boardMakeI2CMaster();
    package.timer = boardMakeCodecTimer();
    package.codec = boardMakeCodec(wq, package.i2c, package.timer,
        pll ? 0 : 256);
  }
  else
  {
    package.reset = pinInit(BOARD_I2S_RST_PIN);
    pinOutput(package.reset, true);
  }

  return package;
}
/*----------------------------------------------------------------------------*/
struct ControlPackage boardSetupControlPackage(struct TimerFactory *factory)
{
  struct ControlPackage package;

  package.csR = pinStub();
  package.csW = pinStub();
  package.spi = NULL;
  package.timer = NULL;

  package.enR = pinInit(BOARD_SPI_EN1_PIN);
  pinOutput(package.enR, true);

  package.csR = pinInit(BOARD_SPI_CS1_PIN);
  pinOutput(package.csR, false);

  package.csW = pinInit(BOARD_SPI_CS0_PIN);
  pinOutput(package.csW, true);

  package.spi = boardMakeSpi();
  package.timer = timerFactoryCreate(factory);

  return package;
}
