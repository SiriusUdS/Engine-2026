#pragma once

// Identifie les nodes du réseau CAN
typedef enum {
    CAN_NODE_FILL_F412   = 0x01,
    CAN_NODE_ENGINE_H747 = 0x02,
} CanNodeId;

// Id du message
typedef enum {
    CAN_ID_CMD_VALVE      = 0x01,   /* FILL  → ENGINE : Change valve status   */
    CAN_ID_STATUS_VALVE   = 0x02,   /* ENGINE → FILL  : Return valve status   */
} CanMsgId;

// Index des valves
typedef enum {
    CAN_VALVE_1 = 1,
    CAN_VALVE_2 = 2,
} CanValveIndex;

// Type de commande (FILL  → ENGINE)
typedef enum {
    CAN_CMD_CLOSE = 0x00,
    CAN_CMD_OPEN  = 0x01,
} CanValveCmd;

// Status de la valve
typedef enum {
    CAN_STATUS_UNKNOWN  = 0x00,
    CAN_STATUS_OPEN     = 0x01,
    CAN_STATUS_OPENING  = 0x02,
    CAN_STATUS_CLOSED    = 0x03,
    CAN_STATUS_CLOSING  = 0x04,
} CanValveStatus;