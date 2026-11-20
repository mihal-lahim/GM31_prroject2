#include "animator.h"

#include <algorithm>
#include <cmath>

namespace Anim
{
	namespace
	{
		constexpr int kMaxActionLayers = 8;

		// keys[index].mTime <= time < keys[index + 1].mTime となるindexを返します。
		// assimpのキーは時刻昇順なので二分探索できます。末尾を越えた場合はkeyCount - 1です。
		template<typename Key>
		unsigned int FindKeyIndex(double time, unsigned int keyCount, const Key* keys)
		{
			if (keyCount < 2 || keys == nullptr) return 0;
			const Key* next = std::upper_bound(keys + 1, keys + keyCount, time,
				[](double value, const Key& key) { return value < key.mTime; });
			return static_cast<unsigned int>(next - keys) - 1;
		}

		template<typename Key>
		float KeyFactor(double time, const Key& current, const Key& next)
		{
			const double keyDuration = next.mTime - current.mTime;
			return keyDuration > 0.0
				? std::clamp(static_cast<float>((time - current.mTime) / keyDuration), 0.0f, 1.0f)
				: 0.0f;
		}

		aiVector3D SampleVectorKeys(double time, unsigned int keyCount, const aiVectorKey* keys,
			const aiVector3D& fallback)
		{
			if (keyCount == 0 || keys == nullptr) return fallback;
			if (keyCount == 1) return keys[0].mValue;
			const unsigned int index = FindKeyIndex(time, keyCount, keys);
			if (index + 1 >= keyCount) return keys[index].mValue;
			const float factor = KeyFactor(time, keys[index], keys[index + 1]);
			return keys[index].mValue + (keys[index + 1].mValue - keys[index].mValue) * factor;
		}

		aiQuaternion SampleQuaternionKeys(double time, unsigned int keyCount, const aiQuatKey* keys,
			const aiQuaternion& fallback)
		{
			if (keyCount == 0 || keys == nullptr) return fallback;
			if (keyCount == 1) return keys[0].mValue;
			const unsigned int index = FindKeyIndex(time, keyCount, keys);
			if (index + 1 >= keyCount) return keys[index].mValue;
			aiQuaternion result;
			aiQuaternion::Interpolate(result, keys[index].mValue, keys[index + 1].mValue,
				KeyFactor(time, keys[index], keys[index + 1]));
			return result.Normalize();
		}

		BoneTransform Lerp(const BoneTransform& from, const BoneTransform& to, float weight)
		{
			BoneTransform result;
			aiQuaternion::Interpolate(result.Rotation, from.Rotation, to.Rotation, weight);
			result.Rotation.Normalize();
			result.Scale = from.Scale + (to.Scale - from.Scale) * weight;
			result.Position = from.Position + (to.Position - from.Position) * weight;
			return result;
		}

		// 時定数smoothingで目標へ近づけます。フレームレートに依存しない指数平滑です。
		float Approach(float current, float target, float step, float smoothing)
		{
			if (smoothing <= 0.0f) return target;
			if (step <= 0.0f) return current;
			return current + (target - current) * (1.0f - std::exp(-step / smoothing));
		}
	}

	BoneTransform BoneTransform::FromMatrix(const aiMatrix4x4& matrix)
	{
		BoneTransform result;
		matrix.Decompose(result.Scale, result.Rotation, result.Position);
		return result;
	}

	void Skeleton::Clear()
	{
		m_Names.clear();
		m_Parents.clear();
		m_BindPose.clear();
		m_BindLocal.clear();
		m_BindGlobal.clear();
		m_Index.clear();
	}

	void Skeleton::Build(const aiNode* root)
	{
		Clear();
		if (root == nullptr) return;
		// 親が子より前に並ぶ深さ優先の順序です。マスクの継承をこの順で1回走査して決めます。
		std::vector<std::string> names;
		std::vector<int> parents;
		std::vector<aiMatrix4x4> bindLocal;
		struct Item { const aiNode* Node; int Parent; };
		std::vector<Item> stack{ { root, -1 } };
		while (!stack.empty())
		{
			const Item item = stack.back();
			stack.pop_back();
			const int index = static_cast<int>(names.size());
			names.emplace_back(item.Node->mName.C_Str());
			parents.push_back(item.Parent);
			bindLocal.push_back(item.Node->mTransformation);
			for (unsigned int child = item.Node->mNumChildren; child > 0; --child)
				stack.push_back({ item.Node->mChildren[child - 1], index });
		}
		Build(names, parents, bindLocal);
	}

	bool Skeleton::Build(const std::vector<std::string>& names, const std::vector<int>& parents,
		const std::vector<aiMatrix4x4>& bindLocal)
	{
		Clear();
		if (names.size() != parents.size() || names.size() != bindLocal.size()) return false;
		for (size_t index = 0; index < names.size(); ++index)
		{
			const int parent = parents[index];
			if (parent >= static_cast<int>(index))
			{
				Clear();
				return false;
			}
			m_Names.push_back(names[index]);
			m_Parents.push_back(parent < 0 ? -1 : parent);
			m_BindPose.push_back(BoneTransform::FromMatrix(bindLocal[index]));
			m_BindLocal.push_back(bindLocal[index]);
			m_BindGlobal.push_back(parent >= 0
				? m_BindGlobal[static_cast<size_t>(parent)] * bindLocal[index] : bindLocal[index]);
			m_Index.emplace(names[index], static_cast<int>(index));
		}
		return true;
	}

	int Skeleton::Find(const std::string& name) const
	{
		const auto found = m_Index.find(name);
		return found != m_Index.end() ? found->second : -1;
	}

	aiVector3D Skeleton::UpInParentSpace(int bone) const
	{
		const aiVector3D fallback(0.0f, 1.0f, 0.0f);
		if (bone < 0 || bone >= Size() || m_BindGlobal.empty()) return fallback;
		// AnimationModelのモデル空間は「ルートノードの逆行列 × ボーンのグローバル行列」です。
		aiMatrix4x4 rootInverse = m_BindGlobal[0];
		rootInverse.Inverse();
		const int parent = Parent(bone);
		aiMatrix4x4 parentToModel = parent >= 0
			? rootInverse * m_BindGlobal[static_cast<size_t>(parent)] : rootInverse;
		parentToModel.Inverse();
		aiVector3D up = aiMatrix3x3(parentToModel) * fallback;
		const float length = up.Length();
		return length > 1e-6f ? up / length : fallback;
	}

	aiVector3D Skeleton::ParentDirectionToModel(int bone, const aiVector3D& direction) const
	{
		if (bone < 0 || bone >= Size() || m_BindGlobal.empty()) return direction;
		aiMatrix4x4 rootInverse = m_BindGlobal[0];
		rootInverse.Inverse();
		const int parent = Parent(bone);
		const aiMatrix4x4 parentToModel = parent >= 0
			? rootInverse * m_BindGlobal[static_cast<size_t>(parent)] : rootInverse;
		return aiMatrix3x3(parentToModel) * direction;
	}

	ClipBinding BindClip(const Skeleton& skeleton, const aiAnimation* animation)
	{
		ClipBinding clip;
		clip.Animation = animation;
		if (animation == nullptr) return clip;
		clip.TicksPerSecond = animation->mTicksPerSecond > 0.0 ? animation->mTicksPerSecond : 25.0;
		clip.DurationTicks = animation->mDuration > 0.0 ? animation->mDuration : 1.0;
		for (unsigned int channelIndex = 0; channelIndex < animation->mNumChannels; ++channelIndex)
		{
			const aiNodeAnim* channel = animation->mChannels[channelIndex];
			if (channel == nullptr) continue;
			const int bone = skeleton.Find(channel->mNodeName.C_Str());
			if (bone >= 0) clip.Channels.emplace_back(bone, channel);
		}
		return clip;
	}

	void SampleClip(const ClipBinding& clip, const Skeleton& skeleton, double ticks, Pose& out)
	{
		out = skeleton.BindPose();
		for (const auto& [bone, channel] : clip.Channels)
		{
			BoneTransform& target = out[static_cast<size_t>(bone)];
			target.Rotation = SampleQuaternionKeys(ticks, channel->mNumRotationKeys, channel->mRotationKeys, target.Rotation);
			if (clip.KeepProportions)
			{
				// 骨の長さと拡縮はバインド姿勢のままにし、腰の位置だけを体格の比に合わせて使います。
				if (bone == clip.RootBone)
				{
					target.Position = SampleVectorKeys(ticks, channel->mNumPositionKeys, channel->mPositionKeys, target.Position)
						* clip.RootPositionScale;
				}
				continue;
			}
			target.Position = SampleVectorKeys(ticks, channel->mNumPositionKeys, channel->mPositionKeys, target.Position);
			target.Scale = SampleVectorKeys(ticks, channel->mNumScalingKeys, channel->mScalingKeys, target.Scale);
		}
	}

	void BlendPose(const Pose& from, const Pose& to, float weight, Pose& out)
	{
		if (from.size() != to.size() || weight >= 1.0f)
		{
			if (&out != &to) out = to;
			return;
		}
		if (weight <= 0.0f)
		{
			if (&out != &from) out = from;
			return;
		}
		out.resize(to.size());
		for (size_t bone = 0; bone < to.size(); ++bone)
			out[bone] = Lerp(from[bone], to[bone], weight);
	}

	void BlendPoseMasked(const Pose& base, const Pose& layer, float weight,
		const std::vector<float>& boneWeights, Pose& out)
	{
		if (boneWeights.empty() || base.size() != layer.size() || boneWeights.size() != layer.size())
		{
			BlendPose(base, layer, weight, out);
			return;
		}
		out.resize(layer.size());
		for (size_t bone = 0; bone < layer.size(); ++bone)
		{
			const float boneWeight = std::clamp(weight * boneWeights[bone], 0.0f, 1.0f);
			out[bone] = boneWeight <= 0.0f ? base[bone]
				: boneWeight >= 1.0f ? layer[bone]
				: Lerp(base[bone], layer[bone], boneWeight);
		}
	}

	void MakeAdditiveDelta(const Pose& pose, const Pose& reference, Pose& delta)
	{
		if (pose.size() != reference.size())
		{
			delta.assign(pose.size(), BoneTransform{});
			return;
		}
		delta.resize(pose.size());
		for (size_t bone = 0; bone < pose.size(); ++bone)
		{
			const BoneTransform& current = pose[bone];
			const BoneTransform& base = reference[bone];
			BoneTransform result;
			aiQuaternion inverse = base.Rotation;
			inverse.Conjugate();
			result.Rotation = (inverse * current.Rotation).Normalize();
			result.Position = current.Position - base.Position;
			auto ratio = [](float value, float denominator)
			{
				return std::fabs(denominator) > 1e-6f ? value / denominator : 1.0f;
			};
			result.Scale = aiVector3D(ratio(current.Scale.x, base.Scale.x),
				ratio(current.Scale.y, base.Scale.y), ratio(current.Scale.z, base.Scale.z));
			delta[bone] = result;
		}
	}

	void ApplyAdditive(const Pose& base, const Pose& delta, float weight,
		const std::vector<float>& boneWeights, Pose& out)
	{
		if (base.size() != delta.size())
		{
			if (&out != &base) out = base;
			return;
		}
		const bool masked = boneWeights.size() == base.size();
		out.resize(base.size());
		const aiQuaternion identity;
		for (size_t bone = 0; bone < base.size(); ++bone)
		{
			const float boneWeight = (std::max)(0.0f, weight * (masked ? boneWeights[bone] : 1.0f));
			const BoneTransform source = base[bone];
			if (boneWeight <= 0.0f)
			{
				out[bone] = source;
				continue;
			}
			const BoneTransform& difference = delta[bone];
			aiQuaternion rotation;
			aiQuaternion::Interpolate(rotation, identity, difference.Rotation, (std::min)(boneWeight, 1.0f));
			BoneTransform result;
			result.Rotation = (source.Rotation * rotation).Normalize();
			result.Position = source.Position + difference.Position * boneWeight;
			const aiVector3D one(1.0f, 1.0f, 1.0f);
			const aiVector3D scale = one + (difference.Scale - one) * boneWeight;
			result.Scale = aiVector3D(source.Scale.x * scale.x, source.Scale.y * scale.y, source.Scale.z * scale.z);
			out[bone] = result;
		}
	}

	float SmoothStep(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	PlaybackRange SanitizeRange(PlaybackRange range)
	{
		range.Speed = (std::max)(range.Speed, 0.01f);
		range.Start = std::clamp(range.Start, 0.0f, 0.99f);
		range.End = std::clamp(range.End, range.Start + 0.01f, 1.0f);
		return range;
	}

	double PhaseToTicks(const ClipBinding& clip, const PlaybackRange& range, double phase)
	{
		const double start = clip.DurationTicks * range.Start;
		const double length = clip.DurationTicks * (range.End - range.Start);
		const double elapsed = phase * length;
		return range.Loop
			? start + std::fmod((std::max)(elapsed, 0.0), length)
			: start + std::clamp(elapsed, 0.0, length - 0.001);
	}

	double CycleSeconds(const ClipBinding& clip, const PlaybackRange& range)
	{
		const double length = clip.DurationTicks * (range.End - range.Start);
		return (std::max)(length / clip.TicksPerSecond / range.Speed, 1e-4);
	}

	Weights1D ComputeWeights1D(const std::vector<float>& thresholds, float parameter)
	{
		Weights1D result;
		if (thresholds.empty() || parameter <= thresholds.front()) return result;
		const int last = static_cast<int>(thresholds.size()) - 1;
		if (parameter >= thresholds.back())
		{
			result.Lower = result.Upper = last;
			return result;
		}
		for (int index = 0; index < last; ++index)
		{
			const float lower = thresholds[static_cast<size_t>(index)];
			const float upper = thresholds[static_cast<size_t>(index) + 1];
			if (parameter < upper)
			{
				result.Lower = index;
				result.Upper = index + 1;
				result.UpperWeight = upper > lower ? (parameter - lower) / (upper - lower) : 1.0f;
				return result;
			}
		}
		result.Lower = result.Upper = last;
		return result;
	}

	std::vector<float> BuildBoneMask(const Skeleton& skeleton,
		const std::vector<std::pair<std::string, float>>& bones)
	{
		const size_t count = static_cast<size_t>(skeleton.Size());
		std::vector<float> specified(count, -1.0f);
		for (const auto& [name, weight] : bones)
		{
			const int bone = skeleton.Find(name);
			if (bone >= 0) specified[static_cast<size_t>(bone)] = std::clamp(weight, 0.0f, 1.0f);
		}
		std::vector<float> mask(count, 0.0f);
		for (size_t bone = 0; bone < count; ++bone)
		{
			const int parent = skeleton.Parent(static_cast<int>(bone));
			mask[bone] = specified[bone] >= 0.0f ? specified[bone]
				: parent >= 0 ? mask[static_cast<size_t>(parent)] : 0.0f;
		}
		return mask;
	}

	// ---------------------------------------------------------------------------------------

	void Animator::SetSkeleton(const aiNode* root)
	{
		Skeleton skeleton;
		skeleton.Build(root);
		SetSkeleton(skeleton);
	}

	void Animator::SetSkeleton(const Skeleton& skeleton)
	{
		m_Skeleton = skeleton;
		m_RootBone = m_Skeleton.Find(m_RootBoneName);
		for (auto& [name, entry] : m_Clips)
		{
			entry.Binding = BindClip(m_Skeleton, entry.Animation);
			ApplyKeepProportions(entry);
		}
		Reset();
	}

	// クリップの最初の腰の高さと、このスケルトンのバインド姿勢の腰の高さの比を、腰の位置に掛ける倍率にします。
	// どちらも立ち姿勢に近いので、体格の違う相手のクリップでも足が床に付きます。
	void Animator::ApplyKeepProportions(ClipEntry& entry) const
	{
		ClipBinding& binding = entry.Binding;
		binding.KeepProportions = entry.KeepProportions;
		binding.RootBone = m_RootBone;
		binding.RootPositionScale = 1.0f;
		if (!entry.KeepProportions || m_RootBone < 0) return;
		const float bindHeight = m_Skeleton.BindPose()[static_cast<size_t>(m_RootBone)].Position.y;
		for (const auto& [bone, channel] : binding.Channels)
		{
			if (bone != m_RootBone || channel->mNumPositionKeys == 0) continue;
			const float clipHeight = channel->mPositionKeys[0].mValue.y;
			if (bindHeight > 1e-3f && clipHeight > 1e-3f) binding.RootPositionScale = bindHeight / clipHeight;
		}
	}

	void Animator::SetClipKeepProportions(const std::string& name, bool keep)
	{
		const auto found = m_Clips.find(name);
		if (found == m_Clips.end()) return;
		found->second.KeepProportions = keep;
		ApplyKeepProportions(found->second);
	}

	void Animator::AddClip(const std::string& name, const aiAnimation* animation)
	{
		if (animation == nullptr) return;
		ClipEntry& entry = m_Clips[name];
		entry.Animation = animation;
		entry.Binding = BindClip(m_Skeleton, animation);
		ApplyKeepProportions(entry);
	}

	void Animator::SetClipInPlace(const std::string& name, bool inPlace)
	{
		const auto found = m_Clips.find(name);
		if (found != m_Clips.end()) found->second.InPlace = inPlace;
	}

	float Animator::GetClipSeconds(const std::string& name) const
	{
		const ClipEntry* clip = FindClip(name);
		if (clip == nullptr || clip->Binding.TicksPerSecond <= 0.0) return 0.0f;
		return static_cast<float>(clip->Binding.DurationTicks / clip->Binding.TicksPerSecond);
	}

	bool Animator::HasClip(const std::string& name) const
	{
		return m_Clips.find(name) != m_Clips.end() || m_BlendSpaces.find(name) != m_BlendSpaces.end();
	}

	void Animator::AddBlendSpace1D(const std::string& name, BlendSpace1D space)
	{
		for (BlendClip& clip : space.Clips) clip.Playback = SanitizeRange(clip.Playback);
		std::stable_sort(space.Clips.begin(), space.Clips.end(),
			[](const BlendClip& a, const BlendClip& b) { return a.Threshold < b.Threshold; });
		m_BlendSpaces[name] = std::move(space);
	}

	void Animator::DefineMask(const std::string& name, const std::vector<std::pair<std::string, float>>& bones)
	{
		m_MaskDefinitions[name] = bones;
	}

	float Animator::GetParameter(const std::string& name) const
	{
		const auto found = m_Parameters.find(name);
		return found != m_Parameters.end() ? found->second : 0.0f;
	}

	const Animator::ClipEntry* Animator::FindClip(const std::string& name) const
	{
		const auto found = m_Clips.find(name);
		return found != m_Clips.end() && found->second.Animation != nullptr ? &found->second : nullptr;
	}

	void Animator::Reset()
	{
		m_BaseName.clear();
		m_BaseClip = nullptr;
		m_BaseSpace = nullptr;
		m_Phase = 0.0;
		m_FreePhases.clear();
		m_SpaceWeights.clear();
		m_BaseSnapshot.clear();
		m_BaseBlendSeconds = 0.0f;
		m_BaseBlendDuration = 0.0f;
		m_LastBasePose.clear();
		m_HasBasePose = false;
		m_Layers.clear();
		m_Pose.clear();
		m_HasPose = false;
	}

	bool Animator::Play(const std::string& name, float blendSeconds)
	{
		if (name == m_BaseName) return true;
		const auto space = m_BlendSpaces.find(name);
		const ClipEntry* clip = space == m_BlendSpaces.end() ? FindClip(name) : nullptr;
		if (space == m_BlendSpaces.end() && clip == nullptr) return false;

		const bool canBlend = !m_BaseName.empty() && m_HasBasePose && blendSeconds > 0.0f;
		m_BaseSnapshot = canBlend ? m_LastBasePose : Pose{};
		m_BaseBlendSeconds = 0.0f;
		m_BaseBlendDuration = canBlend ? blendSeconds : 0.0f;
		m_BaseName = name;
		m_BaseClip = clip;
		m_BaseSpace = space != m_BlendSpaces.end() ? &space->second : nullptr;
		m_Phase = 0.0;
		m_FreePhases.assign(m_BaseSpace != nullptr ? m_BaseSpace->Clips.size() : 0, 0.0);
		m_SpaceWeights.assign(m_FreePhases.size(), 0.0f);
		if (m_BaseSpace != nullptr) m_SmoothedParameter = GetParameter(m_BaseSpace->Parameter);
		return true;
	}

	float Animator::GetBlendSpaceWeight(const std::string& clip) const
	{
		if (m_BaseSpace == nullptr) return clip == m_BaseName ? 1.0f : 0.0f;
		for (size_t index = 0; index < m_BaseSpace->Clips.size() && index < m_SpaceWeights.size(); ++index)
			if (m_BaseSpace->Clips[index].Clip == clip) return m_SpaceWeights[index];
		return 0.0f;
	}

	void Animator::BeginLoopSeamBlend(double cycleSeconds)
	{
		// ループの最後と最初の姿勢が完全には一致しないクリップのため、つなぎ目を短くブレンドします。
		m_BaseSnapshot = m_LastBasePose;
		m_BaseBlendSeconds = 0.0f;
		m_BaseBlendDuration = (std::min)(m_LoopBlendSeconds, static_cast<float>(cycleSeconds) * 0.25f);
	}

	void Animator::EvaluateBlendSpace(const BlendSpace1D& space, float step, Pose& out)
	{
		const size_t count = space.Clips.size();
		m_SmoothedParameter = Approach(m_SmoothedParameter, GetParameter(space.Parameter),
			step, space.ParameterSmoothing);
		std::vector<float> thresholds(count);
		for (size_t index = 0; index < count; ++index) thresholds[index] = space.Clips[index].Threshold;
		const Weights1D weights = ComputeWeights1D(thresholds, m_SmoothedParameter);
		m_SpaceWeights.assign(count, 0.0f);
		if (count == 0)
		{
			out = m_Skeleton.BindPose();
			return;
		}
		m_SpaceWeights[static_cast<size_t>(weights.Lower)] += 1.0f - weights.UpperWeight;
		m_SpaceWeights[static_cast<size_t>(weights.Upper)] += weights.UpperWeight;

		// 同期するクリップは、重みで平均した1周の長さを共通の周期にします。
		double weightSum = 0.0, cycleSum = 0.0;
		std::vector<double> cycles(count, 1.0);
		for (size_t index = 0; index < count; ++index)
		{
			const BlendClip& blendClip = space.Clips[index];
			const ClipEntry* clip = FindClip(blendClip.Clip);
			if (clip == nullptr) continue;
			cycles[index] = CycleSeconds(clip->Binding, blendClip.Playback);
			if (blendClip.Synchronized && m_SpaceWeights[index] > 0.0f)
			{
				weightSum += m_SpaceWeights[index];
				cycleSum += m_SpaceWeights[index] * cycles[index];
			}
		}

		bool wrapped = false;
		double wrappedCycle = 1e9;
		if (step > 0.0f)
		{
			if (weightSum > 0.0)
			{
				const double cycle = cycleSum / weightSum;
				const double previous = m_Phase;
				m_Phase += step / cycle;
				if (std::floor(m_Phase) > std::floor(previous))
				{
					bool loops = false;
					for (size_t index = 0; index < count; ++index)
						loops |= space.Clips[index].Synchronized && space.Clips[index].Playback.Loop
							&& m_SpaceWeights[index] > 0.001f;
					if (loops)
					{
						wrapped = true;
						wrappedCycle = (std::min)(wrappedCycle, cycle);
					}
				}
			}
			for (size_t index = 0; index < count; ++index)
			{
				if (space.Clips[index].Synchronized) continue;
				const double previous = m_FreePhases[index];
				m_FreePhases[index] += step / cycles[index];
				if (std::floor(m_FreePhases[index]) > std::floor(previous) && space.Clips[index].Playback.Loop
					&& m_SpaceWeights[index] > 0.001f)
				{
					wrapped = true;
					wrappedCycle = (std::min)(wrappedCycle, cycles[index]);
				}
				if (space.Clips[index].Playback.Loop) m_FreePhases[index] -= std::floor(m_FreePhases[index]);
			}
			m_Phase -= std::floor(m_Phase);
		}
		if (wrapped && m_HasBasePose && m_BaseBlendSeconds >= m_BaseBlendDuration)
			BeginLoopSeamBlend(wrappedCycle);

		auto sample = [&](size_t index, Pose& target)
		{
			const BlendClip& blendClip = space.Clips[index];
			const ClipEntry* clip = FindClip(blendClip.Clip);
			if (clip == nullptr)
			{
				target = m_Skeleton.BindPose();
				return;
			}
			const double phase = blendClip.Synchronized ? m_Phase : m_FreePhases[index];
			SampleBaseClip(*clip, PhaseToTicks(clip->Binding, blendClip.Playback, phase), target);
		};
		const size_t lower = static_cast<size_t>(weights.Lower);
		const size_t upper = static_cast<size_t>(weights.Upper);
		if (lower == upper || weights.UpperWeight <= 0.0f)
		{
			sample(lower, out);
			return;
		}
		if (weights.UpperWeight >= 1.0f)
		{
			sample(upper, out);
			return;
		}
		sample(lower, m_Scratch);
		sample(upper, m_Scratch2);
		BlendPose(m_Scratch, m_Scratch2, weights.UpperWeight, out);
	}

	void Animator::EvaluateBase(float step, Pose& out)
	{
		m_BaseBlendSeconds += step;
		if (m_BaseSpace != nullptr)
		{
			EvaluateBlendSpace(*m_BaseSpace, step, out);
		}
		else if (m_BaseClip != nullptr)
		{
			const double cycle = CycleSeconds(m_BaseClip->Binding, m_Playback);
			const double previous = m_Phase;
			m_Phase += step / cycle;
			if (m_Playback.Loop && step > 0.0f && m_HasBasePose
				&& std::floor(m_Phase) > std::floor(previous)
				&& m_BaseBlendSeconds >= m_BaseBlendDuration)
				BeginLoopSeamBlend(cycle);
			m_Phase = m_Playback.Loop ? m_Phase - std::floor(m_Phase) : (std::min)(m_Phase, 1.0);
			SampleBaseClip(*m_BaseClip, PhaseToTicks(m_BaseClip->Binding, m_Playback, m_Phase), out);
		}
		else
		{
			out = m_Skeleton.BindPose();
		}

		const float linear = m_BaseBlendDuration > 0.0f
			? std::clamp(m_BaseBlendSeconds / m_BaseBlendDuration, 0.0f, 1.0f) : 1.0f;
		if (linear < 1.0f) BlendPose(m_BaseSnapshot, out, SmoothStep(linear), out);
		else
		{
			m_BaseSnapshot.clear();
			m_BaseBlendDuration = 0.0f;
		}
	}

	Animator::ActionLayer& Animator::LayerAt(int layer)
	{
		const size_t index = static_cast<size_t>(std::clamp(layer, 0, kMaxActionLayers - 1));
		if (m_Layers.size() <= index) m_Layers.resize(index + 1);
		return m_Layers[index];
	}

	const Animator::ActionLayer* Animator::FindLayer(int layer) const
	{
		if (layer < 0 || static_cast<size_t>(layer) >= m_Layers.size()) return nullptr;
		return &m_Layers[static_cast<size_t>(layer)];
	}

	bool Animator::PlayAction(const std::string& clipName, const ActionOptions& options)
	{
		const ClipEntry* clip = FindClip(clipName);
		if (clip == nullptr) return false;
		ActionLayer& layer = LayerAt(options.Layer);
		// 加算と上書きでは層の姿勢の意味(差分か姿勢そのものか)が違うので、同じ方式のときだけつなぎます。
		const bool continuing = layer.Active && layer.HasLastPose && layer.FadeLinear > 0.0f
			&& layer.Options.Blend == options.Blend;
		layer.Clip = clipName;
		layer.Entry = clip;
		layer.Options = options;
		layer.Options.Playback = SanitizeRange(options.Playback);
		layer.Options.FadeIn = (std::max)(options.FadeIn, 0.0f);
		layer.Options.FadeOut = (std::max)(options.FadeOut, 0.0f);
		const auto mask = options.Mask.empty() ? m_MaskDefinitions.end() : m_MaskDefinitions.find(options.Mask);
		layer.Mask = mask != m_MaskDefinitions.end() ? BuildBoneMask(m_Skeleton, mask->second) : std::vector<float>{};
		layer.Phase = 0.0;
		layer.PreviousPhase = 0.0;
		layer.Stopping = false;
		layer.Active = true;
		if (continuing)
		{
			// コンボ: 層の重みは保ったまま、直前の層姿勢から次のクリップへつなぎます。
			layer.Snapshot = layer.LastPose;
			layer.SwitchSeconds = 0.0f;
			layer.SwitchDuration = layer.Options.FadeIn;
		}
		else
		{
			layer.Snapshot.clear();
			layer.SwitchSeconds = 0.0f;
			layer.SwitchDuration = 0.0f;
			layer.HasLastPose = false;
			layer.FadeLinear = layer.Options.FadeIn > 0.0f ? 0.0f : 1.0f;
			layer.MaskStrength = layer.MaskTarget;
		}
		layer.RootStart = SampleRoot(layer, PhaseToTicks(clip->Binding, layer.Options.Playback, 0.0));
		layer.PreviousRoot = layer.RootStart;
		// Lockで腰を固定する位置です。普通は再生を始めた位置ですが、InPlace指定のクリップは
		// バインドポーズ(モデルの原点の真上)にします。先頭から原点を離れている書き出しでも、体が原点に残ります。
		layer.LockOrigin = clip->InPlace && m_RootBone >= 0
			? m_Skeleton.BindPose()[static_cast<size_t>(m_RootBone)].Position : layer.RootStart;
		layer.Reference.clear();
		if (layer.Options.Blend == LayerBlend::Additive)
		{
			const ClipEntry* referenceClip = options.AdditiveReferenceClip.empty()
				? clip : FindClip(options.AdditiveReferenceClip);
			if (referenceClip == nullptr) referenceClip = clip;
			const float fraction = options.AdditiveReference >= 0.0f ? (std::min)(options.AdditiveReference, 1.0f)
				: referenceClip == clip ? layer.Options.Playback.Start : 0.0f;
			const ClipBinding& reference = referenceClip->Binding;
			const double ticks = (std::min)(reference.DurationTicks * fraction, reference.DurationTicks - 0.001);
			SampleClip(reference, m_Skeleton, (std::max)(ticks, 0.0), layer.Reference);
			if (layer.Options.RootMotionMode != RootMotion::Keep && m_RootBone >= 0)
			{
				BoneTransform& root = layer.Reference[static_cast<size_t>(m_RootBone)];
				root.Position -= Horizontal(root.Position - layer.LockOrigin);
			}
		}
		return true;
	}

	void Animator::StopAction(int layerIndex, float fadeOutSeconds)
	{
		if (layerIndex < 0 || static_cast<size_t>(layerIndex) >= m_Layers.size()) return;
		ActionLayer& layer = m_Layers[static_cast<size_t>(layerIndex)];
		if (!layer.Active) return;
		layer.Stopping = true;
		layer.StopFade = fadeOutSeconds >= 0.0f ? fadeOutSeconds : layer.Options.FadeOut;
		if (layer.StopFade <= 0.0f)
		{
			layer.Active = false;
			layer.HasLastPose = false;
		}
	}

	bool Animator::IsActionActive(int layer) const
	{
		const ActionLayer* found = FindLayer(layer);
		return found != nullptr && found->Active;
	}

	const std::string& Animator::GetActionName(int layer) const
	{
		static const std::string empty;
		const ActionLayer* found = FindLayer(layer);
		return found != nullptr && found->Active ? found->Clip : empty;
	}

	float Animator::GetActionTime(int layer) const
	{
		const ActionLayer* found = FindLayer(layer);
		return found != nullptr && found->Active ? static_cast<float>((std::min)(found->Phase, 1.0)) : -1.0f;
	}

	float Animator::GetActionWeight(int layer) const
	{
		const ActionLayer* found = FindLayer(layer);
		return found != nullptr && found->Active ? SmoothStep(found->FadeLinear) * found->LayerWeight : 0.0f;
	}

	bool Animator::ActionPassed(float time, int layer) const
	{
		const ActionLayer* found = FindLayer(layer);
		if (found == nullptr || found->Clip.empty()) return false;
		const double previous = found->PreviousPhase;
		const double current = found->Phase;
		if (current >= previous) return previous < time && time <= current;
		return time > previous || time <= current; // ループで1周した
	}

	aiVector3D Animator::SampleRoot(const ActionLayer& layer, double ticks)
	{
		if (layer.Entry == nullptr) return {};
		return SampleRoot(*layer.Entry, ticks);
	}

	aiVector3D Animator::SampleRoot(const ClipEntry& clip, double ticks) const
	{
		if (m_RootBone < 0) return {};
		const aiVector3D fallback = m_Skeleton.BindPose()[static_cast<size_t>(m_RootBone)].Position;
		for (const auto& [bone, channel] : clip.Binding.Channels)
		{
			if (bone == m_RootBone)
				return SampleVectorKeys(ticks, channel->mNumPositionKeys, channel->mPositionKeys, fallback)
					* (clip.Binding.KeepProportions ? clip.Binding.RootPositionScale : 1.0f);
		}
		return fallback;
	}

	void Animator::SampleBaseClip(const ClipEntry& clip, double ticks, Pose& out) const
	{
		SampleClip(clip.Binding, m_Skeleton, ticks, out);
		if (!clip.InPlace || m_RootBone < 0 || static_cast<size_t>(m_RootBone) >= out.size()) return;
		// 腰の水平位置をバインドポーズ(モデルの原点の真上)に固定します。クリップの先頭で既に
		// 前へずれている書き出しもあるので、先頭ではなくバインドポーズを基準にします。
		BoneTransform& root = out[static_cast<size_t>(m_RootBone)];
		const aiVector3D bind = m_Skeleton.BindPose()[static_cast<size_t>(m_RootBone)].Position;
		root.Position -= Horizontal(root.Position - bind);
	}

	aiVector3D Animator::Horizontal(const aiVector3D& value) const
	{
		const aiVector3D up = m_Skeleton.UpInParentSpace(m_RootBone);
		return value - up * (value * up);
	}

	void Animator::SetLayerWeight(float weight, int layerIndex)
	{
		LayerAt(layerIndex).LayerWeight = std::clamp(weight, 0.0f, 1.0f);
	}

	void Animator::SetActionSpeed(float speed, int layerIndex)
	{
		if (layerIndex < 0 || static_cast<size_t>(layerIndex) >= m_Layers.size()) return;
		ActionLayer& layer = m_Layers[static_cast<size_t>(layerIndex)];
		if (!layer.Active) return;
		layer.Options.Playback.Speed = (std::max)(speed, 0.01f);
	}

	float Animator::GetLayerWeight(int layerIndex) const
	{
		const ActionLayer* found = FindLayer(layerIndex);
		return found != nullptr ? found->LayerWeight : 1.0f;
	}

	void Animator::SetActionMaskStrength(float strength, int layerIndex, float smoothingSeconds)
	{
		ActionLayer& layer = LayerAt(layerIndex);
		layer.MaskTarget = std::clamp(strength, 0.0f, 1.0f);
		layer.MaskSmoothing = (std::max)(smoothingSeconds, 0.0f);
		if (!layer.Active || layer.MaskSmoothing <= 0.0f) layer.MaskStrength = layer.MaskTarget;
	}

	bool Animator::EvaluateAction(ActionLayer& layer, float step, Pose& out)
	{
		if (!layer.Active || layer.Entry == nullptr)
		{
			// 終了後もActionPassedが同じ区間を返し続けないよう、通過区間を空にします。
			if (step > 0.0f) layer.PreviousPhase = layer.Phase;
			return false;
		}
		const PlaybackRange& range = layer.Options.Playback;
		const double cycle = CycleSeconds(layer.Entry->Binding, range);
		if (step > 0.0f)
		{
			layer.PreviousPhase = layer.Phase;
			layer.Phase += step / cycle;
			if (range.Loop) layer.Phase -= std::floor(layer.Phase);
			// 秒の積み上げ誤差で終端の手前に止まらないよう、ごく近ければ終端とみなします。
			else layer.Phase = layer.Phase >= 1.0 - 1e-6 ? 1.0 : layer.Phase;

			if (layer.Stopping)
				layer.FadeLinear -= layer.StopFade > 0.0f ? step / layer.StopFade : 1.0f;
			else
				layer.FadeLinear += layer.Options.FadeIn > 0.0f ? step / layer.Options.FadeIn : 1.0f;
			layer.FadeLinear = std::clamp(layer.FadeLinear, 0.0f, 1.0f);
			if (!range.Loop)
			{
				// 残り時間がFadeOutを切ったら、終端でちょうど0になるよう重みを下げます。
				const float remaining = static_cast<float>((1.0 - layer.Phase) * cycle);
				const float cap = layer.Options.FadeOut > 0.0f ? remaining / layer.Options.FadeOut
					: (layer.Phase >= 1.0 ? 0.0f : 1.0f);
				layer.FadeLinear = (std::min)(layer.FadeLinear, (std::max)(cap, 0.0f));
			}
			layer.MaskStrength = Approach(layer.MaskStrength, layer.MaskTarget, step, layer.MaskSmoothing);
			layer.SwitchSeconds += step;
			if ((layer.Stopping || !range.Loop) && layer.FadeLinear <= 0.0f
				&& (layer.Stopping || layer.Phase >= 1.0))
			{
				layer.Active = false;
				layer.HasLastPose = false;
				return false;
			}
		}

		const bool wrapped = step > 0.0f && range.Loop && layer.Phase < layer.PreviousPhase;
		Pose& pose = m_Scratch;
		SampleClip(layer.Entry->Binding, m_Skeleton,
			PhaseToTicks(layer.Entry->Binding, range, layer.Phase), pose);
		aiVector3D rootTravel{};
		if (m_RootBone >= 0)
		{
			BoneTransform& root = pose[static_cast<size_t>(m_RootBone)];
			const aiVector3D raw = root.Position;
			if (step > 0.0f)
			{
				// ループで1周したときは「終端まで」と「先頭から」の2区間に分けて足します。
				const ClipBinding& binding = layer.Entry->Binding;
				rootTravel = wrapped
					? (SampleRoot(layer, binding.DurationTicks * range.End) - layer.PreviousRoot)
						+ (raw - SampleRoot(layer, binding.DurationTicks * range.Start))
					: raw - layer.PreviousRoot;
				layer.PreviousRoot = raw;
			}
			if (layer.Options.RootMotionMode != RootMotion::Keep)
				root.Position -= Horizontal(raw - layer.LockOrigin);
		}
		const bool additive = layer.Options.Blend == LayerBlend::Additive;
		if (additive) MakeAdditiveDelta(pose, layer.Reference, pose);
		if (layer.SwitchDuration > 0.0f)
		{
			const float linear = std::clamp(layer.SwitchSeconds / layer.SwitchDuration, 0.0f, 1.0f);
			if (linear < 1.0f) BlendPose(layer.Snapshot, pose, SmoothStep(linear), pose);
			else
			{
				layer.SwitchDuration = 0.0f;
				layer.Snapshot.clear();
			}
		}
		layer.LastPose = pose;
		layer.HasLastPose = true;

		const float weight = SmoothStep(layer.FadeLinear) * layer.LayerWeight;
		std::vector<float> effective;
		if (!layer.Mask.empty())
		{
			effective.resize(layer.Mask.size());
			for (size_t bone = 0; bone < effective.size(); ++bone)
				effective[bone] = 1.0f + (layer.Mask[bone] - 1.0f) * layer.MaskStrength;
		}
		if (layer.Options.RootMotionMode == RootMotion::Extract && m_RootBone >= 0)
		{
			const float rootWeight = weight * (effective.empty() ? 1.0f : effective[static_cast<size_t>(m_RootBone)]);
			m_RootMotion += m_Skeleton.ParentDirectionToModel(m_RootBone, Horizontal(rootTravel)) * rootWeight;
		}
		if (additive) ApplyAdditive(out, pose, weight, effective, out);
		else if (effective.empty()) BlendPose(out, pose, weight, out);
		else BlendPoseMasked(out, pose, weight, effective, out);
		return true;
	}

	bool Animator::Advance(float deltaSeconds)
	{
		if (m_BaseName.empty() || m_Skeleton.Size() == 0) return false;
		const float step = (std::max)(deltaSeconds, 0.0f);
		if (step > 0.0f) m_RootMotion = {};
		EvaluateBase(step, m_Pose);
		m_LastBasePose = m_Pose;
		m_HasBasePose = true;
		for (ActionLayer& layer : m_Layers) EvaluateAction(layer, step, m_Pose);
		m_HasPose = true;
		return true;
	}

	bool Animator::SampleSingle(const std::string& clipName, double ticks, Pose& out) const
	{
		const ClipEntry* clip = FindClip(clipName);
		if (clip == nullptr || m_Skeleton.Size() == 0) return false;
		SampleClip(clip->Binding, m_Skeleton, ticks, out);
		return true;
	}
}
