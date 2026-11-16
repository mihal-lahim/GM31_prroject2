#pragma once

#include "Vector3.h"

#include <string>

namespace Engine
{
	// GameObjectが「外から見せたい値」を1つずつ渡す相手です。
	// 同じReflectの記述が、ファイルへの保存・ファイルからの読込・エディタのインスペクタ表示の
	// すべてに使われます。値は参照で渡すので、Visitor側が書き換えることもできます。
	//
	// nameはファイルのキーにもなるので、空白を含まない英数字にし、一度決めたら変えないでください。
	class PropertyVisitor
	{
	public:
		virtual ~PropertyVisitor() = default;

		virtual void Visit(const char* name, float& value) = 0;
		virtual void Visit(const char* name, int& value) = 0;
		virtual void Visit(const char* name, bool& value) = 0;
		virtual void Visit(const char* name, Vector3& value) = 0;
		virtual void Visit(const char* name, std::string& value) = 0;
	};
}
