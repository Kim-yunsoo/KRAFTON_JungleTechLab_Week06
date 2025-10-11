#include "pch.h"
#include "Texture.h"
#include <DDSTextureLoader.h>
#include <WICTextureLoader.h> 

UTexture::UTexture()
{
	Width = 0;
	Height = 0;
	Format = DXGI_FORMAT_UNKNOWN;
}

UTexture::~UTexture()
{
	ReleaseResources();
}

void UTexture::Load(const FString& InFilePath, ID3D11Device* InDevice)
{
	assert(InDevice);

	std::wstring WFilePath;
	WFilePath = std::wstring(InFilePath.begin(), InFilePath.end());

	 
	size_t DotPos = WFilePath.find_last_of(L'.');
	std::wstring extension = WFilePath.substr(DotPos + 1);
	
	HRESULT hr = E_FAIL;

	if (extension == L"dds")
	{
		hr = DirectX::CreateDDSTextureFromFile(
			InDevice,
			WFilePath.c_str(),
			reinterpret_cast<ID3D11Resource**>(&Texture2D),
			&ShaderResourceView
		);
	}
	
	else if (extension == L"png" || extension == L"jpg")
	{
	hr = DirectX::CreateWICTextureFromFile(
		InDevice,
		WFilePath.c_str(),
		reinterpret_cast<ID3D11Resource**>(&Texture2D),
		&ShaderResourceView
	); 

	}
	if (FAILED(hr))
	{
		UE_LOG("!!!LOAD TEXTURE FAILED!!!");
	}

	if (Texture2D)
	{
		D3D11_TEXTURE2D_DESC desc;
		Texture2D->GetDesc(&desc);
		Width = desc.Width;
		Height = desc.Height;
		Format = desc.Format;
	}

	UE_LOG("Successfully loaded DDS texture: %s", InFilePath);
}

void UTexture::ReleaseResources()
{
	if(Texture2D)
	{
		Texture2D->Release();
	}

	if(ShaderResourceView)
	{
		ShaderResourceView->Release();
	}

	Width = 0;
	Height = 0;
	Format = DXGI_FORMAT_UNKNOWN;
}
