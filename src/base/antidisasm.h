#pragma once

#if defined(CONF_FAMILY_WINDOWS) && defined(_MSC_VER)

#define ANTI_DISASM() \
    __asm { \
        __asm _emit 0xEB \
        __asm _emit 0xFF \
        __asm _emit 0xC0 \
        __asm _emit 0x48 \
    }

#define JUNK_CODE() \
    do { \
        volatile int _jnk = 0; \
        if(_jnk == 0x7FFFFFFF) { \
            volatile int *_p = nullptr; \
            *_p = 0; \
        } \
    } while(0)

#define ANTI_DISASM_BLOCK() \
    do { \
        JUNK_CODE(); \
        volatile int _ad_check = 1; \
        if(_ad_check != 1) { \
            volatile int *_crash = nullptr; \
            *_crash = 0xDEAD; \
        } \
    } while(0)

#else

#define ANTI_DISASM()
#define JUNK_CODE()
#define ANTI_DISASM_BLOCK()

#endif