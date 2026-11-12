#pragma once

#include "gameObject.h"
#include "world.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <typeindex>
#include <vector>

namespace Engine
{
	// 型名の文字列からGameObjectを生成するための登録表です。
	// ここへ登録した型だけが、シーンファイルへ保存され、エディタから追加できます。
	// カメラやUIのようにコードで生成するものは登録しません。
	class ObjectTypeRegistry
	{
	public:
		using Factory = GameObject* (*)(World&);

		// 同じ名前を二度登録しても何も起きません。Sceneの初期化から毎回呼んで構いません。
		template<typename T>
		static void Register(const char* name)
		{
			Add(name, typeid(T), [](World& world) -> GameObject* { return world.Spawn<T>(); });
		}

		// 未登録の名前ならnullptrを返します。
		static GameObject* Create(World& world, std::string_view name);
		// 登録されていない型ならnullptrを返します。
		static const char* FindName(const GameObject& gameObject);
		static std::vector<std::string> GetNames();

	private:
		static void Add(const char* name, std::type_index type, Factory factory);
	};

	// 登録済みの型のオブジェクトを、1行1項目のテキストで読み書きします。
	//
	//   object SlowBlock
	//   name 北の壁
	//   id 7
	//   position 0 2.5 20.5
	//   rotation 0 0 0
	//   scale 21 2.5 0.5
	//   bool Obstacle 0
	//   end
	//
	// 回転はラジアンです。transformの後ろはReflectが公開した値で、「型 名前 値」の順です。
	// ファイルにあってReflectにない値は無視し、Reflectにあってファイルにない値は初期値のままにします。
	// 値を足したり消したりしても、古いファイルがそのまま読めます。
	class SceneSerializer
	{
	public:
		using TypeFilter = std::function<bool(std::string_view typeName)>;

		// World内の登録済みオブジェクトをすべて書き出します(破棄待ちのものは除く)。
		static std::string Write(World& world);
		static std::string WriteObject(GameObject& gameObject);

		// テキストからオブジェクトを生成します。filterを渡すと、trueを返した型だけ生成します。
		// 未登録の型や壊れた行は飛ばして読み進め、1つでもあればfalseを返します。
		static bool Read(World& world, std::string_view text,
			std::vector<GameObject*>* created = nullptr, const TypeFilter& filter = nullptr);

		static bool SaveFile(World& world, const std::filesystem::path& path);
		static bool LoadFile(World& world, const std::filesystem::path& path);
		// ファイルの中身をそのまま返します。開けなければ空文字列です。
		static std::string ReadFileText(const std::filesystem::path& path);

		// 登録済みのオブジェクトをすべて破棄予約します。読み直す前に呼びます。
		static void DestroyAll(World& world);
	};
}
