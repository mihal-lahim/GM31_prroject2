#include "main.h"
#include "wireBoxMesh.h"

namespace
{
	struct Face
	{
		Vector3 Normal;
		Vector3 Right;
		Vector3 Up;
		float Shade;
	};

	// 面ごとに明るさを変え、テクスチャ無しでも立体が読めるようにします。
	// RightとUpは頂点を並べるための基底で、向きの正否は下で機械的に判定します。
	const Face kFaces[6] = {
		{ {  0,  1,  0 }, { 1, 0, 0 }, { 0, 0, 1 }, 1.00f },
		{ {  0, -1,  0 }, { 1, 0, 0 }, { 0, 0,-1 }, 0.45f },
		{ {  1,  0,  0 }, { 0, 0,-1 }, { 0, 1, 0 }, 0.80f },
		{ { -1,  0,  0 }, { 0, 0, 1 }, { 0, 1, 0 }, 0.72f },
		{ {  0,  0,  1 }, { 1, 0, 0 }, { 0, 1, 0 }, 0.88f },
		{ {  0,  0, -1 }, {-1, 0, 0 }, { 0, 1, 0 }, 0.64f },
	};
}

namespace WireBoxMesh
{
	void Append(std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
		const Vector3& center, const Vector3& halfSize, const XMFLOAT4& color, bool shadeOnly)
	{
		for (const Face& face : kFaces)
		{
			const Vector3 faceCenter{
				center.x + face.Normal.x * halfSize.x,
				center.y + face.Normal.y * halfSize.y,
				center.z + face.Normal.z * halfSize.z };
			const Vector3 right{
				face.Right.x * halfSize.x, face.Right.y * halfSize.y, face.Right.z * halfSize.z };
			const Vector3 up{
				face.Up.x * halfSize.x, face.Up.y * halfSize.y, face.Up.z * halfSize.z };

			const XMFLOAT4 shaded = shadeOnly
				? XMFLOAT4{ face.Shade, face.Shade, face.Shade, 1.0f }
				: XMFLOAT4{ color.x * face.Shade, color.y * face.Shade, color.z * face.Shade, color.w };

			const unsigned int base = static_cast<unsigned int>(vertices.size());
			const Vector3 corners[4] = {
				faceCenter - right - up,
				faceCenter + right - up,
				faceCenter + right + up,
				faceCenter - right + up };
			const XMFLOAT2 uvs[4] = { {0,1}, {1,1}, {1,0}, {0,0} };
			for (int i = 0; i < 4; ++i)
			{
				VERTEX_3D vertex{};
				vertex.Position = { corners[i].x, corners[i].y, corners[i].z };
				vertex.Normal = { face.Normal.x, face.Normal.y, face.Normal.z };
				vertex.Diffuse = shaded;
				vertex.TexCoord = uvs[i];
				vertices.push_back(vertex);
			}

			// 表面の巻き順を基底の向きから決めます。
			// このレンダラは左手系で、画面上で時計回りの三角形を表とします。
			// 外(法線側)から見て 0→1→2 が反時計回りになるのは Right×Up が法線と
			// 逆を向く組み合わせで、そのとき (0,2,1) が時計回り=表になります。
			// Right×Up が法線と同じ向きなら鏡像なので、並びを反転させます。
			const bool mirrored = Vector3::dot(Vector3::cross(face.Right, face.Up), face.Normal) > 0.0f;
			if (mirrored)
			{
				indices.push_back(base + 0);
				indices.push_back(base + 1);
				indices.push_back(base + 2);
				indices.push_back(base + 0);
				indices.push_back(base + 2);
				indices.push_back(base + 3);
			}
			else
			{
				indices.push_back(base + 0);
				indices.push_back(base + 2);
				indices.push_back(base + 1);
				indices.push_back(base + 0);
				indices.push_back(base + 3);
				indices.push_back(base + 2);
			}
		}
	}
}
