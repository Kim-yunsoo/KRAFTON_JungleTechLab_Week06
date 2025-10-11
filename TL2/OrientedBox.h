#pragma once

 // FOrientedBox (Oriented Bounding Box)
 // - 회전 가능한 바운딩 박스
 // - SAT (Separating Axis Theorem)를 이용한 OBB vs OBB 충돌 검사 지원
struct FOrientedBox
{
    FVector Center;        // 중심점 (월드 좌표)
    FVector HalfExtents;   // 반 크기 (로컬 좌표계 기준)
    FQuat Rotation;        // 회전 (월드 좌표계 기준)

    // 생성자
    FOrientedBox();
    FOrientedBox(const FVector& InCenter, const FVector& InHalfExtents, const FQuat& InRotation);

    // 로컬 축 벡터 (월드 좌표계로 변환된 3개 축)
    FVector GetAxisX() const;
    FVector GetAxisY() const;
    FVector GetAxisZ() const;

    // 8개 모서리 점 계산
    TArray<FVector> GetCorners() const;

    // AABB로 변환 (보수적)
    FBound ToAABB() const;

    // OBB vs OBB 충돌 검사 (SAT)
    bool Intersects(const FOrientedBox& Other) const;

private:
    // 특정 축에 투영했을 때 겹치는지 검사
    bool OverlapOnAxis(const FVector& Axis, const FOrientedBox& Other) const;

    // 축에 OBB 투영 시 최소/최대 값 계산
    void ProjectOntoAxis(const FVector& Axis, float& OutMin, float& OutMax) const;
};
