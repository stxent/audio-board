/*
 * board/audioboard_v1/shared/board_shared.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_AUDIOBOARD_V1_SHARED_BOARD_SHARED_H_
#define BOARD_AUDIOBOARD_V1_SHARED_BOARD_SHARED_H_
/*----------------------------------------------------------------------------*/
#include <halm/pin.h>
#include <stdbool.h>
/*----------------------------------------------------------------------------*/
struct Interface;
struct Interrupt;
struct SoftwareTimerFactory;
struct Timer;
struct Watchdog;
struct WorkQueue;
/*----------------------------------------------------------------------------*/
#define BOARD_ADC_PIN                   PIN(1, 10)
#define BOARD_BUTTON_MIC_PIN            PIN(0, 7)
#define BOARD_BUTTON_SPK_PIN            PIN(1, 8)
#define BOARD_BUTTON_VOL_M_PIN          PIN(1, 11)
#define BOARD_BUTTON_VOL_P_PIN          PIN(0, 2)
#define BOARD_LED_PIN                   PIN(1, 2)
#define BOARD_SPI_CS0_PIN               PIN(0, 3)
#define BOARD_SPI_CS1_PIN               PIN(1, 4)
#define BOARD_SPI_EN1_PIN               PIN(3, 2)

#define BOARD_AMP_GAIN0_PIN             PIN(3, 4)
#define BOARD_AMP_GAIN1_PIN             PIN(3, 5)
#define BOARD_AMP_POWER_PIN             PIN(1, 9)
#define BOARD_I2S_CLK_PIN               PIN(0, 1)
#define BOARD_I2S_MUX_PIN               PIN(2, 0)
#define BOARD_I2S_RST_PIN               PIN(1, 5)

#define BOARD_AUDIO_INPUT_CH_A          CHANNEL_LEFT
#define BOARD_AUDIO_INPUT_PATH_A        AIC3X_MIC_1_IN
#define BOARD_AUDIO_INPUT_CH_B          (CHANNEL_LEFT | CHANNEL_RIGHT)
#define BOARD_AUDIO_INPUT_PATH_B        AIC3X_LINE_2_IN
#define BOARD_AUDIO_OUTPUT_CH_A         CHANNEL_LEFT
#define BOARD_AUDIO_OUTPUT_PATH_A       AIC3X_HP_OUT_DIFF
#define BOARD_AUDIO_OUTPUT_CH_B         (CHANNEL_LEFT | CHANNEL_RIGHT)
#define BOARD_AUDIO_OUTPUT_PATH_B       AIC3X_LINE_OUT_DIFF

struct AdcPackage
{
  struct Interface *adc;
  struct Timer *timer;
};

struct AmpPackage
{
  struct Pin gain0;
  struct Pin gain1;
  struct Pin power;
};

struct ButtonPackage
{
  struct Timer *base;
  struct SoftwareTimerFactory *factory;

  struct Interrupt *buttons[4];
  struct Interrupt *events[4];
  struct Timer *timers[4];
};

struct CodecPackage
{
  struct Entity *codec;
  struct Interface *i2c;
  struct Timer *timer;
  struct Pin mux;
  struct Pin reset;
};

struct ControlPackage
{
  struct Pin enR;
  struct Pin csR;
  struct Pin csW;
  struct Interface *spi;
  struct Timer *timer;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

struct Interface *boardMakeAdc(void);
struct Timer *boardMakeAdcTimer(void);
struct Timer *boardMakeCodecTimer(void);
struct Timer *boardMakeControlTimer(void);
struct Entity *boardMakeCodec(struct WorkQueue *, struct Interface *,
    struct Timer *, uint16_t);
struct Interface *boardMakeI2CMaster(void);
struct Interface *boardMakeI2CSlave(void);
struct Timer *boardMakeLoadTimer(void);
struct Interface *boardMakeMemory(void);
struct Interface *boardMakeSerial(void);
struct Interface *boardMakeSpi(void);
struct Interrupt *boardMakeWakeupInt(void);
struct Watchdog *boardMakeWatchdog(void);

bool boardRecoverState(uint32_t *);
void boardSaveState(uint32_t);
bool boardSetupAdcPackage(struct AdcPackage *);
bool boardSetupAmpPackage(struct AmpPackage *);
bool boardSetupButtonPackage(struct ButtonPackage *);
bool boardSetupCodecPackage(struct CodecPackage *, struct WorkQueue *, bool,
    bool);
bool boardSetupControlPackage(struct ControlPackage *);
void boardResetClock(void);
bool boardSetupClock(void);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_AUDIOBOARD_V1_SHARED_BOARD_SHARED_H_ */
