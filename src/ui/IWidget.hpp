#ifndef IWIDGET_H
#define IWIDGET_H

#include <cassert>
#include <cstdint>
#include <functional>
#include <vector>

#include "render/IRenderQueue.hpp"
#include "utilities/color.hpp"
#include "utilities/logger.hpp"
#include "utilities/vec.hpp"

inline constexpr std::uint8_t LEFT_CLICK = 1;
inline constexpr std::uint8_t RELEASE    = 0;

namespace Optikos
{
enum class ExpandMode : uint8_t
{
    None   = 0,
    Width  = 1,
    Height = 2,
    Both   = 3
};

struct RenderData
{
    std::vector<Vertex>       vertices;
    std::vector<unsigned int> indices;
};

struct Clip
{
    float xMin, xMax, yMin, yMax;
};

class IWidget
{
   public:
    virtual ~IWidget()                     = default;
    virtual Vec2     getPosition() const   = 0;  // return cord of left top corner
    virtual void     setPosition(Vec2 pos) = 0;  // set cord of left top corner
    virtual uint32_t getWidth() const      = 0;
    virtual uint32_t getHeight() const     = 0;
    virtual bool     getVisible() const    = 0;
    virtual bool     getClickable() const  = 0;
    virtual Color    getColor() const      = 0;
    virtual Clip     getClip() const       = 0;

    virtual void updateData() = 0;

    virtual void                             resize(int width, int height) = 0;
    virtual const std::vector<unsigned int>& getIndices() const            = 0;
    virtual const std::vector<Vertex>&       getVertices() const           = 0;

    virtual void       setClickable(bool isClickable) = 0;
    virtual void       setAutoExpand(ExpandMode mode) = 0;
    virtual void       setVisible(bool visible)       = 0;
    virtual ExpandMode isExpand()                     = 0;
    virtual void       setColor(Color color)          = 0;
    virtual void       setClip(Clip clip)             = 0;

    virtual void handleEvent() = 0;
    virtual void handleHover(double, double)
    {
    }
    virtual void resetHover()
    {
    }
    virtual bool wantsHoverEvents() const
    {
        return false;
    }

    virtual bool wantsGetInput() const
    {
        return false;
    }

    virtual void passInput(unsigned int codepoint)
    {
        (void) codepoint;
    }

    virtual void render(IRenderQueue& renderQueue)
    {
        if (getVisible())
        {
            DrawCommand cmd;
            cmd.vertices  = getVertices();
            cmd.indices   = getIndices();
            cmd.shaderId  = 0;
            cmd.textureId = 0;
            renderQueue.submit(std::move(cmd));
        }
    }

    virtual bool handleClick(double x, double y, int action)
    {
        if (getClickable() && isInside(x, y) && action == LEFT_CLICK)
        {
            bool clicked = true;
            handleEvent();
            return clicked;
        }
        return false;
    }

    virtual void handleDrag(double x, double y)
    {
        (void) x;
        (void) y;
    }

    bool isInside(double x, double y) const
    {
        Vec2 pos  = getPosition();
        Clip clip = getClip();

        bool inWidget =
            (pos.x <= x && x <= pos.x + getWidth() && pos.y <= y && y <= pos.y + getHeight());

        bool inClip = (clip.xMin <= x && x <= clip.xMax && clip.yMin <= y && y <= clip.yMax);

        return inWidget && inClip;
    }

   private:
};

}  // namespace Optikos

#endif /* IWIDGET_H */