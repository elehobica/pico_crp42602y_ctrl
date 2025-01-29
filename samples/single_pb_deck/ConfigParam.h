/*-----------------------------------------------------------/
/ ConfigParam.h
/------------------------------------------------------------/
/ Copyright (c) 2024, Elehobica
/ Released under the BSD-2-Clause
/ refer to https://opensource.org/licenses/BSD-2-Clause
/-----------------------------------------------------------*/

#pragma once

#include "FlashParam.h"

//=================================
// Interface of ConfigParam class
//=================================
struct ConfigParam : FlashParamNs::FlashParam {
    static ConfigParam& instance()  // Singleton
    {
        static ConfigParam instance;
        return instance;
    }
    static constexpr uint32_t ID_BASE = FlashParamNs::CFG_ID_BASE;
    // Parameter<T>                      instance           id           name                default size
    FlashParamNs::Parameter<uint32_t>    P_CFG_EQ_TYPE     {ID_BASE + 0, "CFG_EQ_TYPE",      0};
    FlashParamNs::Parameter<uint32_t>    P_CFG_NR_TYPE     {ID_BASE + 1, "CFG_NR_TYPE",      0};
    FlashParamNs::Parameter<uint32_t>    P_CFG_REVERSE_MODE{ID_BASE + 2, "CFG_REVERSE_MODE", 0};
    FlashParamNs::Parameter<bool>        P_CFG_BT_TX_ENABLE{ID_BASE + 3, "CFG_BT_TX_ENABLE", false};
};
