#ifndef OPTIKOS_H
#define OPTIKOS_H

#include <memory>

#include "input/IInputSystem.hpp"
#include "platform/IWindow.hpp"
#include "render/IRenderer.hpp"
#include "ui/UISystem.hpp"
#include "ui/text/TextFont.hpp"
#include "utilities/vec.hpp"

// TODO: wrap to .hpp
#include "ui/sdk/button.hpp"
#include "ui/sdk/container.hpp"
#include "ui/sdk/image.hpp"
#include "ui/sdk/label.hpp"
#include "ui/sdk/scrollContainer.hpp"
#include "ui/sdk/slider.hpp"
#include "ui/sdk/textBox.hpp"

unsigned int constexpr DEFAULT_WIDTH  = 800;
unsigned int constexpr DEFAULT_HEIGHT = 600;

// TODO: Drop down widget.
// TODO: RenderQueue sorting optimization.
// TODO: Documention.

namespace Optikos
{
class Optikos
{
   public:
    Optikos(std::string_view title, unsigned int width = DEFAULT_WIDTH,
            unsigned int height = DEFAULT_HEIGHT);

    bool should_close();
    void begin();
    void end();

    void pushFont(std::string_view path, std::string fontName = DEFAULT0_FONT,
                  float fontSize = DEFAULT0_FONTSIZE);
    void setWindowTitleBar(Color color);

    template <typename T>
    T* addWidget(const uint32_t idx, std::unique_ptr<T> widget)
    {
        return m_uiSystem->add_widget(idx, std::move(widget));
    }

    bool removeWidget(uint32_t id);

    inline Vec2 getCursor()
    {
        Cursor cursor = m_inputSystem->getCursor();
        return Vec2{static_cast<float>(cursor.x), static_cast<float>(cursor.y)};
    }

    inline void bindKey(const std::string& action, int key, unsigned int state = Pressed)
    {
        m_inputSystem->bind(action, key, state);
    }

    inline void unbindKey(const std::string& action)
    {
        m_inputSystem->unbind(action);
    }

    inline void keyAction(const std::string& action, std::function<void()> cb)
    {
        m_inputSystem->onAction(action, cb);
    }

    inline void closeWindow(bool flag)
    {
        m_window->setWindowShouldClose(flag);
    }

    inline void close()
    {
        m_renderer->waitIdle();
    }

    inline IRenderer* getRenderer()
    {
        return m_renderer.get();
    }

   private:
    std::unique_ptr<IRenderer>    m_renderer;
    std::unique_ptr<IWindow>      m_window;
    std::unique_ptr<IInputSystem> m_inputSystem;
    std::unique_ptr<UISystem>     m_uiSystem;
};

}  // namespace Optikos

#endif /* OPTIKOS_H */