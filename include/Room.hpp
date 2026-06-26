#pragma once

#include <SDL.h>

#include <random>
#include <vector>

namespace gaia {

// Which wall a door sits on.
enum class Side { Top, Bottom, Left, Right };

// A door is a centered opening on one wall that leads to another room. The
// player re-enters the target room from the opposite wall's door.
struct Door {
    Side side;
    int  targetRoom;  // index into the room list (ignored when generatesNext)
    // When true, crossing this door generates a brand-new random run room
    // instead of going to a fixed targetRoom. This is what makes runs endless.
    bool generatesNext = false;
};

// One thing for sale in a vendor room. Purely descriptive for now — buying is
// not implemented yet — but it carries the data a future economy will need.
struct ShopItem {
    SDL_Rect    rect;             // display pedestal, world coordinates
    const char* name = "";        // placeholder display name
    int         price = 0;        // cost in (future) currency
    bool        purchased = false; // future: set true once bought
};

// A single room: an interior of a fixed size (so the map is no longer one big
// open field), a floor color, and the doors leading out of it.
struct Room {
    int       width;
    int       height;
    SDL_Color floor;
    std::vector<Door> doors;
    std::vector<SDL_Rect> vendors;
    // Impassable pits in the floor. The player is pushed out of these (see
    // RoomSystem::resolvePlayer); they are also drawn as dark holes.
    std::vector<SDL_Rect> holes;
    // Where enemies spawn for this layout, in world coordinates. Lets each run
    // layout hand-place its encounter instead of scattering enemies randomly.
    std::vector<SDL_Point> enemySpawns;
    // Vendor rooms: a shop with no enemies. shopItems are this room's wares.
    bool isVendorRoom = false;
    std::vector<ShopItem> shopItems;
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
    // How many run rooms deep the player is (0 on the first run room, +1 each
    // time they take a door onward). Reserved for the future fixed "rotation".
    int runDepth() const { return m_runDepth; }
    // Whether the active room is a vendor/shop room.
    bool isVendorRoom() const { return current().isVendorRoom; }
    // The active room's wares (empty unless it is a vendor room).
    const std::vector<ShopItem>& currentShopItems() const {
        return current().shopItems;
    }
    // Forces the NEXT generated run room to be a vendor room. Hook for the
    // future boss flow: "after killing a boss, the vendor room is guaranteed."
    // Not called yet (bosses are not implemented).
    void guaranteeVendorNextRoom() { m_forceVendorNext = true; }
    // Enemy spawn points for the active room (empty for the hub).
    const std::vector<SDL_Point>& currentEnemySpawns() const {
        return current().enemySpawns;
    }
    // Whether the player overlaps the hub's run-start door.
    bool playerInRunDoor(float px, float py, float size) const;
    // Moves the player to the hub spawn / run spawn.
    void resetToHub(float& px, float& py, float size);
    void startRun(float& px, float& py, float size);

    // Draws the current room's floor, walls, doors, and hub markers. Does not clear the
    // screen; the caller clears to the void color first.
    // floorTexture (AssetKind::Floor) is tiled across the interior; pass nullptr
    // to fall back to the flat colour + grid. vendorTexture draws hub stalls.
    void render(SDL_Renderer* renderer, float cameraX, float cameraY,
                SDL_Texture* vendorTexture,
                SDL_Texture* floorTexture = nullptr) const;

    // Clamps the player AABB (top-left px,py with the given size) to the current
    // room's walls. If the player steps through a door, switches to the target
    // room, repositions the player at the matching entrance, and returns true.
    // When doorsUnlocked is false the doors act as solid wall (the player is
    // simply clamped), which is how rooms stay sealed until they are cleared.
    bool resolvePlayer(float& px, float& py, float size, bool doorsUnlocked);

    //clamps spells within the current room's walls
    //will be combined with enemy collision so spells have proper collision
    bool resolveSpell(float& sx, float& sy, float size);

    static constexpr int   kWall       = 16;    // wall thickness, pixels
    static constexpr int   kDoorWidth  = 120;   // door opening, pixels
    static constexpr float kEntryInset = 10.0f; // how far inside a door to spawn

    // How many combat run-room layouts makeRunRoom() can produce; the next-room
    // picker chooses one of these at random for a normal (non-vendor) room.
    static constexpr int kRunLayoutCount = 5;

    // Vendor-room spawn chance. It is a probability spawn that starts very low
    // and rises with run depth, capped, and never appears two rooms in a row.
    //   chance(depth) = min(kVendorChanceCap, kVendorBaseChance + kVendorChanceStep * depth)
    static constexpr float kVendorBaseChance = 0.00f;  // at depth 0
    static constexpr float kVendorChanceStep = 0.05f;  // added per room of depth
    static constexpr float kVendorChanceCap  = 0.60f;  // ceiling

private:
    SDL_Rect interiorRectFor(const Room& room) const;
    void enterRoom(int target, Side entrySide, float& px, float& py, float size);
    // Generates a fresh run room in slot 1 and enters it from entrySide.
    void advanceToNextRoom(Side entrySide, float& px, float& py, float size);
    // Decides and builds the next run room: a vendor room (probability, gated so
    // it never repeats back-to-back, or forced after a boss) or a random combat
    // layout. Updates the vendor-spacing bookkeeping.
    Room makeNextRunRoom();
    static Side opposite(Side s);
    // Builds one of the kRunLayoutCount combat run-room layouts (size, floor,
    // holes, enemy spawns). layout must be in [0, kRunLayoutCount).
    static Room makeRunRoom(int layout);
    // Builds a vendor/shop room (no enemies) with a set of wares for sale.
    Room makeVendorRoom();

    std::vector<Room> m_rooms;
    int m_current = 0;
    int m_winW = 0;
    int m_winH = 0;
    int m_runDepth = 0;  // how many run rooms deep the current run is
    // Normal (non-vendor) rooms since the last vendor room. Used to keep two
    // vendor rooms from spawning back-to-back. Starts at 1 so the first room
    // after the (combat) opener is already eligible.
    int m_roomsSinceVendor = 1;
    // Set by guaranteeVendorNextRoom() to force the next room to be a vendor.
    bool m_forceVendorNext = false;
    // Seeded once; drives random layout selection at the start of each run.
    std::mt19937 m_rng{std::random_device{}()};
};

}  // namespace gaia
