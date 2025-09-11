#pragma once
#include <d3d11.h>
#include <d3dcompiler.h>
#include "Vector.h"

class URHIDevice
{
public:
    URHIDevice() {};
    virtual ~URHIDevice() {};

private:
    // 복사생성자, 연산자 금지
    URHIDevice(const URHIDevice& RHIDevice) = delete;
    URHIDevice& operator=(const URHIDevice& RHIDevice) = delete;

public:
    virtual void Initialize(HWND hWindow) = 0;
    virtual void Release() = 0;
public:
    //getter
    virtual ID3D11Device* GetDevice() = 0;
    virtual ID3D11DeviceContext* GetDeviceContext() = 0;

    // create
    virtual void CreateDeviceAndSwapChain(HWND hWindow) = 0;;
    virtual void CreateFrameBuffer() = 0;
    virtual void CreateRasterizerState() = 0;
    virtual void CreateConstantBuffer() = 0;
    virtual void CreateBlendState() = 0;
    virtual void CreateShader(ID3D11InputLayout** OutSimpleInputLayout, ID3D11VertexShader** OutSimpleVertexShader, ID3D11PixelShader** OutSimplePixelShader) = 0;

    // update
    virtual void UpdateConstantBuffers(const FMatrix& ModelMatrix, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix) = 0;
    virtual void UpdateHighLightConstantBuffers(const uint32 InPicked, const FVector& InColor, const uint32 X, const uint32 Y, const uint32 Z, const uint32 Gizmo) = 0;

    // clear
    virtual void ClearBackBuffer() = 0;
    virtual void ClearDepthBuffer(float Depth, UINT Stenci) = 0;

    virtual void IASetPrimitiveTopology() = 0;
    virtual void RSSetViewport() = 0;
    virtual void RSSetState() = 0;
    virtual void OMSetRenderTargets() = 0;
    virtual void OMSetBlendState(bool bIsBlendMode) = 0;
    virtual void Present() = 0;
};

