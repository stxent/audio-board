/*
 * board/audioboard_v1/applications/active/board.h
 * Copyright (C) 2023 xent
 * Project is distributed under the terms of the GNU General Public License v3.0
 */

#ifndef BOARD_AUDIOBOARD_V1_APPLICATIONS_ACTIVE_BOARD_H_
#define BOARD_AUDIOBOARD_V1_APPLICATIONS_ACTIVE_BOARD_H_
/*----------------------------------------------------------------------------*/
#include "board_shared.h"
#include <dpm/audio/tlv320aic3x.h>
#include <halm/pin.h>
/*----------------------------------------------------------------------------*/
enum [[gnu::packed]] VolumeControlMode
{
  MODE_NONE,
  MODE_MIC,
  MODE_SPK
};

struct Board
{
  struct AdcPackage adcPackage;
  struct AmpPackage ampPackage;
  struct ButtonPackage buttonPackage;
  struct CodecPackage codecPackage;
  struct ControlPackage controlPackage;

  struct
  {
    /* Memory interface for configuration data */
    struct Interface *memory;

    enum VolumeControlMode mode;
    enum AIC3xPath inputPath;
    enum AIC3xPath outputPath;
    uint8_t inputLevel;
    uint8_t outputLevel;
  } config;

  struct
  {
    bool codec;
    bool read;
    bool show;
    bool slave;
    bool suspend;
  } event;

  struct
  {
    struct Pin red;
    uint8_t active;
    uint8_t blink;
    uint8_t state;
  } indication;

  struct
  {
    struct Interface *slave;
    struct Interrupt *wakeup;
    struct Watchdog *watchdog;

    /* Current switch state */
    uint8_t sw;
    /* Autosuspend function timeout */
    uint8_t timeout;
    /* Autosuspend function enabled */
    bool autosuspend;
    /* External 5V power supply is ready */
    bool powered;
  } system;

  struct
  {
    struct Interface *serial;
    struct Timer *timer;

    uint32_t idle;
    uint32_t loops;
  } debug;
};
/*----------------------------------------------------------------------------*/
BEGIN_DECLS

void appBoardInit(struct Board *);
int appBoardStart(struct Board *);

END_DECLS
/*----------------------------------------------------------------------------*/
#endif /* BOARD_AUDIOBOARD_V1_APPLICATIONS_ACTIVE_BOARD_H_ */
