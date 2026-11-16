#include "shaderGalleryUi.h"
#include "main.h"
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_impl_dx11.h"
#include "third_party/imgui/imgui_impl_win32.h"
#include "resourceManager.h"
#include "mouseInput.h"
#include <windows.h>
#include <windowsx.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace
{
GalleryUiState state;
bool initialized = false;
Engine::TextureHandle rampTexture;
constexpr const char* names[] = {
	"照明なし", "頂点平行光", "ピクセル平行光", "ブリン・フォン", "半球ライト",
	"点光源", "リムライト", "スポットライト", "クック・トランス", "Disney PBR",
	"トゥーン（段階）", "トゥーン（ランプ）", "トゥーン（背面輪郭）", "法線マップ", "Sample Stripes"
};
constexpr const char* effects[] = {
	"なし", "グレースケール", "モザイク", "4点平均モザイク", "円形モザイク",
	"ホラー風ノイズ", "ミップ確認", "図形生成", "RGBシフト", "ポスタライズ", "ブルーム"
};
}

GalleryUiState& GetGalleryUiState() { return state; }

bool InitializeGalleryUi()
{
	state = GalleryUiState{};
	state.Enabled[10] = true;
	state.Enabled[11] = true;
	state.Parameters[8] = { 0.35f, 0.5f, 0.0f, 0.0f };
	state.Parameters[9] = { 0.5f, 0.2f, 3.0f, 0.0f };
	// トゥーン3方式は SP31 のシェーダーを移植した sp31-* 定義と同じ意味・既定値です。
	state.Parameters[10] = { 0.4f, 0.7f, 0.25f, 0.3f };
	state.Parameters[11] = { 8.0f / 256.0f, 0.25f, 0.3f, 0.0f };
	state.Parameters[12] = { 8.0f / 256.0f, 0.25f, 0.3f, 0.08f };
	state.Parameters[13] = { 2.5f, 0.0f, 1.0f, 0.0f };
	state.Parameters[14] = { 12.0f, 0.75f, 0.7f, 0.0f };
	// SP31 のギャラリーと同じ初期ライトです。
	state.Light.Enable = TRUE;
	state.Light.Direction = { 0.0f, -1.0f, 0.0f, 0.0f };
	state.Light.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	state.Light.Ambient = { 0.16f, 0.16f, 0.16f, 1.0f };
	state.Light.Position = { 0.0f, 2.5f, -1.0f, 1.0f };
	state.Light.PointLightparam = { 10.0f, 3.0f, 0.0f, 0.0f };
	state.Light.SkyColor = { 0.12f, 0.17f, 0.3f, 1.0f };
	state.Light.GroundColor = { 0.12f, 0.08f, 0.05f, 1.0f };
	state.Light.GroundNormal = { 0.0f, 1.0f, 0.0f, 0.0f };
	// SP31 の BumpField3D と同じ専用ライト(真上 1.0 の点光源、到達距離 3000)。
	state.BumpLight.Enable = TRUE;
	state.BumpLight.Direction = { 0.0f, -1.0f, 0.0f, 0.0f };
	state.BumpLight.Position = { 0.0f, 1.0f, 0.0f, 1.0f };
	state.BumpLight.Diffuse = { 0.8f, 0.8f, 0.8f, 1.0f };
	state.BumpLight.Ambient = { 0.3f, 0.3f, 0.3f, 1.0f };
	state.BumpLight.PointLightparam = { 3000.0f, 0.0f, 0.0f, 0.0f };
	// y は床のスポットライトの縁をぼかす幅(円錐角に対する割合、縁の内側)です。
	state.Light.Angle = { XM_PIDIV4, 0.3f, 0.0f, 0.0f };
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	char fontPath[MAX_PATH]{};
	if (GetWindowsDirectoryA(fontPath, MAX_PATH))
	{
		strcat_s(fontPath, "\\Fonts\\meiryo.ttc");
		if (GetFileAttributesA(fontPath) != INVALID_FILE_ATTRIBUTES)
			io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	}
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	initialized = ImGui_ImplWin32_Init(GetWindow())
		&& ImGui_ImplDX11_Init(Engine::Renderer::GetDevice(), Engine::Renderer::GetDeviceContext());
	if (initialized) rampTexture = Engine::ResourceManager::LoadTexture(L"assets/texture/Toon2.png");
	if (initialized) Mouse_SetAutoCapture(false);
	if (!initialized) ShutdownGalleryUi();
	return initialized;
}

void ShutdownGalleryUi()
{
	rampTexture.reset();
	Mouse_SetAutoCapture(true);
	if (ImGui::GetCurrentContext() == nullptr) return;
	if (initialized) { ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); }
	ImGui::DestroyContext();
	initialized = false;
}

void BeginGalleryUiFrame()
{
	if (!initialized) return;
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	// Engine renders to a fixed 1280x720 back buffer, then DXGI scales it to the
	// fullscreen client area. Keep both ImGui layout and hit testing in that space.
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = { static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) };
	RECT client{};
	POINT cursor{};
	if (GetClientRect(GetWindow(), &client) && GetCursorPos(&cursor)
		&& ScreenToClient(GetWindow(), &cursor) && client.right > client.left && client.bottom > client.top)
	{
		const XMFLOAT2 position = ScaleGalleryMouseToCanvas(static_cast<float>(cursor.x),
			static_cast<float>(cursor.y), client.right - client.left, client.bottom - client.top);
		io.AddMousePosEvent(position.x, position.y);
	}
	ImGui::NewFrame();
	if (state.Playing) return;
	ImGui::SetNextWindowPos({ 12, 12 }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({ 350, 680 }, ImGuiCond_FirstUseEver);
	ImGui::Begin("シェーダー一覧");
	ImGui::TextUnformatted("表示するシェーダーを選択");
	if (ImGui::Button("すべて表示")) state.Enabled.fill(true);
	ImGui::SameLine();
	if (ImGui::Button("すべて非表示")) state.Enabled.fill(false);
	ImGui::BeginChild("modes", { 0, 270 }, true);
	for (int i = 0; i < static_cast<int>(state.Enabled.size()); ++i)
	{
		ImGui::PushID(i);
		ImGui::Checkbox("##enabled", &state.Enabled[i]);
		ImGui::SameLine();
		if (ImGui::Selectable(names[i], state.Selected == i)) state.Selected = i;
		ImGui::PopID();
	}
	ImGui::EndChild();
	ImGui::Checkbox("床を表示", &state.ShowFloor);
	ImGui::SameLine(); ImGui::Checkbox("法線マップ", &state.BumpFloor);
	ImGui::Checkbox("ランプ画像", &state.ShowRamp);
	ImGui::Checkbox("モデル名を表示", &state.ShowNames);
	ImGui::Checkbox("モデルを回転", &state.Animate);
	ImGui::TextDisabled("左ドラッグ: 回転 / 右ドラッグ: 平行移動");
	ImGui::TextDisabled("ホイール・W/S: 拡大縮小 / A/D: 回転");
	ImGui::Separator();
	ImGui::Text("編集中: %s", names[state.Selected]);
	if (ImGui::CollapsingHeader("ライト", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::SliderFloat3("位置", &state.Light.Position.x, -5.0f, 5.0f);
		ImGui::SliderFloat3("方向", &state.Light.Direction.x, -1.0f, 1.0f);
		ImGui::ColorEdit3("光の色", &state.Light.Diffuse.x);
		ImGui::ColorEdit3("環境光", &state.Light.Ambient.x);
		ImGui::SliderFloat("到達距離", &state.Light.PointLightparam.x, 0.1f, 20.0f);
		float degrees = XMConvertToDegrees(state.Light.Angle.x);
		if (ImGui::SliderFloat("スポット角度", &degrees, 1.0f, 90.0f)) state.Light.Angle.x = XMConvertToRadians(degrees);
		ImGui::SliderFloat("スポットの境界ぼかし", &state.Light.Angle.y, 0.0f, 1.0f);
		ImGui::SliderFloat("スポット減衰／リム", &state.Light.PointLightparam.y, 0.1f, 12.0f);
		ImGui::ColorEdit3("空の色", &state.Light.SkyColor.x);
		ImGui::ColorEdit3("地面の色", &state.Light.GroundColor.x);
	}
	if (ImGui::CollapsingHeader("シェーダー設定", ImGuiTreeNodeFlags_DefaultOpen))
	{
		auto& p = state.Parameters[state.Selected];
		if (state.Selected == 8 || state.Selected == 9)
		{
			ImGui::SliderFloat("粗さ", &p.x, 0.05f, 1.0f);
			ImGui::SliderFloat("金属度", &p.y, 0.0f, 1.0f);
			if (state.Selected == 9) ImGui::SliderFloat("ライト数", &p.z, 1.0f, 10.0f, "%.0f");
		}
		else if (state.Selected == 10)
		{
			ImGui::SliderFloat("暗部の境界", &p.x, 0.0f, p.y);
			ImGui::SliderFloat("明部の境界", &p.y, p.x, 1.0f);
			ImGui::SliderFloat("輪郭の幅", &p.z, 0.01f, 0.8f);
			ImGui::SliderFloat("輪郭の暗さ", &p.w, 0.0f, 1.0f);
		}
		else if (state.Selected == 11 || state.Selected == 12)
		{
			ImGui::SliderFloat("ランプの行", &p.x, 0.01f, 0.99f);
			ImGui::SliderFloat("輪郭の幅", &p.y, 0.01f, 0.8f);
			ImGui::SliderFloat("輪郭の暗さ", &p.z, 0.0f, 1.0f);
			if (state.Selected == 12) ImGui::SliderFloat("背面輪郭の幅", &p.w, 0.0f, 0.3f);
		}
		else if (state.Selected == 13) ImGui::SliderFloat("法線の強さ", &p.x, 0.0f, 6.0f);
		else if (state.Selected == 14)
		{
			ImGui::SliderFloat("縞の本数", &p.x, 1.0f, 40.0f);
			ImGui::SliderFloat("コントラスト", &p.y, 0.0f, 1.0f);
			ImGui::SliderFloat("照明との混合", &p.z, 0.0f, 1.0f);
			ImGui::SliderFloat("位相", &p.w, 0.0f, 1.0f);
		}
		else ImGui::TextUnformatted("ライト設定を変更してください。");
	}
	ImGui::End();

	ImGui::SetNextWindowPos({ static_cast<float>(SCREEN_WIDTH - 320), 12 }, ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({ 310, 420 }, ImGuiCond_FirstUseEver);
	ImGui::Begin("画面効果");
	ImGui::Combo("効果", &state.Post, effects, IM_ARRAYSIZE(effects));
	if (state.Post >= 2 && state.Post <= 4) ImGui::SliderFloat("ブロック幅", &state.BlockSize, 1.0f, 64.0f);
	if (state.Post == 4) ImGui::SliderFloat("円の半径", &state.CircleRadius, 0.05f, 0.6f);
	if (state.Post == 5) { ImGui::SliderFloat("ノイズ量", &state.Noise, 0.0f, 0.8f); ImGui::SliderFloat("コントラスト", &state.Contrast, 0.5f, 8.0f); }
	if (state.Post == 6) ImGui::SliderFloat("ミップレベル", &state.MipLevel, 0.0f, 9.0f);
	if (state.Post == 7) { const char* shapes[] = { "円", "円環", "ひし形", "ハート" }; ImGui::Combo("図形", &state.Shape, shapes, 4); ImGui::SliderFloat("大きさ", &state.ShapeScale, 0.1f, 1.0f); }
	if (state.Post == 8) { ImGui::SliderFloat("赤・青の横ずれ", &state.ShiftX, 0.0f, 0.5f); ImGui::SliderFloat("緑の縦ずれ", &state.ShiftY, -0.5f, 0.5f); }
	if (state.Post == 9) { ImGui::SliderFloat("階調数", &state.PosterLevels, 2.0f, 64.0f); ImGui::SliderFloat("ディザ量", &state.Dither, 0.0f, 1.0f); ImGui::Checkbox("グレースケール", &state.PosterGray); ImGui::SliderFloat("コントラスト", &state.PosterContrast, 0.5f, 4.0f); }
	if (state.Post == GalleryBloomPost)
	{
		ImGui::SliderFloat("輝度しきい値", &state.BloomLuminance, 0.0f, 1.0f);
		ImGui::SliderFloat("強さ1（細）", &state.BloomBlend[0], 0.0f, 4.0f);
		ImGui::SliderFloat("強さ2（中）", &state.BloomBlend[1], 0.0f, 4.0f);
		ImGui::SliderFloat("強さ3（広）", &state.BloomBlend[2], 0.0f, 4.0f);
	}
	if (state.Post != 0 && Engine::Renderer::GetPostSource() != nullptr)
	{
		ImGui::TextUnformatted("描画テクスチャ");
		ImGui::Image(reinterpret_cast<ImTextureID>(Engine::Renderer::GetPostSource()), { 270, 152 });
	}
	ImGui::End();

	if (state.BumpFloor)
	{
		// SP31 の BumpField3D と同じく、法線マップの床は専用ライトの位置だけを動かせる。
		ImGui::SetNextWindowPos({ 370, 110 }, ImGuiCond_FirstUseEver);
		ImGui::Begin("法線マップのライト");
		ImGui::SliderFloat("ライト X", &state.BumpLight.Position.x, -3.0f, 3.0f, "%.1f");
		ImGui::SliderFloat("ライト Y", &state.BumpLight.Position.y, -3.0f, 3.0f, "%.1f");
		ImGui::SliderFloat("ライト Z", &state.BumpLight.Position.z, -3.0f, 3.0f, "%.1f");
		ImGui::End();
	}

	ImGui::SetNextWindowPos({ 455, 12 }, ImGuiCond_FirstUseEver);
	ImGui::Begin("映像プレビュー");
	if (ImGui::Button("映像を再生（約22秒）")) state.StartPresentation = true;
	ImGui::TextDisabled("再生中はUI非表示 / Spaceで中止");
	ImGui::End();
	if (state.ShowRamp && rampTexture)
	{
		ImGui::SetNextWindowPos({ static_cast<float>(SCREEN_WIDTH - 180), static_cast<float>(SCREEN_HEIGHT - 210) }, ImGuiCond_FirstUseEver);
		ImGui::Begin("ランプ画像");
		ImGui::Image(reinterpret_cast<ImTextureID>(rampTexture->Get()), { 145, 145 });
		ImGui::End();
	}
}

void DrawGalleryUi()
{
	if (!initialized) return;
	if (state.ShowNames && !state.Playing)
	{
		const XMMATRIX projection = XMMatrixPerspectiveFovLH(GalleryFieldOfView,
			static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, GalleryNearClip, GalleryFarClip);
		for (int i = 0; i < static_cast<int>(state.Enabled.size()); ++i)
		{
			GalleryPlacement placement{};
			if (!GetGalleryPlacement(state, i, placement)) continue;
			const XMVECTOR projected = XMVector3Project(XMVectorSet(placement.Position.x,
				placement.LabelHeight, placement.Position.z, 1.0f),
				0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 1, projection, state.View, XMMatrixIdentity());
			XMFLOAT3 point{};
			XMStoreFloat3(&point, projected);
			if (point.z < 0.0f || point.z > 1.0f) continue;
			const ImVec2 size = ImGui::CalcTextSize(names[i]);
			ImDrawList* draw = ImGui::GetForegroundDrawList();
			draw->AddRectFilled({ point.x - size.x * 0.5f - 4.0f, point.y - 3.0f },
				{ point.x + size.x * 0.5f + 4.0f, point.y + size.y + 3.0f }, IM_COL32(10, 10, 15, 205), 3.0f);
			draw->AddText({ point.x - size.x * 0.5f, point.y }, IM_COL32(255, 255, 255, 255), names[i]);
		}
	}
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool HandleGalleryWindowMessage(unsigned int message, std::uintptr_t wParam, std::intptr_t lParam)
{
	if (initialized) ImGui_ImplWin32_WndProcHandler(GetWindow(), message, wParam, lParam);
	if (initialized && (message == WM_MOUSEMOVE || message == WM_NCMOUSEMOVE))
	{
		POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		if (message == WM_NCMOUSEMOVE) ScreenToClient(GetWindow(), &point);
		RECT client{};
		if (GetClientRect(GetWindow(), &client) && client.right > client.left && client.bottom > client.top)
		{
			const XMFLOAT2 position = ScaleGalleryMouseToCanvas(static_cast<float>(point.x),
				static_cast<float>(point.y), client.right - client.left, client.bottom - client.top);
			ImGui::GetIO().AddMousePosEvent(position.x, position.y);
		}
	}
	if (initialized && message == WM_MOUSEWHEEL && !ImGui::GetIO().WantCaptureMouse)
		state.WheelZoom += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
	return false;
}
