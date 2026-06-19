#ifndef VKSHADER_HPP
#define VKSHADER_HPP

#include <vulkan/vulkan.h>

#include "shader/IShader.hpp"

namespace Optikos
{
class VkShader : public IShader
{
   public:
    ~VkShader() = default;
    unsigned int compileShader(unsigned int type, const std::string& source) override;
    unsigned int createShader(const std::string& vertexShader,
                              const std::string& fragmentShader) override;
    ShaderSouces parseShader(const std::string& file) override;

   private:
};

}  // namespace Optikos

#endif /* VKSHADER_HPP */