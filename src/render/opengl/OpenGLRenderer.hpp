#ifndef OPENGLRENDERER_H
#define OPENGLRENDERER_H

#include <iostream>

#include "platform/IWindow.hpp"
#include "render/IRenderer.hpp"
#include "render/RenderQueue.hpp"
#include "shader/IShader.hpp"
#include "utilities/logger.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glad/glad.h>

/* __debugbreak depends on MSVC add #ifdef for MSVC */
#define call(x) \
    x;          \
    if (error) __debugbreak();

namespace Optikos
{
int constexpr DEFAULT_SHADER       = 0;
int constexpr NO_TEXTURE           = 0;
int constexpr DEFAULT_TEXTURE_UNIT = 0;

int constexpr POSITION_SIZE = 2;
int constexpr COLOR_SIZE    = 4;
int constexpr UV_SIZE       = 2;
int constexpr WH_SIZE       = 4;
int constexpr VERTEX_SIZE   = sizeof(Vertex);

size_t constexpr POSITION_POS = offsetof(Vertex, x);
size_t constexpr COLOR_POS    = offsetof(Vertex, r);
size_t constexpr UV_POS       = offsetof(Vertex, u);
size_t constexpr WH_POS       = offsetof(Vertex, fw);

class OpenGLRenderer : public IRenderer
{
   public:
    explicit OpenGLRenderer(IWindow* window, std::unique_ptr<IShader> shader);
    ~OpenGLRenderer() override;

    void         onWindowResize(int width, int height) override;
    void         beginFrame() override;
    void         endFrame() override;
    void         submit(DrawCommand&& command) override;
    void         flush() override;
    void         swap_buffer() override;
    unsigned int loadTexture(const std::vector<unsigned char>& data, int width,
                             int height) override;

    void resetToDefault() override;
    void restoreStates() override;

    IRenderQueue& getRenderQueue() override;

   private:
    struct Batch
    {
        unsigned int              VAO         = 0;
        unsigned int              VBO         = 0;
        unsigned int              IBO         = 0;
        unsigned int              shaderId    = 0;
        unsigned int              textureId   = 0;
        int                       textureMode = TEXTURE_NONE;
        std::vector<Vertex>       vertices;
        std::vector<unsigned int> indices;

        void clear()
        {
            vertices.clear();
            indices.clear();
        }
    };

    /* depended opengl 4.3+ */
    static void GLAPIENTRY messageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
                                           GLsizei length, const GLchar* message,
                                           const void* userParam);

    void initializeBatch(Batch& batch);
    void renderBatch(const Batch& batch);

    IWindow*                 m_window;
    std::unique_ptr<IShader> m_shader;
    RenderQueue              m_renderQueue;

    std::unordered_map<std::string, unsigned int> m_shaderCache;
    unsigned int                                  m_defaultShader = DEFAULT_SHADER;

    std::unordered_map<std::string, unsigned int> m_textureCache;

    bool m_uiStateSet = false;

    Batch m_currentBatch;

#ifdef ENABLE_GPU_PROFILING
    unsigned int queryID;
    unsigned int maxGpuTime = 0;
    unsigned int minGpuTime = 0;
    unsigned int avg        = 0;
    unsigned int calls      = 0;
#endif
};

}  // namespace Optikos

#endif /* OPENGLRENDERER_H */