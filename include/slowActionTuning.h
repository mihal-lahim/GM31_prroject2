#pragma once

// SlowAction(時間減速アクション試作)の調整値です。
// 単位は距離=m、時間=秒、速さ=m/s。角度は名前にDegreesと付くものだけ度です。
// 既定値はここに書き、assets/config/slow-gameplay.cfg に同名のキー(slow.<メンバー名>)が
// あればそちらで上書きします。再コンパイルなしで手触りを調整できます。
struct SlowActionTuning
{
	// ---- プレイヤー移動 ----
	float WalkSpeed = 6.0f;
	float WalkAcceleration = 70.0f;
	float DashSpeed = 16.0f;
	// ダッシュ中の方向転換の追従率(1/秒)です。小さいほど慣性が強く、大回りになります。
	float DashSteer = 9.0f;
	// 向きを移動方向へ回す速さ(ラジアン/秒)です。
	float TurnSpeed = 25.0f;
	float PlayerRadius = 0.4f;
	float PlayerHeight = 1.8f;
	int PlayerMaxHp = 100;

	// ---- 攻撃 ----
	// 左クリックをこの秒数以上押してから離すと強攻撃になります。
	float StrongHoldSeconds = 0.25f;
	// 攻撃終了前のこの秒数の間に離した入力は、次の攻撃として予約します。
	float AttackBufferSeconds = 0.2f;
	float NormalDuration = 0.26f;
	float NormalHitDelay = 0.05f;
	float NormalRange = 2.8f;
	float NormalHalfAngleDegrees = 40.0f;
	int NormalDamage = 1;
	float StrongDuration = 0.42f;
	float StrongHitDelay = 0.08f;
	float StrongRange = 4.2f;
	float StrongHalfAngleDegrees = 75.0f;
	int StrongDamage = 1;
	// 足元から上下この範囲にいる敵へ届きます。吹き飛び中の敵を地上から追撃できる高さです。
	float AttackVerticalReach = 3.2f;
	// 向きの前方この範囲・角度に敵がいれば、攻撃方向をその敵へ寄せます。
	float AimAssistRange = 6.0f;
	float AimAssistHalfAngleDegrees = 40.0f;
	// 強攻撃が当たった瞬間のヒットストップです。0で無効になります。
	int StrongHitStopFrames = 3;
	float HitStopScale = 0.1f;

	// ---- 時間減速 ----
	float SlowTimeScale = 0.35f;
	// 通常速度と減速を行き来する補間時間です。
	float SlowBlendSeconds = 0.08f;
	float GaugeMax = 100.0f;
	float GaugeDrainPerSecond = 22.0f;
	float GaugeGainNormalHit = 5.0f;
	float GaugeGainStrongHit = 8.0f;
	float GaugeGainPursuit = 18.0f;

	// ---- 吹き飛ばし ----
	float LaunchDistance = 6.0f;
	float LaunchSeconds = 1.2f;
	// 地上から吹き飛ばしたときの頂点の高さです。
	float LaunchHeight = 1.8f;
	// 空中で追撃を重ねても、頂点はこの高さまでしか上がりません。
	float LaunchMaxHeight = 2.4f;
	// 吹き飛びの終わりに初速の何割を失うか(0〜1)。大きいほど出だしが速く、終わりが遅くなります。
	float LaunchSpeedFalloff = 0.8f;
	int MaxWallBounces = 1;
	// 壁で反射した直後に空中へ留まる時間(ワールド時間)です。
	float WallBounceHangSeconds = 0.3f;
	// 着地後の受け身の時間です。
	float RecoverSeconds = 0.45f;

	// ---- 敵 ----
	int EnemyHp = 3;
	float EnemyRadius = 0.45f;
	float EnemyMoveSpeed = 3.5f;
	float EnemyDetectRange = 16.0f;
	// この距離まで近づくと予兆に入ります。
	float EnemyAttackRange = 1.9f;
	// 予兆の後、この距離・角度の中にプレイヤーがいれば当たります。
	float EnemyAttackHitRange = 2.4f;
	float EnemyAttackHalfAngleDegrees = 50.0f;
	float EnemyWindupSeconds = 0.55f;
	float EnemyAttackCooldownSeconds = 1.2f;
	int EnemyAttackDamage = 10;
	// 通常攻撃を地上で受けたときのひるみ時間と、押される距離です。
	float EnemyStaggerSeconds = 0.3f;
	float EnemyStaggerPush = 0.8f;
	// 全滅してから次の一団が出るまでの実時間です(テスト用の再配置で、ウェーブではありません)。
	float EnemyRespawnSeconds = 2.5f;

	// ---- カメラ ----
	float CameraDistance = 7.5f;
	float CameraMinDistance = 1.2f;
	float CameraFocusHeight = 1.5f;
	float CameraDefaultPitchDegrees = 18.0f;
	float CameraMinPitchDegrees = -10.0f;
	float CameraMaxPitchDegrees = 70.0f;
	float CameraFovDegrees = 62.0f;
	float CameraDashFovDegrees = 70.0f;
	float CameraFovBlendRate = 6.0f;
	// 注視点がプレイヤーへ追いつく率(1/秒)です。ダッシュ中は小さくして少し遅らせます。
	float CameraFollowRate = 22.0f;
	float CameraDashFollowRate = 12.0f;
	// 強攻撃で吹き飛ばした敵の方へ向きを寄せる量です。マウスを動かすと中断します。
	float CameraAssistFraction = 0.5f;
	float CameraAssistMaxDegrees = 18.0f;
	float CameraAssistDegreesPerSecond = 70.0f;
	// 壁に遮られたとき、当たった位置からこの距離だけ手前へ寄せます。
	float CameraWallMargin = 0.3f;
};

const SlowActionTuning& GetSlowActionTuning();
