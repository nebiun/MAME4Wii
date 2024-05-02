#pragma once

#ifndef __RSP_H__
#define __RSP_H__

typedef void (*rsp_set_status_func)(const device_config *device, UINT32 status);

typedef struct _rsp_config rsp_config;
struct _rsp_config
{
	read32_space_func dp_reg_r;
	write32_space_func dp_reg_w;
	read32_space_func sp_reg_r;
	write32_space_func sp_reg_w;
	rsp_set_status_func sp_set_status;
};

enum
{
    RSP_PC = 1,
    RSP_R0,
    RSP_R1,
    RSP_R2,
    RSP_R3,
    RSP_R4,
    RSP_R5,
    RSP_R6,
    RSP_R7,
    RSP_R8,
    RSP_R9,
    RSP_R10,
    RSP_R11,
    RSP_R12,
    RSP_R13,
    RSP_R14,
    RSP_R15,
    RSP_R16,
    RSP_R17,
    RSP_R18,
    RSP_R19,
    RSP_R20,
    RSP_R21,
    RSP_R22,
    RSP_R23,
    RSP_R24,
    RSP_R25,
    RSP_R26,
    RSP_R27,
    RSP_R28,
    RSP_R29,
    RSP_R30,
    RSP_R31,
    RSP_SR,
    RSP_NEXTPC,
    RSP_STEPCNT,
};

#define RSP_STATUS_HALT          0x0001
#define RSP_STATUS_BROKE         0x0002
#define RSP_STATUS_DMABUSY       0x0004
#define RSP_STATUS_DMAFULL       0x0008
#define RSP_STATUS_IOFULL        0x0010
#define RSP_STATUS_SSTEP         0x0020
#define RSP_STATUS_INTR_BREAK    0x0040
#define RSP_STATUS_SIGNAL0       0x0080
#define RSP_STATUS_SIGNAL1       0x0100
#define RSP_STATUS_SIGNAL2       0x0200
#define RSP_STATUS_SIGNAL3       0x0400
#define RSP_STATUS_SIGNAL4       0x0800
#define RSP_STATUS_SIGNAL5       0x1000
#define RSP_STATUS_SIGNAL6       0x2000
#define RSP_STATUS_SIGNAL7       0x4000

CPU_GET_INFO( rsp );
#define CPU_RSP CPU_GET_INFO_NAME( rsp )

extern offs_t rsp_dasm_one(char *buffer, offs_t pc, UINT32 op);

#endif /* __RSP_H__ */
