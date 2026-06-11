#include "ui/sdk/button.hpp"

#include "ui/text/TextFont.hpp"

namespace Optikos
{
Button::Button(uint32_t width, uint32_t height, Vec2 position)
    : Widget(width, height, position), m_originalColor(m_color), m_dimmed(m_color)
{
    setHoverDimming(0.5);

    updateData();

    m_isClickable = true;
}

Button::Button(uint32_t width, uint32_t height, Vec2 position, const std::string& text)
    : Button(width, height, position)
{
    setText(text);
}

Button::Button(uint32_t width, uint32_t height, Vec2 position, std::function<void()> event)
    : Button(width, height, position)
{
    m_event = event;
}

void Button::render(Optikos::IRenderQueue& renderQueue)
{
    if (!m_isVisible) return;

    DrawCommand bgCmd;
    bgCmd.vertices  = m_data.vertices;
    bgCmd.indices   = m_data.indices;
    bgCmd.shaderId  = 0;
    bgCmd.textureId = 0;
    renderQueue.submit(std::move(bgCmd));

    if (m_label)
    {
        m_label->render(renderQueue);
    }
}

void Button::updateData()
{
    m_data.indices  = {0, 1, 2, 2, 3, 0};
    m_data.vertices = {
        {m_position.x, m_position.y, m_color.r, m_color.g, m_color.b, m_color.a, 0, 0, m_clip.xMin,
         m_clip.xMax, m_clip.yMin, m_clip.yMax},
        {m_position.x + m_width, m_position.y, m_color.r, m_color.g, m_color.b, m_color.a, 0, 0,
         m_clip.xMin, m_clip.xMax, m_clip.yMin, m_clip.yMax},
        {m_position.x + m_width, m_position.y + m_height, m_color.r, m_color.g, m_color.b,
         m_color.a, 0, 0, m_clip.xMin, m_clip.xMax, m_clip.yMin, m_clip.yMax},
        {m_position.x, m_position.y + m_height, m_color.r, m_color.g, m_color.b, m_color.a, 0, 0,
         m_clip.xMin, m_clip.xMax, m_clip.yMin, m_clip.yMax}};

    if (m_label)
    {
        Clip buttonClip = {m_position.x, m_position.x + m_width, m_position.y,
                           m_position.y + m_height};
        Clip finalClip;
        finalClip.xMin = std::max(buttonClip.xMin, m_clip.xMin);
        finalClip.xMax = std::min(buttonClip.xMax, m_clip.xMax);
        finalClip.yMin = std::max(buttonClip.yMin, m_clip.yMin);
        finalClip.yMax = std::min(buttonClip.yMax, m_clip.yMax);

        m_label->setClip(finalClip);
        m_label->setPosition(m_position);
        m_label->resize(m_width, m_height);
    }
}

void Button::handleEvent()
{
    if (m_event != nullptr) m_event();
}

void Button::handleHover(double x, double y)
{
    (void) x;
    (void) y;
    if (!m_isHover)
    {
        m_isHover = true;
        m_color   = m_dimmed;
        updateData();
        if (m_hoverEvent != nullptr) m_hoverEvent();
    }
}

void Button::resetHover()
{
    m_isHover = false;
    m_color   = m_originalColor;
    updateData();
    if (m_hoverEvent != nullptr) m_hoverEvent();
}

bool Button::wantsHoverEvents() const
{
    return true;
}

void Button::resize(int width, int height)
{
    m_width  = width;
    m_height = height;

    updateData();
}

void Button::setHoverDimming(float dimming)
{
    assert(0.0f <= dimming && dimming <= 1.0f);
    m_dimmed.r = static_cast<unsigned char>(std::min(m_originalColor.r * dimming, 255.0f));
    m_dimmed.g = static_cast<unsigned char>(std::min(m_originalColor.g * dimming, 255.0f));
    m_dimmed.b = static_cast<unsigned char>(std::min(m_originalColor.b * dimming, 255.0f));
    m_dimmed.a = m_originalColor.a;
}

void Button::setEvent(std::function<void()> event)
{
    assert(event != nullptr);
    m_event = event;
}

const std::vector<Vertex>& Button::getVertices() const
{
    return m_data.vertices;
}
const std::vector<unsigned int>& Button::getIndices() const
{
    return m_data.indices;
}

void Button::setText(std::string text)
{
    if (text.empty())
    {
        m_label.reset();
        return;
    }

    if (!m_label)
        m_label = std::make_unique<Label>(text, m_position, m_width, m_height, m_textColor);
    else
        m_label->setText(text);

    updateData();
}

void Button::setFont(std::string font)
{
    if (m_label)
    {
        m_label->setFont(font);
    }
}

void Button::setTextColor(Color color)
{
    m_textColor = color;

    if (m_label)
    {
        m_label->setColor(color);
    }
}

void Button::setColor(Color color)
{
    m_color         = color;
    m_originalColor = color;

    setHoverDimming(0.5);

    updateData();
}

void Button::setPosition(Vec2 pos)
{
    m_position = pos;

    updateData();
}

void Button::setHoverEvent(std::function<void()> event)
{
    assert(event != nullptr);
    m_hoverEvent = event;
}

bool Button::isHover() const
{
    return m_isHover;
}

}  // namespace Optikos