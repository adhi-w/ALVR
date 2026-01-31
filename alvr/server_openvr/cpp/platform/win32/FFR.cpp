#include "FFR.h"

#include "alvr_server/Settings.h"
#include "alvr_server/Utils.h"
#include "alvr_server/bindings.h"

using Microsoft::WRL::ComPtr;
using namespace d3d_render_utils;

namespace {

struct FoveationVars {
    uint32_t targetEyeWidth;
    uint32_t targetEyeHeight;
    uint32_t optimizedEyeWidth;
    uint32_t optimizedEyeHeight;

    float eyeWidthRatio;
    float eyeHeightRatio;

    float centerSizeX;
    float centerSizeY;
    float centerShiftX;
    float centerShiftY;
    float centerShiftXLeft;
    float centerShiftYLeft;
    float centerShiftXRight;
    float centerShiftYRight;
    float edgeRatioX;
    float edgeRatioY;

    // D3D11 constant buffers must be sized to a multiple of 16 bytes.
    // The matching HLSL cbuffer packs to 4 float4 registers (64 bytes).
};

FoveationVars CalculateFoveationVars() {
    float targetEyeWidth = (float)Settings::Instance().m_renderWidth / 2;
    float targetEyeHeight = (float)Settings::Instance().m_renderHeight;

    /*     
    float centerSizeX = (float)Settings::Instance().m_foveationCenterSizeX;
    float centerSizeY = (float)Settings::Instance().m_foveationCenterSizeY;
    float centerShiftX = (float)Settings::Instance().m_foveationCenterShiftX;
    float centerShiftY = (float)Settings::Instance().m_foveationCenterShiftY;
    float edgeRatioX = (float)Settings::Instance().m_foveationEdgeRatioX;
    float edgeRatioY = (float)Settings::Instance().m_foveationEdgeRatioY;
    */

    float centerSizeX = (float)Settings::Instance().m_foveationCenterSizeX;
    float centerSizeY = (float)Settings::Instance().m_foveationCenterSizeY;
    float centerShiftX = (float)Settings::Instance().m_foveationCenterShiftX;
    float centerShiftY = (float)Settings::Instance().m_foveationCenterShiftY;
    float centerShiftXLeft = (float)Settings::Instance().m_foveationCenterShiftXLeft;
    float centerShiftYLeft = (float)Settings::Instance().m_foveationCenterShiftYLeft;
    float centerShiftXRight = (float)Settings::Instance().m_foveationCenterShiftXRight;
    float centerShiftYRight = (float)Settings::Instance().m_foveationCenterShiftYRight;
    float edgeRatioX = (float)Settings::Instance().m_foveationEdgeRatioX;
    float edgeRatioY = (float)Settings::Instance().m_foveationEdgeRatioY;

    float edgeSizeX = targetEyeWidth - centerSizeX * targetEyeWidth;
    float edgeSizeY = targetEyeHeight - centerSizeY * targetEyeHeight;

    float centerSizeXAligned
        = 1. - ceil(edgeSizeX / (edgeRatioX * 2.)) * (edgeRatioX * 2.) / targetEyeWidth;
    float centerSizeYAligned
        = 1. - ceil(edgeSizeY / (edgeRatioY * 2.)) * (edgeRatioY * 2.) / targetEyeHeight;

    float edgeSizeXAligned = targetEyeWidth - centerSizeXAligned * targetEyeWidth;
    float edgeSizeYAligned = targetEyeHeight - centerSizeYAligned * targetEyeHeight;

    float centerShiftXAligned = ceil(centerShiftX * edgeSizeXAligned / (edgeRatioX * 2.))
        * (edgeRatioX * 2.) / edgeSizeXAligned;
    float centerShiftYAligned = ceil(centerShiftY * edgeSizeYAligned / (edgeRatioY * 2.))
        * (edgeRatioY * 2.) / edgeSizeYAligned;

    float centerShiftXAlignedLeft = ceil(centerShiftXLeft * edgeSizeXAligned / (edgeRatioX * 2.))
        * (edgeRatioX * 2.) / edgeSizeXAligned;
    float centerShiftYAlignedLeft = ceil(centerShiftYLeft * edgeSizeYAligned / (edgeRatioY * 2.))
        * (edgeRatioY * 2.) / edgeSizeYAligned;

    float centerShiftXAlignedRight = ceil(centerShiftXRight * edgeSizeXAligned / (edgeRatioX * 2.))
        * (edgeRatioX * 2.) / edgeSizeXAligned;
    float centerShiftYAlignedRight = ceil(centerShiftYRight * edgeSizeYAligned / (edgeRatioY * 2.))
        * (edgeRatioY * 2.) / edgeSizeYAligned;

    float foveationScaleX = (centerSizeXAligned + (1. - centerSizeXAligned) / edgeRatioX);
    float foveationScaleY = (centerSizeYAligned + (1. - centerSizeYAligned) / edgeRatioY);

    float optimizedEyeWidth = foveationScaleX * targetEyeWidth;
    float optimizedEyeHeight = foveationScaleY * targetEyeHeight;

    if (Settings::Instance().m_foveationMethod == 3) {
        float magnitude = (float)Settings::Instance().m_foveationFrwMagnitude;
        if (magnitude <= 0.0f) {
            magnitude = 4.66f;
        }
        centerSizeXAligned = magnitude;
        centerSizeYAligned = magnitude;
    }

    // round the frame dimensions to a number of pixel multiple of 32 for the encoder
    auto optimizedEyeWidthAligned = (uint32_t)ceil(optimizedEyeWidth / 32.f) * 32;
    auto optimizedEyeHeightAligned = (uint32_t)ceil(optimizedEyeHeight / 32.f) * 32;

    float eyeWidthRatioAligned = optimizedEyeWidth / optimizedEyeWidthAligned;
    float eyeHeightRatioAligned = optimizedEyeHeight / optimizedEyeHeightAligned;

    return { (uint32_t)targetEyeWidth,
             (uint32_t)targetEyeHeight,
             optimizedEyeWidthAligned,
             optimizedEyeHeightAligned,
             eyeWidthRatioAligned,
             eyeHeightRatioAligned,
             centerSizeXAligned,
             centerSizeYAligned,
             centerShiftXAligned,
             centerShiftYAligned,
             centerShiftXAlignedLeft,
             centerShiftYAlignedLeft,
             centerShiftXAlignedRight,
             centerShiftYAlignedRight,
             edgeRatioX,
             edgeRatioY
            };
}
}

void FFR::GetOptimizedResolution(uint32_t* width, uint32_t* height) {
    auto fovVars = CalculateFoveationVars();
    *width = fovVars.optimizedEyeWidth * 2;
    *height = fovVars.optimizedEyeHeight;
}

FFR::FFR(ID3D11Device* device)
    : mDevice(device) { }

void FFR::Initialize(ID3D11Texture2D* compositionTexture) {
    auto fovVars = CalculateFoveationVars();
    mDevice->GetImmediateContext(&mImmediateContext);
    mFoveatedRenderingBuffer = CreateBuffer(mDevice.Get(), fovVars, D3D11_USAGE_DEFAULT);

    std::vector<uint8_t> quadShaderCSO(
        QUAD_SHADER_CSO_PTR, QUAD_SHADER_CSO_PTR + QUAD_SHADER_CSO_LEN
    );
    mQuadVertexShader = CreateVertexShader(mDevice.Get(), quadShaderCSO);

    mOptimizedTexture = CreateTexture(
        mDevice.Get(),
        fovVars.optimizedEyeWidth * 2,
        fovVars.optimizedEyeHeight,
        Settings::Instance().m_enableHdr ? DXGI_FORMAT_R16G16B16A16_FLOAT
                                         : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
    );

   if (Settings::Instance().m_enableFoveatedEncoding) {
    // 0: Vanilla (compressAxisAlignedPipeline)
    // 1: D-AADT2 
    // 2: D-AADT3 
    // 3: D-FRW  
    const uint32_t method = Settings::Instance().m_foveationMethod;

    if (method == 0) {
        // --- Vanilla Shaders ---
        std::vector<uint8_t> compressAxisAlignedShaderCSO(
            COMPRESS_AXIS_ALIGNED_CSO_PTR,
            COMPRESS_AXIS_ALIGNED_CSO_PTR + COMPRESS_AXIS_ALIGNED_CSO_LEN
        );
        auto compressAxisAlignedPipeline = RenderPipeline(mDevice.Get());
        compressAxisAlignedPipeline.Initialize(
            { compositionTexture },
            mQuadVertexShader.Get(),
            compressAxisAlignedShaderCSO,
            mOptimizedTexture.Get(),
            mFoveatedRenderingBuffer.Get()
        );
        mPipelines.push_back(compressAxisAlignedPipeline);
    } else if (method == 1) {
        // --- D-AADT2 Shaders ---
        std::vector<uint8_t> aadt2CSO(AADT2_CSO_PTR, AADT2_CSO_PTR + AADT2_CSO_LEN);
        auto aadt2Pipeline = RenderPipeline(mDevice.Get());
        aadt2Pipeline.Initialize(
            { compositionTexture },
            mQuadVertexShader.Get(),
            aadt2CSO,
            mOptimizedTexture.Get(),
            mFoveatedRenderingBuffer.Get()
        );
        mPipelines.push_back(aadt2Pipeline);
    } else if (method == 2) {
        // --- D-AADT3 Shaders ---
        std::vector<uint8_t> aadt3CSO(AADT3_CSO_PTR, AADT3_CSO_PTR + AADT3_CSO_LEN);
        auto aadt3Pipeline = RenderPipeline(mDevice.Get());
        aadt3Pipeline.Initialize(
            { compositionTexture },
            mQuadVertexShader.Get(),
            aadt3CSO,
            mOptimizedTexture.Get(),
            mFoveatedRenderingBuffer.Get()
        );
        mPipelines.push_back(aadt3Pipeline);
    } else if (method == 3) {
        // --- D-FRW Shaders ---
        std::vector<uint8_t> frwCSO(FRW_CSO_PTR, FRW_CSO_PTR + FRW_CSO_LEN);
        auto frwPipeline = RenderPipeline(mDevice.Get());
        frwPipeline.Initialize(
            { compositionTexture },
            mQuadVertexShader.Get(),
            frwCSO,
            mOptimizedTexture.Get(),
            mFoveatedRenderingBuffer.Get()
        );
        mPipelines.push_back(frwPipeline);

    }
 } else {
        mOptimizedTexture = compositionTexture;
    }
}

void FFR::Render(uint64_t targetTimestampNs) {
    if (mFoveatedRenderingBuffer) {
        auto fovVars = CalculateFoveationVars();
        UpdateBuffer(mImmediateContext.Get(), mFoveatedRenderingBuffer.Get(), &fovVars);

        if (ReportFoveationCenterShiftUsed) {
            ReportFoveationCenterShiftUsed(
                targetTimestampNs,
                fovVars.centerShiftXLeft,
                fovVars.centerShiftYLeft,
                fovVars.centerShiftXRight,
                fovVars.centerShiftYRight
            );
        }
    }
    for (auto& p : mPipelines) {
        p.Render();
    }
}

ID3D11Texture2D* FFR::GetOutputTexture() { return mOptimizedTexture.Get(); }
