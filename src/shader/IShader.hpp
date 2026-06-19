#ifndef ISHADER_H
#define ISHADER_H

#include <cassert>
#include <string>
#include <vector>
#include <fstream>

#include "utilities/logger.hpp"

namespace Optikos
{
enum class ShaderType
{
    NONE     = -1,
    VERTEX   = 0,
    FRAGMENT = 1
};

struct ShaderSouces
{
    std::string vertexSource;
    std::string fragmentSource;
};

class IShader
{
   public:
    virtual ~IShader()                                                               = default;
    virtual unsigned int compileShader(unsigned int type, const std::string& source) = 0;
    virtual unsigned int createShader(const std::string& vertexShader,
                                      const std::string& fragmentShader)             = 0;
    virtual ShaderSouces parseShader(const std::string& file)                        = 0;

    virtual std::vector<char> readFile(const std::string& filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            LOG_ERROR("[readFile] failed to open file!", "log");
            throw std::runtime_error("failed to open file!");
        }

        size_t            fileSize = (size_t) file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }
};
}  // namespace Optikos

#endif /* ISHADER_H */