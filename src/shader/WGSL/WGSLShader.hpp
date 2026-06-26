#ifndef WGSL_SHADER_HPP
#define WGSL_SHADER_HPP

#include "shader/IShader.hpp"
#include <webgpu/webgpu_cpp.h>
#include <unordered_map>
#include <string>

namespace Optikos
{
class WGSLShader : public IShader
{
public:
    WGSLShader() = default;
    ~WGSLShader() override = default;

    void setDevice(wgpu::Device device)
    {
        m_device = device;
    }
    
    unsigned int compileShader(unsigned int type, const std::string& source) override;
    unsigned int createShader(const std::string& vertexShader, const std::string& fragmentShader) override;
    ShaderSouces parseShader(const std::string& file) override;

    wgpu::ShaderModule getModule(unsigned int programId) const;

private:
    wgpu::Device m_device;

    std::unordered_map<unsigned int, wgpu::ShaderModule> m_modules;
    unsigned int m_nextModuleId = 1;
};
}

#endif
