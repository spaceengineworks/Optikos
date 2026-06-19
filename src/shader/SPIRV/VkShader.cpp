#include "shader/SPIRV/VkShader.hpp"

namespace Optikos
{
unsigned int VkShader::compileShader(unsigned int type, const std::string& source)
{
    return 0;
}

unsigned int VkShader::createShader(const std::string& vertexShader,
                                    const std::string& fragmentShader)
{
    return 0;
}

ShaderSouces VkShader::parseShader(const std::string& file)
{
    return ShaderSouces();
}

}  // namespace Optikos