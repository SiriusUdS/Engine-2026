#include "BoardEngine.h"

static Valve valves[] = {
    {{&htim1, TIM_CHANNEL_1, 1200, 1800}, VALVE_STATE_UNKNOWN, 0, 100},
    {{&htim1, TIM_CHANNEL_2, 1200, 1800}, VALVE_STATE_UNKNOWN, 0, 100},
};

static ValveHandlerCtx valveCtx = {
    .valves     = valves,
    .valveCount = sizeof(valves) / sizeof(valves[0]),
};

static CanNodeId nodeID = CAN_NODE_ENGINE;

static const CANHandlerEntry handlers[] = {
    { CAN_ID_CMD_VALVE, handler_valve, &valveCtx },
    { CAN_ID_COMM_PING, handler_ping,  &nodeID   },
};

const CANControllerConfig BOARD_ENGINE = {
    .hfdcan       = &hfdcan1,
    .nodeID       = CAN_NODE_ENGINE,
    .handlers     = handlers,
    .handlerCount = sizeof(handlers) / sizeof(handlers[0]),
};

void BOARD_ENGINE_Init(void)
{
    valveInit(&valves[0], 100.0f);
    valveInit(&valves[1], 100.0f);

    if (!CAN_Init(&hfdcan1, CAN_NODE_ENGINE))
        Error_Handler();
}
