#include "sceneSerializer.h"

#include "propertyVisitor.h"

#include <charconv>
#include <climits>
#include <fstream>
#include <sstream>

namespace Engine
{
namespace
{
	struct RegisteredType
	{
		std::string Name;
		std::type_index Type;
		ObjectTypeRegistry::Factory Create;
	};

	std::vector<RegisteredType>& Types()
	{
		static std::vector<RegisteredType> types;
		return types;
	}

	std::string_view Trim(std::string_view text)
	{
		const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
		while (!text.empty() && isSpace(text.front())) text.remove_prefix(1);
		while (!text.empty() && isSpace(text.back())) text.remove_suffix(1);
		return text;
	}

	// 先頭の単語を取り出し、textを残りへ進めます。
	std::string_view TakeWord(std::string_view& text)
	{
		text = Trim(text);
		const std::size_t end = text.find_first_of(" \t");
		const std::string_view word = text.substr(0, end);
		text = end == std::string_view::npos ? std::string_view{} : Trim(text.substr(end));
		return word;
	}

	// 書いた値をそのまま読み戻せる桁数で出します。
	std::string FormatFloat(float value)
	{
		char buffer[32]{};
		snprintf(buffer, sizeof(buffer), "%.9g", static_cast<double>(value));
		return buffer;
	}

	std::string FormatVector(const Vector3& value)
	{
		return FormatFloat(value.x) + " " + FormatFloat(value.y) + " " + FormatFloat(value.z);
	}

	bool ParseFloat(std::string_view text, float& value)
	{
		text = Trim(text);
		float parsed = 0.0f;
		const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || !std::isfinite(parsed))
		{
			return false;
		}
		value = parsed;
		return true;
	}

	bool ParseInt(std::string_view text, long long& value)
	{
		text = Trim(text);
		long long parsed = 0;
		const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
		if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
		{
			return false;
		}
		value = parsed;
		return true;
	}

	bool ParseVector(std::string_view text, Vector3& value)
	{
		Vector3 parsed;
		if (!ParseFloat(TakeWord(text), parsed.x) || !ParseFloat(TakeWord(text), parsed.y)
			|| !ParseFloat(TakeWord(text), parsed.z) || !text.empty())
		{
			return false;
		}
		value = parsed;
		return true;
	}

	// 1行に収めるため、改行だけは空白へ置き換えます。
	std::string SingleLine(std::string text)
	{
		std::replace(text.begin(), text.end(), '\n', ' ');
		std::replace(text.begin(), text.end(), '\r', ' ');
		return text;
	}

	class WriteVisitor final : public PropertyVisitor
	{
	public:
		explicit WriteVisitor(std::string& out) : m_Out(out) {}

		void Visit(const char* name, float& value) override { Line("float", name, FormatFloat(value)); }
		void Visit(const char* name, int& value) override { Line("int", name, std::to_string(value)); }
		void Visit(const char* name, bool& value) override { Line("bool", name, value ? "1" : "0"); }
		void Visit(const char* name, Vector3& value) override { Line("vec3", name, FormatVector(value)); }
		void Visit(const char* name, std::string& value) override { Line("string", name, SingleLine(value)); }

	private:
		std::string& m_Out;

		void Line(const char* kind, const char* name, const std::string& value)
		{
			m_Out += kind;
			m_Out += ' ';
			m_Out += name;
			m_Out += ' ';
			m_Out += value;
			m_Out += '\n';
		}
	};

	struct StoredProperty
	{
		std::string_view Kind;
		std::string_view Name;
		std::string_view Value;
	};

	// ファイルにあった値のうち、名前と型の両方が合うものだけを書き込みます。
	class ReadVisitor final : public PropertyVisitor
	{
	public:
		explicit ReadVisitor(const std::vector<StoredProperty>& properties) : m_Properties(properties) {}
		bool HadError() const { return m_HadError; }

		void Visit(const char* name, float& value) override
		{
			if (const StoredProperty* stored = Find("float", name))
			{
				m_HadError |= !ParseFloat(stored->Value, value);
			}
		}
		void Visit(const char* name, int& value) override
		{
			if (const StoredProperty* stored = Find("int", name))
			{
				long long parsed = 0;
				if (ParseInt(stored->Value, parsed) && parsed >= INT_MIN && parsed <= INT_MAX)
				{
					value = static_cast<int>(parsed);
				}
				else
				{
					m_HadError = true;
				}
			}
		}
		void Visit(const char* name, bool& value) override
		{
			if (const StoredProperty* stored = Find("bool", name))
			{
				long long parsed = 0;
				if (ParseInt(stored->Value, parsed))
				{
					value = parsed != 0;
				}
				else
				{
					m_HadError = true;
				}
			}
		}
		void Visit(const char* name, Vector3& value) override
		{
			if (const StoredProperty* stored = Find("vec3", name))
			{
				m_HadError |= !ParseVector(stored->Value, value);
			}
		}
		void Visit(const char* name, std::string& value) override
		{
			if (const StoredProperty* stored = Find("string", name))
			{
				value = std::string(stored->Value);
			}
		}

	private:
		const std::vector<StoredProperty>& m_Properties;
		bool m_HadError = false;

		const StoredProperty* Find(std::string_view kind, std::string_view name) const
		{
			for (const StoredProperty& property : m_Properties)
			{
				if (property.Kind == kind && property.Name == name)
				{
					return &property;
				}
			}
			return nullptr;
		}
	};

	// "object"から"end"までの1件分です。
	struct StoredObject
	{
		std::string_view Type;
		std::string_view Name;
		std::uint64_t Id = 0;
		Vector3 Position{ 0.0f, 0.0f, 0.0f };
		Vector3 Rotation{ 0.0f, 0.0f, 0.0f };
		Vector3 Scale{ 1.0f, 1.0f, 1.0f };
		std::vector<StoredProperty> Properties;
	};

	bool IsPropertyKind(std::string_view kind)
	{
		return kind == "float" || kind == "int" || kind == "bool" || kind == "vec3" || kind == "string";
	}
}

void ObjectTypeRegistry::Add(const char* name, std::type_index type, Factory factory)
{
	for (const RegisteredType& registered : Types())
	{
		if (registered.Name == name)
		{
			return;
		}
	}
	Types().push_back({ name, type, factory });
}

GameObject* ObjectTypeRegistry::Create(World& world, std::string_view name)
{
	for (const RegisteredType& registered : Types())
	{
		if (registered.Name == name)
		{
			return registered.Create(world);
		}
	}
	return nullptr;
}

const char* ObjectTypeRegistry::FindName(const GameObject& gameObject)
{
	const std::type_index type = typeid(gameObject);
	for (const RegisteredType& registered : Types())
	{
		if (registered.Type == type)
		{
			return registered.Name.c_str();
		}
	}
	return nullptr;
}

std::vector<std::string> ObjectTypeRegistry::GetNames()
{
	std::vector<std::string> names;
	for (const RegisteredType& registered : Types())
	{
		names.push_back(registered.Name);
	}
	return names;
}

std::string SceneSerializer::WriteObject(GameObject& gameObject)
{
	const char* typeName = ObjectTypeRegistry::FindName(gameObject);
	if (typeName == nullptr)
	{
		return {};
	}

	std::string out = std::string("object ") + typeName + "\n";
	if (!gameObject.GetName().empty())
	{
		out += "name " + SingleLine(gameObject.GetName()) + "\n";
	}
	out += "id " + std::to_string(gameObject.GetId()) + "\n";
	out += "position " + FormatVector(gameObject.GetPosition()) + "\n";
	out += "rotation " + FormatVector(gameObject.GetRotation()) + "\n";
	out += "scale " + FormatVector(gameObject.GetScale()) + "\n";
	WriteVisitor visitor(out);
	gameObject.Reflect(visitor);
	out += "end\n";
	return out;
}

std::string SceneSerializer::Write(World& world)
{
	std::string out = "# GM31 scene v1\n";
	for (const auto& gameObject : world.GetObjects())
	{
		if (gameObject->IsPendingDestroy())
		{
			continue;
		}
		const std::string object = WriteObject(*gameObject);
		if (!object.empty())
		{
			out += "\n" + object;
		}
	}
	return out;
}

bool SceneSerializer::Read(World& world, std::string_view text,
	std::vector<GameObject*>* created, const TypeFilter& filter)
{
	bool clean = true;
	bool inObject = false;
	StoredObject stored;

	while (!text.empty())
	{
		const std::size_t lineEnd = text.find('\n');
		std::string_view line = Trim(text.substr(0, lineEnd));
		text = lineEnd == std::string_view::npos ? std::string_view{} : text.substr(lineEnd + 1);
		if (line.empty() || line.front() == '#')
		{
			continue;
		}

		const std::string_view key = TakeWord(line);
		if (!inObject)
		{
			if (key == "object" && !line.empty())
			{
				inObject = true;
				stored = StoredObject{};
				stored.Type = line;
			}
			else
			{
				clean = false;
			}
			continue;
		}

		if (key == "end")
		{
			inObject = false;
			if (filter && !filter(stored.Type))
			{
				continue;
			}
			GameObject* gameObject = ObjectTypeRegistry::Create(world, stored.Type);
			if (gameObject == nullptr)
			{
				clean = false;
				continue;
			}
			gameObject->SetName(std::string(stored.Name));
			// 同じIDが既に使われている場合(複製など)は、Spawnが振った新しいIDのままにします。
			if (stored.Id != 0)
			{
				world.SetObjectId(*gameObject, stored.Id);
			}
			gameObject->SetPosition(stored.Position);
			gameObject->SetRotation(stored.Rotation);
			gameObject->SetScale(stored.Scale);
			ReadVisitor visitor(stored.Properties);
			gameObject->Reflect(visitor);
			clean &= !visitor.HadError();
			gameObject->OnPropertiesChanged();
			if (created != nullptr)
			{
				created->push_back(gameObject);
			}
		}
		else if (key == "name")
		{
			stored.Name = line;
		}
		else if (key == "id")
		{
			long long id = 0;
			if (ParseInt(line, id) && id > 0)
			{
				stored.Id = static_cast<std::uint64_t>(id);
			}
			else
			{
				clean = false;
			}
		}
		else if (key == "position")
		{
			clean &= ParseVector(line, stored.Position);
		}
		else if (key == "rotation")
		{
			clean &= ParseVector(line, stored.Rotation);
		}
		else if (key == "scale")
		{
			clean &= ParseVector(line, stored.Scale);
		}
		else if (IsPropertyKind(key))
		{
			const std::string_view name = TakeWord(line);
			stored.Properties.push_back({ key, name, line });
		}
		else
		{
			clean = false;
		}
	}
	// "end"のないまま終わったオブジェクトは生成しません。
	return clean && !inObject;
}

std::string SceneSerializer::ReadFileText(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
	{
		return {};
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

bool SceneSerializer::SaveFile(World& world, const std::filesystem::path& path)
{
	std::error_code error;
	if (path.has_parent_path())
	{
		std::filesystem::create_directories(path.parent_path(), error);
	}
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		return false;
	}
	file << Write(world);
	return file.good();
}

bool SceneSerializer::LoadFile(World& world, const std::filesystem::path& path)
{
	const std::string text = ReadFileText(path);
	if (text.empty())
	{
		return false;
	}
	return Read(world, text);
}

void SceneSerializer::DestroyAll(World& world)
{
	for (const auto& gameObject : world.GetObjects())
	{
		if (ObjectTypeRegistry::FindName(*gameObject) != nullptr)
		{
			gameObject->MarkForDestroy();
		}
	}
}
}
