#include "cpuintrf.h"

enum
{
	SE3208_PC=1, SE3208_SR, SE3208_ER, SE3208_SP,SE3208_PPC,
	SE3208_R0, SE3208_R1, SE3208_R2, SE3208_R3, SE3208_R4, SE3208_R5, SE3208_R6, SE3208_R7
};

#define SE3208_INT	0

extern CPU_GET_INFO( se3208 );
#define CPU_SE3208 CPU_GET_INFO_NAME( se3208 )

CPU_DISASSEMBLE( se3208 );
