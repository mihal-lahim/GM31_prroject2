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
	// 通常攻撃は3段のコンボです。大剣の3連撃のクリップ(Akai_Slash.fbx)を3区間に分けて1段ずつ再生します。
	// Start/End/Hitは元クリップに対する割合(0〜1)です。攻撃の長さと当たる瞬間はこの区間と再生倍率から決まるので、
	// 斬る動きと当たり判定が常に一致します。
	float Combo1Start = 0.14f;
	float Combo1Hit = 0.19f;
	float Combo1End = 0.31f;
	float Combo2Start = 0.31f;
	float Combo2Hit = 0.475f;
	float Combo2End = 0.56f;
	float Combo3Start = 0.56f;
	float Combo3Hit = 0.71f;
	float Combo3End = 0.95f;
	float ComboPlaybackSpeed = 2.0f;
	// 攻撃が終わってからこの秒数以内に通常攻撃を出すと、次の段へつながります。
	float ComboChainSeconds = 0.3f;
	// 右手に持たせる剣(slowSword.obj、長さ1.4m)の見た目です。向きは指の骨から毎フレーム決めるので、
	// ここでは大きさと、握る位置のずれ(剣の軸方向・刃の向き・厚み方向、メートル)だけを調整します。
	float SwordScale = 1.0f;
	float SwordGripOffsetX = 0.0f;
	float SwordGripOffsetY = 0.0f;
	float SwordGripOffsetZ = 0.0f;
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
	// 移動入力があればその方向、なければ今の向きを基準にします。
	float AimAssistRange = 6.0f;
	float AimAssistHalfAngleDegrees = 50.0f;
	// 通常攻撃は寄せた敵をロックし、当たる瞬間まで向きを追わせながら踏み込みます。
	// 踏み込みは敵との隙間がNormalLungeGapになるまでで、最大NormalLungeMaxDistanceまでです。
	float NormalLungeMaxDistance = 2.2f;
	float NormalLungeGap = 0.7f;
	float AimTrackDegreesPerSecond = 720.0f;
	// 強攻撃が当たった瞬間のヒットストップです。0で無効になります。
	int StrongHitStopFrames = 9;
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
	// 通常攻撃3段目の打ち上げです。前へはほとんど飛ばさず、真上へ高く打ち上げます。
	// 頂点はLaunchMaxHeightの制限を受けません(AttackVerticalReach以下にすると地上から追撃できます)。
	float FinisherLaunchDistance = 0.5f;
	float FinisherLaunchHeight = 3.0f;
	float FinisherLaunchSeconds = 1.1f;
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

	// ---- 射撃型の敵 ----
	int GunnerHp = 2;
	float GunnerRadius = 0.4f;
	float GunnerMoveSpeed = 3.0f;
	// この距離の間に収まるよう、近づいたり離れたりします。
	float GunnerMinRange = 6.0f;
	float GunnerMaxRange = 12.0f;
	// 狙いを付けてから撃つまでの時間です。この間、照準線が見えます。
	float GunnerWindupSeconds = 0.75f;
	float GunnerCooldownSeconds = 1.8f;
	float BulletSpeed = 9.0f;
	float BulletRadius = 0.3f;
	float BulletLifeSeconds = 4.0f;
	int BulletDamage = 12;
	// 打ち返した弾の速さの倍率と、当たった敵へのダメージです。打ち返した弾は敵を吹き飛ばします。
	float DeflectSpeedScale = 1.8f;
	int DeflectDamage = 2;

	// ---- 重量型の敵 ----
	int BruteHp = 8;
	float BruteRadius = 0.7f;
	float BruteMoveSpeed = 2.4f;
	float BruteAttackRange = 2.6f;
	float BruteAttackHitRange = 3.4f;
	float BruteAttackHalfAngleDegrees = 85.0f;
	float BruteWindupSeconds = 0.95f;
	float BruteAttackCooldownSeconds = 1.6f;
	int BruteAttackDamage = 25;
	// 強攻撃をこの回数当てると装甲が割れて打ち上がります。予兆中に当てると一撃で割れます。
	int BruteArmor = 2;
	float BruteStaggerSeconds = 0.18f;
	// 吹き飛ぶ距離の倍率です。重いので短く飛びます。
	float BruteLaunchScale = 0.6f;

	// ---- 手触り ----
	// 通常攻撃1・2段目のヒットストップ(フレーム数)。3段目はFinisherHitStopFramesを使います。
	int NormalHitStopFrames = 7;
	int FinisherHitStopFrames = 11;
	int PursuitHitStopFrames = 9;
	int KillHitStopFrames = 13;
	// 画面揺れの強さ(m)と、揺れが収まる速さ(1/秒)です。
	float ShakeStrongHit = 0.12f;
	float ShakeWallBounce = 0.18f;
	float ShakeKill = 0.22f;
	float ShakePlayerDamage = 0.3f;
	float ShakeDecay = 7.0f;
	// 被弾した敵を白く光らせる時間(実時間)です。
	float HitFlashSeconds = 0.07f;

	// ---- スコアとコンボ ----
	// 最後に当ててからこの時間(ワールド時間)が過ぎるとコンボが切れます。減速中は切れにくくなります。
	float ComboWindowSeconds = 2.5f;
	// コンボがこの数増えるごとに、倍率がComboMultiplierStepずつ上がります。
	int ComboMultiplierEvery = 5;
	float ComboMultiplierStep = 0.5f;
	float ComboMultiplierMax = 4.0f;
	int ScoreNormalHit = 10;
	int ScoreStrongHit = 20;
	int ScorePursuit = 40;
	int ScoreWallBounce = 30;
	int ScoreDeflect = 50;
	int ScoreKillMelee = 100;
	int ScoreKillGunner = 150;
	int ScoreKillBrute = 300;
	int ScoreClearBonus = 500;

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
