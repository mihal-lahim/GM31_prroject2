#pragma once

#include "engineCommon.h"
#include "particleSystem.h"

#include <random>

// SlowActionの各.cppで共有する小さな計算と、仮エフェクトの発生処理です。
namespace SlowUtil
{
	// yaw=0が+Z、yawが増えると+Xへ向く、エンジン共通の向きです。
	inline Vector3 ForwardFromYaw(float yaw) { return { sinf(yaw), 0.0f, cosf(yaw) }; }
	inline Vector3 RightFromYaw(float yaw) { return { cosf(yaw), 0.0f, -sinf(yaw) }; }
	inline float YawFromDirection(const Vector3& direction) { return atan2f(direction.x, direction.z); }

	// -π〜πへ折り返します。
	inline float WrapAngle(float angle)
	{
		angle = fmodf(angle + XM_PI, XM_2PI);
		if (angle < 0.0f)
		{
			angle += XM_2PI;
		}
		return angle - XM_PI;
	}

	inline Vector3 Flatten(const Vector3& value) { return { value.x, 0.0f, value.z }; }

	// 更新順とテスト結果を再現できるよう、乱数の種は固定です。
	inline float RandomRange(float minimum, float maximum)
	{
		static std::mt19937 engine{ 20261112u };
		return std::uniform_real_distribution<float>(minimum, maximum)(engine);
	}

	// 一点から四方へ散る火花です。timeChannel は ParticleSystem::Emit と同じです。敵・弾など世界側の出来事は
	// SlowClock::kWorldTimeChannel を渡し、減速中は宙をゆっくり漂わせます。プレイヤー側は0(実時間)のままです。
	inline void EmitBurst(const Vector3& origin, int count, float speed, float scale, int life, const XMFLOAT4& color,
		int timeChannel = 0)
	{
		for (int i = 0; i < count; ++i)
		{
			Vector3 direction{ RandomRange(-1.0f, 1.0f), RandomRange(-0.3f, 1.0f), RandomRange(-1.0f, 1.0f) };
			direction.normalize();
			ParticleSystem::Emit(origin, direction * RandomRange(speed * 0.4f, speed),
				RandomRange(scale * 0.7f, scale * 1.2f), life, color, -6.0f, 0.95f, timeChannel);
		}
	}

	// 攻撃範囲の扇形を、外周と中ほどの2本の弧で見せます。
	inline void EmitFan(const Vector3& origin, const Vector3& direction, float range, float halfAngle,
		int count, float scale, int life, const XMFLOAT4& color, int timeChannel = 0)
	{
		const float yaw = YawFromDirection(direction);
		for (int i = 0; i < count; ++i)
		{
			const float t = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.5f;
			const Vector3 spoke = ForwardFromYaw(yaw + (t * 2.0f - 1.0f) * halfAngle);
			ParticleSystem::Emit(origin + spoke * range, spoke * 2.0f, scale, life, color, 0.0f, 0.93f, timeChannel);
			ParticleSystem::Emit(origin + spoke * (range * 0.55f), spoke * 5.0f, scale * 0.7f, life, color, 0.0f, 0.93f, timeChannel);
		}
	}
}
