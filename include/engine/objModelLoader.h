#pragma once

#include <string>
#include <unordered_map>

struct MODEL_OBJ;
struct MODEL_MATERIAL;

namespace ObjModelLoader
{
	void LoadObj(const char* fileName, MODEL_OBJ* model);
	void LoadMaterial(
		const char* fileName,
		std::unordered_map<std::string, MODEL_MATERIAL>& materials);
}
