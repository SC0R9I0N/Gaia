#include "Room.hpp"

#include <algorithm>
#include <cmath>

namespace gaia {

namespace {

// Hub column and urn placements — shared by render() and currentProps() so the
// drawn art and its collision footprint can never drift apart.
constexpr SDL_Rect kHubPillars[] = {
    {226, 150, 72, 152}, {1502, 150, 72, 152},
    {226, 858, 72, 152}, {1502, 858, 72, 152},
    {428, 544, 64, 140}, {1308, 544, 64, 140}
};
constexpr SDL_Rect kHubUrns[] = {
    {320, 250, 44, 52}, {1436, 250, 44, 52},
    {320, 800, 44, 52}, {1436, 800, 44, 52},
    {506, 470, 40, 48}, {1254, 470, 40, 48}
};

// The solid lower "footprint" of a prop: only the bottom band collides, so the
// taller art above it can be walked behind. Tuned per kind to sit at the base.
SDL_Rect basePropCollider(const SDL_Rect& d, PropKind kind) {
    int inset = 12, baseH = 24;
    if (kind == PropKind::Pillar) { inset = 10; baseH = 26; }
    else if (kind == PropKind::Urn) { inset = 6; baseH = 14; }
    if (baseH > d.h) baseH = d.h;
    return SDL_Rect{d.x + inset, d.y + d.h - baseH, d.w - 2 * inset, baseH};
}

// Minimum-translation push of the player's AABB out of a solid rect.
void resolveAABB(float& px, float& py, float size, const SDL_Rect& c) {
    const float pl = px, pr = px + size, pt = py, pb = py + size;
    const float cl = static_cast<float>(c.x), cr = static_cast<float>(c.x + c.w);
    const float ct = static_cast<float>(c.y), cb = static_cast<float>(c.y + c.h);
    if (pr <= cl || pl >= cr || pb <= ct || pt >= cb) return;  // no overlap
    const float oL = pr - cl, oR = cr - pl, oT = pb - ct, oB = cb - pt;
    const float mx = oL < oR ? -oL : oR;   // signed push along x / y
    const float my = oT < oB ? -oT : oB;
    if (std::fabs(mx) < std::fabs(my)) px += mx;
    else py += my;
}

}  // namespace

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
    hub.floor  = SDL_Color{178, 135, 82, 255};
    hub.hasRunDoor = true;
    hub.runDoor = SDL_Rect{840, 360, 120, 96};
    hub.hasRuneVendor = true;
    hub.runeVendor = SDL_Rect{500, 430, 80, 96};
    hub.hasShadyVendor = true;
    hub.shadyVendor = SDL_Rect{1220, 430, 80, 96};
    hub.hasConsciousnessConsole = true;
    hub.consciousnessConsole = SDL_Rect{840, 830, 120, 96};
    hub.hasSkillTreeConsole = true;
    hub.skillTreeConsole = SDL_Rect{500, 760, 80, 96};
    hub.hasArtifactStorage = true;
    hub.artifactStorage = SDL_Rect{1220, 760, 80, 96};
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

bool RoomSystem::playerInRuneVendor(float px, float py, float size) const {
    const Room& room = current();
    if (!room.hasRuneVendor) {
        return false;
    }
    const SDL_Rect player{
        static_cast<int>(px),
        static_cast<int>(py),
        static_cast<int>(size),
        static_cast<int>(size)};
    SDL_Rect interaction = room.runeVendor;
    interaction.x -= 24;
    interaction.y -= 24;
    interaction.w += 48;
    interaction.h += 48;
    return SDL_HasIntersection(&player, &interaction) == SDL_TRUE;
}

bool RoomSystem::playerInShadyVendor(float px, float py, float size) const {
    const Room& room = current();
    if (!room.hasShadyVendor) {
        return false;
    }
    const SDL_Rect player{
        static_cast<int>(px),
        static_cast<int>(py),
        static_cast<int>(size),
        static_cast<int>(size)};
    SDL_Rect interaction = room.shadyVendor;
    interaction.x -= 24;
    interaction.y -= 24;
    interaction.w += 48;
    interaction.h += 48;
    return SDL_HasIntersection(&player, &interaction) == SDL_TRUE;
}

bool RoomSystem::playerInConsciousnessConsole(float px, float py, float size) const {
    const Room& room = current();
    if (!room.hasConsciousnessConsole) {
        return false;
    }
    const SDL_Rect player{
        static_cast<int>(px),
        static_cast<int>(py),
        static_cast<int>(size),
        static_cast<int>(size)};
    SDL_Rect interaction = room.consciousnessConsole;
    interaction.x -= 24;
    interaction.y -= 24;
    interaction.w += 48;
    interaction.h += 48;
    return SDL_HasIntersection(&player, &interaction) == SDL_TRUE;
}

bool RoomSystem::playerInSkillTreeConsole(float px, float py, float size) const {
    const Room& room = current();
    if (!room.hasSkillTreeConsole) {
        return false;
    }
    const SDL_Rect player{
        static_cast<int>(px),
        static_cast<int>(py),
        static_cast<int>(size),
        static_cast<int>(size)};
    SDL_Rect interaction = room.skillTreeConsole;
    interaction.x -= 24;
    interaction.y -= 24;
    interaction.w += 48;
    interaction.h += 48;
    return SDL_HasIntersection(&player, &interaction) == SDL_TRUE;
}

bool RoomSystem::playerInArtifactStorage(float px, float py, float size) const {
    const Room& room = current();
    if (!room.hasArtifactStorage) {
        return false;
    }
    const SDL_Rect player{
        static_cast<int>(px),
        static_cast<int>(py),
        static_cast<int>(size),
        static_cast<int>(size)};
    SDL_Rect interaction = room.artifactStorage;
    interaction.x -= 24;
    interaction.y -= 24;
    interaction.w += 48;
    interaction.h += 48;
    return SDL_HasIntersection(&player, &interaction) == SDL_TRUE;
}

void RoomSystem::resetToHub(float& px, float& py, float size) {
    m_current = 0;
    m_runDepth = 0;
    m_roomsSinceVendor = 0;
    m_forceVendorNext = false;
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

    // Solid prop footprints (the lower base of pillars/vendors/urns/consoles).
    // Resolve after the walls, then re-clamp so a push can't eject the player
    // through a wall.
    for (const PropGeom& g : currentProps()) {
        resolveAABB(px, py, size, g.collider);
    }
    if (px < left)        px = left;
    else if (px > right)  px = right;
    if (py < top)         py = top;
    else if (py > bottom) py = bottom;

    return false;
}

bool RoomSystem::playerOverPit(float px, float py, float size) const {
    const float cx = px + size * 0.5f;
    const float cy = py + size * 0.5f;
    for (const SDL_Rect& hole : current().holes) {
        const float hl = static_cast<float>(hole.x);
        const float hr = static_cast<float>(hole.x + hole.w);
        const float ht = static_cast<float>(hole.y);
        const float hb = static_cast<float>(hole.y + hole.h);
        if (cx > hl && cx < hr && cy > ht && cy < hb) {
            return true;
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

    const Room& room = current();
    for (const SDL_Rect& hole : room.holes) {
        const float closestX = std::clamp(sx, static_cast<float>(hole.x),
                                          static_cast<float>(hole.x + hole.w));
        const float closestY = std::clamp(sy, static_cast<float>(hole.y),
                                          static_cast<float>(hole.y + hole.h));
        const float dx = sx - closestX;
        const float dy = sy - closestY;
        if (dx * dx + dy * dy <= size * size) {
            return true;
        }
    }

    return false;
}

void RoomSystem::render(SDL_Renderer* renderer, float cameraX, float cameraY,
                        SDL_Texture* vendorTexture,
                        SDL_Texture* runeVendorTexture,
                        SDL_Texture* shadyVendorTexture,
                        SDL_Texture* consciousnessConsoleTexture,
                        SDL_Texture* skillTreeConsoleTexture,
                        SDL_Texture* artifactStorageTexture,
                        SDL_Texture* runPortalTexture,
                        const HubTextures& hub,
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
    // Tiles a texture to fill a world-space rect, anchored to a global world
    // grid (so neighbouring bands line up and tiles don't swim under the
    // camera) and clipped to the rect so partial edge tiles never spill out.
    auto tileTexture = [&](SDL_Texture* tex, const SDL_Rect& worldRect) {
        if (!tex) return;
        int tw = 0, th = 0;
        SDL_QueryTexture(tex, nullptr, nullptr, &tw, &th);
        if (tw <= 0 || th <= 0) return;
        const SDL_Rect clip = screenRect(worldRect);
        SDL_RenderSetClipRect(renderer, &clip);
        const int startX = worldRect.x - (((worldRect.x % tw) + tw) % tw);
        const int startY = worldRect.y - (((worldRect.y % th) + th) % th);
        for (int y = startY; y < worldRect.y + worldRect.h; y += th) {
            for (int x = startX; x < worldRect.x + worldRect.w; x += tw) {
                SDL_Rect dst{static_cast<int>(x - cameraX),
                             static_cast<int>(y - cameraY), tw, th};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
            }
        }
        SDL_RenderSetClipRect(renderer, nullptr);
    };
    // Draws a single sprite stretched to a world-space rect.
    auto drawSprite = [&](SDL_Texture* tex, const SDL_Rect& worldRect) {
        if (!tex) return;
        const SDL_Rect dst = screenRect(worldRect);
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
    };
    auto drawEllipse = [&](int cx, int cy, int rx, int ry,
                           SDL_Color color, bool filled) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        for (int y = -ry; y <= ry; ++y) {
            const float t = 1.0f - static_cast<float>(y * y) /
                static_cast<float>(ry * ry);
            const int half = static_cast<int>(rx * std::sqrt(t > 0.0f ? t : 0.0f));
            if (filled) {
                SDL_RenderDrawLine(renderer, cx - half, cy + y, cx + half, cy + y);
            } else {
                SDL_RenderDrawPoint(renderer, cx - half, cy + y);
                SDL_RenderDrawPoint(renderer, cx + half, cy + y);
            }
        }
    };

    // Floor.
    const SDL_Rect floor = screenRect(interior);
    constexpr int kTile = 96;
    if (isHub() && hub.floor) {
        // Sandstone brick floor: tile the seamless texture across the interior.
        SDL_SetRenderDrawColor(renderer, 178, 135, 82, 255);
        SDL_RenderFillRect(renderer, &floor);
        tileTexture(hub.floor, interior);
    } else if (isHub()) {
        SDL_SetRenderDrawColor(renderer, 178, 135, 82, 255);
        SDL_RenderFillRect(renderer, &floor);
        // Staggered tiles overrun the interior (62px wide plus up to 22px of
        // stagger, stepped every 64px), so clip them to the floor to keep edge
        // tiles from spilling onto the bottom and right walls.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_RenderSetClipRect(renderer, &floor);
        for (int y = interior.y; y < interior.y + interior.h; y += 64) {
            for (int x = interior.x; x < interior.x + interior.w; x += 64) {
                const int stagger = ((y / 64) % 2) * 22;
                const SDL_Rect tile{
                    static_cast<int>(x + stagger - cameraX),
                    static_cast<int>(y - cameraY),
                    62,
                    62};
                const int tone = ((x / 64) + (y / 64)) % 4;
                SDL_SetRenderDrawColor(renderer,
                                       static_cast<Uint8>(184 + tone * 5),
                                       static_cast<Uint8>(140 + tone * 4),
                                       static_cast<Uint8>(84 + tone * 3),
                                       255);
                SDL_RenderFillRect(renderer, &tile);
                // Beveled stone: a lit top-left edge and a shadowed bottom-right
                // edge give each flagstone a little depth instead of reading flat.
                SDL_SetRenderDrawColor(renderer, 214, 176, 116, 150);
                SDL_RenderDrawLine(renderer, tile.x, tile.y,
                                   tile.x + tile.w - 1, tile.y);
                SDL_RenderDrawLine(renderer, tile.x, tile.y,
                                   tile.x, tile.y + tile.h - 1);
                SDL_SetRenderDrawColor(renderer, 120, 84, 48, 170);
                SDL_RenderDrawLine(renderer, tile.x, tile.y + tile.h - 1,
                                   tile.x + tile.w - 1, tile.y + tile.h - 1);
                SDL_RenderDrawLine(renderer, tile.x + tile.w - 1, tile.y,
                                   tile.x + tile.w - 1, tile.y + tile.h - 1);
            }
        }
        SDL_RenderSetClipRect(renderer, nullptr);

        const SDL_Rect centralPlaza = screenRect(SDL_Rect{560, 250, 680, 650});
        const int plazaCx = centralPlaza.x + centralPlaza.w / 2;
        const int plazaCy = centralPlaza.y + centralPlaza.h / 2;
        const int plazaRx = centralPlaza.w / 2;
        const int plazaRy = centralPlaza.h / 2;
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        // A tiled-stone medallion: a warm inlay fading inward, ringed by a few
        // concentric bands so the plaza reads as a crafted centerpiece.
        drawEllipse(plazaCx, plazaCy, plazaRx, plazaRy,
                    SDL_Color{210, 164, 98, 95}, true);
        drawEllipse(plazaCx, plazaCy, plazaRx * 78 / 100, plazaRy * 78 / 100,
                    SDL_Color{220, 176, 110, 90}, true);
        drawEllipse(plazaCx, plazaCy, plazaRx * 40 / 100, plazaRy * 40 / 100,
                    SDL_Color{232, 190, 124, 95}, true);
        drawEllipse(plazaCx, plazaCy, plazaRx, plazaRy,
                    SDL_Color{122, 84, 44, 210}, false);
        drawEllipse(plazaCx, plazaCy, plazaRx * 78 / 100, plazaRy * 78 / 100,
                    SDL_Color{150, 104, 56, 170}, false);
        drawEllipse(plazaCx, plazaCy, plazaRx * 40 / 100, plazaRy * 40 / 100,
                    SDL_Color{150, 104, 56, 150}, false);

        const SDL_Rect northSouth = screenRect(SDL_Rect{830, 320, 140, 580});
        const SDL_Rect eastWest = screenRect(SDL_Rect{470, 520, 860, 118});
        SDL_SetRenderDrawColor(renderer, 196, 149, 84, 135);
        SDL_RenderFillRect(renderer, &northSouth);
        SDL_RenderFillRect(renderer, &eastWest);
        SDL_SetRenderDrawColor(renderer, 132, 88, 44, 175);
        SDL_RenderDrawRect(renderer, &northSouth);
        SDL_RenderDrawRect(renderer, &eastWest);

        const SDL_Rect rug = screenRect(SDL_Rect{720, 505, 360, 150});
        SDL_SetRenderDrawColor(renderer, 116, 48, 42, 155);
        SDL_RenderFillRect(renderer, &rug);
        SDL_SetRenderDrawColor(renderer, 218, 166, 86, 210);
        SDL_RenderDrawRect(renderer, &rug);
        const SDL_Rect rugInner{rug.x + 12, rug.y + 12, rug.w - 24, rug.h - 24};
        SDL_RenderDrawRect(renderer, &rugInner);
    } else if (floorTexture) {
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
    if (isHub() && hub.wall) {
        // Sandstone brick walls: a thick band tiled around the interior. The
        // band sits in the void outside the play area, so collision (which
        // clamps the player to the interior) is unaffected by its thickness.
        constexpr int kHubWall = 44;
        tileTexture(hub.wall, SDL_Rect{interior.x - kHubWall, interior.y - kHubWall,
                                       interior.w + 2 * kHubWall, kHubWall});
        tileTexture(hub.wall, SDL_Rect{interior.x - kHubWall, interior.y + interior.h,
                                       interior.w + 2 * kHubWall, kHubWall});
        tileTexture(hub.wall, SDL_Rect{interior.x - kHubWall, interior.y,
                                       kHubWall, interior.h});
        tileTexture(hub.wall, SDL_Rect{interior.x + interior.w, interior.y,
                                       kHubWall, interior.h});
        // A soft contact shadow where the walls meet the floor adds depth.
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 36, 24, 10, 120);
        SDL_RenderDrawRect(renderer, &floor);
    } else {
        if (isHub()) {
            SDL_SetRenderDrawColor(renderer, 126, 82, 40, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 64, 66, 78, 255);
        }
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
        if (isHub()) {
            SDL_SetRenderDrawColor(renderer, 204, 150, 76, 255);
            SDL_RenderDrawRect(renderer, &screenTop);
            SDL_RenderDrawRect(renderer, &screenBottom);
            SDL_RenderDrawRect(renderer, &screenLeft);
            SDL_RenderDrawRect(renderer, &screenRight);
            SDL_SetRenderDrawColor(renderer, 92, 54, 24, 255);
            for (int x = interior.x; x <= interior.x + interior.w; x += 96) {
                SDL_RenderDrawLine(renderer, static_cast<int>(x - cameraX),
                                   static_cast<int>(interior.y - kWall - cameraY),
                                   static_cast<int>(x - cameraX),
                                   static_cast<int>(interior.y - cameraY));
                SDL_RenderDrawLine(renderer, static_cast<int>(x - cameraX),
                                   static_cast<int>(interior.y + interior.h - cameraY),
                                   static_cast<int>(x - cameraX),
                                   static_cast<int>(interior.y + interior.h + kWall - cameraY));
            }
            for (int y = interior.y; y <= interior.y + interior.h; y += 96) {
                SDL_RenderDrawLine(renderer, static_cast<int>(interior.x - kWall - cameraX),
                                   static_cast<int>(y - cameraY),
                                   static_cast<int>(interior.x - cameraX),
                                   static_cast<int>(y - cameraY));
                SDL_RenderDrawLine(renderer, static_cast<int>(interior.x + interior.w - cameraX),
                                   static_cast<int>(y - cameraY),
                                   static_cast<int>(interior.x + interior.w + kWall - cameraX),
                                   static_cast<int>(y - cameraY));
            }
        }
    }

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

    if (isHub()) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        // Banners hang from the top wall, flanking the run portal.
        if (hub.banner) {
            const SDL_Rect banners[] = {
                {576, -12, 48, 96}, {1176, -12, 48, 96}
            };
            for (const SDL_Rect& b : banners) drawSprite(hub.banner, b);
        }

        // Sandstone columns frame the hall at the corners and mid-walls. With no
        // pillar art, fall back to a simple capped block.
        for (const SDL_Rect& p : kHubPillars) {
            // Contact shadow grounds the column and separates it from the floor.
            const SDL_Rect ps = screenRect(p);
            drawEllipse(ps.x + ps.w / 2, ps.y + ps.h - 4,
                        ps.w / 2 + 2, 11, SDL_Color{0, 0, 0, 95}, true);
            if (hub.pillar) {
                drawSprite(hub.pillar, p);
            } else {
                SDL_SetRenderDrawColor(renderer, 150, 98, 48, 255);
                SDL_RenderFillRect(renderer, &ps);
                SDL_SetRenderDrawColor(renderer, 224, 168, 88, 255);
                SDL_RenderDrawRect(renderer, &ps);
            }
        }

        // Urns scattered near the columns add lived-in clutter.
        if (hub.urn) {
            for (const SDL_Rect& u : kHubUrns) {
                const SDL_Rect us = screenRect(u);
                drawEllipse(us.x + us.w / 2, us.y + us.h - 3,
                            us.w / 2, 7, SDL_Color{0, 0, 0, 90}, true);
                drawSprite(hub.urn, u);
            }
        }

        // Animated wall lanterns around the perimeter. Each cycles its
        // sprite-sheet frame with a per-lantern offset so the flames (and their
        // baked-in glow) never flicker in unison.
        const SDL_Point lanterns[] = {
            {360, 70}, {900, 70}, {1440, 70},
            {360, 1030}, {900, 1030}, {1440, 1030},
            {72, 560}, {1728, 560}
        };
        if (hub.lantern) {
            int sheetW = 0, sheetH = 0;
            SDL_QueryTexture(hub.lantern, nullptr, nullptr, &sheetW, &sheetH);
            const int frameSize = sheetH > 0 ? sheetH : 1;
            const int frameCount =
                sheetW / frameSize > 0 ? sheetW / frameSize : 1;
            const Uint32 ticks = SDL_GetTicks();
            for (int i = 0; i < static_cast<int>(SDL_arraysize(lanterns)); ++i) {
                const int frame = static_cast<int>(
                    ((ticks / 90) + static_cast<Uint32>(i) * 3) %
                    static_cast<Uint32>(frameCount));
                const SDL_Rect src{frame * frameSize, 0, frameSize, frameSize};
                const SDL_Rect dst{
                    static_cast<int>(lanterns[i].x - cameraX) - frameSize / 2,
                    static_cast<int>(lanterns[i].y - cameraY) - frameSize / 2,
                    frameSize, frameSize};
                SDL_RenderCopy(renderer, hub.lantern, &src, &dst);
            }
        } else {
            // Fallback: a small flickering glow + flame, phase-offset per lamp.
            const float lt = static_cast<float>(SDL_GetTicks());
            for (int i = 0; i < static_cast<int>(SDL_arraysize(lanterns)); ++i) {
                const int sx = static_cast<int>(lanterns[i].x - cameraX);
                const int sy = static_cast<int>(lanterns[i].y - cameraY);
                const float flick = 0.5f + 0.5f *
                    std::sin(lt * 0.006f + static_cast<float>(i) * 1.7f);
                drawEllipse(sx, sy, 26, 22,
                            SDL_Color{255, 160, 60,
                                      static_cast<Uint8>(40 + flick * 50)}, true);
                drawEllipse(sx, sy, 6, 11 + static_cast<int>(flick * 4),
                            SDL_Color{255, 210, 120, 255}, true);
            }
        }

        const SDL_Point spawn = spawnCenter();
        const SDL_Rect plateWorld{spawn.x - 98, spawn.y - 34, 196, 68};
        const SDL_Rect plate = screenRect(plateWorld);
        const int pcx = plate.x + plate.w / 2;
        const int pcy = plate.y + plate.h / 2;
        const int prx = plate.w / 2;
        const int pry = plate.h / 2;
        const float spawnTime = static_cast<float>(SDL_GetTicks());
        // One slow, smooth breath drives every part of the pad, so the whole
        // thing glows together instead of jittering.
        const float pulse = 0.5f + 0.5f * std::sin(spawnTime * 0.0022f);

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        // Halo breathing under the dais.
        drawEllipse(pcx, pcy + 4, prx + 22, pry + 16,
                    SDL_Color{45, 130, 255,
                              static_cast<Uint8>(38 + pulse * 52)}, true);
        drawEllipse(pcx, pcy + 2, prx + 10, pry + 8,
                    SDL_Color{74, 152, 255,
                              static_cast<Uint8>(52 + pulse * 66)}, true);

        // Beveled stone dais: a shadow base, a lit top face, and a darker inset
        // so the disc reads as raised, polished rock.
        drawEllipse(pcx, pcy + 3, prx, pry, SDL_Color{40, 40, 50, 255}, true);
        drawEllipse(pcx, pcy, prx, pry, SDL_Color{96, 94, 106, 255}, true);
        drawEllipse(pcx, pcy - 3, prx - 14, pry - 10,
                    SDL_Color{122, 120, 134, 255}, true);
        drawEllipse(pcx, pcy - 4, prx - 30, pry - 20,
                    SDL_Color{70, 70, 84, 255}, true);
        drawEllipse(pcx, pcy, prx, pry, SDL_Color{26, 26, 34, 255}, false);

        // Glowing summoning circle: two concentric rings inscribed on the face.
        const Uint8 runeA = static_cast<Uint8>(150 + pulse * 95);
        drawEllipse(pcx, pcy - 2, prx - 20, pry - 14,
                    SDL_Color{95, 210, 255, runeA}, false);
        drawEllipse(pcx, pcy - 2, prx - 34, pry - 23,
                    SDL_Color{150, 232, 255, runeA}, false);

        // A ring of rune ticks rotating slowly around the circle — a calm,
        // deliberate spin in place of the old static chevron clutter.
        const float spin = spawnTime * 0.0006f;
        const int ringRx = prx - 20;
        const int ringRy = pry - 14;
        SDL_SetRenderDrawColor(renderer, 130, 224, 255, runeA);
        for (int k = 0; k < 8; ++k) {
            const float a = spin +
                static_cast<float>(k) * 6.2831853f / 8.0f;
            const int tx = pcx + static_cast<int>(std::cos(a) * ringRx);
            const int ty = (pcy - 2) + static_cast<int>(std::sin(a) * ringRy);
            const SDL_Rect tick{tx - 2, ty - 2, 4, 4};
            SDL_RenderFillRect(renderer, &tick);
        }

        // Clean central sigil: a four-point star with a small ring at its heart.
        const SDL_Color sig{170, 238, 255,
                            static_cast<Uint8>(180 + pulse * 75)};
        const int scy = pcy - 2;
        const int armX = prx - 48;
        const int armY = pry - 16;
        SDL_SetRenderDrawColor(renderer, sig.r, sig.g, sig.b, sig.a);
        SDL_RenderDrawLine(renderer, pcx, scy - armY, pcx + armX, scy);
        SDL_RenderDrawLine(renderer, pcx + armX, scy, pcx, scy + armY);
        SDL_RenderDrawLine(renderer, pcx, scy + armY, pcx - armX, scy);
        SDL_RenderDrawLine(renderer, pcx - armX, scy, pcx, scy - armY);
        drawEllipse(pcx, scy, 7, 6, sig, false);
    }

    if (room.hasRunDoor) {
        const SDL_Rect portal = screenRect(room.runDoor);
        if (runPortalTexture) {
            int sheetW = 0;
            int sheetH = 0;
            SDL_QueryTexture(runPortalTexture, nullptr, nullptr, &sheetW, &sheetH);
            const int frameSize = sheetH > 0 ? sheetH : 1;
            const int frameCount = sheetW / frameSize > 0 ? sheetW / frameSize : 1;
            const int frame = static_cast<int>((SDL_GetTicks() / 110) %
                                               static_cast<Uint32>(frameCount));
            const SDL_Rect src{frame * frameSize, 0, frameSize, frameSize};
            SDL_RenderCopy(renderer, runPortalTexture, &src, &portal);
        } else {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 66, 110, 190, 220);
            SDL_RenderFillRect(renderer, &portal);
            SDL_SetRenderDrawColor(renderer, 120, 210, 255, 255);
            SDL_RenderDrawRect(renderer, &portal);
            const SDL_Rect inner{portal.x + 20, portal.y + 14, portal.w - 40, portal.h - 28};
            SDL_SetRenderDrawColor(renderer, 30, 42, 95, 235);
            SDL_RenderFillRect(renderer, &inner);
        }
    }

    for (const SDL_Rect& vendor : room.vendors) {
        const SDL_Rect dst = screenRect(vendor);
        if (vendorTexture) {
            SDL_RenderCopy(renderer, vendorTexture, nullptr, &dst);
        } else {
            SDL_SetRenderDrawColor(renderer, 180, 70, 95, 255);
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, 255, 220, 120, 255);
            SDL_RenderDrawRect(renderer, &dst);
        }
    }

    if (room.hasRuneVendor) {
        const SDL_Rect dst = screenRect(room.runeVendor);
        if (runeVendorTexture) {
            SDL_RenderCopy(renderer, runeVendorTexture, nullptr, &dst);
        } else {
            SDL_SetRenderDrawColor(renderer, 70, 90, 180, 255);
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, 120, 240, 255, 255);
            SDL_RenderDrawRect(renderer, &dst);
        }
    }

    if (room.hasShadyVendor) {
        const SDL_Rect dst = screenRect(room.shadyVendor);
        if (shadyVendorTexture) {
            SDL_RenderCopy(renderer, shadyVendorTexture, nullptr, &dst);
        } else {
            SDL_SetRenderDrawColor(renderer, 42, 38, 52, 255);
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, 215, 175, 80, 255);
            SDL_RenderDrawRect(renderer, &dst);
        }
    }

    if (room.hasConsciousnessConsole) {
        const SDL_Rect dst = screenRect(room.consciousnessConsole);
        if (consciousnessConsoleTexture) {
            SDL_RenderCopy(renderer, consciousnessConsoleTexture, nullptr, &dst);
        } else {
            SDL_SetRenderDrawColor(renderer, 70, 64, 120, 255);
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, 170, 145, 255, 255);
            SDL_RenderDrawRect(renderer, &dst);
        }
    }

    if (room.hasSkillTreeConsole) {
        const SDL_Rect dst = screenRect(room.skillTreeConsole);
        if (skillTreeConsoleTexture) {
            SDL_RenderCopy(renderer, skillTreeConsoleTexture, nullptr, &dst);
        } else {
            SDL_SetRenderDrawColor(renderer, 34, 82, 58, 255);
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, 90, 255, 150, 255);
            SDL_RenderDrawRect(renderer, &dst);
        }
    }

    if (room.hasArtifactStorage) {
        const SDL_Rect dst = screenRect(room.artifactStorage);
        if (artifactStorageTexture) {
            SDL_RenderCopy(renderer, artifactStorageTexture, nullptr, &dst);
        } else {
            SDL_SetRenderDrawColor(renderer, 90, 65, 38, 255);
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, 255, 190, 90, 255);
            SDL_RenderDrawRect(renderer, &dst);
        }
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

std::vector<PropGeom> RoomSystem::currentProps() const {
    std::vector<PropGeom> out;
    const Room& room = current();
    auto add = [&](const SDL_Rect& draw, PropKind kind) {
        out.push_back(PropGeom{draw, basePropCollider(draw, kind), kind});
    };
    if (isHub()) {
        for (const SDL_Rect& p : kHubPillars) add(p, PropKind::Pillar);
        for (const SDL_Rect& u : kHubUrns)    add(u, PropKind::Urn);
    }
    if (room.hasRuneVendor)           add(room.runeVendor, PropKind::RuneVendor);
    if (room.hasShadyVendor)          add(room.shadyVendor, PropKind::ShadyVendor);
    if (room.hasConsciousnessConsole) add(room.consciousnessConsole, PropKind::Consciousness);
    if (room.hasSkillTreeConsole)     add(room.skillTreeConsole, PropKind::SkillTree);
    if (room.hasArtifactStorage)      add(room.artifactStorage, PropKind::Artifact);
    for (const SDL_Rect& v : room.vendors) add(v, PropKind::Vendor);
    return out;
}

void RoomSystem::renderOccluders(SDL_Renderer* renderer, float cameraX, float cameraY,
                                 SDL_Texture* vendorTexture,
                                 SDL_Texture* runeVendorTexture,
                                 SDL_Texture* shadyVendorTexture,
                                 SDL_Texture* consciousnessConsoleTexture,
                                 SDL_Texture* skillTreeConsoleTexture,
                                 SDL_Texture* artifactStorageTexture,
                                 const HubTextures& hub, float playerFeetY) const {
    for (const PropGeom& g : currentProps()) {
        // Only props whose feet are below the player (the player is standing
        // behind them) need re-drawing over the player; the rest already drew
        // correctly behind the player in render().
        if (static_cast<float>(g.draw.y + g.draw.h) <= playerFeetY) {
            continue;
        }
        const SDL_Rect dst{
            static_cast<int>(g.draw.x - cameraX),
            static_cast<int>(g.draw.y - cameraY),
            g.draw.w, g.draw.h};
        SDL_Texture* tex = nullptr;
        SDL_Color fill{120, 96, 70, 255};
        SDL_Color border{0, 0, 0, 0};
        switch (g.kind) {
            case PropKind::Pillar:
                tex = hub.pillar; fill = {150, 98, 48, 255}; border = {224, 168, 88, 255}; break;
            case PropKind::Urn:
                tex = hub.urn; break;  // no fallback art (matches render())
            case PropKind::Vendor:
                tex = vendorTexture; fill = {180, 70, 95, 255}; border = {255, 220, 120, 255}; break;
            case PropKind::RuneVendor:
                tex = runeVendorTexture; fill = {70, 90, 180, 255}; border = {120, 240, 255, 255}; break;
            case PropKind::ShadyVendor:
                tex = shadyVendorTexture; fill = {42, 38, 52, 255}; border = {215, 175, 80, 255}; break;
            case PropKind::Consciousness:
                tex = consciousnessConsoleTexture; fill = {70, 64, 120, 255}; border = {170, 145, 255, 255}; break;
            case PropKind::SkillTree:
                tex = skillTreeConsoleTexture; fill = {34, 82, 58, 255}; border = {90, 255, 150, 255}; break;
            case PropKind::Artifact:
                tex = artifactStorageTexture; fill = {90, 65, 38, 255}; border = {255, 190, 90, 255}; break;
        }
        if (tex) {
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
        } else if (g.kind != PropKind::Urn) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, fill.r, fill.g, fill.b, 255);
            SDL_RenderFillRect(renderer, &dst);
            SDL_SetRenderDrawColor(renderer, border.r, border.g, border.b, 255);
            SDL_RenderDrawRect(renderer, &dst);
        }
    }
}

}  // namespace gaia
