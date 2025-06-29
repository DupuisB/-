#include "../include/player.hpp"
#include "../include/utils.hpp"
#include "../include/constants.hpp"
#include "../include/game_object.hpp" // Required for GameObject class properties
#include <cmath> // For std::abs, std::max, std::sqrt
#include <algorithm> // For std::min, std::max
#include <iostream>
#include <SFML/Audio.hpp> // For sf::Music

// Sound management
sf::SoundBuffer jumpSoundBuffer;
sf::SoundBuffer runningSoundBuffer;
std::unique_ptr<sf::Sound> jumpSound;
std::unique_ptr<sf::Sound> runningSound;
bool soundsInitialized = false;

void initializeSounds() {
    if (!soundsInitialized) {
        if (!jumpSoundBuffer.loadFromFile("../assets/audio/jumpsound.wav")) {
            std::cerr << "Failed to load jump sound!" << std::endl;
            return;
        }
        
        if (!runningSoundBuffer.loadFromFile("../assets/audio/runningsound.wav")) {
            std::cerr << "Failed to load running sound!" << std::endl;
            return;
        }
        
        // Créez les objets Sound avec les buffers
        jumpSound = std::make_unique<sf::Sound>(jumpSoundBuffer);
        runningSound = std::make_unique<sf::Sound>(runningSoundBuffer);
        
        // Configurez les sons
        jumpSound->setVolume(5.0f);
        runningSound->setLooping(true);
        runningSound->setVolume(30.0f);
        
        soundsInitialized = true;
    }
}

// Helper function to get the sign of a number
inline float sign(float val) {
    if (val > 0.0f) return 1.0f;
    if (val < 0.0f) return -1.0f;
    return 0.0f;
}

void movePlayer(b2WorldId worldId, b2BodyId playerBodyId, GameObject& playerGameObject,
                const std::vector<GameObject>& allGameObjects,
                bool jumpKeyHeld, bool leftKeyHeld, bool rightKeyHeld, float dt) {

    if (B2_IS_NULL(playerBodyId)) return;
    // --- Player Physics Parameters ---
    // Horizontal Movement
    static const float PLAYER_MAX_SPEED = 20.0f;
    static const float PLAYER_GROUND_ACCELERATION = 100.0f;
    static const float PLAYER_AIR_ACCELERATION = 60.0f;
    static const float PLAYER_GROUND_DECELERATION = 100.0f;
    static const float PLAYER_TURN_SPEED_FACTOR = 1.5f;

    // Jump
    static const float PLAYER_JUMP_HEIGHT = 5.0f;
    static const float PLAYER_TIME_TO_JUMP_APEX = 0.6f;


    // Gravity Modification
    static const float PLAYER_FALL_GRAVITY_FACTOR = 5.0f;
    static const float PLAYER_JUMP_CUT_GRAVITY_FACTOR = 2.5f;

    // Derived Jump & Gravity Values
    static const float WORLD_GRAVITY_MAGNITUDE = 10.0f;
    static const float PLAYER_EFFECTIVE_GRAVITY_MAGNITUDE = (2.0f * PLAYER_JUMP_HEIGHT) / (PLAYER_TIME_TO_JUMP_APEX * PLAYER_TIME_TO_JUMP_APEX);
    static const float PLAYER_INITIAL_JUMP_VELOCITY = PLAYER_EFFECTIVE_GRAVITY_MAGNITUDE * PLAYER_TIME_TO_JUMP_APEX;
    static const float PLAYER_BASE_GRAVITY_SCALE = PLAYER_EFFECTIVE_GRAVITY_MAGNITUDE / WORLD_GRAVITY_MAGNITUDE;
    static const float PLAYER_COYOTE_TIME = 0.0f;
    static const float PLAYER_JUMP_BUFFER_TIME = 0.1f;


    // --- Player State Variables ---
    static bool isGrounded = false;
    static bool wasGroundedLastFrame = false;
    static bool isJumping = false;
    static float coyoteTimer = PLAYER_COYOTE_TIME;
    static float jumpBufferTimer = PLAYER_JUMP_BUFFER_TIME;
    static bool previousJumpKeyHeld = false;
    b2Vec2 playerVel=b2Body_GetLinearVelocity(playerBodyId);

    // --- Input Processing ---
    bool jumpKeyJustPressed = jumpKeyHeld && !previousJumpKeyHeld;
    previousJumpKeyHeld = jumpKeyHeld;

    // --- Facing Direction ---
    // Read current flip state from GameObject, if it's already flipped, it means it's facing left.
    bool currentFacingLeft = playerGameObject.spriteFlipped; 
    bool targetFacingLeft = currentFacingLeft;

    if (leftKeyHeld) targetFacingLeft = true;
    if (rightKeyHeld) targetFacingLeft = false;

    // --- Ground Check ---
    wasGroundedLastFrame = isGrounded;
    if(!wasGroundedLastFrame && playerVel.y > 0.01f && jumpKeyHeld) {
        isGrounded = false; 
    } else {
        
        isGrounded = false;
        b2ContactData contactData[10];
        int count = b2Body_GetContactData(playerBodyId, contactData, 10);
        for (int i = 0; i < count; ++i) {
            if (contactData[i].manifold.pointCount > 0) {
                b2BodyId bodyA = b2Shape_GetBody(contactData[i].shapeIdA);
                b2BodyId bodyB = b2Shape_GetBody(contactData[i].shapeIdB);
                b2BodyId otherBodyId = b2_nullBodyId;
                float supportingNormalY = 0.0f;

                if (B2_ID_EQUALS(bodyA, playerBodyId)) {
                    otherBodyId = bodyB;
                    supportingNormalY = -contactData[i].manifold.normal.y;
                } else if (B2_ID_EQUALS(bodyB, playerBodyId)) {
                    otherBodyId = bodyA;
                    supportingNormalY = contactData[i].manifold.normal.y;
                } else {
                    continue;
                }

                // Check if the other body is a GameObject that can be jumped on
                for(const auto& gameObject : allGameObjects) { // Use allGameObjects for ground check
                    if (B2_ID_EQUALS(otherBodyId, gameObject.bodyId)) {
                        if (gameObject.canJumpOn && supportingNormalY > 0.7f) { // Check if contact normal is mostly upward
                            isGrounded = true;
                        }
                        break;
                    }
                }
                if (isGrounded) break;
            }
        }
    }
    // Update Coyote Time & Jump State
    if (isGrounded) {
        coyoteTimer = PLAYER_COYOTE_TIME;
        isJumping=false;
    } else {
        coyoteTimer = std::max(0.0f, coyoteTimer - dt);
    }

    // Update Jump Buffer
    if (jumpKeyJustPressed) {
        jumpBufferTimer = PLAYER_JUMP_BUFFER_TIME;
    } else {
        jumpBufferTimer = std::max(0.0f, jumpBufferTimer - dt);
    }
    
    bool justLanded = isGrounded && !wasGroundedLastFrame;

    // --- Handle Jumping ---
    bool canJumpFromState = (isGrounded || coyoteTimer > 0.0f);
    bool tryJumpFromBuffer = justLanded && (jumpBufferTimer > 0.0f);

    if (tryJumpFromBuffer || (jumpKeyJustPressed && canJumpFromState)) {
        b2Body_SetLinearVelocity(playerBodyId, {playerVel.x, PLAYER_INITIAL_JUMP_VELOCITY});
        isJumping = true;
        jumpBufferTimer = 0.0f;
        coyoteTimer = 0.0f;
        isGrounded = false;

        // Play jump sound effect
        if (jumpSound->getStatus() != sf::SoundSource::Status::Playing) {
            jumpSound->play();
        }

    }

    // --- Gravity Modification ---
    float currentGravityScale = PLAYER_BASE_GRAVITY_SCALE;

    if (isJumping && playerVel.y > 0.01f ) {
        if(jumpKeyHeld){
            currentGravityScale = PLAYER_BASE_GRAVITY_SCALE;
        }
        else{
        currentGravityScale = PLAYER_BASE_GRAVITY_SCALE * PLAYER_JUMP_CUT_GRAVITY_FACTOR;
        }
    } else if (playerVel.y < -0.01f) {
        currentGravityScale = PLAYER_BASE_GRAVITY_SCALE * PLAYER_FALL_GRAVITY_FACTOR;
    }
    if (isGrounded && !isJumping) {
         currentGravityScale = PLAYER_BASE_GRAVITY_SCALE; 
    }


    b2Body_SetGravityScale(playerBodyId, currentGravityScale);


    // --- Animation State ---
    std::string nextAnimation = playerGameObject.currentAnimationName; // Default to current
    if (isGrounded) {
        if (leftKeyHeld || rightKeyHeld) {
            nextAnimation = "walk";
        } else {
            nextAnimation = "idle";
        }
    } else { // In air
        if (playerVel.y > 0.1f) { // Moving upwards (positive Y in Box2D is up)
            nextAnimation = "jump";
        } else if (playerVel.y < -0.1f) { // Moving downwards
            nextAnimation = "fall";
        } else { // Near apex or very slight Y movement
            // Keep jump if was jumping, or fall if was falling, else default to fall
            if (playerGameObject.currentAnimationName == "jump" || playerGameObject.currentAnimationName == "fall") {
                 nextAnimation = playerGameObject.currentAnimationName;
            } else {
                 nextAnimation = "fall"; // Default for in-air without strong vertical movement
            }
        }
    }
    playerGameObject.setPlayerAnimation(nextAnimation, targetFacingLeft);


    // --- Horizontal Movement ---
    float forceX = 0.0f;
    float currentVelX = playerVel.x;
    float playerMass = b2Body_GetMass(playerBodyId);

    if (leftKeyHeld || rightKeyHeld) {
        float direction = leftKeyHeld ? -1.0f : 1.0f;
        float accelRate = isGrounded ? PLAYER_GROUND_ACCELERATION : PLAYER_AIR_ACCELERATION;

        // Apply turn speed factor if changing direction
        if (sign(currentVelX) != 0 && sign(currentVelX) != direction) {
            accelRate *= PLAYER_TURN_SPEED_FACTOR;
        }

        if ((direction > 0 && currentVelX < PLAYER_MAX_SPEED) || (direction < 0 && currentVelX > -PLAYER_MAX_SPEED)) {
             forceX = direction * accelRate * playerMass;
        }

    } else {
        if (isGrounded && std::abs(currentVelX) > 0.01f) {
            float decelForceMagnitude = PLAYER_GROUND_DECELERATION * playerMass;
            forceX = -sign(currentVelX) * decelForceMagnitude;

            // Prevent deceleration from overshooting and reversing direction in a single frame
            if (std::abs(forceX * dt / playerMass) > std::abs(currentVelX)) {
                forceX = -currentVelX * playerMass / dt;
            }
        }
    }

    if (forceX != 0.0f) {
        b2Body_ApplyForceToCenter(playerBodyId, {forceX, 0.0f}, true);
    }


    //Running sound logic
    if (isGrounded && (leftKeyHeld || rightKeyHeld)) {
        if (runningSound->getStatus() != sf::SoundSource::Status::Playing) {
            runningSound->play();
        }
    } else {
        if (runningSound->getStatus() == sf::SoundSource::Status::Playing) {
            runningSound->stop();
        }
    }

}
