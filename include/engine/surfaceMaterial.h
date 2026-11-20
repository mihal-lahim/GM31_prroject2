#pragma once

#include "renderer.h"
#include "resourceManager.h"
#include "shaderCatalog.h"

#include <string>
#include <vector>

namespace Engine
{
// SP31 のシェーダー教材を GM31 の描画対象ごとに選択するための設定。
enum class SurfaceShader : int
{
    Unlit, Directional, BlinnPhong, Hemisphere, Point, Rim, Spot,
    Toon, CookTorrance, Disney, Water, VertexDirectional, Bump, ToonRamp,
    Grayscale, ToonOutline
};

class SurfaceMaterial
{
public:
    SurfaceShader Shader = SurfaceShader::Unlit;
    MATERIAL Properties{};
    XMFLOAT4 Parameter{ 0.45f, 0.0f, 1.0f, 0.0f };
    TextureHandle Texture;
    TextureHandle DetailTexture;

    // 登録済み効果はenumの追加なしで名前から読み込める。
    bool Load(std::string_view id, TextureHandle texture = {})
    {
        const ShaderDefinition* definition = FindShaderDefinition(id);
        if (!definition || definition->Usage != ShaderUsage::Surface
            || definition->Passes.empty()) return false;
        std::vector<PassResources> passes(definition->Passes.size());
        for (std::size_t i = 0; i < definition->Passes.size(); ++i)
        {
            passes[i].Vertex = ResourceManager::LoadVertexShader(
                std::string(definition->Passes[i].VertexFile));
            passes[i].Pixel = ResourceManager::LoadPixelShader(
                std::string(definition->Passes[i].PixelFile));
            if (!passes[i].Vertex || !passes[i].Pixel) return false;
        }
        m_Definition = definition;
        m_Passes = std::move(passes);
        Texture = std::move(texture);
        DetailTexture.reset();
        Parameter = definition->Defaults;
        ResetProperties();
        return true;
    }

    bool SetFloat(std::string_view name, float value)
    {
        if (!m_Definition || name.empty()) return false;
        for (std::size_t i = 0; i < m_Definition->ParameterNames.size(); ++i)
            if (m_Definition->ParameterNames[i] == name)
                return SetXMFloat4Component(Parameter, i, value);
        return false;
    }

    bool SetTexture(std::string_view name, TextureHandle texture)
    {
        if (!m_Definition) return false;
        if (name == m_Definition->TextureNames[0])
        {
            Texture = std::move(texture);
            Properties.TextureEnable = Texture != nullptr;
            return true;
        }
        if (!m_Definition->TextureNames[1].empty()
            && name == m_Definition->TextureNames[1])
        {
            DetailTexture = std::move(texture);
            return true;
        }
        return false;
    }

    const ShaderDefinition* Definition() const { return m_Definition; }

    // 失敗したときは何も変えずにfalseを返します(直前まで使えていたマテリアルを壊しません)。
    bool Load(SurfaceShader shader, TextureHandle texture = {})
    {
        if (shader == SurfaceShader::Water || shader == SurfaceShader::ToonOutline)
        {
            if (!Load(shader == SurfaceShader::Water ? "water" : "toon-outline", std::move(texture)))
                return false;
            Shader = shader;
            return true;
        }
        PassResources pass;
        pass.Vertex = ResourceManager::LoadVertexShader(shader == SurfaceShader::VertexDirectional
            ? "spVertexLitVS.cso" : "classroomLitVS.cso");
        pass.Pixel = ResourceManager::LoadPixelShader("spSurfacePS.cso");
        if (!pass.Vertex || !pass.Pixel) return false;
        Shader = shader;
        m_Definition = nullptr;
        Texture = std::move(texture);
        DetailTexture.reset();
        m_Passes.assign(1, std::move(pass));
        ResetProperties();
        return true;
    }

    // すべてのパスのシェーダーが揃っているときだけtrueです(2パス目以降の欠けも見逃しません)。
    bool Ready() const
    {
        if (m_Passes.empty()) return false;
        for (const PassResources& pass : m_Passes)
            if (!pass.Vertex || !pass.Pixel) return false;
        return true;
    }

    std::size_t PassCount() const { return m_Definition ? m_Definition->Passes.size() : 1; }

    bool BindPass(std::size_t index) const
    {
        if (!Ready() || index >= PassCount() || index >= m_Passes.size()) return false;
        auto* context = Renderer::GetDeviceContext();
        if (context == nullptr) return false;
        const PassResources& pass = m_Passes[index];
        Renderer::SetCullFront(m_Definition
            && m_Definition->Passes[index].Cull == ShaderCull::Front);
        context->IASetInputLayout(pass.Vertex->InputLayout.Get());
        context->VSSetShader(pass.Vertex->Shader.Get(), nullptr, 0);
        context->PSSetShader(pass.Pixel->Shader.Get(), nullptr, 0);
        MATERIAL properties = Properties;
        properties.Dummy[0] = DetailTexture ? 1.0f : 0.0f;
        Renderer::SetMaterial(properties);
        XMFLOAT4 parameters = Parameter;
        if (m_Definition && m_Definition->Passes[index].OverrideParameters)
            parameters = m_Definition->Passes[index].Parameters;
        else if (m_Definition && m_Definition->LegacyMode >= 0)
            parameters.w = static_cast<float>(m_Definition->LegacyMode);
        else if (!m_Definition)
            parameters.w = static_cast<float>(static_cast<int>(Shader));
        Renderer::SetParameter(parameters);
        ID3D11ShaderResourceView* view = Texture ? Texture->Get() : nullptr;
        context->PSSetShaderResources(0, 1, &view);
        ID3D11ShaderResourceView* detail = DetailTexture ? DetailTexture->Get() : nullptr;
        context->PSSetShaderResources(1, 1, &detail);
        return true;
    }

    void Bind() const { (void)BindPass(PassCount() - 1); }

private:
    void ResetProperties()
    {
        Properties.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
        Properties.Ambient = Properties.Diffuse;
        Properties.Specular = { 0.18f, 0.18f, 0.18f, 1.0f };
        Properties.Emission = {};
        Properties.Shininess = 30.0f;
        Properties.TextureEnable = Texture != nullptr;
        Properties.Dummy[0] = 0.0f;
        Properties.Dummy[1] = 0.0f;
    }

    struct PassResources
    {
        VertexShaderHandle Vertex;
        PixelShaderHandle Pixel;
    };
    const ShaderDefinition* m_Definition = nullptr;
    std::vector<PassResources> m_Passes{ 1 };
};
}
