#pragma once

#include <SDL.h>

#include <vector>

namespace gaia {

// Which wall a door sits on.
enum class Side { Top, Bottom, Left, Right };

// A door is a centered opening on one wall that leads to another room. The
// player re-enters the target room from the opposite wall's door.
struct Door {
    Side side;
    int  targetRoom;  // index into the room list
};

// A single room: an interior of a fixed size (so the map is no longer one big
// open field), a floor color, and the doors leading out of it.
struct Room {
    int       width;
    int       height;
    SDL_Color floor;
    std::vector<Door> doors;
    std::vector<SDL_Rect> vendors;
    SDL_Rect runDoor{};
    bool hasRunDoor = false;
};

// Owns every room and tracks which one is active. Only the current room is ever
// rendered, so neighboring rooms are never visible at the same time; crossing a
// door swaps the active room and repositions the player. Also provides the
// basic collision used to keep the player inside the current room's walls.
class RoomSystem {
public:
    // Builds the rooms in world space.
    void init(int windowWidth, int windowHeight);

    const Room& current() const { return m_rooms[m_current]; }
    // Interior rectangle of the current room, in world coordinates.
    SDL_Rect interiorRect() const;
    // Center point of the spawn room, used to place the player at startup.
    SDL_Point spawnCenter() const;

    bool isHub() const { return m_current == 0; }
    // Whether the player overlaps the hub's run-start door.
    bool playerInRunDoor(float px, float py, float size) const;
    // Moves the player to the hub spawn / run spawn.
    void resetToHub(float& px, float& py, float size);
    void startRun(float& px, float& py, float size);

    // Draws the current room's floor, walls, doors, and hub markers. Does not clear the
    // screen; the caller clears to the void color first.
    void render(SDL_Renderer* renderer, float cameraX, float cameraY,
                SDL_Texture* vendorTexture) const;

    // Clamps the player AABB (top-left px,py with the given size) to the current
    // room's walls. If the player steps through a door, switches to the target
    // room, repositions the player at the matching entrance, and returns true.
    bool resolvePlayer(float& px, float& py, float size);

    static constexpr int   kWall       = 16;    // wall thickness, pixels
    static constexpr int   kDoorWidth  = 120;   // door opening, pixels
    static constexpr float kEntryInset = 10.0f; // how far inside a door to spawn

private:
    SDL_Rect interiorRectFor(const Room& room) const;
    void enterRoom(int target, Side entrySide, float& px, float& py, float size);
    static Side opposite(Side s);

    std::vector<Room> m_rooms;
    int m_current = 0;
    int m_winW = 0;
    int m_winH = 0;
};

}  // namespace gaia
