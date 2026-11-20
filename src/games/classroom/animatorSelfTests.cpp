// Animator(ブレンドツリー・アクション層)の数値テストです。GPUもFBXも使わず、
// 小さな骨格とクリップをメモリ上に作って、合成結果の角度・位置を確かめます。
// ClassroomWalk.exe --self-test で実行し、結果を self-test-animator.log へ書きます。

#include "animator.h"

#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace
{
	using namespace Anim;

	constexpr const char* kRoot = "Root";
	constexpr const char* kHips = "mixamorig:Hips";
	constexpr const char* kSpine = "mixamorig:Spine";
	constexpr const char* kArm = "mixamorig:RightArm";
	constexpr const char* kLeg = "mixamorig:LeftUpLeg";
	constexpr int kHipsIndex = 1;
	constexpr int kArmIndex = 3;
	constexpr int kLegIndex = 4;

	aiQuaternion RotationZ(float angle)
	{
		return aiQuaternion(aiVector3D(0.0f, 0.0f, 1.0f), angle);
	}

	float AngleZ(const BoneTransform& bone)
	{
		return 2.0f * std::atan2(bone.Rotation.z, bone.Rotation.w);
	}

	bool Near(float value, float expected, float tolerance = 1e-3f)
	{
		return std::fabs(value - expected) <= tolerance;
	}

	float PoseDifference(const Pose& a, const Pose& b)
	{
		if (a.size() != b.size()) return 1e9f;
		float result = 0.0f;
		for (size_t bone = 0; bone < a.size(); ++bone)
		{
			const aiMatrix4x4 ma = a[bone].ToMatrix();
			const aiMatrix4x4 mb = b[bone].ToMatrix();
			for (unsigned int row = 0; row < 4; ++row)
				for (unsigned int column = 0; column < 4; ++column)
					result = (std::max)(result, std::fabs(ma[row][column] - mb[row][column]));
		}
		return result;
	}

	// 1本のボーンに、0tickから最終tickまで線形に変わる回転(Z軸)と位置のキーを付けます。
	struct ChannelSpec
	{
		const char* Bone;
		float AngleStart;
		float AngleEnd;
		aiVector3D PositionStart;
		aiVector3D PositionEnd;
		bool HasPosition;
	};

	std::unique_ptr<aiAnimation> MakeClip(double durationTicks, const std::vector<ChannelSpec>& specs)
	{
		auto animation = std::make_unique<aiAnimation>();
		animation->mTicksPerSecond = 30.0;
		animation->mDuration = durationTicks;
		animation->mNumChannels = static_cast<unsigned int>(specs.size());
		animation->mChannels = new aiNodeAnim*[specs.size()];
		for (size_t index = 0; index < specs.size(); ++index)
		{
			const ChannelSpec& spec = specs[index];
			aiNodeAnim* channel = new aiNodeAnim();
			channel->mNodeName.Set(spec.Bone);
			channel->mNumRotationKeys = 2;
			channel->mRotationKeys = new aiQuatKey[2];
			channel->mRotationKeys[0] = aiQuatKey(0.0, RotationZ(spec.AngleStart));
			channel->mRotationKeys[1] = aiQuatKey(durationTicks, RotationZ(spec.AngleEnd));
			if (spec.HasPosition)
			{
				channel->mNumPositionKeys = 2;
				channel->mPositionKeys = new aiVectorKey[2];
				channel->mPositionKeys[0] = aiVectorKey(0.0, spec.PositionStart);
				channel->mPositionKeys[1] = aiVectorKey(durationTicks, spec.PositionEnd);
			}
			animation->mChannels[index] = channel;
		}
		return animation;
	}

	// テスト用の骨格とクリップ一式です。aiAnimationの寿命をAnimatorより長く保ちます。
	struct Fixture
	{
		std::vector<std::unique_ptr<aiAnimation>> Clips;
		Animator Graph;

		Fixture()
		{
			aiMatrix4x4 hips;
			aiMatrix4x4::Translation(aiVector3D(0.0f, 100.0f, 0.0f), hips);
			Skeleton skeleton;
			skeleton.Build({ kRoot, kHips, kSpine, kArm, kLeg }, { -1, 0, 1, 2, 1 },
				{ aiMatrix4x4(), hips, aiMatrix4x4(), aiMatrix4x4(), aiMatrix4x4() });
			Graph.SetSkeleton(skeleton);
			const aiVector3D standing(0.0f, 100.0f, 0.0f);
			Add("Idle", 30.0, { { kLeg, 0.0f, 0.0f, {}, {}, false } });
			// 1秒周期の歩きと0.5秒周期の走り。脚の角度で位相が分かるようにします。
			Add("Walk", 30.0, { { kLeg, 0.0f, 0.8f, {}, {}, false } });
			Add("Run", 15.0, { { kLeg, 0.0f, 1.6f, {}, {}, false } });
			// 1秒の攻撃。腕を振り、腰が前(+Z)へ50・下へ10動きます。
			Add("Attack", 30.0, {
				{ kArm, 0.0f, 1.5f, {}, {}, false },
				{ kHips, 0.0f, 0.0f, standing, aiVector3D(0.0f, 90.0f, 50.0f), true } });
			Add("Attack2", 30.0, { { kArm, 1.0f, 1.0f, {}, {}, false } });
			// 0.5秒で腰が前へ30進む回避。ループさせて周回時の移動量も確かめます。
			Add("Dodge", 15.0, { { kHips, 0.0f, 0.0f, standing, aiVector3D(0.0f, 100.0f, 30.0f), true } });
			// 加算用の被弾リアクション。腕0.2→1.0rad、脚0→0.3rad。
			Add("Flinch", 30.0, {
				{ kArm, 0.2f, 1.0f, {}, {}, false },
				{ kLeg, 0.0f, 0.3f, {}, {}, false } });
		}

		void Add(const char* name, double ticks, const std::vector<ChannelSpec>& specs)
		{
			Clips.push_back(MakeClip(ticks, specs));
			Graph.AddClip(name, Clips.back().get());
		}

		const BoneTransform& Bone(int index) const { return Graph.GetPose()[static_cast<size_t>(index)]; }

		void AddLocomotion(float smoothing)
		{
			BlendSpace1D space;
			space.Parameter = "Speed";
			space.ParameterSmoothing = smoothing;
			BlendClip idle{ "Idle", 0.0f, {}, false };
			BlendClip walk{ "Walk", 1.0f, {}, true };
			BlendClip run{ "Run", 3.0f, {}, true };
			space.Clips = { run, idle, walk }; // 登録順に関係なくしきい値で並べ替えられること
			Graph.AddBlendSpace1D("Locomotion", space);
		}
	};

	bool TestWeights1D()
	{
		const std::vector<float> thresholds{ 0.0f, 2.0f, 5.0f };
		const Weights1D below = ComputeWeights1D(thresholds, -1.0f);
		const Weights1D middle = ComputeWeights1D(thresholds, 1.0f);
		const Weights1D second = ComputeWeights1D(thresholds, 3.5f);
		const Weights1D above = ComputeWeights1D(thresholds, 9.0f);
		return below.Lower == 0 && below.UpperWeight == 0.0f
			&& middle.Lower == 0 && middle.Upper == 1 && Near(middle.UpperWeight, 0.5f)
			&& second.Lower == 1 && second.Upper == 2 && Near(second.UpperWeight, 0.5f)
			&& above.Lower == 2 && above.Upper == 2;
	}

	bool TestBoneMaskInheritance()
	{
		Fixture fixture;
		const auto mask = BuildBoneMask(fixture.Graph.GetSkeleton(), { { kSpine, 0.5f }, { kArm, 1.0f } });
		return mask.size() == 5 && mask[0] == 0.0f && mask[kHipsIndex] == 0.0f
			&& mask[2] == 0.5f && mask[kArmIndex] == 1.0f && mask[kLegIndex] == 0.0f;
	}

	// 歩き(1秒)と走り(0.5秒)を半々で混ぜると、周期0.75秒で両方の位相がそろうこと。
	bool TestBlendSpaceSynchronizesPhase()
	{
		Fixture fixture;
		fixture.AddLocomotion(0.0f);
		fixture.Graph.SetParameter("Speed", 2.0f);
		fixture.Graph.Play("Locomotion", 0.0f);
		fixture.Graph.Advance(0.375f);
		// 位相0.5: 歩きの脚0.4rad、走りの脚0.8rad → 半々で0.6rad
		return Near(fixture.Graph.GetBlendSpaceWeight("Walk"), 0.5f)
			&& Near(fixture.Graph.GetBlendSpaceWeight("Run"), 0.5f)
			&& Near(AngleZ(fixture.Bone(kLegIndex)), 0.6f);
	}

	bool TestBlendSpaceParameterSmoothing()
	{
		Fixture fixture;
		fixture.AddLocomotion(0.1f);
		fixture.Graph.SetParameter("Speed", 0.0f);
		fixture.Graph.Play("Locomotion", 0.0f);
		fixture.Graph.Advance(0.0f);
		fixture.Graph.SetParameter("Speed", 3.0f);
		fixture.Graph.Advance(0.1f);
		const float expected = 3.0f * (1.0f - std::exp(-1.0f));
		const bool smoothed = Near(fixture.Graph.GetSmoothedParameter(), expected);
		for (int frame = 0; frame < 120; ++frame) fixture.Graph.Advance(1.0f / 60.0f);
		return smoothed && Near(fixture.Graph.GetSmoothedParameter(), 3.0f)
			&& Near(fixture.Graph.GetBlendSpaceWeight("Run"), 1.0f);
	}

	bool TestBaseCrossfadeIsContinuous()
	{
		Fixture fixture;
		fixture.Graph.Play("Walk", 0.0f);
		fixture.Graph.Advance(0.5f);
		const Pose before = fixture.Graph.GetPose();
		fixture.Graph.Play("Run", 0.2f);
		fixture.Graph.Advance(0.0f);
		const bool continuous = PoseDifference(before, fixture.Graph.GetPose()) < 1e-4f;
		fixture.Graph.Advance(0.1f);
		const bool moving = PoseDifference(before, fixture.Graph.GetPose()) > 1e-3f;
		fixture.Graph.Advance(0.15f); // 合計0.25秒で切り替え完了。走りの位相0.5 → 0.8rad
		return continuous && moving && Near(AngleZ(fixture.Bone(kLegIndex)), 0.8f);
	}

	bool TestLoopSeamIsContinuous()
	{
		Fixture fixture;
		fixture.Graph.Play("Walk", 0.0f);
		fixture.Graph.Advance(1.0f - 0.008f);
		const Pose end = fixture.Graph.GetPose();
		fixture.Graph.Advance(0.016f);
		return PoseDifference(end, fixture.Graph.GetPose()) < 1e-3f;
	}

	bool TestActionFadesInAndOut()
	{
		Fixture fixture;
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions options;
		options.FadeIn = 0.1f;
		options.FadeOut = 0.2f;
		options.RootMotionMode = RootMotion::Keep;
		fixture.Graph.PlayAction("Attack", options);
		fixture.Graph.Advance(0.0f);
		const bool startsAtBase = Near(AngleZ(fixture.Bone(kArmIndex)), 0.0f);
		fixture.Graph.Advance(0.05f);
		// フェード半分(smoothstep 0.5)で、攻撃の腕0.075radの半分
		const bool halfWay = Near(fixture.Graph.GetActionWeight(), 0.5f)
			&& Near(AngleZ(fixture.Bone(kArmIndex)), 0.0375f);
		fixture.Graph.Advance(0.45f);
		const bool full = Near(fixture.Graph.GetActionWeight(), 1.0f)
			&& Near(AngleZ(fixture.Bone(kArmIndex)), 0.75f)
			&& Near(fixture.Graph.GetActionTime(), 0.5f);
		fixture.Graph.Advance(0.4f); // 残り0.1秒 → フェードアウト半分
		const bool fadingOut = Near(fixture.Graph.GetActionWeight(), 0.5f)
			&& fixture.Graph.IsActionActive();
		fixture.Graph.Advance(0.1f);
		const bool finished = !fixture.Graph.IsActionActive()
			&& Near(AngleZ(fixture.Bone(kArmIndex)), 0.0f)
			&& fixture.Graph.ActionPassed(1.0f);
		fixture.Graph.Advance(0.1f);
		return startsAtBase && halfWay && full && fadingOut && finished && !fixture.Graph.ActionPassed(1.0f);
	}

	// 再生中に倍率を変えても位相は飛ばず、その後の進みが新しい倍率で決まること。
	bool TestActionSpeedChangesRateKeepsPhase()
	{
		Fixture fixture;
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions options;
		options.Playback = { 1.0f, 0.0f, 1.0f, false };
		options.FadeIn = 0.0f;
		options.RootMotionMode = RootMotion::Keep;
		fixture.Graph.PlayAction("Attack", options);
		// 1秒の攻撃を等速で0.25秒: 位相0.25。
		fixture.Graph.Advance(0.25f);
		const bool atQuarter = Near(fixture.Graph.GetActionTime(), 0.25f);
		// 倍率を2倍にしても、その瞬間の位相は変わりません。
		fixture.Graph.SetActionSpeed(2.0f);
		fixture.Graph.Advance(0.0f);
		const bool sameInstant = Near(fixture.Graph.GetActionTime(), 0.25f);
		// 2倍で0.25秒進むと、位相は0.25 + 0.5 = 0.75。
		fixture.Graph.Advance(0.25f);
		const bool doubled = Near(fixture.Graph.GetActionTime(), 0.75f);
		// 0.5倍に落として0.25秒進むと、位相は0.75 + 0.125 = 0.875。
		fixture.Graph.SetActionSpeed(0.5f);
		fixture.Graph.Advance(0.25f);
		const bool slowed = Near(fixture.Graph.GetActionTime(), 0.875f);
		// 動いていない層への指定は何もしません。
		fixture.Graph.StopAction(0, 0.0f);
		fixture.Graph.SetActionSpeed(4.0f);
		return atQuarter && sameInstant && doubled && slowed && !fixture.Graph.IsActionActive();
	}

	bool TestActionPassedFiresOnce()
	{
		Fixture fixture;
		fixture.Graph.Play("Idle", 0.0f);
		fixture.Graph.PlayAction("Attack");
		int fired = 0;
		for (int frame = 0; frame < 90; ++frame)
		{
			fixture.Graph.Advance(1.0f / 60.0f);
			fixture.Graph.Advance(0.0f); // 時間0の再評価で判定が消えたり増えたりしないこと
			fired += fixture.Graph.ActionPassed(0.4f) ? 1 : 0;
		}
		return fired == 1;
	}

	// 上半身マスク: 腕は攻撃、脚は歩き。強さ0にすると全身が攻撃(脚は攻撃側の0rad)になること。
	bool TestUpperBodyMask()
	{
		Fixture fixture;
		fixture.Graph.DefineMask("UpperBody", { { kSpine, 1.0f } });
		fixture.Graph.Play("Walk", 0.0f);
		ActionOptions options;
		options.Mask = "UpperBody";
		options.FadeIn = 0.0f;
		options.Playback.Loop = true;
		fixture.Graph.PlayAction("Attack", options);
		fixture.Graph.Advance(0.5f);
		const bool masked = Near(AngleZ(fixture.Bone(kArmIndex)), 0.75f)
			&& Near(AngleZ(fixture.Bone(kLegIndex)), 0.4f)
			&& Near(fixture.Bone(kHipsIndex).Position.z, 0.0f);
		fixture.Graph.SetActionMaskStrength(0.0f, 0, 0.0f);
		fixture.Graph.Advance(0.0f);
		const bool fullBody = Near(AngleZ(fixture.Bone(kArmIndex)), 0.75f)
			&& Near(AngleZ(fixture.Bone(kLegIndex)), 0.0f);
		return masked && fullBody;
	}

	bool TestRootMotionLock()
	{
		Fixture fixture;
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions options;
		options.FadeIn = 0.0f;
		fixture.Graph.PlayAction("Attack", options);
		fixture.Graph.Advance(0.5f);
		const aiVector3D locked = fixture.Bone(kHipsIndex).Position;
		Fixture unlocked;
		unlocked.Graph.Play("Idle", 0.0f);
		options.RootMotionMode = RootMotion::Keep;
		unlocked.Graph.PlayAction("Attack", options);
		unlocked.Graph.Advance(0.5f);
		const aiVector3D moved = unlocked.Bone(kHipsIndex).Position;
		// 水平(Z)の踏み込みだけを消し、上下(Y)の動きは残します。
		return Near(locked.z, 0.0f) && Near(locked.y, 95.0f)
			&& Near(moved.z, 25.0f) && Near(moved.y, 95.0f);
	}

	bool TestComboIsContinuous()
	{
		Fixture fixture;
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions options;
		options.FadeIn = 0.1f;
		fixture.Graph.PlayAction("Attack", options);
		fixture.Graph.Advance(0.5f);
		const Pose before = fixture.Graph.GetPose();
		fixture.Graph.PlayAction("Attack2", options);
		fixture.Graph.Advance(0.0f);
		const bool continuous = PoseDifference(before, fixture.Graph.GetPose()) < 1e-4f
			&& Near(fixture.Graph.GetActionWeight(), 1.0f);
		fixture.Graph.Advance(0.1f);
		return continuous && fixture.Graph.GetActionName() == "Attack2"
			&& Near(AngleZ(fixture.Bone(kArmIndex)), 1.0f);
	}

	bool TestStopActionFadesOut()
	{
		Fixture fixture;
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions options;
		options.FadeIn = 0.0f;
		options.Playback.Loop = true;
		fixture.Graph.PlayAction("Attack2", options);
		fixture.Graph.Advance(0.1f);
		fixture.Graph.StopAction(0, 0.2f);
		fixture.Graph.Advance(0.1f);
		const bool half = Near(fixture.Graph.GetActionWeight(), 0.5f)
			&& Near(AngleZ(fixture.Bone(kArmIndex)), 0.5f);
		fixture.Graph.Advance(0.1f);
		return half && !fixture.Graph.IsActionActive() && Near(AngleZ(fixture.Bone(kArmIndex)), 0.0f);
	}

	bool TestActionKeepsRunningOverBaseTransition()
	{
		Fixture fixture;
		fixture.Graph.DefineMask("UpperBody", { { kSpine, 1.0f } });
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions options;
		options.Mask = "UpperBody";
		options.FadeIn = 0.0f;
		fixture.Graph.PlayAction("Attack", options);
		fixture.Graph.Advance(0.2f);
		// 攻撃中に走り出しても、攻撃は途切れず進み続けること
		fixture.Graph.Play("Walk", 0.2f);
		fixture.Graph.Advance(0.3f);
		return fixture.Graph.IsActionActive() && Near(fixture.Graph.GetActionTime(), 0.5f)
			&& Near(AngleZ(fixture.Bone(kArmIndex)), 0.75f)
			&& Near(AngleZ(fixture.Bone(kLegIndex)), 0.24f);
	}

	bool TestRootMotionExtract()
	{
		Fixture fixture;
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions options;
		options.FadeIn = 0.0f;
		options.RootMotionMode = RootMotion::Extract;
		fixture.Graph.PlayAction("Attack", options);
		float travelled = 0.0f;
		for (int frame = 0; frame < 10; ++frame)
		{
			fixture.Graph.Advance(0.05f);
			fixture.Graph.Advance(0.0f); // 時間0の再評価で移動量が変わらないこと
			travelled += fixture.Graph.GetRootMotion().z;
		}
		// 半分まで再生 → 前へ25。姿勢の腰は動かさず、上下(95)だけ残ります。
		const bool half = Near(travelled, 25.0f) && Near(fixture.Bone(kHipsIndex).Position.z, 0.0f)
			&& Near(fixture.Bone(kHipsIndex).Position.y, 95.0f)
			&& Near(fixture.Graph.GetRootMotion().x, 0.0f) && Near(fixture.Graph.GetRootMotion().y, 0.0f);
		for (int frame = 0; frame < 20; ++frame)
		{
			fixture.Graph.Advance(0.05f);
			travelled += fixture.Graph.GetRootMotion().z;
		}
		// 最後の0.2秒はフェードアウトの重み(smoothstep 0.84, 0.5, 0.16, 0)ぶん減るので 50 - 6.25。
		return half && !fixture.Graph.IsActionActive() && Near(travelled, 43.75f)
			&& fixture.Graph.GetRootMotion().z == 0.0f;
	}

	bool TestRootMotionAcrossLoop()
	{
		Fixture fixture;
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions options;
		options.FadeIn = 0.0f;
		options.RootMotionMode = RootMotion::Extract;
		options.Playback.Loop = true;
		fixture.Graph.PlayAction("Dodge", options);
		fixture.Graph.Advance(0.3f);
		float travelled = fixture.Graph.GetRootMotion().z;
		fixture.Graph.Advance(0.3f); // 1周(0.5秒・30)を越える
		travelled += fixture.Graph.GetRootMotion().z;
		return Near(travelled, 36.0f);
	}

	// ベース層で前へ進むクリップを、InPlaceでその場に固定する(上下の動きは残す)。
	bool TestBaseClipInPlace()
	{
		Fixture moving;
		moving.Graph.Play("Dodge", 0.0f);
		moving.Graph.Advance(0.1f);
		const aiVector3D moved = moving.Bone(kHipsIndex).Position;
		Fixture fixture;
		fixture.Graph.SetClipInPlace("Dodge", true);
		fixture.Graph.Play("Dodge", 0.0f);
		fixture.Graph.Advance(0.1f);
		const aiVector3D locked = fixture.Bone(kHipsIndex).Position;
		return moved.z > 1.0f && Near(locked.z, 0.0f) && Near(locked.y, moved.y);
	}

	bool TestRootMotionIgnoresMaskedRoot()
	{
		Fixture fixture;
		fixture.Graph.DefineMask("UpperBody", { { kSpine, 1.0f } });
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions options;
		options.FadeIn = 0.0f;
		options.Mask = "UpperBody";
		options.RootMotionMode = RootMotion::Extract;
		fixture.Graph.PlayAction("Attack", options);
		fixture.Graph.Advance(0.2f);
		// 腰がマスクの外なら、腰の移動も使いません(上半身だけの攻撃で滑らない)。
		return Near(fixture.Graph.GetRootMotion().z, 0.0f);
	}

	bool TestAdditiveAddsDifference()
	{
		Fixture fixture;
		fixture.Graph.Play("Walk", 0.0f);
		ActionOptions options;
		options.FadeIn = 0.0f;
		options.Blend = LayerBlend::Additive;
		options.Layer = 1;
		fixture.Graph.PlayAction("Flinch", options);
		fixture.Graph.Advance(0.5f);
		// 基準(先頭)からの差分: 腕+0.4、脚+0.15。歩きの脚0.4に足されて0.55。
		const bool full = Near(AngleZ(fixture.Bone(kArmIndex)), 0.4f)
			&& Near(AngleZ(fixture.Bone(kLegIndex)), 0.55f);
		fixture.Graph.SetLayerWeight(0.5f, 1);
		fixture.Graph.Advance(0.0f);
		const bool half = Near(AngleZ(fixture.Bone(kArmIndex)), 0.2f)
			&& Near(AngleZ(fixture.Bone(kLegIndex)), 0.475f)
			&& Near(fixture.Graph.GetActionWeight(1), 0.5f);
		return full && half;
	}

	bool TestAdditiveOverAttack()
	{
		Fixture fixture;
		fixture.Graph.Play("Idle", 0.0f);
		ActionOptions attack;
		attack.FadeIn = 0.0f;
		fixture.Graph.PlayAction("Attack", attack);
		ActionOptions flinch;
		flinch.FadeIn = 0.0f;
		flinch.Blend = LayerBlend::Additive;
		flinch.AdditiveReference = 1.0f; // 最後のフレームを基準にする
		flinch.Layer = 1;
		fixture.Graph.PlayAction("Flinch", flinch);
		fixture.Graph.Advance(0.5f);
		// 攻撃の腕0.75 + (0.6 - 1.0) = 0.35。攻撃を止めずに被弾の揺れだけ足せること。
		return Near(AngleZ(fixture.Bone(kArmIndex)), 0.35f)
			&& fixture.Graph.IsActionActive(0) && fixture.Graph.IsActionActive(1);
	}

	// 傾いた姿勢だけのクリップ(Attack2: 腕1.0rad固定)を、Idleを基準に重ねる。
	bool TestAdditiveReferenceClip()
	{
		Fixture fixture;
		fixture.Graph.Play("Walk", 0.0f);
		ActionOptions lean;
		lean.FadeIn = 0.0f;
		lean.Blend = LayerBlend::Additive;
		lean.AdditiveReferenceClip = "Idle";
		lean.Playback.Loop = true;
		lean.Layer = 2;
		fixture.Graph.PlayAction("Attack2", lean);
		fixture.Graph.SetLayerWeight(0.25f, 2);
		fixture.Graph.Advance(0.5f);
		return Near(AngleZ(fixture.Bone(kArmIndex)), 0.25f) && Near(AngleZ(fixture.Bone(kLegIndex)), 0.4f);
	}

	bool IsFinitePose(const Pose& pose)
	{
		if (pose.empty()) return false;
		for (const BoneTransform& bone : pose)
		{
			const float values[] = {
				bone.Position.x, bone.Position.y, bone.Position.z,
				bone.Rotation.w, bone.Rotation.x, bone.Rotation.y, bone.Rotation.z,
				bone.Scale.x, bone.Scale.y, bone.Scale.z };
			for (const float value : values)
				if (!std::isfinite(value)) return false;
		}
		return true;
	}

	// 長さがほぼ0のクリップでも、tick位置はクリップの中に収まります(以前は clamp の上限が下限を下回っていました)。
	bool TestPhaseToTicksShortClip()
	{
		bool ok = true;
		for (const double duration : { 0.0005, 0.0 })
		{
			ClipBinding binding;
			binding.TicksPerSecond = 30.0;
			binding.DurationTicks = duration;
			for (const bool loop : { false, true })
			{
				const PlaybackRange range{ 1.0f, 0.0f, 1.0f, loop };
				for (const double phase : { 0.0, 0.5, 1.0, 2.5, -1.0, std::nan("") })
				{
					const double ticks = PhaseToTicks(binding, range, phase);
					ok &= std::isfinite(ticks) && ticks >= 0.0 && ticks <= duration;
				}
			}
		}

		// Animatorから非ループで再生しても落ちず、姿勢は有限のままです。
		Fixture fixture;
		fixture.Add("Blink", 0.0005, { { kArm, 0.0f, 1.0f, {}, {}, false } });
		fixture.Graph.SetPlayback({ 1.0f, 0.0f, 1.0f, false });
		fixture.Graph.Play("Blink", 0.0f);
		for (int frame = 0; frame < 5; ++frame) fixture.Graph.Advance(1.0f / 60.0f);
		ok &= IsFinitePose(fixture.Graph.GetPose());
		ActionOptions once;
		once.Playback = { 1.0f, 0.0f, 1.0f, false };
		fixture.Graph.PlayAction("Blink", once);
		for (int frame = 0; frame < 5; ++frame) fixture.Graph.Advance(1.0f / 60.0f);
		return ok && IsFinitePose(fixture.Graph.GetPose());
	}

	// 再生中のブレンドツリーをクリップ数の違うものへ差し替えても、位相の配列が新しい数に合わせられます。
	bool TestBlendSpaceReplacedWhilePlaying()
	{
		Fixture fixture;
		fixture.AddLocomotion(0.0f);
		fixture.Graph.Play("Locomotion", 0.0f);
		fixture.Graph.SetParameter("Speed", 2.0f);
		fixture.Graph.Advance(0.1f);

		// 3本 → 5本。後ろの2本は同期しない(クリップごとの位相を使う)クリップです。
		BlendSpace1D wider;
		wider.Parameter = "Speed";
		wider.ParameterSmoothing = 0.0f;
		wider.Clips = {
			{ "Idle", 0.0f, {}, false },
			{ "Walk", 1.0f, {}, true },
			{ "Run", 2.0f, {}, true },
			{ "Attack2", 3.0f, {}, false },
			{ "Flinch", 4.0f, {}, false } };
		fixture.Graph.AddBlendSpace1D("Locomotion", wider);
		fixture.Graph.SetParameter("Speed", 3.5f);
		bool ok = true;
		for (int frame = 0; frame < 10; ++frame)
		{
			ok &= fixture.Graph.Advance(0.05f);
		}
		return ok && IsFinitePose(fixture.Graph.GetPose())
			&& fixture.Graph.GetCurrentName() == "Locomotion"
			&& Near(fixture.Graph.GetBlendSpaceWeight("Attack2"), 0.5f)
			&& Near(fixture.Graph.GetBlendSpaceWeight("Flinch"), 0.5f);
	}

	// NaN・無限大をどの入口へ渡しても、姿勢は有限のままです。
	bool TestNonFiniteInputsKeepPoseFinite()
	{
		const float nan = std::nanf("");
		const float inf = std::numeric_limits<float>::infinity();
		bool ok = true;

		const PlaybackRange sanitized = SanitizeRange({ nan, inf, nan, true });
		ok &= sanitized.Speed == 1.0f && sanitized.Start == 0.0f && sanitized.End == 1.0f;

		Fixture fixture;
		fixture.AddLocomotion(0.1f);
		fixture.Graph.Play("Idle", 0.0f);
		fixture.Graph.Advance(0.1f);
		ok &= fixture.Graph.Play("Locomotion", nan);
		fixture.Graph.SetParameter("Speed", nan);
		fixture.Graph.SetParameter("Speed", 1.5f);
		fixture.Graph.SetParameter("Speed", inf);
		ok &= fixture.Graph.GetParameter("Speed") == 1.5f;
		fixture.Graph.SetPlayback({ nan, inf, nan, true });
		fixture.Graph.Advance(nan);
		fixture.Graph.Advance(inf);
		fixture.Graph.Advance(-inf);

		ActionOptions attack;
		attack.FadeIn = nan;
		attack.FadeOut = inf;
		attack.Playback = { inf, nan, inf, false };
		ok &= fixture.Graph.PlayAction("Attack", attack);
		ActionOptions flinch;
		flinch.Blend = LayerBlend::Additive;
		flinch.AdditiveReference = nan;
		flinch.FadeIn = -inf;
		flinch.Layer = 1;
		ok &= fixture.Graph.PlayAction("Flinch", flinch);
		fixture.Graph.SetLayerWeight(nan, 1);
		fixture.Graph.SetActionSpeed(inf, 0);
		fixture.Graph.SetActionSpeed(nan, 0);
		fixture.Graph.SetActionMaskStrength(nan, 0, inf);
		fixture.Graph.SetActionMaskStrength(0.5f, 0, nan);
		for (int frame = 0; frame < 30; ++frame)
		{
			fixture.Graph.Advance(1.0f / 60.0f);
			ok &= IsFinitePose(fixture.Graph.GetPose());
		}
		ok &= fixture.Graph.GetLayerWeight(1) == 1.0f;

		// 無限大のフェードアウトは再生時のFadeOut(ここでは既定の0.2秒)に置き換わるので、ちゃんと終わります。
		fixture.Graph.StopAction(0, inf);
		fixture.Graph.StopAction(1, nan);
		for (int frame = 0; frame < 60; ++frame)
		{
			fixture.Graph.Advance(1.0f / 60.0f);
			ok &= IsFinitePose(fixture.Graph.GetPose());
		}
		return ok && !fixture.Graph.IsActionActive(0) && !fixture.Graph.IsActionActive(1);
	}

	// 閾値や平滑化が NaN・無限大のブレンドツリーでも、並べ替えが壊れず(未定義動作にならず)姿勢は有限です。
	bool TestBlendSpaceNonFiniteThresholds()
	{
		const float nan = std::nanf("");
		const float inf = std::numeric_limits<float>::infinity();
		Fixture fixture;
		BlendSpace1D space;
		space.Parameter = "Speed";
		space.ParameterSmoothing = nan;
		space.Clips = {
			{ "Run", 3.0f, {}, true },
			{ "Idle", nan, {}, false },
			{ "Walk", inf, {}, true },
			{ "Attack2", -inf, {}, false } };
		fixture.Graph.AddBlendSpace1D("Locomotion", space);
		bool ok = fixture.Graph.Play("Locomotion", 0.0f);
		fixture.Graph.SetParameter("Speed", 3.0f);
		for (int frame = 0; frame < 30; ++frame)
		{
			ok &= fixture.Graph.Advance(1.0f / 60.0f);
			ok &= IsFinitePose(fixture.Graph.GetPose());
		}
		// 平滑化は NaN → 0(平滑化なし)なのですぐに 3 になり、閾値 3 の走りだけが選ばれます。
		ok &= Near(fixture.Graph.GetSmoothedParameter(), 3.0f)
			&& Near(fixture.Graph.GetBlendSpaceWeight("Run"), 1.0f);

		// 負の平滑化も 0 として扱います。
		space.ParameterSmoothing = -1.0f;
		fixture.Graph.AddBlendSpace1D("Locomotion", space);
		fixture.Graph.SetParameter("Speed", 0.0f);
		ok &= fixture.Graph.Advance(1.0f / 60.0f);
		return ok && Near(fixture.Graph.GetSmoothedParameter(), 0.0f) && IsFinitePose(fixture.Graph.GetPose());
	}

	// NaN・無限大のマスクの重みは無視し(親から受け継ぐ)、姿勢は有限のままです。
	bool TestBoneMaskIgnoresNonFiniteWeights()
	{
		const float nan = std::nanf("");
		const float inf = std::numeric_limits<float>::infinity();
		Fixture fixture;
		const auto mask = BuildBoneMask(fixture.Graph.GetSkeleton(),
			{ { kHips, 0.25f }, { kSpine, nan }, { kArm, 1.0f }, { kLeg, inf } });
		bool ok = mask.size() == 5 && mask[kHipsIndex] == 0.25f && mask[2] == 0.25f
			&& mask[kArmIndex] == 1.0f && mask[kLegIndex] == 0.25f;

		fixture.Graph.DefineMask("Broken", { { kSpine, nan }, { kArm, inf }, { kLeg, 0.5f } });
		fixture.Graph.Play("Walk", 0.0f);
		ActionOptions options;
		options.Mask = "Broken";
		options.FadeIn = 0.0f;
		ok &= fixture.Graph.PlayAction("Attack", options);
		for (int frame = 0; frame < 30; ++frame)
		{
			fixture.Graph.Advance(1.0f / 60.0f);
			ok &= IsFinitePose(fixture.Graph.GetPose());
		}
		return ok;
	}

	// クリップを登録して体格を合わせる指定をした後で腰のボーンを指定しても、腰の高さの比が決め直されます。
	bool TestSetRootBoneRefreshesKeepProportions()
	{
		constexpr const char* kPelvis = "Pelvis";
		aiMatrix4x4 pelvis;
		aiMatrix4x4::Translation(aiVector3D(0.0f, 100.0f, 0.0f), pelvis);
		Skeleton skeleton;
		skeleton.Build({ kRoot, kPelvis }, { -1, 0 }, { aiMatrix4x4(), pelvis });
		Animator graph;
		graph.SetSkeleton(skeleton);

		// 2倍の背丈のキャラクター用のクリップです。腰は高さ200から1秒で260まで上がります。
		std::unique_ptr<aiAnimation> clip = MakeClip(30.0, {
			{ kPelvis, 0.0f, 0.0f, aiVector3D(0.0f, 200.0f, 0.0f), aiVector3D(0.0f, 260.0f, 0.0f), true } });
		graph.AddClip("Tall", clip.get());
		graph.SetClipKeepProportions("Tall", true);
		graph.Play("Tall", 0.0f);
		graph.Advance(0.0f);
		// 腰のボーンがまだ分からない("mixamorig:Hips"が無い)ので、腰もバインド姿勢の高さのままです。
		const bool beforeIsBind = Near(graph.GetPose()[1].Position.y, 100.0f);

		graph.SetRootBone(kPelvis);
		graph.Advance(0.5f);
		// 腰の動きを使い、先頭の高さの比 100/200 を掛けます。0.5秒時点のクリップの高さ230 → 115。
		// (比が決め直されないと、腰のボーンが分からないままなので100のままです。)
		return beforeIsBind && Near(graph.GetPose()[1].Position.y, 115.0f);
	}
}

bool RunAnimatorSelfTests()
{
	struct TestCase
	{
		const char* Name;
		bool (*Run)();
	};
	const TestCase tests[] = {
		{ "1D blend weights", TestWeights1D },
		{ "bone mask inheritance", TestBoneMaskInheritance },
		{ "blend space synchronizes phase", TestBlendSpaceSynchronizesPhase },
		{ "blend space parameter smoothing", TestBlendSpaceParameterSmoothing },
		{ "base crossfade is continuous", TestBaseCrossfadeIsContinuous },
		{ "loop seam is continuous", TestLoopSeamIsContinuous },
		{ "action fades in and out", TestActionFadesInAndOut },
		{ "action passed fires once", TestActionPassedFiresOnce },
		{ "action speed changes rate keeping phase", TestActionSpeedChangesRateKeepsPhase },
		{ "upper body mask", TestUpperBodyMask },
		{ "root motion lock", TestRootMotionLock },
		{ "combo is continuous", TestComboIsContinuous },
		{ "stop action fades out", TestStopActionFadesOut },
		{ "action keeps running over base transition", TestActionKeepsRunningOverBaseTransition },
		{ "root motion extract", TestRootMotionExtract },
		{ "root motion across loop", TestRootMotionAcrossLoop },
		{ "root motion ignores masked root", TestRootMotionIgnoresMaskedRoot },
		{ "base clip in place", TestBaseClipInPlace },
		{ "additive adds difference", TestAdditiveAddsDifference },
		{ "additive over attack", TestAdditiveOverAttack },
		{ "additive reference clip", TestAdditiveReferenceClip },
		{ "phase to ticks short clip", TestPhaseToTicksShortClip },
		{ "blend space replaced while playing", TestBlendSpaceReplacedWhilePlaying },
		{ "non-finite inputs keep pose finite", TestNonFiniteInputsKeepPoseFinite },
		{ "set root bone refreshes keep proportions", TestSetRootBoneRefreshesKeepProportions },
		{ "blend space non-finite thresholds", TestBlendSpaceNonFiniteThresholds },
		{ "bone mask ignores non-finite weights", TestBoneMaskIgnoresNonFiniteWeights },
	};
	std::ofstream log("self-test-animator.log");
	bool passed = true;
	for (const TestCase& test : tests)
	{
		const bool result = test.Run();
		log << (result ? "PASS " : "FAIL ") << test.Name << '\n';
		passed &= result;
	}
	return passed;
}
