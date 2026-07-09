#include "main.h"
#include "player.h"
#include "manager.h"
#include "tree.h"
#include "box.h"

void Player::ResolveLegacyCollisions(const Vector3& oldPosition)
{
	auto trees = Manager::GetGameObjects<Tree>();
	for (Tree* tree : trees)
	{
		Vector3 treePos = tree->GetPosition();
		Vector3 playerPos = m_Position;

		treePos.y = 0.0f;
		playerPos.y = 0.0f;

		Vector3 direction = playerPos - treePos;
		float length = direction.length();

		if (length > 0.0001f && length < 1.5f)
		{
			direction /= length;
			direction *= 1.5f - length;
			m_Position += direction;
		}
	}

	auto boxes = Manager::GetGameObjects<Box>();
	for (Box* box : boxes)
	{
		Vector3 boxPos = box->GetPosition();
		Vector3 boxScale = box->GetScale();

		if (boxPos.x - boxScale.x < m_Position.x &&
			m_Position.x < boxPos.x + boxScale.x &&
			boxPos.z - boxScale.z < m_Position.z &&
			m_Position.z < boxPos.z + boxScale.z)
		{
			if (boxPos.y + boxScale.y < m_Position.y &&
				m_Position.y < boxPos.y + boxScale.y * 2.0f)
			{
				m_Position.y = boxPos.y + boxScale.y * 2.0f;
				m_Velocity.y = 0.0f;
				m_OnGround = true;
			}
			else if (boxPos.y - boxScale.y < m_Position.y &&
				m_Position.y < boxPos.y + boxScale.y)
			{
				m_Position.x = oldPosition.x;
				m_Position.z = oldPosition.z;
				m_Velocity.x = 0.0f;
				m_Velocity.z = 0.0f;
			}
		}
	}
}
