#pragma once
#include "can/CANControllerConfig.h"
#include "main.h"
#include "can/handlers/handlerValve.h"
#include "can/handlers/handlerPing.h"
#include "valve/ValveController.h"

extern const CANControllerConfig BOARD_ENGINE;

void BOARD_ENGINE_Init(void);