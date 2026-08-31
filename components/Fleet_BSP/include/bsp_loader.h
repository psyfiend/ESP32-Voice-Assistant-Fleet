#pragma once

// This macro is defined in platformio.ini per environment
#ifdef BSP_HEADER
    #include BSP_HEADER
#else
    #error "No BSP_HEADER defined! Check your platformio.ini build_flags."
#endif