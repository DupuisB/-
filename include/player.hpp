#ifndef PLAYER_HPP
#define PLAYER_HPP

#include <SFML/Graphics.hpp>
#include <box2d/box2d.h>
#include <vector>

// Forward declaration
class GameObject;

/**
 * @brief Handles all player movement mechanics including jumping and horizontal movement.
 * 
 * This function implements player controls with advanced platformer mechanics:
 * - Horizontal movement with speed limiting
 * - Jump mechanics with variable height control
 * - Coyote time (allowing jumps shortly after leaving platforms)
 * - Jump buffering (queuing jumps slightly before landing)
 * 
 * @param worldId The Box2D world ID
 * @param playerBodyId The Box2D body ID of the player
 * @param playerGameObject A reference to the player's GameObject for animation control
 * @param allGameObjects A constant reference to the vector of all GameObjects in the scene (for ground check)
 * @param jumpKeyHeld Whether the jump key is currently held
 * @param leftKeyHeld Whether the left movement key is currently held
 * @param rightKeyHeld Whether the right movement key is currently held
 * @param dt Delta time since the last frame in seconds
 */
void movePlayer(b2WorldId worldId, b2BodyId playerBodyId, GameObject& playerGameObject,
                const std::vector<GameObject>& allGameObjects,
                bool jumpKeyHeld, bool leftKeyHeld, bool rightKeyHeld, float dt);

void initializeSounds();

#endif // PLAYER_HPP
