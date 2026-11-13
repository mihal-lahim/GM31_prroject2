#pragma once

// 描画・GPU・Worldに依存しないアニメーションの合成処理です。
// AnimationModelはこのAnimatorが作った姿勢を受け取り、足IKとスキニングだけを行います。
// 数値だけで完結するので、--self-test でGPUなしに検証できます(animatorSelfTests.cpp)。
//
// 構成(上から順に合成します):
//   ベース層   : 単体クリップ、または1Dブレンドツリー。切り替えは姿勢スナップショットからのクロスフェード。
//   アクション層: 攻撃などの単発クリップ。ボーンマスクで上半身だけ・全身を選べ、フェードイン/アウトする。
//                層の番号が大きいほど後から上書きします。

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "assimp/anim.h"
#include "assimp/matrix4x4.h"
#include "assimp/quaternion.h"
#include "assimp/scene.h"
#include "assimp/vector3.h"

namespace Anim
{
	// ボーン1本のローカル変換(親ボーン空間)です。行列のままでは補間できないので分解して持ちます。
	struct BoneTransform
	{
		aiVector3D Scale{ 1.0f, 1.0f, 1.0f };
		aiQuaternion Rotation{};
		aiVector3D Position{};

		aiMatrix4x4 ToMatrix() const { return aiMatrix4x4(Scale, Rotation, Position); }
		bool operator==(const BoneTransform& other) const
		{
			return Scale == other.Scale && Rotation == other.Rotation && Position == other.Position;
		}
		static BoneTransform FromMatrix(const aiMatrix4x4& matrix);
	};

	// Skeletonのボーン番号順に並んだ姿勢です。
	using Pose = std::vector<BoneTransform>;

	// aiNode階層を、親が必ず子より前に来る番号付きの配列へ変換したものです。
	class Skeleton
	{
	public:
		void Build(const aiNode* root);
		// 名前・親番号・バインド時のローカル行列から作ります。parents[i] < i であること(根は-1)。
		// aiNodeを作らずに済むので、テストで使います。
		bool Build(const std::vector<std::string>& names, const std::vector<int>& parents,
			const std::vector<aiMatrix4x4>& bindLocal);
		void Clear();
		int Find(const std::string& name) const;
		int Size() const { return static_cast<int>(m_Names.size()); }
		const std::string& Name(int bone) const { return m_Names[static_cast<size_t>(bone)]; }
		int Parent(int bone) const { return m_Parents[static_cast<size_t>(bone)]; }
		const Pose& BindPose() const { return m_BindPose; }
		// 分解前のバインド行列です。動かないボーンへ丸め誤差のない元の行列を書き戻すために使います。
		const aiMatrix4x4& BindLocalMatrix(int bone) const { return m_BindLocal[static_cast<size_t>(bone)]; }
		// モデル空間の上方向(+Y)を、指定ボーンの親空間で表した単位ベクトルです(バインド姿勢基準)。
		aiVector3D UpInParentSpace(int bone) const;
		// 指定ボーンの親空間の方向ベクトルを、モデル空間へ変換します(バインド姿勢基準)。
		aiVector3D ParentDirectionToModel(int bone, const aiVector3D& direction) const;

	private:
		std::vector<std::string> m_Names;
		std::vector<int> m_Parents;
		Pose m_BindPose;
		std::vector<aiMatrix4x4> m_BindLocal;
		std::vector<aiMatrix4x4> m_BindGlobal;
		std::unordered_map<std::string, int> m_Index;
	};

	// クリップのチャンネルをボーン番号へ結び付けたものです。名前検索を毎フレーム行わないためです。
	struct ClipBinding
	{
		const aiAnimation* Animation = nullptr;
		std::vector<std::pair<int, const aiNodeAnim*>> Channels;
		double TicksPerSecond = 25.0;
		double DurationTicks = 1.0;
	};

	// 再生範囲です。Start/Endは元クリップに対する割合(0〜1)、Speedは再生倍率です。
	struct PlaybackRange
	{
		float Speed = 1.0f;
		float Start = 0.0f;
		float End = 1.0f;
		bool Loop = true;
	};

	ClipBinding BindClip(const Skeleton& skeleton, const aiAnimation* animation);
	// バインド姿勢から始め、クリップにあるボーンだけをticks時点の値で上書きします。
	void SampleClip(const ClipBinding& clip, const Skeleton& skeleton, double ticks, Pose& out);
	// 回転は球面線形補間、位置と拡縮は線形補間です。outはfrom/toと同じ配列でも構いません。
	void BlendPose(const Pose& from, const Pose& to, float weight, Pose& out);
	// boneWeights[b] * weight の割合で base から layer へ近づけます。boneWeightsが空なら全ボーン同じ重みです。
	void BlendPoseMasked(const Pose& base, const Pose& layer, float weight,
		const std::vector<float>& boneWeights, Pose& out);
	// 3t^2 - 2t^3。切り替えの始まりと終わりの速度を0にします。
	float SmoothStep(float t);
	// 再生範囲を正規化位相(0〜1)で表したときの、元クリップ上のtick位置です。
	double PhaseToTicks(const ClipBinding& clip, const PlaybackRange& range, double phase);
	// 再生範囲を1周するのにかかる秒数(再生倍率込み)です。
	double CycleSeconds(const ClipBinding& clip, const PlaybackRange& range);
	PlaybackRange SanitizeRange(PlaybackRange range);

	// 昇順のしきい値列に対する1Dブレンドの重みです。Lower/Upperの2本だけが寄与します。
	struct Weights1D
	{
		int Lower = 0;
		int Upper = 0;
		float UpperWeight = 0.0f;
	};
	Weights1D ComputeWeights1D(const std::vector<float>& thresholds, float parameter);

	// 加算ブレンド用の差分です。delta = reference^-1 * pose (回転)、pose - reference (位置)、pose / reference (拡縮)。
	void MakeAdditiveDelta(const Pose& pose, const Pose& reference, Pose& delta);
	// base に delta を weight * boneWeights[b] の割合で足し込みます。boneWeightsが空なら全ボーン同じ重みです。
	void ApplyAdditive(const Pose& base, const Pose& delta, float weight,
		const std::vector<float>& boneWeights, Pose& out);

	// 指定ボーンとその子孫に重みを付けたマスクです。子孫は最も近い指定済みの祖先の値を継ぎ、
	// どの指定ボーンの子孫でもないボーンは0になります。
	std::vector<float> BuildBoneMask(const Skeleton& skeleton,
		const std::vector<std::pair<std::string, float>>& bones);

	// 1Dブレンドツリーに並べるクリップです。
	struct BlendClip
	{
		std::string Clip;
		float Threshold = 0.0f;
		PlaybackRange Playback{};
		// trueのクリップ同士は正規化位相をそろえて再生します(歩きと走りの足運びを合わせる)。
		// Idleのように周期が全く違うクリップはfalseにして、自分の速さで再生させます。
		bool Synchronized = true;
	};

	struct BlendSpace1D
	{
		std::string Parameter;
		std::vector<BlendClip> Clips;
		// パラメータ変化を追従させる時定数(秒)。0なら即座に反映します。
		float ParameterSmoothing = 0.1f;
	};

	// アクションのルートボーン(腰)の水平移動の扱いです。上下の動きはどれでも姿勢に残します。
	enum class RootMotion
	{
		Keep,    // クリップのまま動かす(モデルの中で腰が前へ出ていく)
		Lock,    // 打ち消してその場で再生する
		Extract, // 姿勢からは打ち消し、移動量をGetRootMotionで渡す(ゲーム側でキャラを動かす)
	};

	enum class LayerBlend
	{
		Override, // 下の層の姿勢を置き換える(攻撃など)
		Additive, // 基準姿勢からの差分を下の層へ足す(被弾リアクション・傾きなど)
	};

	struct ActionOptions
	{
		// DefineMaskで登録したマスク名です。空なら全身に適用します。
		std::string Mask;
		float FadeIn = 0.12f;
		float FadeOut = 0.2f;
		PlaybackRange Playback{ 1.0f, 0.0f, 1.0f, false };
		// 既定はLock。Mixamoの攻撃は前へ踏み込む物が多く、そのままだと足が滑って見えるためです。
		RootMotion RootMotionMode = RootMotion::Lock;
		LayerBlend Blend = LayerBlend::Override;
		// 加算ブレンドの基準にするフレーム(元クリップに対する割合)。負ならPlayback.Start(再生範囲の最初)です。
		float AdditiveReference = -1.0f;
		// 基準を別のクリップから取る場合のクリップ名です(体を傾けた1フレームのクリップに対してIdleを基準にする、など)。
		// 空なら再生するクリップ自身です。この場合AdditiveReferenceが負なら0(先頭)を使います。
		std::string AdditiveReferenceClip;
		int Layer = 0;
	};

	class Animator
	{
	public:
		Animator() = default;
		// 自分のクリップ表を指すポインタを持つので、コピーは禁止します(ムーブはノードごと移るので安全)。
		Animator(const Animator&) = delete;
		Animator& operator=(const Animator&) = delete;
		Animator(Animator&&) = default;
		Animator& operator=(Animator&&) = default;

		// ボーン構成を設定し、登録済みクリップを結び直します。再生状態は初期化します。
		void SetSkeleton(const aiNode* root);
		void SetSkeleton(const Skeleton& skeleton);
		const Skeleton& GetSkeleton() const { return m_Skeleton; }
		// クリップの寿命は呼び出し側(AnimationModel)が保証します。
		void AddClip(const std::string& name, const aiAnimation* animation);
		bool HasClip(const std::string& name) const;
		// 元クリップ全体の長さ(秒、再生倍率1)です。クリップが無ければ0です。
		float GetClipSeconds(const std::string& name) const;
		void AddBlendSpace1D(const std::string& name, BlendSpace1D space);
		void DefineMask(const std::string& name, const std::vector<std::pair<std::string, float>>& bones);
		void SetRootBone(const std::string& name) { m_RootBoneName = name; m_RootBone = m_Skeleton.Find(name); }
		// ベース層(Play/ブレンドツリー)で再生するときに、腰の水平移動を打ち消してその場で再生します。
		// Mixamoで「In Place」にせず書き出した走りなど、前へ進んでしまうクリップに使います。
		// アクション層でLock再生するときも、再生を始めた位置ではなく原点の真上に腰を固定します。
		void SetClipInPlace(const std::string& name, bool inPlace);

		void SetParameter(const std::string& name, float value) { m_Parameters[name] = value; }
		float GetParameter(const std::string& name) const;

		// ベース層 -------------------------------------------------------------
		// 単体クリップの再生範囲です。ブレンドツリーには各BlendClipの値を使います。
		void SetPlayback(const PlaybackRange& range) { m_Playback = SanitizeRange(range); }
		// クリップ名またはブレンドツリー名を再生します。同じ名前なら何もしません。
		bool Play(const std::string& name, float blendSeconds);
		const std::string& GetCurrentName() const { return m_BaseName; }
		// ベース層の重み(しきい値の昇順に並べたクリップ)。テストと確認用です。
		float GetBlendSpaceWeight(const std::string& clip) const;
		float GetSmoothedParameter() const { return m_SmoothedParameter; }

		// アクション層 ---------------------------------------------------------
		// 再生中の層へ呼ぶと、その層の現在姿勢から次のクリップへつなぎます(コンボ)。
		bool PlayAction(const std::string& clip, const ActionOptions& options = {});
		// fadeOutSeconds < 0 なら再生時に指定したFadeOutを使います。
		void StopAction(int layer = 0, float fadeOutSeconds = -1.0f);
		// フェードアウトが終わるまでtrueです。
		bool IsActionActive(int layer = 0) const;
		// 再生中のクリップ名。無ければ空です。
		const std::string& GetActionName(int layer = 0) const;
		// 再生範囲に対する進み具合(0〜1)。再生していなければ-1です。
		float GetActionTime(int layer = 0) const;
		// 直前の時間を進めたAdvanceで、正規化時刻timeを通過したらtrueです(攻撃判定の発生などに使います)。
		bool ActionPassed(float time, int layer = 0) const;
		// 現在のフェード重み(0〜1、smoothstep適用後)です。
		float GetActionWeight(int layer = 0) const;
		// 1でマスクどおり、0で全身に適用します。移動中は上半身だけ、停止中は全身、のように切り替えます。
		void SetActionMaskStrength(float strength, int layer = 0, float smoothingSeconds = 0.15f);
		// 層全体にかける重み(0〜1、既定1)。旋回量に応じた傾きなど、重みをゲーム側で決めるときに使います。
		void SetLayerWeight(float weight, int layer = 0);
		float GetLayerWeight(int layer = 0) const;
		// 直前の時間を進めたAdvanceで、RootMotion::Extractの層が動かした量(モデル空間・水平のみ)。
		// 層の重みとマスクのかかり具合を掛けた値です。時間0のAdvanceでは変わりません。
		const aiVector3D& GetRootMotion() const { return m_RootMotion; }

		// 時間を進めて姿勢を作ります。deltaSeconds = 0 は時間を進めずに作り直すだけです。
		bool Advance(float deltaSeconds);
		const Pose& GetPose() const { return m_Pose; }
		bool HasPose() const { return m_HasPose; }
		// 合成を使わず、1つのクリップの指定tick位置をそのまま取り出します(旧APIの互換用)。
		bool SampleSingle(const std::string& clip, double ticks, Pose& out) const;
		void Reset();

	private:
		struct ClipEntry
		{
			const aiAnimation* Animation = nullptr;
			ClipBinding Binding;
			bool InPlace = false;
		};
		struct ActionLayer
		{
			std::string Clip;
			const ClipEntry* Entry = nullptr;
			ActionOptions Options;
			std::vector<float> Mask;
			double Phase = 0.0;
			double PreviousPhase = 0.0;
			// フェードの線形な進み具合です。出力の重みにはsmoothstepをかけます。
			float FadeLinear = 0.0f;
			bool Stopping = false;
			float StopFade = 0.2f;
			float MaskStrength = 1.0f;
			float MaskTarget = 1.0f;
			float MaskSmoothing = 0.15f;
			// コンボで次のクリップへつなぐときの、切り替え前の姿勢です。
			Pose Snapshot;
			float SwitchSeconds = 0.0f;
			float SwitchDuration = 0.0f;
			Pose LastPose;
			bool HasLastPose = false;
			aiVector3D RootStart{};
			// Lockで腰を固定する水平位置です(普通はRootStart、InPlace指定のクリップはバインドポーズ)。
			aiVector3D LockOrigin{};
			// 直前に評価したときの、打ち消す前の腰の位置(親空間)。移動量の差分を取るのに使います。
			aiVector3D PreviousRoot{};
			// 加算ブレンドの基準姿勢です。
			Pose Reference;
			float LayerWeight = 1.0f;
			bool Active = false;
		};

		const ClipEntry* FindClip(const std::string& name) const;
		void EvaluateBase(float step, Pose& out);
		void EvaluateBlendSpace(const BlendSpace1D& space, float step, Pose& out);
		bool EvaluateAction(ActionLayer& layer, float step, Pose& out);
		ActionLayer& LayerAt(int layer);
		const ActionLayer* FindLayer(int layer) const;
		void BeginLoopSeamBlend(double cycleSeconds);
		aiVector3D SampleRoot(const ActionLayer& layer, double ticks);
		aiVector3D SampleRoot(const ClipEntry& clip, double ticks) const;
		// ベース層用のサンプリングです。InPlaceのクリップは腰の水平位置をバインドポーズに固定します。
		void SampleBaseClip(const ClipEntry& clip, double ticks, Pose& out) const;
		aiVector3D Horizontal(const aiVector3D& value) const;

		Skeleton m_Skeleton;
		std::unordered_map<std::string, ClipEntry> m_Clips;
		std::unordered_map<std::string, BlendSpace1D> m_BlendSpaces;
		std::unordered_map<std::string, std::vector<std::pair<std::string, float>>> m_MaskDefinitions;
		std::unordered_map<std::string, float> m_Parameters;
		std::string m_RootBoneName = "mixamorig:Hips";
		int m_RootBone = -1;

		// ベース層
		std::string m_BaseName;
		const ClipEntry* m_BaseClip = nullptr;
		const BlendSpace1D* m_BaseSpace = nullptr;
		PlaybackRange m_Playback{};
		double m_Phase = 0.0;
		std::vector<double> m_FreePhases;
		float m_SmoothedParameter = 0.0f;
		std::vector<float> m_SpaceWeights;
		Pose m_BaseSnapshot;
		float m_BaseBlendSeconds = 0.0f;
		float m_BaseBlendDuration = 0.0f;
		float m_LoopBlendSeconds = 0.12f;
		Pose m_LastBasePose;
		bool m_HasBasePose = false;

		std::vector<ActionLayer> m_Layers;
		Pose m_Pose;
		aiVector3D m_RootMotion{};
		Pose m_Scratch;
		Pose m_Scratch2;
		bool m_HasPose = false;
	};
}
