#ifndef CANOPEN_H
#define CANOPEN_H

// ================================
// Standard-CANopen COB-IDs (Base)
// ================================
#define COB_ID_SYNC      0x080
#define COB_ID_TIME      0x100
#define COB_ID_EMCY_BASE 0x080
#define COB_ID_TPDO1     0x180
#define COB_ID_RPDO1     0x200
#define COB_ID_TPDO2     0x280
#define COB_ID_RPDO2     0x300
#define COB_ID_TPDO3     0x380
#define COB_ID_RPDO3     0x400
#define COB_ID_TPDO4     0x480
#define COB_ID_RPDO4     0x500
#define COB_ID_TSDO_BASE 0x580
#define COB_ID_RSDO_BASE 0x600
#define COB_ID_HB_BASE   0x700
#define COB_ID_NMT       0x000

// ================================
// NMT-Befehle
// ================================
#define NMT_CMD_START_NODE      0x01
#define NMT_CMD_STOP_NODE       0x02
#define NMT_CMD_ENTER_PREOP     0x80
#define NMT_CMD_RESET_NODE      0x81
#define NMT_CMD_RESET_COMM      0x82

#endif
