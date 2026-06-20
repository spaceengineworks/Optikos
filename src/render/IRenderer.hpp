#ifndef IRENDERER_H
#define IRENDERER_H

#include "render/IRenderQueue.hpp"

namespace Optikos
{
class IRenderer
{
   public:
    virtual ~IRenderer()                                       = default;
    virtual void         onWindowResize(int width, int height) = 0; /* TODO: can be optimmized with & */
    virtual void         beginFrame()                          = 0;
    virtual void         endFrame()                            = 0;
    virtual void         submit(DrawCommand&& command)   = 0;
    virtual void         flush()                               = 0;
    virtual void         swap_buffer()                         = 0;
    virtual unsigned int loadTexture(const std::vector<unsigned char>& data, int width,
                                     int height)               = 0;
    virtual void         waitIdle()
    {
        // used to finish last render if needed
    }

    virtual void resetToDefault() = 0;
    virtual void restoreStates()  = 0;

    virtual IRenderQueue& getRenderQueue() = 0;
};

}  // namespace Optikos

#endif /* IRENDERER_H */