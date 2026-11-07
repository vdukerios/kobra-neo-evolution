#include "bsp_rmu.h"


uint8_t rmu_get_reset_cause(void)
{
    uint8_t res = 0;

    stc_rmu_rstcause_t stcRmuRstCause;

    MEM_ZERO_STRUCT(stcRmuRstCause);

    RMU_GetResetCause(&stcRmuRstCause);

    if (Set == stcRmuRstCause.enPowerOn) res |= RST_CAU_POWER_ON;
    if (Set == stcRmuRstCause.enBrownOut) res |= RST_CAU_BROWN_OUT;
    if (Set == stcRmuRstCause.enRstPin) res |= RST_CAU_EXTERNAL;
    if (Set == stcRmuRstCause.enWdt || Set == stcRmuRstCause.enSwdt) res |= RST_CAU_WATCHDOG;
    if (Set == stcRmuRstCause.enSoftware) res |= RST_CAU_SOFTWARE;

    return res;
}

uint8_t rmu_clear_reset_cause(void)
{
    RMU_ClrResetFlag();

    return 0;
}


