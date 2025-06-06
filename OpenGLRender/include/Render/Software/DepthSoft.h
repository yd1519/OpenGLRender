#ifndef DEPTHSOFT_H
#define DEPTHSOFT_H

#include "Render/PipelineStates.h"

namespace OpenGL {

    bool DepthTest(float& a, float& b, DepthFunction func) {
        switch (func) {
        case DepthFunc_NEVER:     return false;
        case DepthFunc_LESS:      return a < b;
        case DepthFunc_EQUAL:     return std::fabs(a - b) <= std::numeric_limits<float>::epsilon();
        case DepthFunc_LEQUAL:    return a <= b;
        case DepthFunc_GREATER:   return a > b;
        case DepthFunc_NOTEQUAL:  return std::fabs(a - b) > std::numeric_limits<float>::epsilon();
        case DepthFunc_GEQUAL:    return a >= b;
        case DepthFunc_ALWAYS:    return true;
        }
        return a < b;
    }
}

#endif
