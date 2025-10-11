#include "pch.h"
#include "RenderingStats.h"
#include <algorithm>

URenderingStatsCollector& URenderingStatsCollector::GetInstance()
{
    static URenderingStatsCollector Instance;
    return Instance;
}

void URenderingStatsCollector::UpdateFrameStats(const FRenderingStats& InNewStats)
{
    if (!bEnabled)
    {
        return;
    }
        
    CurrentFrameStats = InNewStats;
    
    // 프레임 히스토리에 추가
    FrameHistory[FrameHistoryIndex] = InNewStats;
    FrameHistoryIndex = (FrameHistoryIndex + 1) % AVERAGE_FRAME_COUNT;
    ValidFrameCount = std::min(ValidFrameCount + 1, AVERAGE_FRAME_COUNT);
    
    // 평균 계산
    CalculateAverageStats();
}

void URenderingStatsCollector::BeginFrame()
{
    if (!bEnabled)
    {
        return;
    }
        
    FrameStartTime = std::chrono::high_resolution_clock::now();
    CurrentFrameStats.Reset();
}

void URenderingStatsCollector::EndFrame()
{
    if (!bEnabled)
    {
        return;
    }
        
    auto EndTime = std::chrono::high_resolution_clock::now();
    auto Duration = std::chrono::duration_cast<std::chrono::microseconds>(EndTime - FrameStartTime);
    CurrentFrameStats.TotalRenderTime = Duration.count() / 1000.0f; // ms로 변환
}

void URenderingStatsCollector::UpdatePickingStats(double InPickingTimeMs)
{
    LastPickingTimeMs = InPickingTimeMs;
    NumPickingAttempts++;
    AccumulatedPickingTimeMs += InPickingTimeMs;
}

double URenderingStatsCollector::GetLastPickingTime() const
{
    return LastPickingTimeMs;
}

uint64_t URenderingStatsCollector::GetNumPickingAttempts() const
{
    return NumPickingAttempts;
}

double URenderingStatsCollector::GetAccumulatedPickingTime() const
{
    return AccumulatedPickingTimeMs;
}

void URenderingStatsCollector::ResetStats()
{
    CurrentFrameStats.Reset();
    AverageStats.Reset();
    
    for (uint32 i = 0; i < AVERAGE_FRAME_COUNT; ++i)
    {
        FrameHistory[i].Reset();
    }
    
    FrameHistoryIndex = 0;
    ValidFrameCount = 0;
}

void URenderingStatsCollector::UpdateDecalStats(const FDecalRenderingStats& InStats)
{
    if (!bEnabled)
        return;

    // 현재 프레임 통계 업데이트
    CurrentDecalStats = InStats;

    // 히스토리에 추가
    DecalStatsHistory[DecalStatsHistoryIndex] = InStats;
    DecalStatsHistoryIndex = (DecalStatsHistoryIndex + 1) % AVERAGE_FRAME_COUNT;

    // 평균 계산
    FDecalRenderingStats SumStats;
    for (uint32 i = 0; i < AVERAGE_FRAME_COUNT; ++i)
    {
        SumStats.TotalDecalCount += DecalStatsHistory[i].TotalDecalCount;
        SumStats.ActiveDecalCount += DecalStatsHistory[i].ActiveDecalCount;
        SumStats.AffectedMeshesCount += DecalStatsHistory[i].AffectedMeshesCount;
        SumStats.DecalDrawCalls += DecalStatsHistory[i].DecalDrawCalls;
        SumStats.DecalPassTimeMs += DecalStatsHistory[i].DecalPassTimeMs;
        SumStats.CollisionCheckTimeMs += DecalStatsHistory[i].CollisionCheckTimeMs;
        SumStats.DecalShaderChanges += DecalStatsHistory[i].DecalShaderChanges;
        SumStats.DecalBlendStateChanges += DecalStatsHistory[i].DecalBlendStateChanges;
        SumStats.DecalDepthStateChanges += DecalStatsHistory[i].DecalDepthStateChanges;
    }

    // 평균값 계산
    AverageDecalStats.TotalDecalCount = SumStats.TotalDecalCount / AVERAGE_FRAME_COUNT;
    AverageDecalStats.ActiveDecalCount = SumStats.ActiveDecalCount / AVERAGE_FRAME_COUNT;
    AverageDecalStats.AffectedMeshesCount = SumStats.AffectedMeshesCount / AVERAGE_FRAME_COUNT;
    AverageDecalStats.DecalDrawCalls = SumStats.DecalDrawCalls / AVERAGE_FRAME_COUNT;
    AverageDecalStats.DecalPassTimeMs = SumStats.DecalPassTimeMs / static_cast<float>(AVERAGE_FRAME_COUNT);
    AverageDecalStats.CollisionCheckTimeMs = SumStats.CollisionCheckTimeMs / static_cast<float>(AVERAGE_FRAME_COUNT);
    AverageDecalStats.DecalShaderChanges = SumStats.DecalShaderChanges / AVERAGE_FRAME_COUNT;
    AverageDecalStats.DecalBlendStateChanges = SumStats.DecalBlendStateChanges / AVERAGE_FRAME_COUNT;
    AverageDecalStats.DecalDepthStateChanges = SumStats.DecalDepthStateChanges / AVERAGE_FRAME_COUNT;

    // 평균의 평균값 계산
    AverageDecalStats.CalculateAverages();
}

void URenderingStatsCollector::BeginDecalPass()
{
    if (!bEnabled)
        return;

    // 데칼 패스 통계 초기화
    CurrentDecalStats.Reset();

    // 타이머 시작
    DecalPassStartTime = std::chrono::high_resolution_clock::now();
}

void URenderingStatsCollector::EndDecalPass()
{
    if (!bEnabled)
        return;

    // 타이머 종료 및 시간 계산
    auto DecalPassEndTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float, std::milli> Duration = DecalPassEndTime - DecalPassStartTime;
    CurrentDecalStats.DecalPassTimeMs = Duration.count();

    // 평균값 계산
    CurrentDecalStats.CalculateAverages();

    // 통계 업데이트 (히스토리에 저장 및 평균 계산)
    UpdateDecalStats(CurrentDecalStats);
}

void URenderingStatsCollector::CalculateAverageStats()
{
    if (ValidFrameCount == 0)
        return;
        
    AverageStats.Reset();
    
    // 모든 유효한 프레임의 평균 계산
    for (uint32 i = 0; i < ValidFrameCount; ++i)
    {
        const FRenderingStats& Frame = FrameHistory[i];
        
        AverageStats.TotalDrawCalls += Frame.TotalDrawCalls;
        AverageStats.MaterialChanges += Frame.MaterialChanges;
        AverageStats.TextureChanges += Frame.TextureChanges;
        AverageStats.ShaderChanges += Frame.ShaderChanges;
        AverageStats.BasePassDrawCalls += Frame.BasePassDrawCalls;
        AverageStats.DepthPrePassDrawCalls += Frame.DepthPrePassDrawCalls;
        AverageStats.TranslucentPassDrawCalls += Frame.TranslucentPassDrawCalls;
        AverageStats.DebugPassDrawCalls += Frame.DebugPassDrawCalls;
        AverageStats.TotalRenderTime += Frame.TotalRenderTime;
    }
    
    // 평균 계산
    float InvCount = 1.0f / static_cast<float>(ValidFrameCount);
    AverageStats.TotalDrawCalls = static_cast<uint32>(AverageStats.TotalDrawCalls * InvCount);
    AverageStats.MaterialChanges = static_cast<uint32>(AverageStats.MaterialChanges * InvCount);
    AverageStats.TextureChanges = static_cast<uint32>(AverageStats.TextureChanges * InvCount);
    AverageStats.ShaderChanges = static_cast<uint32>(AverageStats.ShaderChanges * InvCount);
    AverageStats.BasePassDrawCalls = static_cast<uint32>(AverageStats.BasePassDrawCalls * InvCount);
    AverageStats.DepthPrePassDrawCalls = static_cast<uint32>(AverageStats.DepthPrePassDrawCalls * InvCount);
    AverageStats.TranslucentPassDrawCalls = static_cast<uint32>(AverageStats.TranslucentPassDrawCalls * InvCount);
    AverageStats.DebugPassDrawCalls = static_cast<uint32>(AverageStats.DebugPassDrawCalls * InvCount);
    AverageStats.TotalRenderTime *= InvCount;
}