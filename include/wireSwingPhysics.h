#pragma once
#include "Vector3.h"
#include "wireGameplayTuning.h"
#include <algorithm>

namespace WireSwing
{
inline Vector3 ClampSpeed(Vector3 velocity, float limit)
{
    const float speed = velocity.length();
    if (speed > limit && speed > 0.0001f) velocity *= limit / speed;
    return velocity;
}
inline void Integrate(Vector3& velocity, Vector3 toAnchor, float ropeLength, float dt,
    const WireGrappleTuning& tuning, float& tangentialSpeed, float& tension)
{
    const float distance = toAnchor.length();
    if (distance <= tuning.StopDistance) { tangentialSpeed = 0; tension = 0; return; }
    toAnchor /= distance;
    const Vector3 away = -toAnchor;
    const float radialSpeed = Vector3::dot(velocity,away);
    tangentialSpeed = (velocity-away*radialSpeed).length();
    const float stretch = std::max(0.0f,distance-ropeLength);
    const float centripetal = distance >= ropeLength ? tangentialSpeed*tangentialSpeed/std::max(distance,1.0f) : 0.0f;
    tension = stretch*tuning.ConstraintStrength + std::max(0.0f,radialSpeed)*tuning.RadialDamping + centripetal;
    if (distance >= ropeLength && radialSpeed > 0)
        velocity -= away * std::min(radialSpeed,radialSpeed*tuning.RadialDamping*dt);
    velocity += toAnchor*((tuning.PullAcceleration+stretch*tuning.ConstraintStrength+centripetal)*dt);
    velocity = ClampSpeed(velocity,tuning.RewireMaxSpeed);
}
// 伸びないロープとして扱う場合の拘束です。牽引もバネも使いません。
// 今の速度で進んだ先を予測し、支点からropeLengthより遠くなる分だけ支点側へ戻して、
// 戻した位置へ着くように速度を作り直します。たるんでいる間は速度を変えません。
inline void IntegrateRigid(Vector3& velocity, Vector3 toPivot, float ropeLength, float dt,
    const WireGrappleTuning& tuning, float& tangentialSpeed, float& tension)
{
    tension = 0;
    const float distance = toPivot.length();
    if (distance <= 0.0001f || dt <= 0.0f) { tangentialSpeed = 0; return; }
    const Vector3 away = toPivot / -distance;
    tangentialSpeed = (velocity-away*Vector3::dot(velocity,away)).length();
    const Vector3 predicted = velocity*dt - toPivot;
    const float predictedDistance = predicted.length();
    if (predictedDistance > ropeLength && predictedDistance > 0.0001f)
    {
        const Vector3 before = velocity;
        velocity = (predicted*(ropeLength/predictedDistance) + toPivot) / dt;
        tension = (velocity-before).length() / dt;
    }
    velocity = ClampSpeed(velocity,tuning.RewireMaxSpeed);
}
inline Vector3 Release(Vector3 velocity,const WireGrappleTuning& tuning)
{
    const float speed = velocity.length();
    if (speed <= 0.0001f) return velocity;
    const float timing = std::clamp(velocity.y/22.0f,0.0f,1.0f);
    velocity += velocity/speed*(tuning.ReleaseBoost*timing);
    velocity.y += tuning.ReleaseUpBoost*timing;
    // 通常上限より速く落下していても、解除だけで既存の慣性を削りません。
    return ClampSpeed(velocity,std::max(speed,tuning.ReleaseMaxSpeed));
}
}
