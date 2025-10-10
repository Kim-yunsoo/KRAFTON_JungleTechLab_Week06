#include"pch.h"
#include "OBoundingBoxComponent.h"
#include "Vector.h"

std::vector<FVector> MakeVerticesFromFBox(const FBox& Box)
{
    const FVector& Min = Box.Min;
    const FVector& Max = Box.Max;

    return {
        {Min.X, Min.Y, Min.Z},
        {Max.X, Min.Y, Min.Z},
        {Min.X, Max.Y, Min.Z},
        {Max.X, Max.Y, Min.Z},
        {Min.X, Min.Y, Max.Z},
        {Max.X, Min.Y, Max.Z},
        {Min.X, Max.Y, Max.Z},
        {Max.X, Max.Y, Max.Z}
    };
}


UOBoundingBoxComponent::UOBoundingBoxComponent()
    : LocalMin(FVector{}), LocalMax(FVector{})
{
   //SetMaterial("CollisionDebug.hlsl");
} 

void UOBoundingBoxComponent::UpdateFromWorld(const FMatrix& World)
{
    Center = FVector(World.M[3][0], World.M[3][1], World.M[3][2]);
    
    FVector Axis0(World.M[0][0], World.M[0][1], World.M[0][2]);
    FVector Axis1(World.M[1][0], World.M[1][1], World.M[1][2]);
    FVector Axis2(World.M[2][0], World.M[2][1], World.M[2][2]);
    
    Extent.X = Axis0.Size() * 0.5f;
    Extent.Y = Axis1.Size() * 0.5f;
    Extent.Z = Axis2.Size() * 0.5f;
    
    Axis0.Normalize(); Axis[0] = Axis0;
    Axis1.Normalize(); Axis[1] = Axis1;
    Axis2.Normalize(); Axis[2] = Axis2;
}

bool UOBoundingBoxComponent::IntersectWithAABB(FBound AABB)
{
    // TODO: SAT Algorithm 
    FVector t = AABB.GetCenter() - GetCenter();

    auto SAT = [&](const FVector& L) -> bool {
        
        float rA = AABB.GetExtent().X * abs(L.X)
            + AABB.GetExtent().Y * abs(L.Y)
            + AABB.GetExtent().Z * abs(L.Z);

        float rB = abs(Extent.X * L.Dot(Axis[0]))
                   + abs(Extent.Y * L.Dot(Axis[1]))
                   + abs(Extent.Z * L.Dot(Axis[2]));
        
        const float dist = abs(t.Dot(L));

        return dist > rA + rB;
    }; 

    if (SAT(FVector(1, 0, 0))) return false;
    if (SAT(FVector(0, 1, 0))) return false;
    if (SAT(FVector(0, 0, 1))) return false;
    
    if(SAT(Axis[0])) return false;
    if(SAT(Axis[1])) return false;
    if(SAT(Axis[2])) return false;
    

    const FVector A[3] = { FVector(1,0,0), FVector(0,1,0), FVector(0,0,1) };
    const FVector B[3] = { Axis[0], Axis[1], Axis[2]};

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            const FVector L = FVector::Cross(A[i], B[j]);
            if (L.SizeSquared() > 1e-10 && SAT(L)) return false;
        }
    }

    return true;
}

void UOBoundingBoxComponent::SetFromVertices(const std::vector<FVector>& Verts)
{
    if (Verts.empty()) return;

    LocalMin = LocalMax = Verts[0];
    for (auto& v : Verts)
    {
        LocalMin = LocalMin.ComponentMin(v);
        LocalMax = LocalMax.ComponentMax(v);
    }
    FString MeshName = FString("OBB_") + AttachParent->GetName();
    UResourceManager::GetInstance().CreateBoxWireframeMesh(LocalMin, LocalMax, MeshName);
    //SetMeshResource(MeshName);
}

FBox UOBoundingBoxComponent::GetWorldBox() const
{
    auto corners = GetLocalCorners();

    FVector MinW = GetWorldTransform().TransformPosition(corners[0]);
    FVector MaxW = MinW;

    for (auto& c : corners)
    {
        FVector wc = GetWorldTransform().TransformPosition(c);
        MinW = MinW.ComponentMin(wc);
        MaxW = MaxW.ComponentMax(wc);
    }//MinW, MaxW
    return FBox();
}

/*FVector UOBoundingBoxComponent::GetExtent() const
{
    return (LocalMax - LocalMin) * 0.5f;   
}*/

FVector UOBoundingBoxComponent::GetExtent() const
{
    return Extent;   
}

std::vector<FVector> UOBoundingBoxComponent::GetLocalCorners() const
{
    return {
        {LocalMin.X, LocalMin.Y, LocalMin.Z},
        {LocalMax.X, LocalMin.Y, LocalMin.Z},
        {LocalMin.X, LocalMax.Y, LocalMin.Z},
        {LocalMax.X, LocalMax.Y, LocalMin.Z},
        {LocalMin.X, LocalMin.Y, LocalMax.Z},
        {LocalMax.X, LocalMin.Y, LocalMax.Z},
        {LocalMin.X, LocalMax.Y, LocalMax.Z},
        {LocalMax.X, LocalMax.Y, LocalMax.Z}
    };
}


FBox UOBoundingBoxComponent::GetWorldOBBFromAttachParent() const
{
	
    if (!AttachParent) return FBox();

    // AttachParent의 로컬 코너들
    auto corners = GetLocalCorners();

    // 월드 변환된 첫 번째 점으로 초기화
    FVector MinW = AttachParent->GetWorldTransform().TransformPosition(corners[0]);
    FVector MaxW = MinW;

    for (auto& c : corners)
    {
        FVector wc = AttachParent->GetWorldTransform().TransformPosition(c);
        MinW = MinW.ComponentMin(wc);
        MaxW = MaxW.ComponentMax(wc);
    }

    //BBWorldMatrix
    return FBox(MinW, MaxW);
}

void UOBoundingBoxComponent::Render(URenderer* Renderer, const FMatrix& ViewMatrix, const FMatrix& ProjectionMatrix)
{
    // Draw oriented bounding box as lines using Center, Axis[3], Extent
    TArray<FVector> Start;
    TArray<FVector> End;
    TArray<FVector4> Color;

    // Parameters are ignored; CreateLineData will use Center/Axis/Extent
    CreateLineData(Start, End, Color);
    Renderer->AddLines(Start, End, Color);
}

void UOBoundingBoxComponent::CreateLineData( 
    OUT TArray<FVector>& Start,
    OUT TArray<FVector>& End,
    OUT TArray<FVector4>& Color)
{
    // Build world-space corners from Center, Axis, Extent
    const FVector U = Axis[0] * Extent.X;
    const FVector V = Axis[1] * Extent.Y;
    const FVector W = Axis[2] * Extent.Z;

    const FVector c0 = Center - U - V - W;
    const FVector c1 = Center + U - V - W;
    const FVector c2 = Center + U + V - W;
    const FVector c3 = Center - U + V - W;
    const FVector c4 = Center - U - V + W;
    const FVector c5 = Center + U - V + W;
    const FVector c6 = Center + U + V + W;
    const FVector c7 = Center - U + V + W;

    const FVector4 lineColor(1.0f, 1.0f, 0.0f, 1.0f); // yellow

    auto add = [&](const FVector& a, const FVector& b)
    {
        Start.Add(a); End.Add(b); Color.Add(lineColor);
    };

    // bottom face
    add(c0, c1); add(c1, c2); add(c2, c3); add(c3, c0);
    // top face
    add(c4, c5); add(c5, c6); add(c6, c7); add(c7, c4);
    // vertical edges
    add(c0, c4); add(c1, c5); add(c2, c6); add(c3, c7);
}

