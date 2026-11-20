#include "sceneEditor.h"

#include "main.h"
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_impl_dx11.h"
#include "third_party/imgui/imgui_impl_win32.h"
#include "mouseInput.h"
#include "propertyVisitor.h"
#include "renderer.h"
#include "sceneSerializer.h"
#include "world.h"

#include <windowsx.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace
{
	bool g_Initialized = false;
	bool g_FrameBegun = false;
	bool g_Active = false;
	void (*g_ExtraDraw)() = nullptr;
	World* g_World = nullptr;
	std::filesystem::path g_RuntimePath;
	std::filesystem::path g_SourcePath;
	SceneEditor::CameraPose g_Camera;
	// ポインタではなくIDで覚えます。選択中のオブジェクトが破棄・再生成されても安全です。
	std::uint64_t g_SelectedId = 0;
	bool g_ShowAll = false;
	std::string g_Status;

	constexpr float kNearClip = 0.1f;
	constexpr float kFarClip = 200.0f;

	// エンジンは固定の1280x720へ描いてから画面へ拡大するので、マウス座標も同じ空間へ直します。
	ImVec2 ClientToCanvas(float clientX, float clientY, int clientWidth, int clientHeight)
	{
		return { clientWidth > 0 ? clientX * SCREEN_WIDTH / clientWidth : 0.0f,
			clientHeight > 0 ? clientY * SCREEN_HEIGHT / clientHeight : 0.0f };
	}

	Vector3 CameraForward()
	{
		const float cosPitch = cosf(g_Camera.Pitch);
		return { sinf(g_Camera.Yaw) * cosPitch, -sinf(g_Camera.Pitch), cosf(g_Camera.Yaw) * cosPitch };
	}

	bool IsSaveable(const GameObject& gameObject)
	{
		return Engine::ObjectTypeRegistry::FindName(gameObject) != nullptr;
	}

	std::string DisplayName(const GameObject& gameObject)
	{
		if (!gameObject.GetName().empty())
		{
			return gameObject.GetName();
		}
		if (const char* typeName = Engine::ObjectTypeRegistry::FindName(gameObject))
		{
			return typeName;
		}
		return typeid(gameObject).name();
	}

	// ワールド座標を画面(1280x720)座標へ。カメラの後ろにあればfalseです。
	bool WorldToScreen(const Vector3& position, ImVec2& screen)
	{
		const Vector3 forward = CameraForward();
		const XMMATRIX view = XMMatrixLookToLH(
			XMVectorSet(g_Camera.Position.x, g_Camera.Position.y, g_Camera.Position.z, 1.0f),
			XMVectorSet(forward.x, forward.y, forward.z, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
		const XMMATRIX projection = XMMatrixPerspectiveFovLH(XMConvertToRadians(g_Camera.FovDegrees),
			static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, kNearClip, kFarClip);
		XMFLOAT4 clip{};
		XMStoreFloat4(&clip, XMVector4Transform(XMVectorSet(position.x, position.y, position.z, 1.0f), view * projection));
		if (clip.w <= kNearClip)
		{
			return false;
		}
		screen.x = (clip.x / clip.w * 0.5f + 0.5f) * SCREEN_WIDTH;
		screen.y = (0.5f - clip.y / clip.w * 0.5f) * SCREEN_HEIGHT;
		return true;
	}

	// 右ドラッグで見回し、WASDで水平移動、Q/Eで上下、Shiftで加速、ホイールで前後します。
	void UpdateCamera()
	{
		const ImGuiIO& io = ImGui::GetIO();
		if (!io.WantCaptureMouse)
		{
			if (ImGui::IsMouseDown(ImGuiMouseButton_Right))
			{
				g_Camera.Yaw += io.MouseDelta.x * 0.005f;
				g_Camera.Pitch = std::clamp(g_Camera.Pitch + io.MouseDelta.y * 0.005f, -1.5f, 1.5f);
			}
			if (io.MouseWheel != 0.0f)
			{
				g_Camera.Position += CameraForward() * (io.MouseWheel * 2.0f);
			}
		}
		if (io.WantTextInput)
		{
			return;
		}
		const Vector3 forward{ sinf(g_Camera.Yaw), 0.0f, cosf(g_Camera.Yaw) };
		const Vector3 right{ cosf(g_Camera.Yaw), 0.0f, -sinf(g_Camera.Yaw) };
		Vector3 move{ 0.0f, 0.0f, 0.0f };
		if (ImGui::IsKeyDown(ImGuiKey_W)) move += forward;
		if (ImGui::IsKeyDown(ImGuiKey_S)) move -= forward;
		if (ImGui::IsKeyDown(ImGuiKey_D)) move += right;
		if (ImGui::IsKeyDown(ImGuiKey_A)) move -= right;
		if (ImGui::IsKeyDown(ImGuiKey_E)) move.y += 1.0f;
		if (ImGui::IsKeyDown(ImGuiKey_Q)) move.y -= 1.0f;
		const float speed = ImGui::IsKeyDown(ImGuiKey_LeftShift) ? 36.0f : 12.0f;
		g_Camera.Position += move * (speed * io.DeltaTime);
	}

	// 保存対象のオブジェクトの位置に点を打ち、クリックで一番近いものを選びます。
	// コライダーを持たないオブジェクトも選べるよう、画面上の距離で決めます。
	void DrawMarkersAndPick()
	{
		constexpr float kPickRadius = 28.0f;
		const ImGuiIO& io = ImGui::GetIO();
		const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse;
		ImDrawList* draw = ImGui::GetBackgroundDrawList();
		float bestDistance = kPickRadius;
		std::uint64_t picked = 0;

		for (const auto& gameObject : g_World->GetObjects())
		{
			if (gameObject->IsPendingDestroy() || !IsSaveable(*gameObject))
			{
				continue;
			}
			ImVec2 screen;
			if (!WorldToScreen(gameObject->GetPosition(), screen))
			{
				continue;
			}
			const bool selected = gameObject->GetId() == g_SelectedId;
			draw->AddCircleFilled(screen, selected ? 6.0f : 4.0f,
				selected ? IM_COL32(255, 220, 60, 255) : IM_COL32(255, 255, 255, 150));
			if (selected)
			{
				draw->AddCircle(screen, 14.0f, IM_COL32(255, 220, 60, 255), 24, 2.0f);
				const std::string label = DisplayName(*gameObject);
				draw->AddText({ screen.x + 18.0f, screen.y - 10.0f }, IM_COL32(255, 220, 60, 255), label.c_str());
			}
			if (clicked)
			{
				const float distance = hypotf(screen.x - io.MousePos.x, screen.y - io.MousePos.y);
				if (distance < bestDistance)
				{
					bestDistance = distance;
					picked = gameObject->GetId();
				}
			}
		}
		if (clicked)
		{
			g_SelectedId = picked;
		}
	}

	void Save()
	{
		bool saved = !g_RuntimePath.empty() && Engine::SceneSerializer::SaveFile(*g_World, g_RuntimePath);
		if (saved && !g_SourcePath.empty())
		{
			if (Engine::SceneSerializer::SaveFile(*g_World, g_SourcePath))
			{
				g_Status = "保存しました(ソースと実行フォルダ)";
				return;
			}
			g_Status = "実行フォルダへは保存、ソース側は失敗";
			return;
		}
		g_Status = saved ? "保存しました(実行フォルダのみ)" : "保存に失敗しました";
	}

	void ReloadFromFile()
	{
		// ソース側があればそちらを正とします。実行フォルダの方はビルドで上書きされる複製です。
		std::error_code error;
		const std::filesystem::path& path = !g_SourcePath.empty() && std::filesystem::exists(g_SourcePath, error)
			? g_SourcePath : g_RuntimePath;
		const std::string text = Engine::SceneSerializer::ReadFileText(path);
		if (text.empty())
		{
			g_Status = "ファイルを読めませんでした";
			return;
		}
		Engine::SceneSerializer::DestroyAll(*g_World);
		g_Status = Engine::SceneSerializer::Read(*g_World, text)
			? "ファイルから読み直しました" : "読み直しました(読めない行あり)";
	}

	void DrawHierarchy()
	{
		ImGui::SetNextWindowPos({ 12.0f, 12.0f }, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize({ 300.0f, 470.0f }, ImGuiCond_FirstUseEver);
		ImGui::Begin("階層");
		if (ImGui::Button("保存")) Save();
		ImGui::SameLine();
		if (ImGui::Button("ファイルから読み直す")) ReloadFromFile();
		if (!g_Status.empty())
		{
			ImGui::TextWrapped("%s", g_Status.c_str());
		}

		if (ImGui::BeginCombo("##add", "追加する型を選ぶ"))
		{
			for (const std::string& typeName : Engine::ObjectTypeRegistry::GetNames())
			{
				if (!ImGui::Selectable(typeName.c_str()))
				{
					continue;
				}
				if (GameObject* created = Engine::ObjectTypeRegistry::Create(*g_World, typeName))
				{
					// 視線の先の床の高さへ置きます。
					Vector3 position = g_Camera.Position + CameraForward() * 8.0f;
					position.y = 0.0f;
					created->SetPosition(position);
					created->OnPropertiesChanged();
					g_SelectedId = created->GetId();
				}
			}
			ImGui::EndCombo();
		}

		GameObject* selected = g_World->FindById(g_SelectedId);
		const bool canEdit = selected != nullptr && IsSaveable(*selected);
		ImGui::BeginDisabled(!canEdit);
		if (ImGui::Button("複製") && canEdit)
		{
			// 1件分を書き出して読み戻します。IDは衝突するので、新しい番号が振られます。
			std::vector<GameObject*> created;
			Engine::SceneSerializer::Read(*g_World, Engine::SceneSerializer::WriteObject(*selected), &created);
			if (!created.empty())
			{
				created[0]->SetPosition(selected->GetPosition() + Vector3(1.0f, 0.0f, 1.0f));
				created[0]->OnPropertiesChanged();
				g_SelectedId = created[0]->GetId();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("削除") && canEdit)
		{
			selected->MarkForDestroy();
			g_SelectedId = 0;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::Checkbox("すべて表示", &g_ShowAll);
		ImGui::Separator();

		ImGui::BeginChild("objects");
		for (const auto& gameObject : g_World->GetObjects())
		{
			if (gameObject->IsPendingDestroy() || (!g_ShowAll && !IsSaveable(*gameObject)))
			{
				continue;
			}
			// 同じ名前が並んでも区別できるよう、IDをウィジェットの識別子に使います。
			ImGui::PushID(static_cast<int>(gameObject->GetId()));
			const std::string label = DisplayName(*gameObject);
			if (ImGui::Selectable(label.c_str(), gameObject->GetId() == g_SelectedId))
			{
				g_SelectedId = gameObject->GetId();
			}
			ImGui::PopID();
		}
		ImGui::EndChild();
		ImGui::End();
	}

	// Reflectが公開した値を、型に合った入力欄として並べます。
	class InspectorVisitor final : public Engine::PropertyVisitor
	{
	public:
		bool Changed = false;

		void Visit(const char* name, float& value) override { Changed |= ImGui::DragFloat(name, &value, 0.05f); }
		void Visit(const char* name, int& value) override { Changed |= ImGui::DragInt(name, &value, 0.1f); }
		void Visit(const char* name, bool& value) override { Changed |= ImGui::Checkbox(name, &value); }
		void Visit(const char* name, Vector3& value) override { Changed |= ImGui::DragFloat3(name, &value.x, 0.05f); }
		void Visit(const char* name, std::string& value) override
		{
			char buffer[256]{};
			strncpy_s(buffer, value.c_str(), _TRUNCATE);
			if (ImGui::InputText(name, buffer, sizeof(buffer)))
			{
				value = buffer;
				Changed = true;
			}
		}
	};

	void DrawInspector()
	{
		ImGui::SetNextWindowPos({ SCREEN_WIDTH - 352.0f, 12.0f }, ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize({ 340.0f, 470.0f }, ImGuiCond_FirstUseEver);
		ImGui::Begin("インスペクタ");
		GameObject* selected = g_World->FindById(g_SelectedId);
		if (selected == nullptr)
		{
			ImGui::TextWrapped("階層の一覧か、画面上の点をクリックして選択します。");
			ImGui::End();
			return;
		}

		const char* typeName = Engine::ObjectTypeRegistry::FindName(*selected);
		ImGui::Text("型: %s", typeName != nullptr ? typeName : typeid(*selected).name());
		ImGui::Text("ID: %llu", static_cast<unsigned long long>(selected->GetId()));
		if (typeName == nullptr)
		{
			ImGui::TextWrapped("この型は登録されていないので、シーンファイルには保存されません。");
		}

		char nameBuffer[128]{};
		strncpy_s(nameBuffer, selected->GetName().c_str(), _TRUNCATE);
		if (ImGui::InputText("名前", nameBuffer, sizeof(nameBuffer)))
		{
			selected->SetName(nameBuffer);
		}

		// 値の変更は必ずSetterを通します。静的コライダーの空間インデックスがそこで更新されます。
		bool changed = false;
		Vector3 position = selected->GetPosition();
		if (ImGui::DragFloat3("位置", &position.x, 0.05f))
		{
			selected->SetPosition(position);
			changed = true;
		}
		const Vector3 rotation = selected->GetRotation();
		float degrees[3]{ XMConvertToDegrees(rotation.x), XMConvertToDegrees(rotation.y), XMConvertToDegrees(rotation.z) };
		if (ImGui::DragFloat3("回転(度)", degrees, 1.0f))
		{
			selected->SetRotation({ XMConvertToRadians(degrees[0]), XMConvertToRadians(degrees[1]), XMConvertToRadians(degrees[2]) });
			changed = true;
		}
		Vector3 scale = selected->GetScale();
		if (ImGui::DragFloat3("スケール", &scale.x, 0.05f, 0.01f, 1000.0f))
		{
			selected->SetScale(scale);
			changed = true;
		}

		ImGui::SeparatorText("プロパティ");
		InspectorVisitor visitor;
		selected->Reflect(visitor);
		if (changed || visitor.Changed)
		{
			selected->OnPropertiesChanged();
		}

		ImGui::SeparatorText("コンポーネント");
		for (const auto& component : selected->GetComponents())
		{
			ImGui::BulletText("%s", typeid(*component).name());
		}
		ImGui::End();
	}
}

namespace SceneEditor
{
bool InitializeUi()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	// ウィンドウ配置を実行フォルダへ書き出さないようにします。
	io.IniFilename = nullptr;
	char fontPath[MAX_PATH]{};
	if (GetWindowsDirectoryA(fontPath, MAX_PATH))
	{
		strcat_s(fontPath, "\\Fonts\\meiryo.ttc");
		if (GetFileAttributesA(fontPath) != INVALID_FILE_ATTRIBUTES)
		{
			io.Fonts->AddFontFromFileTTF(fontPath, 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
		}
	}
	// 編集していない間は、ゲームが隠したカーソルをImGuiが戻さないようにします。
	io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	g_Initialized = ImGui_ImplWin32_Init(GetWindow())
		&& ImGui_ImplDX11_Init(Engine::Renderer::GetDevice(), Engine::Renderer::GetDeviceContext());
	if (!g_Initialized)
	{
		ShutdownUi();
	}
	return g_Initialized;
}

void ShutdownUi()
{
	// 終了処理の途中なので、カーソルを捕捉し直さずに状態だけ戻します。
	if (g_Active)
	{
		g_Active = false;
		Mouse_SetAutoCapture(true);
	}
	g_World = nullptr;
	if (ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}
	if (g_Initialized)
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
	}
	ImGui::DestroyContext();
	g_Initialized = false;
	g_FrameBegun = false;
}

void BeginUiFrame()
{
	if (!g_Initialized)
	{
		return;
	}
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = { static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT) };
	RECT client{};
	POINT cursor{};
	if (GetClientRect(GetWindow(), &client) && GetCursorPos(&cursor)
		&& ScreenToClient(GetWindow(), &cursor) && client.right > client.left && client.bottom > client.top)
	{
		const ImVec2 position = ClientToCanvas(static_cast<float>(cursor.x), static_cast<float>(cursor.y),
			client.right - client.left, client.bottom - client.top);
		io.AddMousePosEvent(position.x, position.y);
	}
	ImGui::NewFrame();
	g_FrameBegun = true;
}

void DrawUi()
{
	// レンダーテストのようにBeginUiFrameを通らない描画では、何もしません。
	if (!g_Initialized || !g_FrameBegun)
	{
		return;
	}
	g_FrameBegun = false;
	if (g_Active && g_World != nullptr)
	{
		UpdateCamera();
		DrawMarkersAndPick();
		DrawHierarchy();
		DrawInspector();
	}
	// アプリ独自のウィンドウ(調整パネルなど)は、編集の有無に関係なくここで描きます。
	if (g_ExtraDraw != nullptr)
	{
		g_ExtraDraw();
	}
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void SetExtraDrawCallback(void (*callback)())
{
	g_ExtraDraw = callback;
}

bool HandleWindowMessage(unsigned int message, std::uintptr_t wParam, std::intptr_t lParam)
{
	if (!g_Initialized)
	{
		return false;
	}
	ImGui_ImplWin32_WndProcHandler(GetWindow(), message, wParam, lParam);
	if (message == WM_MOUSEMOVE || message == WM_NCMOUSEMOVE)
	{
		POINT point{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
		if (message == WM_NCMOUSEMOVE)
		{
			ScreenToClient(GetWindow(), &point);
		}
		RECT client{};
		if (GetClientRect(GetWindow(), &client) && client.right > client.left && client.bottom > client.top)
		{
			const ImVec2 position = ClientToCanvas(static_cast<float>(point.x), static_cast<float>(point.y),
				client.right - client.left, client.bottom - client.top);
			ImGui::GetIO().AddMousePosEvent(position.x, position.y);
		}
	}
	return false;
}

void SetWorld(World* world)
{
	g_World = world;
}

void SetScenePaths(const std::filesystem::path& runtimePath, const std::filesystem::path& sourcePath)
{
	g_RuntimePath = runtimePath;
	g_SourcePath = sourcePath;
}

void SetActive(bool active)
{
	if (g_Active == active)
	{
		return;
	}
	g_Active = active;
	g_Status.clear();
	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiIO& io = ImGui::GetIO();
		if (active) io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
		else io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	}
	// 編集中はカーソルを自由にし、戻るときはゲーム用の捕捉へ戻します。
	if (active)
	{
		Mouse_SetAutoCapture(false);
	}
	else
	{
		Mouse_SetAutoCapture(true);
		Mouse_Reset();
		// 自分のウィンドウが前面にあるときだけ捕捉します(非表示で動くテスト中に画面のカーソルを奪わないため)。
		if (GetForegroundWindow() == GetWindow())
		{
			Mouse_SuppressUntilButtonsReleased();
			Mouse_SetCursorCaptured(true);
		}
	}
}

bool IsActive()
{
	return g_Active;
}

bool WantsKeyboard()
{
	return g_Active && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantTextInput;
}

void Select(std::uint64_t objectId)
{
	g_SelectedId = objectId;
}

const CameraPose& GetCamera()
{
	return g_Camera;
}

void SetCamera(const CameraPose& pose)
{
	g_Camera = pose;
}

std::filesystem::path FindSourcePath(const std::filesystem::path& relativePath, const char* markerFile)
{
	std::error_code error;
	std::filesystem::path directory = std::filesystem::current_path(error);
	while (!error && !directory.empty())
	{
		if (std::filesystem::exists(directory / markerFile, error))
		{
			return directory / relativePath;
		}
		const std::filesystem::path parent = directory.parent_path();
		if (parent == directory)
		{
			break;
		}
		directory = parent;
	}
	return {};
}
}
