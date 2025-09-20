﻿#pragma once
#include "ResourceBase.h"
#include "Enums.h"
#include <d3d11.h>

class UStaticMesh : public UResourceBase
{
public:
    DECLARE_CLASS(UStaticMesh, UResourceBase)

    UStaticMesh() = default;
    virtual ~UStaticMesh() override;

    void Load(const FString& InFilePath, ID3D11Device* InDevice, EVertexLayoutType InVertexType = EVertexLayoutType::PositionColor);
    void Load(FMeshData* InData, ID3D11Device* InDevice, EVertexLayoutType InVertexType = EVertexLayoutType::PositionColor);

    ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer; }
    ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer; }
    uint32 GetVertexCount() { return VertexCount; }
    uint32 GetIndexCount() { return IndexCount; }
    void SetIndexCount(uint32 Cnt) { IndexCount = Cnt; }

	// CPU-side mesh data 접근 (legacy)
    // const FMeshData* GetMeshData() const { return MeshDataCPU; }
	const FStaticMesh* GetMeshData() const { return StaticMeshAsset; }

	const FString& GetAssetPathFileName() const { return StaticMeshAsset ? StaticMeshAsset->PathFileName : FilePath; }
    void SetStaticMeshAsset(FStaticMesh* InStaticMesh) { StaticMeshAsset = InStaticMesh; }
	FStaticMesh* GetStaticMeshAsset() const { return StaticMeshAsset; }

private:
    void CreateVertexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice, EVertexLayoutType InVertexType);
	void CreateVertexBuffer(FStaticMesh* InStaticMesh, ID3D11Device* InDevice, EVertexLayoutType InVertexType);
    void CreateIndexBuffer(FMeshData* InMeshData, ID3D11Device* InDevice);
	void CreateIndexBuffer(FStaticMesh* InStaticMesh, ID3D11Device* InDevice);
    void ReleaseResources();

    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer = nullptr;
    uint32 VertexCount = 0;     // 정점 개수
    uint32 IndexCount = 0;     // 버텍스 점의 개수 
    EVertexLayoutType VertexType = EVertexLayoutType::PositionColor;  // 버텍스 타입

    // FMeshData* MeshDataCPU = nullptr;  // CPU-side mesh data 보관 (leagacy)
    FStaticMesh* StaticMeshAsset = nullptr;
};