#pragma once
#include "instr.h"
#undef V
#undef IF
#undef ELSE
#undef ENDIF
#undef FOR
#undef ENDFOR
#undef END
#undef WHILE
#undef ENDWHILE
#undef BREAK
#undef CONTINUE
#undef RETURN
#undef REPEAT
#undef AS_LONG_AS
#undef CASE
#undef ENDCASE
#undef WHEN
#undef DO
#undef DONE
#undef OR
#undef DEFAULT
#undef OBF_BEGIN
#undef OBF_END
namespace obf_c
{
    using namespace obf;

    template<int V>
    inline float F()
    {
        volatile int x = N(V);
        return (float)x;
    }

    template<int V>
    inline int I()
    {
        volatile int x = N(V);
        return x;
    }

    template<int NUM, int DEN>
    inline float Frac()
    {
        volatile int n = N(NUM);
        volatile int d = N(DEN);
        return (float)n / (float)d;
    }
}

#define OBF_F(x) obf_c::F<(int)(x)>()
#define OI(x) obf_c::I<x>()
#define OFR(n, d) obf_c::Frac<n, d>()