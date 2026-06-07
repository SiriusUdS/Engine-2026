#include "BoardEngine.h"

static Valve valves[] = {
    {
        .servoConfig = {&htim1, TIM_CHANNEL_1, 1200, 1800},
        .openLimitSwitch  = {VALVE0_OPEN_LIMIT_GPIO_Port, VALVE0_OPEN_LIMIT_Pin},
        .closeLimitSwitch = {VALVE0_CLOSE_LIMIT_GPIO_Port, VALVE0_CLOSE_LIMIT_Pin},
        .maxTransitTimeoutMs = 25000,
        .state = VALVE_STATE_UNKNOWN
    },
    {
        .servoConfig = {&htim1, TIM_CHANNEL_2, 1200, 1800},
        .openLimitSwitch  = {VALVE1_OPEN_LIMIT_GPIO_Port, VALVE1_OPEN_LIMIT_Pin},
        .closeLimitSwitch = {VALVE1_CLOSE_LIMIT_GPIO_Port, VALVE1_CLOSE_LIMIT_Pin},
        .maxTransitTimeoutMs = 25000,
        .state = VALVE_STATE_UNKNOWN
    }
};

static ValveHandlerCtx valveCtx = {
    .valves     = valves,
    .valveCount = sizeof(valves) / sizeof(valves[0]),
};

static CanNodeId nodeID = CAN_NODE_ECU;

static const CANHandlerEntry handlers[] = {
    { CAN_ID_CMD_VALVE, handler_valve, &valveCtx },
    { CAN_ID_COMM_PING, handler_ping,  &nodeID },
};

const CANControllerConfig BOARD_ENGINE = {
    .nodeID       = CAN_NODE_ECU,
    .handlers     = handlers,
    .handlerCount = sizeof(handlers) / sizeof(handlers[0]),
};

void BOARD_ENGINE_Init(void)
{
    for (uint32_t i = 0; i < valveCtx.valveCount; i++) {
        valveInit(&valves[i], 25000); 
    }

    if (!CAN_Init(&hfdcan1, CAN_NODE_ECU)) Error_Handler();
}

void BOARD_ENGINE_Update(void)
{
    for (uint32_t i = 1; i < valveCtx.valveCount; i++) {
        // Read each switch
        bool openPinSet = (HAL_GPIO_ReadPin(valves[i].openLimitSwitch.port, 
                                            valves[i].openLimitSwitch.pin) == GPIO_PIN_SET);
                                             
        bool closePinSet = (HAL_GPIO_ReadPin(valves[i].closeLimitSwitch.port, 
                                            valves[i].closeLimitSwitch.pin) == GPIO_PIN_SET);

        // Update the valve state
        valveUpdate(&valves[i], openPinSet, closePinSet);
    }
}

void BOARD_ENGINE_SendValveStatus(void)
{
    ValveStatusPacket packet;

    for (uint32_t i = 0; i < valveCtx.valveCount; i++) {
        
        // Map the index to enum
        CanValveIndex exposedIndex = (i == 0) ? CAN_VALVE_1 : CAN_VALVE_2;
        CanValveStatus currentStatus = (CanValveStatus)valves[i].state;

        // Make the packet
        valveStatusPacketMake(
            CAN_NODE_ECU, 
            CAN_NODE_FCU, 
            exposedIndex, 
            currentStatus, 
            &packet
        );

        // Send the packet
        if (!CAN_Send(packet.header.code, packet.payload.data)) {
            break; 
        }
    }
}