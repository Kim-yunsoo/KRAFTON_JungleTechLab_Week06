#pragma once
#include "ResourceBase.h"

class UShader : public UResourceBase
{
public:
	DECLARE_CLASS(UShader, UResourceBase)

	void Load(const FString& ShaderPath, ID3D11Device* InDevice, EVertexLayoutType InLayoutType);

	ID3D11InputLayout* GetInputLayout() const { return InputLayout; }
	ID3D11VertexShader* GetVertexShader() const { return VertexShader; }
	ID3D11PixelShader* GetPixelShader() const { return PixelShader; }
protected:
	virtual ~UShader();

private:
	ID3DBlob* VSBlob = nullptr;
	ID3DBlob* PSBlob = nullptr;

	ID3D11InputLayout* InputLayout = nullptr;
	ID3D11VertexShader* VertexShader = nullptr;
	ID3D11PixelShader* PixelShader = nullptr;

	void CreateInputLayout(ID3D11Device* Device, EVertexLayoutType InLayoutType);
	void ReleaseResources();
};

struct FVertexPositionColor
{
	static const D3D11_INPUT_ELEMENT_DESC* GetLayout()
	{
		static const D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		return layout;
	}
	static uint32 GetLayoutCount() { return 2; }
};

struct FVertexPositionColorTexturNormal
{
	static const D3D11_INPUT_ELEMENT_DESC* GetLayout()
	{
		static const D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		return layout;
	}

	static uint32 GetLayoutCount() { return 2; }
};

struct FVertexPositionBillBoard
{
	static const D3D11_INPUT_ELEMENT_DESC* GetLayout()
	{
		static const D3D11_INPUT_ELEMENT_DESC layout[] = {
			{ "WORLDPOSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "SIZE", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, sizeof(float)*3, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{ "UVRECT", 0, DXGI_FORMAT_R32G32B32_FLOAT, sizeof(float)*3+sizeof(float)*2, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		};
		return layout;
	}

	static uint32 GetLayoutCount() { return 3; }
};