#ifndef BRAWL_ARENA_RENDER_STATE_H
#define BRAWL_ARENA_RENDER_STATE_H

#include "rlgl.h"

// raylib batches immediate-mode primitives and billboards. OpenGL depth-mask changes
// do not flush that batch, so changing the mask around queued geometry can make a
// transparent quad write depth when it is finally submitted. Keep the flush and state
// transition inseparable and prohibit raw depth-mask toggles in presentation sources.
static inline void RenderBeginNoDepthWrite(void)
{
    rlDrawRenderBatchActive();
    rlDisableDepthMask();
}

static inline void RenderEndNoDepthWrite(void)
{
    rlDrawRenderBatchActive();
    rlEnableDepthMask();
}

#endif
