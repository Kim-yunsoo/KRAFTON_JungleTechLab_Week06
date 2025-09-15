#include "pch.h"
#include "MeshLoader.h"
#include "ObjectFactory.h"
#include "d3dtk/DDSTextureLoader.h"

#define GRIDNUM 100
#define AXISLENGTH 100

UResourceManager::~UResourceManager()
{
    Clear();
}

UResourceManager& UResourceManager::GetInstance()
{
    static UResourceManager* Instance = nullptr;
    if (Instance == nullptr)
    {
        Instance = NewObject<UResourceManager>();
    }
    return *Instance;
}

void UResourceManager::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InContext)
{
    Device = InDevice;
    Resources.SetNum(static_cast<uint8>(ResourceType::End));

    Context = InContext;
    CreateGridMesh(GRIDNUM,"Grid");
    CreateAxisMesh(AXISLENGTH,"Axis");

    CreateDefaultShader();
}

FResourceData* UResourceManager::CreateOrGetResourceData(const FString& Name, uint32 Size , const TArray<uint32>& Indicies)
{
    auto it = ResourceMap.find(Name);
    if(it!=ResourceMap.end())
    {
        return it->second;
    }

    FResourceData* ResourceData = new FResourceData();

    CreateDynamicVertexBuffer(ResourceData, Size, Device);
    //CreateIndexBuffer(ResourceData, Indicies, Device);

    ResourceMap[Name] = ResourceData;
    return ResourceData;
}

UMaterial* UResourceManager::GetOrCreateMaterial(const FString& Name, EVertexLayoutType layoutType)
{
    auto it = MaterialMap.find(Name);
    if (it != MaterialMap.end())
        return it->second;

    // FName → FString 변환
    FString BaseName = Name;

    // Shader, Texture 로드
    UShader* Shader;
    UTexture* Texture;

    if (UResourceManager::GetInstance().Get<UShader>(Name))
    {
        Shader = UResourceManager::GetInstance().Get<UShader>(Name);
    }
    else
    {
        Shader = UResourceManager::GetInstance().Load<UShader>(Name, layoutType);
    }
    if (UResourceManager::GetInstance().Get<UTexture>(Name))
    {
        Texture = UResourceManager::GetInstance().Get<UTexture>(Name);
    }
    else
    {
        Texture = UResourceManager::GetInstance().Load<UTexture>(Name);
    }
    // Material 생성
    UMaterial* Mat = NewObject<UMaterial>();
    if (Shader)  Mat->SetShader(Shader);
    if (Texture) Mat->SetTexture(Texture);

    MaterialMap[Name] = Mat;
    return Mat;
}

// 전체 해제
void UResourceManager::Clear()
{
    { //Deprecated Part
        for (auto& [Key, Data] : StaticMeshMap)
        {
            if (Data)
            {
                ObjectFactory::DeleteObject(Data);
            }
        }
        StaticMeshMap.clear();

        for (auto& [Key, Data] : ResourceMap)
        {
            if (Data)
            {
                if (Data->VertexBuffer)
                {
                    Data->VertexBuffer->Release();
                    Data->VertexBuffer = nullptr;
                }
                if (Data->IndexBuffer)
                {
                    Data->IndexBuffer->Release();
                    Data->IndexBuffer = nullptr;
                }
                delete Data;
            }
        }
        ResourceMap.clear();
    }

    for (auto& Array : Resources)
    {
        for (auto& Resource : Array)
        {
            if(Resource.second)
            {
                DeleteObject(Resource.second);
                Resource.second = nullptr;
            }
        }
        Array.Empty();
    }
    Resources.Empty();

    // Instance lifetime is managed by ObjectFactory
}

void UResourceManager::CreateAxisMesh(float Length, const FString& FilePath)
{
    // 이미 있으면 패스
    if (ResourceMap[FilePath])
    {
        return;
    }

    TArray<FVector> axisVertices;
    TArray<FVector4> axisColors;
    TArray<uint32> axisIndices;

    // X축 (빨강)
    axisVertices.push_back(FVector(0.0f, 0.0f, 0.0f));       // 원점
    axisVertices.push_back(FVector(Length, 0.0f, 0.0f));     // +X
    axisColors.push_back(FVector4(1.0f, 0.0f, 0.0f, 1.0f));  // 빨강
    axisColors.push_back(FVector4(1.0f, 0.0f, 0.0f, 1.0f));  // 빨강
    axisIndices.push_back(0);
    axisIndices.push_back(1);

    // Y축 (초록)
    axisVertices.push_back(FVector(0.0f, 0.0f, 0.0f));       // 원점
    axisVertices.push_back(FVector(0.0f, Length, 0.0f));     // +Y
    axisColors.push_back(FVector4(0.0f, 1.0f, 0.0f, 1.0f));  // 초록
    axisColors.push_back(FVector4(0.0f, 1.0f, 0.0f, 1.0f));  // 초록
    axisIndices.push_back(2);
    axisIndices.push_back(3);

    // Z축 (파랑)
    axisVertices.push_back(FVector(0.0f, 0.0f, 0.0f));       // 원점
    axisVertices.push_back(FVector(0.0f, 0.0f, Length));     // +Z
    axisColors.push_back(FVector4(0.0f, 0.0f, 1.0f, 1.0f));  // 파랑
    axisColors.push_back(FVector4(0.0f, 0.0f, 1.0f, 1.0f));  // 파랑
    axisIndices.push_back(4);
    axisIndices.push_back(5);

    FMeshData* MeshData = new FMeshData();
    MeshData->Vertices = axisVertices;
    MeshData->Color = axisColors;
    MeshData->Indices = axisIndices;

    UMesh* Mesh = NewObject<UMesh>();
    Mesh->Load(MeshData, Device);
    Add<UMesh>("Axis", Mesh);

    UMeshLoader::GetInstance().AddMeshData("Axis", MeshData);
}
void UResourceManager::CreateGridMesh(int N, const FString& FilePath)
{
    if (ResourceMap[FilePath])
    {
        return;
    }
    TArray<FVector> gridVertices;
    TArray<FVector4> gridColors;
    TArray<uint32> gridIndices;
    // Z축 방향 선
    for (int i = -N; i <= N; i++)
    {
        if (i == 0)
        {
            continue;
        }
        // 색 결정: 5의 배수면 흰색, 아니면 회색
        float r = 0.1f, g = 0.1f, b = 0.1f;
        if (i % 5 == 0) { r = g = b = 0.4f; }
        if (i % 10 == 0) { r = g = b = 1.0f; }

        // 정점 2개 추가 (Z축 방향 라인)
        gridVertices.push_back(FVector((float)i, 0.0f, (float)-N));
        gridVertices.push_back(FVector((float)i, 0.0f, (float)N));
        gridColors.push_back(FVector4(r, g, b, 1.0f));
        gridColors.push_back(FVector4(r, g, b, 1.0f));

        // 인덱스 추가
        uint32 base = static_cast<uint32>(gridVertices.size());
        gridIndices.push_back(base - 2);
        gridIndices.push_back(base - 1);
    }

    // X축 방향 선
    for (int j = -N; j <= N; j++)
    {
        if (j == 0)
        {
            continue;
        }
        // 색 결정: 5의 배수면 흰색, 아니면 회색
        float r = 0.1f, g = 0.1f, b = 0.1f;

        if (j % 5 == 0) { r = g = b = 0.4f; }
        if (j % 10 == 0) { r = g = b = 1.0f; }

        // 정점 2개 추가 (X축 방향 라인)
        gridVertices.push_back(FVector((float)-N, 0.0f, (float)j));
        gridVertices.push_back(FVector((float)N, 0.0f, (float)j));
        gridColors.push_back(FVector4(r, g, b, 1.0f));
        gridColors.push_back(FVector4(r, g, b, 1.0f));

        // 인덱스 추가
        uint32 base = static_cast<uint32>(gridVertices.size());
        gridIndices.push_back(base - 2);
        gridIndices.push_back(base - 1);
    }

    gridVertices.push_back(FVector(0.0f, 0.0f, (float)-N));
    gridVertices.push_back(FVector(0.0f, 0.0f, 0.0f));
    gridColors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    gridColors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    uint32 base = static_cast<uint32>(gridVertices.size());
    gridIndices.push_back(base - 2);
    gridIndices.push_back(base - 1);

    gridVertices.push_back(FVector((float)-N, 0.0f, 0.0f));
    gridVertices.push_back(FVector(0.0f, 0.0f, 0.0f));
    gridColors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    gridColors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    base = static_cast<uint32>(gridVertices.size());
    gridIndices.push_back(base - 2);
    gridIndices.push_back(base - 1);

    FMeshData* MeshData = new FMeshData();
    MeshData->Vertices = gridVertices;
    MeshData->Color = gridColors;
    MeshData->Indices = gridIndices;

    UMesh* Mesh = NewObject<UMesh>();
    Mesh->Load(MeshData, Device);
    Add<UMesh>("Grid", Mesh);

    UMeshLoader::GetInstance().AddMeshData("Grid", MeshData);
}

void UResourceManager::CreateBoxWireframeMesh(const FVector& Min, const FVector& Max, const FString& FilePath)
{
    // 이미 있으면 패스
    if (ResourceMap[FilePath])
    {
        return;
    }

    TArray<FVector> vertices;
    TArray<FVector4> colors;
    TArray<uint32> indices;

    // ─────────────────────────────
    // 8개의 꼭짓점 (AABB)
    // ─────────────────────────────
    vertices.push_back(FVector(Min.X, Min.Y, Min.Z)); // 0
    vertices.push_back(FVector(Max.X, Min.Y, Min.Z)); // 1
    vertices.push_back(FVector(Max.X, Max.Y, Min.Z)); // 2
    vertices.push_back(FVector(Min.X, Max.Y, Min.Z)); // 3
    vertices.push_back(FVector(Min.X, Min.Y, Max.Z)); // 4
    vertices.push_back(FVector(Max.X, Min.Y, Max.Z)); // 5
    vertices.push_back(FVector(Max.X, Max.Y, Max.Z)); // 6
    vertices.push_back(FVector(Min.X, Max.Y, Max.Z)); // 7

    // 색상 (디버깅용 흰색)
    for (int i = 0; i < 8; i++)
    {
        colors.push_back(FVector4(1.0f, 1.0f, 1.0f, 1.0f));
    }

    // ─────────────────────────────
    // 12개의 선 (24 인덱스)
    // ─────────────────────────────
    uint32 boxIndices[] = {
        // 바닥
        0, 1, 1, 2, 2, 3, 3, 0,
        // 천장
        4, 5, 5, 6, 6, 7, 7, 4,
        // 기둥
        0, 4, 1, 5, 2, 6, 3, 7
    };

    indices.insert(indices.end(), std::begin(boxIndices), std::end(boxIndices));

    // ─────────────────────────────
    // MeshData 생성 및 등록
    // ─────────────────────────────
    FMeshData* MeshData = new FMeshData();
    MeshData->Vertices = vertices;
    MeshData->Color = colors;
    MeshData->Indices = indices;

    UMesh* Mesh = NewObject<UMesh>();
    Mesh->Load(MeshData, Device);
    //Mesh->SetTopology(EPrimitiveTopology::LineList); // ✅ 꼭 LineList로 설정

    Add<UMesh>(FilePath, Mesh);

    UMeshLoader::GetInstance().AddMeshData(FilePath, MeshData);
}

void UResourceManager::CreateDefaultShader()
{
    Load<UShader>("Primitive.hlsl", EVertexLayoutType::PositionColor);
    Load<UShader>("CollisionDebug.hlsl", EVertexLayoutType::PositionColor);
    Load<UShader>("TextBillboard.hlsl", EVertexLayoutType::PositionBillBoard);
}

void UResourceManager::CreateDynamicVertexBuffer(FResourceData* data, uint32 Size, ID3D11Device* Device)
{
    if (Size == 0) return;

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DYNAMIC;
    vbd.ByteWidth = static_cast<UINT>(sizeof(FBillboardCharInfo) * Size);
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA vinitData = {};

    HRESULT hr = Device->CreateBuffer(&vbd, nullptr, &data->VertexBuffer);
    if (FAILED(hr))
    {
        delete data;
        return;
    }

    data->VertexCount = static_cast<uint32>(Size);
    data->ByteWidth = vbd.ByteWidth;
}


void UResourceManager::UpdateDynamicVertexBuffer(const FString& Name, TArray<FBillboardCharInfo>& vertices)
{
    if (ResourceMap.find(Name) == ResourceMap.end()) return;
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    Context->Map(ResourceMap[Name]->VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);//리소스 데이터의 버텍스 데이터를 mappedResource에 매핑
    memcpy(mappedResource.pData, vertices.data(), sizeof(FBillboardCharInfo) * vertices.size()); //vertices.size()만큼의 Character info를 vertices에서 pData로 복사해가라
    Context->Unmap(ResourceMap[Name]->VertexBuffer, 0);//언맵
}

FTextureData* UResourceManager::CreateOrGetTextureData(const FWideString& FilePath)
{
    auto it = TextureMap.find(FilePath);
    if (it!=TextureMap.end())
    {
        return it->second;
    }

    FTextureData* Data = new FTextureData();
    HRESULT hr = DirectX::CreateDDSTextureFromFile(Device, FilePath.c_str(), &Data->Texture, &Data->TextureSRV, 0, nullptr);
    if (FAILED(hr))
    {
        int a = 0;
        D3D11_SAMPLER_DESC SamplerDesc;
        ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
    }
    D3D11_SAMPLER_DESC SamplerDesc;
    ZeroMemory(&SamplerDesc, sizeof(SamplerDesc));
    SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    SamplerDesc.MinLOD = 0;
    SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = Device->CreateSamplerState(&SamplerDesc, &Data->SamplerState);
    if (FAILED(hr))
    {

    }

    D3D11_BLEND_DESC blendDesc;
    ZeroMemory(&blendDesc, sizeof(blendDesc));
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    // 멤버 변수 m_pAlphaBlendState에 저장
    hr = Device->CreateBlendState(&blendDesc, &Data->BlendState);
    if (FAILED(hr))
    {

    }
    TextureMap[FilePath] = Data;
    return TextureMap[FilePath];
}
