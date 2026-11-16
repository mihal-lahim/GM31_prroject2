#include "main.h"
#include "player.h"
#include "world.h"
#include "tree.h"
#include "box.h"

void Player::ResolveLegacyCollisions(const Vector3& oldPosition)
{
	auto trees = GetWorld().FindAll<Tree>();
	for (Tree* tree : trees)
	{
		Vector3 treePos = tree->GetPosition();
		Vector3 playerPos = GetPosition();

		treePos.y = 0.0f;
		playerPos.y = 0.0f;

		Vector3 direction = playerPos - treePos;
		float length = direction.length();

		if (length > 0.0001f && length < 1.5f)
		{
			direction /= length;
			direction *= 1.5f - length;
			SetPosition(GetPosition() + (direction));
		}
	}

	auto boxes = GetWorld().FindAll<Box>();
	for (Box* box : boxes)
	{
		Vector3 boxPos = box->GetPosition();
		Vector3 boxScale = box->GetScale();

		if (boxPos.x - boxScale.x < GetPosition().x &&
			GetPosition().x < boxPos.x + boxScale.x &&
			boxPos.z - boxScale.z < GetPosition().z &&
			GetPosition().z < boxPos.z + boxScale.z)
		{
			if (boxPos.y + boxScale.y < GetPosition().y &&
				GetPosition().y < boxPos.y + boxScale.y * 2.0f)
			{
				{ Vector3 position = GetPosition(); position.y = boxPos.y + boxScale.y * 2.0f; SetPosition(position); }
				m_Velocity.y = 0.0f;
				m_OnGround = true;
			}
			else if (boxPos.y - boxScale.y < GetPosition().y &&
				GetPosition().y < boxPos.y + boxScale.y)
			{
				{ Vector3 position = GetPosition(); position.x = oldPosition.x; SetPosition(position); }
				{ Vector3 position = GetPosition(); position.z = oldPosition.z; SetPosition(position); }
				m_Velocity.x = 0.0f;
				m_Velocity.z = 0.0f;
			}
		}
	}
}
