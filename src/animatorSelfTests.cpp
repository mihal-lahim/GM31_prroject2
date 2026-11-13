// Animator(ブレンドツリー・アクション層)の数値テストです。GPUもFBXも使わず、
// 小さな骨格とクリップをメモリ上に作って、合成結果の角度・位置を確かめます。
// ClassroomWalk.exe --self-test で実行し、結果を self-test-animator.log へ書きます。

#include "animator.h"

#include <cmath>
#include <fstream>
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
		options.LockRootMotion = false;
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
		options.LockRootMotion = false;
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
		{ "upper body mask", TestUpperBodyMask },
		{ "root motion lock", TestRootMotionLock },
		{ "combo is continuous", TestComboIsContinuous },
		{ "stop action fades out", TestStopActionFadesOut },
		{ "action keeps running over base transition", TestActionKeepsRunningOverBaseTransition },
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
