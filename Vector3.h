#pragma once

#include <math.h>

class Vector3
{
public:
	float x;
	float y;
	float z;

public:
	// コンストラクタ
	Vector3() {}
	// コピーコンストラクタ
	Vector3(const Vector3& vector3) : x(vector3.x), y(vector3.y), z(vector3.z) {}
	// 3つの値を指定するコンストラクタ
	Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
	
	// 代入
	Vector3& operator=(const Vector3& vector3)
	{
		x = vector3.x;
		y = vector3.y;
		z = vector3.z;

		return *this;
	}

	// 等しさチェック
	bool operator ==(const Vector3& vector3) const
	{
		return x == vector3.x && y == vector3.y && z == vector3.z;
	}

	bool operator !=(const Vector3& vector3) const
	{
		return x != vector3.x && y != vector3.y && z != vector3.z;
	}

	// ベクトル操作
	// 0に設定する
	void zero()
	{
		x = 0.0f;
		y = 0.0f;
		z = 0.0f;
	}

	// 反転したベクトルを返す
	Vector3 operator-() const
	{
		return Vector3(-x, -y, -z);
	}

	// 加算減算
	Vector3 operator+(const Vector3& vector3) const
	{
		return Vector3(x + vector3.x, y + vector3.y, z + vector3.z);
	}

	Vector3 operator-(const Vector3& vector3) const
	{
		return Vector3(x - vector3.x, y - vector3.y, z - vector3.z);
	}

	Vector3 operator*(float value) const
	{
		return Vector3(x * value, y * value, z * value);
	}

	Vector3 operator/(float value) const
	{
		float oneOver = 1.0f / value;
		return Vector3(x * oneOver, y * oneOver, z * oneOver);
	}

	Vector3& operator +=(const Vector3& vector3)
	{
		x += vector3.x;
		y += vector3.y;
		z += vector3.z;
		return *this;
	}

	Vector3& operator -=(const Vector3& vector3)
	{
		x -= vector3.x;
		y -= vector3.y;
		z -= vector3.z;
		return *this;
	}

	Vector3& operator *=(float value)
	{
		x *= value;
		y *= value;
		z *= value;
		return *this;
	}

	Vector3& operator /=(float value)
	{
		float oneOver = 1.0f / value;
		x *= oneOver;
		y *= oneOver;
		z *= oneOver;
		return *this;
	}

	// 正規化
	void normalize()
	{
		float magSq = x * x + y * y + z * z;
		if (magSq > 0.0f)
		{
			// ゼロ確認
			float oneOverMag = 1.0f / sqrtf(magSq);
			x *= oneOverMag;
			y *= oneOverMag;
			z *= oneOverMag;
		}
	}

	//外積
	static Vector3 cross(const Vector3& a, const Vector3& b)
	{
		return Vector3(a.y * b.z - a.z * b.y,
					   a.z * b.x - a.x * b.z,
					   a.x * b.y - a.y * b.x);
	}
	//長さ
	float length() const
	{
		return sqrtf(x * x + y * y + z * z);
	}
};

 