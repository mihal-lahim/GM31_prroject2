//--------------------------------------------------------------------------------------
// File: keyboard_utf8.cpp
//
// キーボードモジュール
//
//--------------------------------------------------------------------------------------
#include "keyboard_utf8.h"

#include <cassert>

static_assert(sizeof(Keyboard_State) == 256 / 8, "キーボード状態構造体のサイズ不一致");

static Keyboard_State gState = {};
static Keyboard_State gStateOld = {};
// ステップ間に押して離された短い入力を失わないための押下ラッチです。
// keycopy()(=シミュレーションステップが入力を消費した後)でクリアされます。
static Keyboard_State gStickyPressed = {};

namespace
{
    bool IsValidKey(int key)
    {
        return key >= 0 && key <= 0xfe;
    }

    unsigned int KeyBit(int key)
    {
        return 1u << (key & 0x1f);
    }
}

void keycopy()
{
    gStateOld = gState; // 前回のキー情報を保存
    gStickyPressed = {}; // 消費済みの押下ラッチをクリア
}

static void keyDown(int key)
{
    if (!IsValidKey(key))
    {
        return;
    }

    const unsigned int bit = KeyBit(key);
    // キーリピートのWM_KEYDOWNでは再ラッチしないよう、未押下→押下の遷移だけを記録します。
    if ((gState.bits[key >> 5] & bit) == 0)
    {
        gStickyPressed.bits[key >> 5] |= bit;
    }
    gState.bits[key >> 5] |= bit;
}

static void keyUp(int key)
{
    if (!IsValidKey(key))
    {
        return;
    }

    gState.bits[key >> 5] &= ~KeyBit(key);
}

void Keyboard_Initialize(void)
{
    Keyboard_Reset();
}

bool Keyboard_IsKeyDown(Keyboard_Keys key, const Keyboard_State* pState)
{
    const int keyValue = static_cast<int>(key);
    if (!pState || !IsValidKey(keyValue))
    {
        return false;
    }

    return (pState->bits[keyValue >> 5] & KeyBit(keyValue)) != 0;
}

// トリガー判定
bool Keyboard_IsKeyDownTrigger(Keyboard_Keys key)
{
    const int keyValue = static_cast<int>(key);
    if (!IsValidKey(keyValue))
    {
        return false;
    }

    const unsigned int bit = KeyBit(keyValue);
    const unsigned int current = gState.bits[keyValue >> 5] & bit;
    const unsigned int previous = gStateOld.bits[keyValue >> 5] & bit;
    const unsigned int sticky = gStickyPressed.bits[keyValue >> 5] & bit;
    // stickyは実際の0→1遷移を記録しているため優先します。これにより
    // 「押しっぱなし→ステップ間に離して再押下」や「ステップ間に押して離した」入力も
    // 1回だけ成立します(長押しはキーリピートを再ラッチしないため連発しません)。
    return sticky != 0 || (previous == 0 && current != 0);
}

bool Keyboard_IsKeyUp(Keyboard_Keys key, const Keyboard_State* pState)
{
    const int keyValue = static_cast<int>(key);
    if (!pState || !IsValidKey(keyValue))
    {
        return false;
    }

    return (pState->bits[keyValue >> 5] & KeyBit(keyValue)) == 0;
}

bool Keyboard_IsKeyDown(Keyboard_Keys key)
{
    return Keyboard_IsKeyDown(key, &gState);
}

bool Keyboard_IsKeyUp(Keyboard_Keys key)
{
    return Keyboard_IsKeyUp(key, &gState);
}

// 現在のキーボード状態を取得する
const Keyboard_State* Keyboard_GetState(void)
{
    return &gState;
}

const Keyboard_State* Keyboard_GetStateOld(void)
{
    return &gStateOld;
}

void Keyboard_Reset(void)
{
    gState = {};
    gStateOld = {};
    gStickyPressed = {};
}

// キーボード制御用のウィンドウメッセージ処理
void Keyboard_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    bool down = false;

    switch (message)
    {
    case WM_ACTIVATEAPP:
    // 同じアプリの別のウィンドウやダイアログへフォーカスが移ると WM_ACTIVATEAPP は来ず、
    // 離したキーの WM_KEYUP も届かないので、押したままに見えないよう同じく捨てます。
    case WM_KILLFOCUS:
        Keyboard_Reset();
        return;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        down = true;
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        break;

    default:
        return;
    }

    int vk = static_cast<int>(wParam);
    switch (vk)
    {
    case VK_SHIFT:
        vk = static_cast<int>(MapVirtualKey((static_cast<unsigned int>(lParam) & 0x00ff0000) >> 16u, MAPVK_VSC_TO_VK_EX));
        // スキャンコード0の合成イベントなどでは0が返ります。0のキーを押した扱いにせず左Shiftとみなします。
        if (vk == 0)
        {
            vk = VK_LSHIFT;
        }
        if (!down)
        {
            // 左Shiftと右Shiftの両方が同時に押された場合にクリアされるようにするための回避策
            keyUp(VK_LSHIFT);
            keyUp(VK_RSHIFT);
        }
        break;

    case VK_CONTROL:
        vk = (static_cast<UINT>(lParam) & 0x01000000) ? VK_RCONTROL : VK_LCONTROL;
        break;

    case VK_MENU:
        vk = (static_cast<UINT>(lParam) & 0x01000000) ? VK_RMENU : VK_LMENU;
        break;
    }

    if (down)
    {
        keyDown(vk);
    }
    else
    {
        keyUp(vk);
    }
}
