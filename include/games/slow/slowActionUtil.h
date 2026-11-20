#pragma once

#include "engineCommon.h"
#include "gameTime.h"
#include "particleSystem.h"

#include <algorithm>
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

	// 当たった瞬間のヒットマークです(桜井流「ヒットマーク」: 明るい所でも暗い所でも一瞬で分かるように)。
	//   1. 暗い核と煙(アルファ合成の暗い色): 明るい石畳や白い光の上でも、当たった所がくっきり沈んで見えます。
	//   2. 白い閃光(加算・HDR): 一瞬だけ強く光り、ブルームで滲みます。時間減速の暗い画面でも目立ちます。
	//   3. 鋭い火花(加算・HDR): 攻撃の向きへ飛び散り、どちらから殴ったかが分かります。
	// どれもヒットストップで止まらない時間(GameTime::kHitStopFreeChannel)で動くので、止まった画面の中で
	// 火花と衝撃だけが弾けます。strength は 1=通常、大きいほど粒が多く大きくなります。sparkColor は火花の色(1を超えてよい)です。
	inline void EmitHitMark(const Vector3& point, const Vector3& direction, float strength, const XMFLOAT4& sparkColor)
	{
		constexpr int channel = Engine::GameTime::kHitStopFreeChannel;
		using Blend = ParticleSystem::Blend;
		const float s = (std::max)(strength, 0.5f);
		Vector3 forward = direction;
		if (forward.length() < 0.01f)
		{
			forward = { 0.0f, 0.0f, 1.0f };
		}
		forward.normalize();

		// 1. 暗い核(その場で少し膨らみながら消える)と、殴られた面から手前へ噴き出して昇る煙です。
		//    攻撃の向き(体の内側)へ流すと体に隠れるので、煙は攻撃と逆向き(殴った側)へ出します。
		const XMFLOAT4 dark{ 0.02f, 0.018f, 0.03f, 0.95f };
		ParticleSystem::Emit(point, { 0.0f, 0.0f, 0.0f }, 1.2f * s, 14, dark, 0.0f, 1.03f, channel, Blend::Alpha);
		const int smoke = static_cast<int>(3.0f + 2.0f * s);
		for (int i = 0; i < smoke; ++i)
		{
			Vector3 drift{ RandomRange(-1.0f, 1.0f), RandomRange(0.0f, 0.8f), RandomRange(-1.0f, 1.0f) };
			drift.normalize();
			ParticleSystem::Emit(point + drift * 0.15f, drift * RandomRange(0.8f, 1.8f) - forward * 0.9f,
				RandomRange(0.45f, 0.65f) * s, 22, { 0.05f, 0.045f, 0.06f, 0.75f }, 0.6f, 1.03f, channel, Blend::Alpha);
		}

		// 2. 白い閃光です。HDR の明るさ(1を超える)でブルームへ渡します。暗い核より小さく短くして、核の縁が残るようにします。
		ParticleSystem::Emit(point - forward * 0.05f, { 0.0f, 0.0f, 0.0f }, 0.5f * s, 4, { 3.2f, 3.0f, 2.6f, 1.0f },
			0.0f, 0.75f, channel);

		// 3. 攻撃の向きへの円錐に飛ぶ鋭い火花です。
		const int sparks = static_cast<int>(8.0f + 6.0f * s);
		for (int i = 0; i < sparks; ++i)
		{
			Vector3 spread{ RandomRange(-0.7f, 0.7f), RandomRange(-0.3f, 0.9f), RandomRange(-0.7f, 0.7f) };
			Vector3 velocity = forward + spread;
			velocity.normalize();
			ParticleSystem::Emit(point, velocity * RandomRange(7.0f, 13.0f) * (0.8f + 0.2f * s), RandomRange(0.09f, 0.16f),
				static_cast<int>(RandomRange(9.0f, 15.0f)), sparkColor, -9.0f, 0.9f, channel);
		}
	}
}
