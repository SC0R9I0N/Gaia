#include "Room.hpp"

#include <algorithm>

namespace gaia {

void RoomSystem::init(int windowWidth, int windowHeight) {
    m_winW = windowWidth;
    m_winH = windowHeight;
    m_current = 0;
    m_rooms.clear();

    // Room 0: the roguelite hub. This is the safe starting zone where future
    // character selection and upgrade vendors will live.
    Room hub;
    hub.width  = 1800;
    hub.height = 1100;
    hub.floor  = SDL_Color{45, 39, 54, 255};
    hub.hasRunDoor = true;
    hub.runDoor = SDL_Rect{840, 430, 120, 96};
    hub.vendors.push_back(SDL_Rect{500, 430, 80, 96});
    hub.vendors.push_back(SDL_Rect{1220, 430, 80, 96});
    hub.vendors.push_back(SDL_Rect{500, 760, 80, 96});
    hub.vendors.push_back(SDL_Rect{1220, 760, 80, 96});
    m_rooms.push_back(hub);

    // Room 1: the run map. A placeholder layout lives here until a run starts;
    // startRun() swaps in a randomly chosen layout each time. It is intentionally
    // much larger than the logical screen so gameplay uses camera scrolling.
    m_rooms.push_back(makeRunRoom(0));
}

// The library of run-room layouts. Each varies the interior size, the floor
// colour, the pits/holes carved out of the floor, and where enemies spawn. Add
// a new layout here and bump RoomSystem::kRunLayoutCount to include it in the
// random rotation. Coordinates are world-space with the interior's top-left at
// (0, 0); every layout keeps a clear safe zone around its centre because the
// player spawns there (see startRun()).
Room RoomSystem::makeRunRoom(int layout) {
    Room r;
    r.hasRunDoor = false;

    switch (layout) {
        case 0: {  // "Arena" — compact, open, a ring of foes around the spawn.
            r.width  = 1600;
            r.height = 1100;
            r.floor  = SDL_Color{38, 44, 40, 255};
            const int cx = r.width / 2, cy = r.height / 2;
            r.enemySpawns = {
                {cx - 420, cy - 260}, {cx + 420, cy - 260},
                {cx - 540, cy},       {cx + 540, cy},
                {cx - 420, cy + 260}, {cx + 420, cy + 260},
                {cx,       cy - 330}, {cx,       cy + 330},
            };
            break;
        }
        case 1: {  // "Twin Pits" — two big pits flank a central corridor.
            r.width  = 2200;
            r.height = 1500;
            r.floor  = SDL_Color{44, 40, 52, 255};
            r.holes = {
                SDL_Rect{280, 500, 520, 500},
                SDL_Rect{1400, 500, 520, 500},
            };
            r.enemySpawns = {
                {1100, 230}, {900, 250}, {1300, 250},
                {1100, 1270}, {900, 1250}, {1300, 1250},
                {170, 750}, {2030, 750},
            };
            break;
        }
        case 2: {  // "Long Hall" — wide and short, scattered small pits.
            r.width  = 3000;
            r.height = 900;
            r.floor  = SDL_Color{40, 46, 52, 255};
            r.holes = {
                SDL_Rect{500, 330, 240, 200},
                SDL_Rect{1100, 150, 220, 180},
                SDL_Rect{1700, 520, 240, 180},
                SDL_Rect{2300, 330, 240, 200},
            };
            r.enemySpawns = {
                {300, 200}, {300, 700}, {1500, 160}, {1500, 740},
                {2700, 200}, {2700, 700}, {2000, 450},
            };
            break;
        }
        case 3: {  // "Grand Hall" — large square, four corner pits, a plus path.
            r.width  = 2400;
            r.height = 2400;
            r.floor  = SDL_Color{48, 42, 58, 255};
            r.holes = {
                SDL_Rect{360, 360, 420, 420},
                SDL_Rect{1620, 360, 420, 420},
                SDL_Rect{360, 1620, 420, 420},
                SDL_Rect{1620, 1620, 420, 420},
            };
            // Spawns sit on the clear "plus" corridor between the corner pits.
            r.enemySpawns = {
                {1200, 240}, {1000, 300}, {1400, 300},
                {1200, 2160}, {1000, 2100}, {1400, 2100},
                {240, 1200}, {2160, 1200},
            };
            break;
        }
        case 4:
        default: {  // "Gauntlet" — vertical pit barriers with central gaps.
            r.width  = 2600;
            r.height = 1400;
            r.floor  = SDL_Color{42, 40, 48, 255};
            r.holes = {
                SDL_Rect{600, 0, 200, 560},   SDL_Rect{600, 840, 200, 560},
                SDL_Rect{1800, 0, 200, 560},  SDL_Rect{1800, 840, 200, 560},
            };
            r.enemySpawns = {
                {1200, 300}, {1200, 1100}, {2300, 380}, {2300, 1020},
                {2470, 700}, {1000, 700},
            };
            break;
        }
    }

    // Every run room has one exit door on the right wall that leads onward to a
    // freshly generated room. All layouts keep the right-centre approach to this
    // door reachable. (Later this can become a fixed "rotation" of rooms.)
    r.doors.push_back(Door{Side::Right, 1, /*generatesNext=*/true});
    return r;
}

// A vendor/shop room: a small, enemy-free room with a shopkeeper and a row of
// item pedestals. Because it has no enemies its exit door is unlocked on entry,
// so the player can browse and leave whenever. Buying is not implemented yet;
// each pedestal just carries placeholder ware data for the future economy.
Room RoomSystem::makeVendorRoom() {
    Room r;
    r.hasRunDoor = false;
    r.isVendorRoom = true;
    r.width  = 1500;
    r.height = 1000;
    r.floor  = SDL_Color{54, 46, 40, 255};  // warmer "shop" tone

    const int cx = r.width / 2;
    // Shopkeeper stall near the top-centre (reuses the vendor sprite).
    r.vendors.push_back(SDL_Rect{cx - 40, 150, 80, 96});

    // A small per-room catalogue. These are stand-ins; a real run would draw a
    // curated, run-specific inventory here.
    static const struct { const char* name; int price; } catalogue[] = {
        {"Health Vial",  30}, {"Spell Scroll", 45}, {"Power Core",  60},
        {"Shield Cell",  35}, {"Swift Boots",  50}, {"Arcane Gem",  70},
    };
    constexpr int kCatalogueSize =
        static_cast<int>(sizeof(catalogue) / sizeof(catalogue[0]));

    constexpr int kItemCount = 3;        // wares this vendor offers
    constexpr int kSpacing   = 240;      // gap between pedestals
    const int rowY    = r.height / 2 + 120;
    const int startX  = cx - kSpacing;   // three pedestals centred on cx
    std::uniform_int_distribution<int> pick(0, kCatalogueSize - 1);
    for (int i = 0; i < kItemCount; ++i) {
        const auto& entry = catalogue[pick(m_rng)];
        ShopItem item;
        item.rect  = SDL_Rect{startX + i * kSpacing - 24, rowY - 24, 48, 48};
        item.name  = entry.name;
        item.price = entry.price;
        r.shopItems.push_back(item);
    }

    r.doors.push_back(Door{Side::Right, 1, /*generatesNext=*/true});
    return r;
}

// Chooses the next run room. The vendor room is a probability spawn whose chance
// rises with run depth; it is forced after a boss (future), and never spawns two
// rooms in a row. Otherwise a random combat layout is built.
Room RoomSystem::makeNextRunRoom() {
    bool vendor = false;
    if (m_forceVendorNext) {
        vendor = true;  // guaranteed after a boss kill (hook; not triggered yet)
    } else if (m_roomsSinceVendor >= 1) {  // at least one normal room between vendors
        const float chance = std::min(
            kVendorChanceCap,
            kVendorBaseChance + kVendorChanceStep * static_cast<float>(m_runDepth));
        std::uniform_real_distribution<float> roll(0.0f, 1.0f);
        vendor = roll(m_rng) < chance;
    }
    m_forceVendorNext = false;

    if (vendor) {
        m_roomsSinceVendor = 0;
        return makeVendorRoom();
    }
    ++m_roomsSinceVendor;
    std::uniform_int_distribution<int> pick(0, kRunLayoutCount - 1);
    return makeRunRoom(pick(m_rng));
}

SDL_Rect RoomSystem::interiorRectFor(const Room& room) const {
    // Rooms live in their own world coordinate space with the interior's top-left
    // at the origin. The camera decides which part of that world is visible.
    return SDL_Rect{0, 0, room.width, room.height};
}

SDL_Rect RoomSystem::interiorRect() const {
    return interiorRectFor(current());
}

SDL_Point RoomSystem::spawnCenter() const {
    const SDL_Rect r = interiorRectFor(m_rooms[0]);
    return SDL_Point{r.x + r.w / 2, r.y + r.h / 2 + 160};
}

bool RoomSystem::playerInRunDoor(float px, float py, float size) const {
    const Room& room = current();
    if (!room.hasRunDoor) {
        return false;
    }
    const SDL_Rect player{
        static_cast<int>(px),
        static_cast<int>(py),
        static_cast<int>(size),
        static_cast<int>(size)};
    return SDL_HasIntersection(&player, &room.runDoor) == SDL_TRUE;
}

void RoomSystem::resetToHub(float& px, float& py, float size) {
    m_current = 0;
    const SDL_Point spawn = spawnCenter();
    px = static_cast<float>(spawn.x) - size * 0.5f;
    py = static_cast<float>(spawn.y) - size * 0.5f;
}

void RoomSystem::startRun(float& px, float& py, float size) {
    // The run always opens on a combat room, never a vendor.
    std::uniform_int_distribution<int> pick(0, kRunLayoutCount - 1);
    m_rooms[1] = makeRunRoom(pick(m_rng));
    m_runDepth = 0;
    m_roomsSinceVendor = 1;   // opener counts as one normal room
    m_forceVendorNext = false;

    m_current = 1;
    const SDL_Rect r = interiorRect();
    // Layouts keep their centre clear, so the player always spawns on solid floor.
    px = static_cast<float>(r.x + r.w / 2) - size * 0.5f;
    py = static_cast<float>(r.y + r.h / 2) - size * 0.5f;
}

void RoomSystem::advanceToNextRoom(Side entrySide, float& px, float& py, float size) {
    // Move one room deeper, then build the next room (combat or vendor) and
    // enter it from the wall the player came through.
    ++m_runDepth;
    m_rooms[1] = makeNextRunRoom();
    enterRoom(1, entrySide, px, py, size);
}

Side RoomSystem::opposite(Side s) {
    switch (s) {
        case Side::Top:    return Side::Bottom;
        case Side::Bottom: return Side::Top;
        case Side::Left:   return Side::Right;
        case Side::Right:  return Side::Left;
    }
    return Side::Top;
}

void RoomSystem::enterRoom(int target, Side entrySide, float& px, float& py, float size) {
    m_current = target;

    const SDL_Rect r = interiorRect();
    const float left   = static_cast<float>(r.x);
    const float top    = static_cast<float>(r.y);
    const float right  = static_cast<float>(r.x + r.w) - size;
    const float bottom = static_cast<float>(r.y + r.h) - size;
    const float midX   = r.x + r.w * 0.5f - size * 0.5f;
    const float midY   = r.y + r.h * 0.5f - size * 0.5f;

    // Place the player just inside the wall they entered through, centered on
    // that wall's (centered) door.
    switch (entrySide) {
        case Side::Top:    px = midX;                 py = top + kEntryInset;    break;
        case Side::Bottom: px = midX;                 py = bottom - kEntryInset; break;
        case Side::Left:   px = left + kEntryInset;   py = midY;                 break;
        case Side::Right:  px = right - kEntryInset;  py = midY;                 break;
    }
}

bool RoomSystem::resolvePlayer(float& px, float& py, float size, bool doorsUnlocked) {
    const SDL_Rect r = interiorRect();
    const float left   = static_cast<float>(r.x);
    const float top    = static_cast<float>(r.y);
    const float right  = static_cast<float>(r.x + r.w) - size;
    const float bottom = static_cast<float>(r.y + r.h) - size;

    const float cx = px + size * 0.5f;  // player center
    const float cy = py + size * 0.5f;
    const float midX = r.x + r.w * 0.5f;
    const float midY = r.y + r.h * 0.5f;
    const float half = kDoorWidth * 0.5f;

    // A door is crossed when the player reaches its wall while aligned with the
    // (centered) opening. Checked before clamping so the door isn't a wall.
    // While the room is sealed (doorsUnlocked == false) this loop is skipped, so
    // the clamp below stops the player at the threshold like any other wall.
    for (const Door& d : current().doors) {
        if (!doorsUnlocked) break;

        bool crossing = false;
        switch (d.side) {
            case Side::Top:
                crossing = (cx > midX - half && cx < midX + half) && (py <= top);
                break;
            case Side::Bottom:
                crossing = (cx > midX - half && cx < midX + half) && (py >= bottom);
                break;
            case Side::Left:
                crossing = (cy > midY - half && cy < midY + half) && (px <= left);
                break;
            case Side::Right:
                crossing = (cy > midY - half && cy < midY + half) && (px >= right);
                break;
        }
        if (crossing) {
            if (d.generatesNext) {
                advanceToNextRoom(opposite(d.side), px, py, size);
            } else {
                enterRoom(d.targetRoom, opposite(d.side), px, py, size);
            }
            return true;
        }
    }

    // Otherwise the walls are solid: clamp the player inside the interior.
    if (px < left)        px = left;
    else if (px > right)  px = right;
    if (py < top)         py = top;
    else if (py > bottom) py = bottom;

    // Pits are impassable: if the player AABB overlaps a hole, push it back out
    // along whichever axis has the shallowest overlap (so they slide along the
    // pit's edge rather than snapping across it).
    for (const SDL_Rect& hole : current().holes) {
        const float hl = static_cast<float>(hole.x);
        const float hr = static_cast<float>(hole.x + hole.w);
        const float ht = static_cast<float>(hole.y);
        const float hb = static_cast<float>(hole.y + hole.h);
        const float pl = px, pr = px + size, pt = py, pb = py + size;
        if (pr <= hl || pl >= hr || pb <= ht || pt >= hb) {
            continue;  // no overlap
        }
        const float penLeft  = pr - hl;  // distance to clear by moving left
        const float penRight = hr - pl;  // ... right
        const float penUp    = pb - ht;  // ... up
        const float penDown  = hb - pt;  // ... down
        const float minX = std::min(penLeft, penRight);
        const float minY = std::min(penUp, penDown);
        if (minX < minY) {
            px += (penLeft < penRight) ? -penLeft : penRight;
        } else {
            py += (penUp < penDown) ? -penUp : penDown;
        }
    }
    return false;
}

bool RoomSystem::resolveSpell(float& sx, float& sy, float size) {
    const SDL_Rect r = interiorRect();
    const float left   = static_cast<float>(r.x) + size;
    const float top    = static_cast<float>(r.y) + size;
    const float right  = static_cast<float>(r.x + r.w) - size;
    const float bottom = static_cast<float>(r.y + r.h) - size;

    // clamp the spell inside the interior of the current room
    if (sx < left)        return true;
    else if (sx > right)  return true;
    if (sy < top)         return true;
    else if (sy > bottom) return true;

    return false;
}

void RoomSystem::render(SDL_Renderer* renderer, float cameraX, float cameraY,
                        SDL_Texture* vendorTexture,
                        SDL_Texture* floorTexture) const {
    const Room&    room     = current();
    const SDL_Rect interior = interiorRect();
    auto screenRect = [&](const SDL_Rect& world) {
        return SDL_Rect{
            static_cast<int>(world.x - cameraX),
            static_cast<int>(world.y - cameraY),
            world.w,
            world.h};
    };

    // Floor.
    const SDL_Rect floor = screenRect(interior);
    constexpr int kTile = 96;
    if (floorTexture) {
        // Tile the floor sprite across the interior. Tiles are anchored to world
        // coordinates so they stay put as the camera moves, and clipped to the
        // interior so edge tiles never spill onto the walls.
        SDL_RenderSetClipRect(renderer, &floor);
        for (int y = interior.y; y < interior.y + interior.h; y += kTile) {
            for (int x = interior.x; x < interior.x + interior.w; x += kTile) {
                SDL_Rect dst{static_cast<int>(x - cameraX),
                             static_cast<int>(y - cameraY), kTile, kTile};
                SDL_RenderCopy(renderer, floorTexture, nullptr, &dst);
            }
        }
        SDL_RenderSetClipRect(renderer, nullptr);
    } else {
        // Fallback: a flat colour with a subtle grid so movement reads.
        SDL_SetRenderDrawColor(renderer, room.floor.r, room.floor.g, room.floor.b, 255);
        SDL_RenderFillRect(renderer, &floor);
        SDL_SetRenderDrawColor(renderer, room.floor.r + 12, room.floor.g + 12,
                               room.floor.b + 12, 255);
        for (int x = interior.x; x <= interior.x + interior.w; x += kTile) {
            SDL_RenderDrawLine(renderer,
                               static_cast<int>(x - cameraX),
                               static_cast<int>(interior.y - cameraY),
                               static_cast<int>(x - cameraX),
                               static_cast<int>(interior.y + interior.h - cameraY));
        }
        for (int y = interior.y; y <= interior.y + interior.h; y += kTile) {
            SDL_RenderDrawLine(renderer,
                               static_cast<int>(interior.x - cameraX),
                               static_cast<int>(y - cameraY),
                               static_cast<int>(interior.x + interior.w - cameraX),
                               static_cast<int>(y - cameraY));
        }
    }

    // Pits in the floor: a dark hole with a slightly lighter rim for depth.
    for (const SDL_Rect& hole : room.holes) {
        const SDL_Rect h = screenRect(hole);
        SDL_SetRenderDrawColor(renderer, 10, 10, 14, 255);
        SDL_RenderFillRect(renderer, &h);
        SDL_SetRenderDrawColor(renderer, 34, 36, 46, 255);
        SDL_RenderDrawRect(renderer, &h);
        const SDL_Rect lip{h.x + 4, h.y + 4, h.w - 8, h.h - 8};
        SDL_SetRenderDrawColor(renderer, 20, 22, 28, 255);
        SDL_RenderDrawRect(renderer, &lip);
    }

    // Wall frame around the floor.
    SDL_SetRenderDrawColor(renderer, 64, 66, 78, 255);
    const SDL_Rect topW{interior.x - kWall, interior.y - kWall,
                        interior.w + 2 * kWall, kWall};
    const SDL_Rect botW{interior.x - kWall, interior.y + interior.h,
                        interior.w + 2 * kWall, kWall};
    const SDL_Rect leftW{interior.x - kWall, interior.y, kWall, interior.h};
    const SDL_Rect rightW{interior.x + interior.w, interior.y, kWall, interior.h};
    const SDL_Rect screenTop = screenRect(topW);
    const SDL_Rect screenBottom = screenRect(botW);
    const SDL_Rect screenLeft = screenRect(leftW);
    const SDL_Rect screenRight = screenRect(rightW);
    SDL_RenderFillRect(renderer, &screenTop);
    SDL_RenderFillRect(renderer, &screenBottom);
    SDL_RenderFillRect(renderer, &screenLeft);
    SDL_RenderFillRect(renderer, &screenRight);

    // Door openings, drawn over the wall frame.
    SDL_SetRenderDrawColor(renderer, 150, 110, 60, 255);
    const int midX = interior.x + interior.w / 2;
    const int midY = interior.y + interior.h / 2;
    for (const Door& d : room.doors) {
        SDL_Rect o{};
        switch (d.side) {
            case Side::Top:
                o = SDL_Rect{midX - kDoorWidth / 2, interior.y - kWall, kDoorWidth, kWall};
                break;
            case Side::Bottom:
                o = SDL_Rect{midX - kDoorWidth / 2, interior.y + interior.h, kDoorWidth, kWall};
                break;
            case Side::Left:
                o = SDL_Rect{interior.x - kWall, midY - kDoorWidth / 2, kWall, kDoorWidth};
                break;
            case Side::Right:
                o = SDL_Rect{interior.x + interior.w, midY - kDoorWidth / 2, kWall, kDoorWidth};
                break;
        }
        const SDL_Rect screenOpening = screenRect(o);
        SDL_RenderFillRect(renderer, &screenOpening);
    }

    if (room.hasRunDoor) {
        const SDL_Rect portal = screenRect(room.runDoor);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 66, 110, 190, 220);
        SDL_RenderFillRect(renderer, &portal);
        SDL_SetRenderDrawColor(renderer, 120, 210, 255, 255);
        SDL_RenderDrawRect(renderer, &portal);
        const SDL_Rect inner{portal.x + 20, portal.y + 14, portal.w - 40, portal.h - 28};
        SDL_SetRenderDrawColor(renderer, 30, 42, 95, 235);
        SDL_RenderFillRect(renderer, &inner);
    }

    for (const SDL_Rect& vendor : room.vendors) {
        const SDL_Rect dst = screenRect(vendor);
        if (vendorTexture) {
            SDL_RenderCopy(renderer, vendorTexture, nullptr, &dst);
        } else {
            SDL_SetRenderDrawColor(renderer, 180, 70, 95, 255);
            SDL_RenderFillRect(renderer, &dst);
        }
        SDL_SetRenderDrawColor(renderer, 255, 220, 120, 255);
        SDL_RenderDrawRect(renderer, &dst);
    }

    // Vendor wares: a pedestal with a glowing item marker on top. The item's
    // name/price are drawn by the HUD layer (Game) where the font lives.
    for (const ShopItem& item : room.shopItems) {
        const SDL_Rect dst = screenRect(item.rect);
        // Pedestal base.
        SDL_SetRenderDrawColor(renderer, 70, 60, 52, 255);
        SDL_Rect base{dst.x - 4, dst.y + dst.h - 10, dst.w + 8, 14};
        SDL_RenderFillRect(renderer, &base);
        SDL_SetRenderDrawColor(renderer, 110, 96, 80, 255);
        SDL_RenderDrawRect(renderer, &base);
        // Item marker (dimmed once purchased, for the future economy).
        if (item.purchased) {
            SDL_SetRenderDrawColor(renderer, 90, 90, 96, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 245, 215, 120, 255);
        }
        SDL_RenderFillRect(renderer, &dst);
        SDL_SetRenderDrawColor(renderer, 255, 245, 200, 255);
        SDL_RenderDrawRect(renderer, &dst);
    }
}

}  // namespace gaia
