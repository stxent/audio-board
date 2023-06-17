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
#include <halm/generic/software_timer.h>
#include <halm/generic/work_queue.h>
#include <halm/platform/lpc/adc.h>
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
#define PRI_TIMER_DBG 2

#define PRI_BUTTON    1
#define PRI_I2C       1
#define PRI_SERIAL    1
#define PRI_SPI       1

#define PRI_ADC       0
#define PRI_TIMER_SYS 0
#define PRI_WAKEUP    0
/*----------------------------------------------------------------------------*/
static const struct AdcConfig adcConfig = {
    .pins = (const PinNumber []){BOARD_ADC_PIN, 0},
    .event = ADC_CT32B0_MAT0,
    .priority = PRI_ADC,
    .channel = 0
};

static const struct GpTimerConfig adcTimerConfig = {
    .frequency = 1000000,
    .channel = GPTIMER_CT32B0
};

static const struct PinIntConfig buttonIntConfigs[] = {
    /* MIC */
    {
        .pin = BOARD_BUTTON_MIC_PIN,
        .event = PIN_FALLING,
        .pull = PIN_PULLUP
    },
    /* SPK */
    {
        .pin = BOARD_BUTTON_SPK_PIN,
        .event = PIN_FALLING,
        .pull = PIN_PULLUP
    },
    /* VOL- */
    {
        .pin = BOARD_BUTTON_VOL_M_PIN,
        .event = PIN_FALLING,
        .pull = PIN_PULLUP
    },
    /* VOL+ */
    {
        .pin = BOARD_BUTTON_VOL_P_PIN,
        .event = PIN_FALLING,
        .pull = PIN_PULLUP
    }
};

static const struct GpTimerConfig buttonTimerConfig = {
    .frequency = 1000,
    .priority = PRI_BUTTON,
    .channel = GPTIMER_CT16B0
};

static const struct GpTimerConfig codecTimerConfig = {
    .frequency = 1000,
    .priority = PRI_I2C,
    .channel = GPTIMER_CT16B1
};

static const struct GpTimerConfig controlTimerConfig = {
    .frequency = 1000000,
    .priority = PRI_TIMER_SYS,
    .channel = GPTIMER_CT32B1
};

static const struct I2CConfig i2cMasterConfig = {
    .rate = 400000, /* Initial rate */
    .scl = PIN(0, 4),
    .sda = PIN(0, 5),
    .priority = PRI_I2C,
    .channel = 0
};

static const struct I2CSlaveConfig i2cSlaveConfig = {
    .size = SLAVE_REG_COUNT,
    .scl = PIN(0, 4),
    .sda = PIN(0, 5),
    .priority = PRI_I2C,
    .channel = 0
};

static const struct SerialConfig serialConfig = {
    .rxLength = 16,
    .txLength = 128,
    .rate = 19200,
    .rx = PIN(1, 6),
    .tx = PIN(1, 7),
    .priority = PRI_SERIAL,
    .channel = 0
};

static const struct SpiConfig spiConfig = {
    .rate = 1000000,
    .miso = PIN(0, 8),
    .mosi = PIN(0, 9),
    .sck = PIN(0, 6),
    .priority = PRI_SPI,
    .channel = 0,
    .mode = 0
};

static const struct WakeupIntConfig wakeupIntConfig = {
    .pin = i2cMasterConfig.sda,
    .priority = PRI_WAKEUP,
    .event = PIN_FALLING,
    .pull = PIN_NOPULL
};

static const struct WdtConfig wdtConfig = {
    .period = 1000
};
/*----------------------------------------------------------------------------*/
static const struct ExternalOscConfig extOscConfig = {
    .frequency = 12000000
};

static const struct GenericClockConfig mainClockConfigExt = {
    .source = CLOCK_EXTERNAL,
    .divisor = 3
};

static const struct GenericClockConfig mainClockConfigInt = {
    .source = CLOCK_INTERNAL,
    .divisor = 3
};

static const struct ClockOutputConfig outputClockConfig = {
    .source = CLOCK_EXTERNAL,
    .divisor = 1,
    .pin = BOARD_I2S_CLK_PIN
};

static const struct WdtOscConfig wdtOscConfig = {
    .frequency = WDT_FREQ_600
};

static const struct GenericClockConfig wdtClockConfig = {
    .source = CLOCK_WDT
};
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeAdc(void)
{
  return init(Adc, &adcConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeAdcTimer(void)
{
  return init(GpTimer, &adcTimerConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeCodecTimer(void)
{
  return init(GpTimer, &codecTimerConfig);
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeControlTimer(void)
{
  return init(GpTimer, &controlTimerConfig);
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
      .prescaler = prescaler,
      .reset = BOARD_I2S_RST_PIN
  };
  struct TLV320AIC3x * const codec = init(TLV320AIC3x, &codecConfig);

  if (codec != NULL)
  {
    codecSetUpdateWorkQueue(codec, wq);
    codecReset(codec, 44100, AIC3X_NONE, AIC3X_NONE);
  }

  return (struct Entity *)codec;
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2CMaster(void)
{
  return init(I2C, &i2cMasterConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeI2CSlave(void)
{
  static const uint32_t address = SLAVE_ADDRESS;

  struct Interface * const interface = init(I2CSlave, &i2cSlaveConfig);

  if (interface != NULL)
  {
    const enum Result res = ifSetParam(interface, IF_ADDRESS, &address);

    assert(res == E_OK);
    (void)res;
  }

  return interface;
}
/*----------------------------------------------------------------------------*/
struct Timer *boardMakeLoadTimer(void)
{
  return init(SysTick, &(struct SysTickConfig){PRI_TIMER_DBG});
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeMemory(void)
{
  return init(Flash, NULL);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSerial(void)
{
  return init(Serial, &serialConfig);
}
/*----------------------------------------------------------------------------*/
struct Interface *boardMakeSpi(void)
{
  return init(Spi, &spiConfig);
}
/*----------------------------------------------------------------------------*/
struct Interrupt *boardMakeWakeupInt(void)
{
  return init(WakeupInt, &wakeupIntConfig);
}
/*----------------------------------------------------------------------------*/
struct Watchdog *boardMakeWatchdog(void)
{
  if (clockEnable(WdtOsc, &wdtOscConfig) != E_OK)
    return NULL;
  while (!clockReady(WdtOsc));

  if (clockEnable(WdtClock, &wdtClockConfig) != E_OK)
    return NULL;
  while (!clockReady(WdtClock));

  return init(Wdt, &wdtConfig);
}
/*----------------------------------------------------------------------------*/
bool boardSetupAdcPackage(struct AdcPackage *package)
{
  package->adc = NULL;

  package->timer = boardMakeAdcTimer();
  if (package->timer == NULL)
    return false;

  package->adc = boardMakeAdc();
  return package->adc != NULL;
}
/*----------------------------------------------------------------------------*/
bool boardSetupAmpPackage(struct AmpPackage *package)
{
  package->gain0 = pinStub();
  package->gain1 = pinStub();

  package->power = pinInit(BOARD_AMP_POWER_PIN);
  pinOutput(package->power, false);

  package->gain0 = pinInit(BOARD_AMP_GAIN0_PIN);
  pinOutput(package->gain0, false);

  package->gain1 = pinInit(BOARD_AMP_GAIN1_PIN);
  pinOutput(package->gain1, false);

  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupButtonPackage(struct ButtonPackage *package)
{
  static_assert(ARRAY_SIZE(buttonIntConfigs) == ARRAY_SIZE(package->buttons),
      "Incorrect button count");
  static_assert(ARRAY_SIZE(buttonIntConfigs) == ARRAY_SIZE(package->events),
      "Incorrect event count");
  static_assert(ARRAY_SIZE(buttonIntConfigs) == ARRAY_SIZE(package->timers),
      "Incorrect timer count");

  package->factory = NULL;

  for (size_t i = 0; i < ARRAY_SIZE(buttonIntConfigs); ++i)
  {
    package->buttons[i] = NULL;
    package->events[i] = NULL;
    package->timers[i] = NULL;
  }

  package->base = init(GpTimer, &buttonTimerConfig);
  if (package->base == NULL)
    return false;
  timerSetOverflow(package->base, 5);

  package->factory = init(SoftwareTimerFactory,
      &(struct SoftwareTimerFactoryConfig){package->base});
  if (package->factory == NULL)
    return false;

  for (size_t i = 0; i < ARRAY_SIZE(buttonIntConfigs); ++i)
  {
    package->events[i] = init(PinInt, &buttonIntConfigs[i]);
    if (package->events[i] == NULL)
      return false;

    package->timers[i] = softwareTimerCreate(package->factory);
    if (package->timers[i] == NULL)
      return false;
    timerSetOverflow(package->timers[i], 2);

    const struct ButtonConfig buttonConfig = {
        .interrupt = package->events[i],
        .timer = package->timers[i],
        .pin = buttonIntConfigs[i].pin,
        .delay = 2,
        .level = false
    };
    package->buttons[i] = init(Button, &buttonConfig);
    if (package->buttons[i] == NULL)
      return false;
  }

  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupCodecPackage(struct CodecPackage *package, struct WorkQueue *wq,
    bool active, bool pll)
{
  package->codec = NULL;
  package->i2c = NULL;
  package->timer = NULL;

  package->mux = pinInit(BOARD_I2S_MUX_PIN);
  pinOutput(package->mux, true);

  if (active)
  {
    package->i2c = boardMakeI2CMaster();
    if (package->i2c == NULL)
      return false;

    package->timer = boardMakeCodecTimer();
    if (package->timer == NULL)
      return false;

    package->codec = boardMakeCodec(wq, package->i2c, package->timer,
        pll ? 0 : 256);
    if (package->codec == NULL)
      return false;
  }
  else
  {
    package->reset = pinInit(BOARD_I2S_RST_PIN);
    pinOutput(package->reset, true);
  }

  return true;
}
/*----------------------------------------------------------------------------*/
bool boardSetupControlPackage(struct ControlPackage *package)
{
  package->csR = pinStub();
  package->csW = pinStub();
  package->spi = NULL;
  package->timer = NULL;

  package->enR = pinInit(BOARD_SPI_EN1_PIN);
  pinOutput(package->enR, true);

  package->csR = pinInit(BOARD_SPI_CS1_PIN);
  pinOutput(package->csR, false);

  package->csW = pinInit(BOARD_SPI_CS0_PIN);
  pinOutput(package->csW, true);

  package->spi = boardMakeSpi();
  if (package->spi == NULL)
    return false;

  package->timer = boardMakeControlTimer();
  return package->timer != NULL;
}
/*----------------------------------------------------------------------------*/
void boardResetClock(void)
{
  clockEnable(MainClock, &mainClockConfigInt);
  clockDisable(ClockOutput);
  clockDisable(ExternalOsc);
}
/*----------------------------------------------------------------------------*/
bool boardSetupClock(void)
{
  if (clockEnable(ExternalOsc, &extOscConfig) != E_OK)
    return false;
  while (!clockReady(ExternalOsc));

  if (clockEnable(MainClock, &mainClockConfigExt) != E_OK)
    return false;

  if (clockEnable(ClockOutput, &outputClockConfig) != E_OK)
    return false;
  while (!clockReady(ClockOutput));

  return true;
}
