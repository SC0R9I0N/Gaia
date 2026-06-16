#include "Room.hpp"

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

    // Room 1: the first run map. It is intentionally much larger than the
    // logical screen so gameplay uses camera scrolling instead of showing it all.
    Room run;
    run.width  = 2600;
    run.height = 1800;
    run.floor  = SDL_Color{38, 44, 40, 255};
    m_rooms.push_back(run);
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
    m_current = 1;
    const SDL_Rect r = interiorRect();
    px = static_cast<float>(r.x + r.w / 2) - size * 0.5f;
    py = static_cast<float>(r.y + r.h / 2) - size * 0.5f;
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

bool RoomSystem::resolvePlayer(float& px, float& py, float size) {
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
    for (const Door& d : current().doors) {
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
            enterRoom(d.targetRoom, opposite(d.side), px, py, size);
            return true;
        }
    }

    // Otherwise the walls are solid: clamp the player inside the interior.
    if (px < left)        px = left;
    else if (px > right)  px = right;
    if (py < top)         py = top;
    else if (py > bottom) py = bottom;
    return false;
}

void RoomSystem::render(SDL_Renderer* renderer, float cameraX, float cameraY,
                        SDL_Texture* vendorTexture) const {
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
    SDL_SetRenderDrawColor(renderer, room.floor.r, room.floor.g, room.floor.b, 255);
    const SDL_Rect floor = screenRect(interior);
    SDL_RenderFillRect(renderer, &floor);

    // A subtle floor grid makes the hub/readable placeholder maps feel textured.
    SDL_SetRenderDrawColor(renderer, room.floor.r + 12, room.floor.g + 12,
                           room.floor.b + 12, 255);
    constexpr int kTile = 96;
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
}

}  // namespace gaia
