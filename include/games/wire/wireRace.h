#pragma once
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <string>

// 入力・描画から独立したレース状態。記録やゴーストもこの時計を基準にします。
namespace WireRace
{
inline constexpr float GoalHeight = 300.0f;
inline constexpr int CourseVersion = 1;
inline std::wstring FormatTime(float seconds)
{
    if (!std::isfinite(seconds) || seconds < 0.0f) return L"--:--.--";
    const int ticks = static_cast<int>(std::min(seconds, 5999.99f) * 100.0f);
    wchar_t buffer[32];
    swprintf_s(buffer, L"%02d:%02d.%02d", ticks / 6000, (ticks / 100) % 60, ticks % 100);
    return buffer;
}
struct Run
{
    float Seconds = 0.0f;
    bool Finished = false;
    bool RecordEligible = true;
    void Tick(float dt, bool paused, bool debugModified)
    {
        if (debugModified) RecordEligible = false;
        if (!Finished && !paused && std::isfinite(dt) && dt > 0.0f) Seconds += dt;
    }
    // 最上部のゲートとの線分交差。高速で通り抜けても取りこぼしません。
    bool TryFinish(float px, float py, float pz, float x, float y, float z)
    {
        if (Finished) return false;
        const float a[] = {px, py, pz}, b[] = {x, y, z};
        const float lo[] = {-11.0f, GoalHeight, -11.0f};
        const float hi[] = {11.0f, GoalHeight + 18.0f, 11.0f};
        float enter = 0.0f, leave = 1.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float delta = b[i] - a[i];
            if (std::abs(delta) < 0.00001f)
            {
                if (a[i] < lo[i] || a[i] > hi[i]) return false;
                continue;
            }
            float nearT = (lo[i] - a[i]) / delta, farT = (hi[i] - a[i]) / delta;
            if (nearT > farT) std::swap(nearT, farT);
            enter = std::max(enter, nearT);
            leave = std::min(leave, farT);
            if (enter > leave) return false;
        }
        Finished = true;
        return true;
    }
};
}
