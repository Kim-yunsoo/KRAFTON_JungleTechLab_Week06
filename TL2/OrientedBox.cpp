#include "pch.h"
#include "OrientedBox.h"

FOrientedBox::FOrientedBox()
    : Center(FVector())
    , HalfExtents(FVector::One())
    , Rotation(FQuat::Identity())
{
}

FOrientedBox::FOrientedBox(const FVector& InCenter, const FVector& InHalfExtents, const FQuat& InRotation)
    : Center(InCenter)
    , HalfExtents(InHalfExtents)
    , Rotation(InRotation)
{
}

// 로컬 축 벡터
FVector FOrientedBox::GetAxisX() const
{
    return Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

FVector FOrientedBox::GetAxisY() const
{
    return Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f));
}

FVector FOrientedBox::GetAxisZ() const
{
    return Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f));
}

// 8개 모서리 점 계산
TArray<FVector> FOrientedBox::GetCorners() const
{
    TArray<FVector> Corners;
    Corners.reserve(8);

    // 로컬 좌표계의 8개 모서리
    FVector LocalCorners[8] = {
        FVector(-HalfExtents.X, -HalfExtents.Y, -HalfExtents.Z),
        FVector(+HalfExtents.X, -HalfExtents.Y, -HalfExtents.Z),
        FVector(-HalfExtents.X, +HalfExtents.Y, -HalfExtents.Z),
        FVector(+HalfExtents.X, +HalfExtents.Y, -HalfExtents.Z),
        FVector(-HalfExtents.X, -HalfExtents.Y, +HalfExtents.Z),
        FVector(+HalfExtents.X, -HalfExtents.Y, +HalfExtents.Z),
        FVector(-HalfExtents.X, +HalfExtents.Y, +HalfExtents.Z),
        FVector(+HalfExtents.X, +HalfExtents.Y, +HalfExtents.Z),
    };

    // 월드 좌표계로 변환
    for (int32 i = 0; i < 8; ++i)
    {
        FVector WorldCorner = Rotation.RotateVector(LocalCorners[i]) + Center;
        Corners.push_back(WorldCorner);
    }

    return Corners;
}

// AABB로 변환
FBound FOrientedBox::ToAABB() const
{
    TArray<FVector> Corners = GetCorners();

    FVector MinCorner = FVector(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector MaxCorner = FVector(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const FVector& Corner : Corners)
    {
        MinCorner.X = FMath::Min(MinCorner.X, Corner.X);
        MinCorner.Y = FMath::Min(MinCorner.Y, Corner.Y);
        MinCorner.Z = FMath::Min(MinCorner.Z, Corner.Z);

        MaxCorner.X = FMath::Max(MaxCorner.X, Corner.X);
        MaxCorner.Y = FMath::Max(MaxCorner.Y, Corner.Y);
        MaxCorner.Z = FMath::Max(MaxCorner.Z, Corner.Z);
    }

    return FBound(MinCorner, MaxCorner);
}

// OBB vs OBB 충돌 검사 (SAT)
bool FOrientedBox::Intersects(const FOrientedBox& Other) const
{
    // SAT: 15개 분리 축 검사
    // 1-3: this의 3개 축
    // 4-6: Other의 3개 축
    // 7-15: 3x3 외적 축

    FVector AxesA[3] = { GetAxisX(), GetAxisY(), GetAxisZ() };
    FVector AxesB[3] = { Other.GetAxisX(), Other.GetAxisY(), Other.GetAxisZ() };

    // 1-3: A의 면 법선 (3개 축)
    for (int32 i = 0; i < 3; ++i)
    {
        if (!OverlapOnAxis(AxesA[i], Other))
            return false; // 분리 축 발견 → 충돌 X
    }

    // 4-6: B의 면 법선 (3개 축)
    for (int32 i = 0; i < 3; ++i)
    {
        if (!OverlapOnAxis(AxesB[i], Other))    // Other가 아니라 *this 아닌가?
            return false;
    }

    // 7-15: 외적 축 (A축 x B축)
    for (int32 i = 0; i < 3; ++i)
    {
        for (int32 j = 0; j < 3; ++j)
        {
            FVector CrossAxis = FVector::Cross(AxesA[i], AxesB[j]);

            // 평행한 축은 스킵 (외적 결과가 0에 가까움)
            if (CrossAxis.SizeSquared() < 1e-6f)
                continue;

            CrossAxis.Normalize();

            if (!OverlapOnAxis(CrossAxis, Other))
                return false;
        }
    }

    // 모든 축에서 겹침 → 충돌 O
    return true;
}

// 축 투영 겹침 검사
bool FOrientedBox::OverlapOnAxis(const FVector& Axis, const FOrientedBox& Other) const
{
    float MinA, MaxA, MinB, MaxB;
    ProjectOntoAxis(Axis, MinA, MaxA);
    Other.ProjectOntoAxis(Axis, MinB, MaxB);

    // 1D 선분 겹침 검사
    return !(MaxA < MinB || MaxB < MinA);
}

// 축 투영 범위 계산
void FOrientedBox::ProjectOntoAxis(const FVector& Axis, float& OutMin, float& OutMax) const
{
    // 중심점 투영
    float CenterProj = Center.Dot(Axis);

    // 반경 계산 (각 로컬 축의 기여도)
    FVector Axes[3] = { GetAxisX(), GetAxisY(), GetAxisZ() };
    float Radius = 0.0f;

    for (int32 i = 0; i < 3; ++i)
    {
        Radius += abs(Axes[i].Dot(Axis)) * HalfExtents[i];
    }

    OutMin = CenterProj - Radius;
    OutMax = CenterProj + Radius;
}
