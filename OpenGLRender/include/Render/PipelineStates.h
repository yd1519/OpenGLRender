#ifndef PIPELINESTATES_H
#define PIPELINESTATES_H

#include "Render/RenderStates.h"

namespace OpenGL {

class PipelineStates {
public:
    explicit PipelineStates(const RenderStates& states)// explicit 关键字禁止隐式类型转换
        : renderStates(states) {}

    virtual ~PipelineStates() = default;

public:
    RenderStates renderStates;
};

}

#endif
