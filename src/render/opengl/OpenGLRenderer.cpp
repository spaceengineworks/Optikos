#include "OpenGLRenderer.hpp"

#include "ui/text/TextFont.hpp"

namespace Optikos
{
OpenGLRenderer::OpenGLRenderer(IWindow* window, std::unique_ptr<IShader> shader)
    : m_window(window), m_shader(std::move(shader))
{
    m_window->makeContextCurrent();

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
        throw std::runtime_error("GLAD init failed");

    LOG_TRACE(reinterpret_cast<const char*>(glGetString(GL_VERSION)), "log");

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(OpenGLRenderer::messageCallback, this);

#ifndef OPTIKOS_SHADER_PATH
#define OPTIKOS_SHADER_PATH "res/shaders/"
#endif
    ShaderSouces source = m_shader->parseShader(std::string(OPTIKOS_SHADER_PATH) + "shader.vert");
    m_defaultShader     = m_shader->createShader(source.vertexSource, source.fragmentSource);

    initializeBatch(m_currentBatch);
}

OpenGLRenderer::~OpenGLRenderer()
{
    if (m_currentBatch.VAO) glDeleteVertexArrays(1, &m_currentBatch.VAO);
    if (m_currentBatch.VBO) glDeleteBuffers(1, &m_currentBatch.VBO);
    if (m_currentBatch.IBO) glDeleteBuffers(1, &m_currentBatch.IBO);

    for (auto& [name, id] : m_shaderCache)
    {
        glDeleteProgram(id);
    }
    if (m_defaultShader) glDeleteProgram(m_defaultShader);

    for (auto& [name, id] : m_textureCache)
    {
        glDeleteTextures(1, &id);
    }

#ifdef ENABLE_GPU_PROFILING
    glDeleteQueries(1, &queryID);
#endif
}

void OpenGLRenderer::initializeBatch(Batch& batch)
{
    glGenVertexArrays(1, &batch.VAO);
    glBindVertexArray(batch.VAO);

    glGenBuffers(1, &batch.VBO);
    glBindBuffer(GL_ARRAY_BUFFER, batch.VBO);

    glGenBuffers(1, &batch.IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.IBO);

    glVertexAttribPointer(0, POSITION_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE,
                          (void*) (POSITION_POS));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, COLOR_SIZE, GL_UNSIGNED_BYTE, GL_TRUE, VERTEX_SIZE,
                          (void*) (COLOR_POS));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, UV_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE, (void*) (UV_POS));
    glEnableVertexAttribArray(2);

    glVertexAttribPointer(3, WH_SIZE, GL_FLOAT, GL_FALSE, VERTEX_SIZE, (void*) (WH_POS));
    glEnableVertexAttribArray(3);

#ifdef ENABLE_GPU_PROFILING
    glGenQueries(1, &queryID);
#endif
}

void OpenGLRenderer::beginFrame()
{
#ifdef ENABLE_GPU_PROFILING
    glBeginQuery(GL_TIME_ELAPSED, queryID);
#endif

    glClear(GL_COLOR_BUFFER_BIT);
    m_renderQueue.clear();
    m_uiStateSet = false;
}

void OpenGLRenderer::submit(DrawCommand&& command)
{
    m_renderQueue.submit(std::move(command));
}

void OpenGLRenderer::flush()
{
    // TODO: sort by texture idx and then by shader idx to lower draw calls (probably will neede BTS
    // + tree created in start of program and not here).

    auto& commands = m_renderQueue.getMutableCommands();

    std::sort(commands.begin(), commands.end(),
              [](const DrawCommand& a, const DrawCommand& b)
              {
                  if (a.shaderId != b.shaderId) return a.shaderId < b.shaderId;
                  if (a.textureId != b.textureId) return a.textureId < b.textureId;
                  return a.textureMode < b.textureMode;
              });

    for (const auto& cmd : commands)
    {
        unsigned int shaderId  = cmd.shaderId != DEFAULT_SHADER ? cmd.shaderId : m_defaultShader;
        unsigned int textureId = cmd.textureId != NO_TEXTURE ? cmd.textureId : NO_TEXTURE;

        if ((m_currentBatch.shaderId != DEFAULT_SHADER || m_currentBatch.textureId != NO_TEXTURE) &&
            (m_currentBatch.shaderId != shaderId || m_currentBatch.textureId != textureId))
        {
            renderBatch(m_currentBatch);
            m_currentBatch.clear();
        }

        m_currentBatch.shaderId    = shaderId;
        m_currentBatch.textureId   = textureId;
        m_currentBatch.textureMode = cmd.textureMode;

        unsigned int vertexOffset = static_cast<unsigned int>(m_currentBatch.vertices.size());
        m_currentBatch.vertices.insert(m_currentBatch.vertices.end(), cmd.vertices.begin(),
                                       cmd.vertices.end());

        for (auto idx : cmd.indices)
        {
            m_currentBatch.indices.push_back(idx + vertexOffset);
        }
    }

    if (!m_currentBatch.vertices.empty())
    {
        renderBatch(m_currentBatch);
        m_currentBatch.clear();
    }
}

void OpenGLRenderer::renderBatch(const Batch& batch)
{
    if (!m_uiStateSet) restoreStates();

    glBindVertexArray(batch.VAO);

    glBindBuffer(GL_ARRAY_BUFFER, batch.VBO);
    glBufferData(GL_ARRAY_BUFFER, batch.vertices.size() * VERTEX_SIZE, batch.vertices.data(),
                 GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, batch.IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, batch.indices.size() * sizeof(unsigned int),
                 batch.indices.data(), GL_STATIC_DRAW);

    glUseProgram(batch.shaderId);

    bool hasTexture = false;
    if (batch.textureId != NO_TEXTURE)
    {
        hasTexture = true;
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, batch.textureId);

        int texLoc = glGetUniformLocation(batch.shaderId, "uTexture");
        if (texLoc != -1) glUniform1i(texLoc, DEFAULT_TEXTURE_UNIT);
    }

    int toggleLoc = glGetUniformLocation(batch.shaderId, "uHasTexture");
    if (toggleLoc != -1) glUniform1i(toggleLoc, hasTexture ? batch.textureMode : 0);

    // TODO: unsigned int loc but is should be int [Shader] because if we get error it will be -1.
    int screenLoc = glGetUniformLocation(batch.shaderId, "uScreenSize");
    glUniform2f(screenLoc, (float) m_window->getWidth(), (float) m_window->getHeight());

    glDrawElements(GL_TRIANGLES, static_cast<int>(batch.indices.size()), GL_UNSIGNED_INT, nullptr);
}

void OpenGLRenderer::endFrame()
{
    flush();
    resetToDefault();

#ifdef ENABLE_GPU_PROFILING
    glEndQuery(GL_TIME_ELAPSED);
    unsigned int result;
    glGetQueryObjectuiv(queryID, GL_QUERY_RESULT, &result);

    maxGpuTime = std::max(result, maxGpuTime);
    minGpuTime = (calls == 0) ? result : std::min(result, minGpuTime);
    avg += result;
    calls++;

    if (calls % 60 == 0)
    {
        std::cout << "=== GPU Profiling (after " << calls << " frames) ===" << std::endl;
        std::cout << "Max: " << (maxGpuTime / 1e+6) << " ms" << std::endl;
        std::cout << "Min: " << (minGpuTime / 1e+6) << " ms" << std::endl;
        std::cout << "Avg: " << (avg / calls) / 1e+6 << " ms" << std::endl;
        std::cout << "Avg FPS: " << 1000.0 / ((avg / calls) / 1e+6) << std::endl;
    }
#endif
}

void OpenGLRenderer::swap_buffer()
{
    glfwSwapBuffers(static_cast<GLFWwindow*>(m_window->native_handle())); /* Used for glfw window*/
}

void OpenGLRenderer::onWindowResize(int width, int height)
{
    glViewport(0, 0, width, height);
}

IRenderQueue& OpenGLRenderer::getRenderQueue()
{
    return m_renderQueue;
}

void GLAPIENTRY OpenGLRenderer::messageCallback(GLenum source, GLenum type, GLuint id,
                                                GLenum severity, GLsizei length,
                                                const GLchar* message, const void* userParam)
{
    (void) source;
    (void) type;
    (void) length;
    (void) userParam;
    (void) id;

    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION)
    {
        return;
    }

    LOG_ERROR(std::string("OpenGL: ") + message, "log");
}

unsigned int OpenGLRenderer::loadTexture(const std::vector<unsigned char>& data, int width,
                                         int height)
{
    unsigned int textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return textureId;
}

void OpenGLRenderer::resetToDefault()
{
    glDisable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ZERO);
    glBlendEquation(GL_FUNC_ADD);

    glDisable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);

    glDisable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glStencilMask(0xFF);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, m_window->getWidth(), m_window->getHeight());

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_POLYGON_OFFSET_LINE);
    glDisable(GL_POLYGON_OFFSET_POINT);
    glPolygonOffset(0.0f, 0.0f);

    glDisable(GL_PRIMITIVE_RESTART);

#ifdef GL_CLIP_DISTANCE0
    for (int i = 0; i < 8; i++)
    {
        glDisable(GL_CLIP_DISTANCE0 + i);
    }
#endif

    glEnable(GL_MULTISAMPLE);

    glActiveTexture(GL_TEXTURE0);

    glUseProgram(0);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_uiStateSet = false;
}

void OpenGLRenderer::restoreStates()
{
    glEnable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);

    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glViewport(0, 0, m_window->getWidth(), m_window->getHeight());
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glActiveTexture(GL_TEXTURE0);

    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_PRIMITIVE_RESTART);
#ifdef GL_CLIP_DISTANCE0
    glDisable(GL_CLIP_DISTANCE0);
#endif

    m_uiStateSet = true;
}

}  // namespace Optikos