#include "WGSLShader.hpp"
#include <webgpu/webgpu_cpp.h>
#include <fstream>
#include <sstream>

namespace Optikos
{
ShaderSouces WGSLShader::parseShader(const std::string& file)
{
    std::fstream stream(file);
    std::string line;
    std::stringstream ss;
    
    while (getline(stream, line)) {
        if (line.find("#shader") != std::string::npos) continue;
        ss << line << '\n';
    }
    return { ss.str(), "" };
}

unsigned int WGSLShader::compileShader(unsigned int type, const std::string& source)
{
    return 0; 
}

unsigned int WGSLShader::createShader(const std::string& vertexShader, const std::string& fragmentShader)
{
    wgpu::ShaderSourceWGSL wgslSource{};
    wgslSource.code = vertexShader.c_str();

    wgpu::ShaderModuleDescriptor descriptor{};
    descriptor.nextInChain = &wgslSource;

    wgpu::ShaderModule shaderModule = m_device.CreateShaderModule(&descriptor);

    unsigned int id = m_nextModuleId++;
    m_modules[id] = std::move(shaderModule);

    return id;
}

wgpu::ShaderModule WGSLShader::getModule(unsigned int programId) const
{
    auto it = m_modules.find(programId);
    if (it != m_modules.end()) {
        return it->second;
    }
    return nullptr;
}
}
