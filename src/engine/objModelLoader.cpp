#include "objModelLoader.h"

#include "main.h"
#include "modelRenderer.h"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace
{
	void DebugLog(const std::string& message)
	{
		OutputDebugStringA((message + "\n").c_str());
	}

	Engine::MATERIAL MakeDefaultMaterialData()
	{
		Engine::MATERIAL material{};
		material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		material.Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
		material.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
		material.Shininess = 0.0f;
		material.TextureEnable = FALSE;
		return material;
	}

	MODEL_MATERIAL MakeDefaultMaterial(const std::string& name = "default")
	{
		MODEL_MATERIAL material{};
		material.Name = name;
		material.Material = MakeDefaultMaterialData();
		return material;
	}

	std::string Trim(const std::string& value)
	{
		const char* whitespace = " \t\r\n";
		size_t first = value.find_first_not_of(whitespace);
		if (first == std::string::npos)
		{
			return "";
		}

		size_t last = value.find_last_not_of(whitespace);
		return value.substr(first, last - first + 1);
	}

	std::string RemoveComment(const std::string& line)
	{
		size_t comment = line.find('#');
		if (comment == std::string::npos)
		{
			return line;
		}

		return line.substr(0, comment);
	}

	bool TryParseInt(const std::string& value, int& result)
	{
		if (value.empty())
		{
			return false;
		}

		// intに収まらない数は、切り捨てると別の頂点を指してしまうので受け付けません。
		char* end = nullptr;
		errno = 0;
		const long long parsed = std::strtoll(value.c_str(), &end, 10);
		if (end == value.c_str() || *end != '\0' || errno == ERANGE
			|| parsed < (std::numeric_limits<int>::min)() || parsed > (std::numeric_limits<int>::max)())
		{
			return false;
		}

		result = static_cast<int>(parsed);
		return true;
	}

	std::vector<std::string> Split(const std::string& value, char delimiter)
	{
		std::vector<std::string> parts;
		std::string part;
		std::stringstream stream(value);
		while (std::getline(stream, part, delimiter))
		{
			parts.push_back(part);
		}

		return parts;
	}

	// OBJの番号(1始まり、負は末尾から)を0始まりの添字にします。範囲外は-1です。
	// 要素数とintの足し算が溢れないよう、64bitで計算します。
	int ResolveObjIndex(int objIndex, size_t elementCount)
	{
		std::int64_t resolved = -1;
		if (objIndex > 0)
		{
			resolved = static_cast<std::int64_t>(objIndex) - 1;
		}
		else if (objIndex < 0)
		{
			resolved = static_cast<std::int64_t>(elementCount) + static_cast<std::int64_t>(objIndex);
		}

		if (resolved >= 0 && static_cast<std::uint64_t>(resolved) < elementCount
			&& resolved <= (std::numeric_limits<int>::max)())
		{
			return static_cast<int>(resolved);
		}
		return -1;
	}

	// 「キーワード 残り全部」の行から、残り全部を前後の空白を除いて返します。空白を含むファイル名に使います。
	std::string RestOfLine(std::stringstream& stream)
	{
		std::string rest;
		std::getline(stream, rest);
		return Trim(rest);
	}

	// count 個の数を一時変数へ読み、すべて読めて有限なときだけ out へ書きます。
	// MSVC のストリームは "nan" や "inf" も読めてしまうので、有限かどうかも確かめます。
	// 途中で失敗した行は、成分の一部だけが書き換わることのないよう、何も変えません。
	bool ReadFiniteFloats(std::stringstream& stream, float* out, int count)
	{
		float values[4]{};
		for (int i = 0; i < count; ++i)
		{
			if (!(stream >> values[i]) || !std::isfinite(values[i]))
			{
				return false;
			}
		}
		for (int i = 0; i < count; ++i)
		{
			out[i] = values[i];
		}
		return true;
	}

	// 頂点の行(v / vn / vt)用です。面は頂点を通し番号で指すので、行を飛ばすと後ろの頂点の番号が
	// 全部ずれて、正しい面まで別の頂点を指してしまいます。そこで読めない値("nan" など、処理系によっては
	// 数として読めない)や NaN・無限大を含む行も1つの頂点として数え、fallback の値で埋めます。
	void ReadVertexFloats(std::stringstream& stream, float* out, const float* fallback, int count)
	{
		float values[4]{};
		bool valid = true;
		for (int i = 0; valid && i < count; ++i)
		{
			valid = (stream >> values[i]) && std::isfinite(values[i]);
		}
		for (int i = 0; i < count; ++i)
		{
			out[i] = valid ? values[i] : fallback[i];
		}
	}

	struct ObjVertexRef
	{
		int Position = -1;
		int TexCoord = -1;
		int Normal = -1;
	};

	bool ParseObjVertexRef(const std::string& token, const std::vector<XMFLOAT3>& positions, const std::vector<XMFLOAT2>& texcoords, const std::vector<XMFLOAT3>& normals, ObjVertexRef& ref)
	{
		std::vector<std::string> parts = Split(token, '/');
		if (parts.empty())
		{
			return false;
		}

		int index = 0;
		if (!TryParseInt(parts[0], index))
		{
			return false;
		}
		ref.Position = ResolveObjIndex(index, positions.size());
		if (ref.Position < 0)
		{
			return false;
		}

		if (parts.size() > 1 && TryParseInt(parts[1], index))
		{
			ref.TexCoord = ResolveObjIndex(index, texcoords.size());
		}
		if (parts.size() > 2 && TryParseInt(parts[2], index))
		{
			ref.Normal = ResolveObjIndex(index, normals.size());
		}

		return true;
	}

	void EnsureSubset(MODEL_OBJ& modelObj, const MODEL_MATERIAL& material)
	{
		if (!modelObj.SubsetArray.empty())
		{
			return;
		}

		SUBSET subset{};
		subset.StartIndex = static_cast<unsigned int>(modelObj.IndexArray.size());
		subset.IndexNum = 0;
		subset.Material = material;
		modelObj.SubsetArray.push_back(subset);
	}

	void BeginSubset(MODEL_OBJ& modelObj, const MODEL_MATERIAL& material)
	{
		if (!modelObj.SubsetArray.empty() && modelObj.SubsetArray.back().IndexNum == 0)
		{
			modelObj.SubsetArray.back().Material = material;
			return;
		}

		SUBSET subset{};
		subset.StartIndex = static_cast<unsigned int>(modelObj.IndexArray.size());
		subset.IndexNum = 0;
		subset.Material = material;
		modelObj.SubsetArray.push_back(subset);
	}

	void UpdateCurrentSubsetIndexCount(MODEL_OBJ& modelObj)
	{
		if (modelObj.SubsetArray.empty())
		{
			return;
		}

		SUBSET& subset = modelObj.SubsetArray.back();
		subset.IndexNum = static_cast<unsigned int>(modelObj.IndexArray.size()) - subset.StartIndex;
	}

	bool AppendObjVertex(const ObjVertexRef& ref, const std::vector<XMFLOAT3>& positions, const std::vector<XMFLOAT2>& texcoords, const std::vector<XMFLOAT3>& normals, MODEL_OBJ& modelObj, unsigned int& vertexIndex)
	{
		if (ref.Position < 0 || static_cast<size_t>(ref.Position) >= positions.size())
		{
			return false;
		}

		Engine::VERTEX_3D vertex{};
		vertex.Position = positions[ref.Position];
		vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex.TexCoord = XMFLOAT2(0.0f, 0.0f);

		if (ref.TexCoord >= 0 && static_cast<size_t>(ref.TexCoord) < texcoords.size())
		{
			vertex.TexCoord = texcoords[ref.TexCoord];
		}
		if (ref.Normal >= 0 && static_cast<size_t>(ref.Normal) < normals.size())
		{
			vertex.Normal = normals[ref.Normal];
		}

		vertexIndex = static_cast<unsigned int>(modelObj.VertexArray.size());
		modelObj.VertexArray.push_back(vertex);
		return true;
	}

	std::filesystem::path ResolveSiblingPath(const std::filesystem::path& baseFile, const std::string& relativePath)
	{
		std::filesystem::path path(relativePath);
		if (path.is_absolute())
		{
			return path.lexically_normal();
		}

		return (baseFile.parent_path() / path).lexically_normal();
	}
}

void ObjModelLoader::LoadObj(const char* FileName, MODEL_OBJ* ModelObj)
{
	if (ModelObj == nullptr)
	{
		return;
	}

	*ModelObj = MODEL_OBJ{};
	if (FileName == nullptr)
	{
		DebugLog("ModelRenderer: OBJ file name is null.");
		return;
	}

	std::filesystem::path objPath(FileName);
	std::ifstream file(objPath);
	if (!file)
	{
		DebugLog(std::string("ModelRenderer: failed to open OBJ ") + FileName);
		return;
	}

	std::vector<XMFLOAT3> positions;
	std::vector<XMFLOAT3> normals;
	std::vector<XMFLOAT2> texcoords;
	std::unordered_map<std::string, MODEL_MATERIAL> materials;
	MODEL_MATERIAL currentMaterial = MakeDefaultMaterial();

	std::string line;
	while (std::getline(file, line))
	{
		line = Trim(RemoveComment(line));
		if (line.empty())
		{
			continue;
		}

		std::stringstream stream(line);
		std::string token;
		stream >> token;

		if (token == "mtllib")
		{
			// 空白を含むファイル名も読めるよう、行の残りをまとめて取ります。
			const std::string materialFileName = RestOfLine(stream);
			if (!materialFileName.empty())
			{
				std::filesystem::path materialPath = ResolveSiblingPath(objPath, materialFileName);
				LoadMaterial(materialPath.string().c_str(), materials);
			}
		}
		else if (token == "usemtl")
		{
			// 空白を含む名前(newmtl と同じく行の残り全体)で探します。
			const std::string materialName = RestOfLine(stream);
			auto material = materials.find(materialName);
			currentMaterial = (material != materials.end()) ? material->second : MakeDefaultMaterial(materialName);
			BeginSubset(*ModelObj, currentMaterial);
		}
		else if (token == "v")
		{
			// 読めない・非有限の位置は原点に置き換えます(番号を保つため。ReadVertexFloats)。
			constexpr float fallback[3] = { 0.0f, 0.0f, 0.0f };
			float values[3]{};
			ReadVertexFloats(stream, values, fallback, 3);
			positions.push_back({ values[0], values[1], values[2] });
		}
		else if (token == "vn")
		{
			constexpr float fallback[3] = { 0.0f, 1.0f, 0.0f };
			float values[3]{};
			ReadVertexFloats(stream, values, fallback, 3);
			normals.push_back({ values[0], values[1], values[2] });
		}
		else if (token == "vt")
		{
			constexpr float fallback[2] = { 0.0f, 0.0f };
			float values[2]{};
			ReadVertexFloats(stream, values, fallback, 2);
			texcoords.push_back({ 1.0f - values[0], 1.0f - values[1] });
		}
		else if (token == "f")
		{
			EnsureSubset(*ModelObj, currentMaterial);

			std::vector<unsigned int> faceIndices;
			std::string faceToken;
			while (stream >> faceToken)
			{
				ObjVertexRef ref;
				if (!ParseObjVertexRef(faceToken, positions, texcoords, normals, ref))
				{
					continue;
				}

				unsigned int vertexIndex = 0;
				if (AppendObjVertex(ref, positions, texcoords, normals, *ModelObj, vertexIndex))
				{
					faceIndices.push_back(vertexIndex);
				}
			}

			if (faceIndices.size() >= 3)
			{
				for (size_t i = 1; i + 1 < faceIndices.size(); i++)
				{
					ModelObj->IndexArray.push_back(faceIndices[0]);
					ModelObj->IndexArray.push_back(faceIndices[i]);
					ModelObj->IndexArray.push_back(faceIndices[i + 1]);
				}
				UpdateCurrentSubsetIndexCount(*ModelObj);
			}
		}
	}

	ModelObj->SubsetArray.erase(
		std::remove_if(ModelObj->SubsetArray.begin(), ModelObj->SubsetArray.end(), [](const SUBSET& subset)
			{
				return subset.IndexNum == 0;
			}),
		ModelObj->SubsetArray.end());

	ModelObj->VertexNum = static_cast<unsigned int>(ModelObj->VertexArray.size());
	ModelObj->IndexNum = static_cast<unsigned int>(ModelObj->IndexArray.size());
	ModelObj->SubsetNum = static_cast<unsigned int>(ModelObj->SubsetArray.size());
}

void ObjModelLoader::LoadMaterial(const char* FileName, std::unordered_map<std::string, MODEL_MATERIAL>& MaterialMap)
{
	if (FileName == nullptr)
	{
		return;
	}

	std::filesystem::path materialPath(FileName);
	std::ifstream file(materialPath);
	if (!file)
	{
		DebugLog(std::string("ModelRenderer: failed to open MTL ") + FileName);
		return;
	}

	MODEL_MATERIAL* currentMaterial = nullptr;
	std::string line;
	while (std::getline(file, line))
	{
		line = Trim(RemoveComment(line));
		if (line.empty())
		{
			continue;
		}

		std::stringstream stream(line);
		std::string token;
		stream >> token;

		if (token == "newmtl")
		{
			// 空白を含む名前(例: Steel Plate)も1つの名前として扱います。空の名前は無視します。
			const std::string name = RestOfLine(stream);
			if (!name.empty())
			{
				MaterialMap[name] = MakeDefaultMaterial(name);
				currentMaterial = &MaterialMap[name];
			}
		}
		else if (currentMaterial == nullptr)
		{
			continue;
		}
		else if (token == "Ka" || token == "Kd" || token == "Ks")
		{
			// 3成分すべてが読めて有限なときだけ反映します(壊れた行で一部だけ変わらないように)。
			float values[3]{};
			if (ReadFiniteFloats(stream, values, 3))
			{
				XMFLOAT4& color = token == "Ka" ? currentMaterial->Material.Ambient
					: token == "Kd" ? currentMaterial->Material.Diffuse : currentMaterial->Material.Specular;
				color = { values[0], values[1], values[2], 1.0f };
			}
		}
		else if (token == "Ns")
		{
			float value = 0.0f;
			if (ReadFiniteFloats(stream, &value, 1))
			{
				currentMaterial->Material.Shininess = value;
			}
		}
		else if (token == "d")
		{
			float value = 0.0f;
			if (ReadFiniteFloats(stream, &value, 1))
			{
				currentMaterial->Material.Diffuse.w = value;
			}
		}
		else if (token == "map_Kd")
		{
			// 空白を含むファイル名も読めるよう、行の残りをまとめて取ります。
			const std::string textureFileName = RestOfLine(stream);
			if (!textureFileName.empty())
			{
				currentMaterial->TextureName = ResolveSiblingPath(materialPath, textureFileName).string();
			}
		}
	}
}
