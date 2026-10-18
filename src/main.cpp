// Snake Slither: a minimal grid-based Snake game.
// One codebase for desktop (SDL2) and browser (Emscripten + SDL2).

#include <SDL.h>

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <deque>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kCellSize = 20;
constexpr int kCols = kWindowWidth / kCellSize;   // 64
constexpr int kRows = kWindowHeight / kCellSize;  // 36
constexpr Uint32 kStepMs = 220;                   // snake moves one cell per step
constexpr float kRadius = 7.5f;                   // half the snake's thickness
constexpr size_t kMaxQueuedTurns = 2;

constexpr size_t kMaxApplesOnField = 10;
constexpr int kMaxApplesPerRun = 50;
constexpr Uint32 kSpawnMinMs = 3000;  // next apple appears 3 to 6 seconds after the last
constexpr Uint32 kSpawnMaxMs = 6000;

struct Point {
    int x;
    int y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

struct Vec {
    float x;
    float y;
};

// 3x5 bitmap digits, one row per 3 bits, so the score needs no font library.
constexpr unsigned short kDigits[10] = {
    0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF,
};

// 8x8 apple sprite: 0 transparent, 1 leaf, 2 red, 3 dark red, 4 shine.
constexpr char kApple[8][9] = {
    "00001000",
    "00011000",
    "02312200",
    "24332000",
    "24222000",
    "22422330",
    "02223300",
    "00202000",
};
enum AppleKind { kRed, kYellow, kGreen };

struct AppleType {
    int points;
    SDL_Color palette[5];  // indexed by the sprite digits above
};

constexpr AppleType kAppleTypes[3] = {
    {1, {{0, 0, 0, 0}, {0, 255, 0, 255}, {255, 0, 0, 255}, {195, 0, 0, 255}, {255, 255, 0, 255}}},
    {3, {{0, 0, 0, 0}, {0, 255, 0, 255}, {255, 220, 0, 255}, {195, 150, 0, 255}, {255, 255, 210, 255}}},
    {5, {{0, 0, 0, 0}, {120, 70, 20, 255}, {60, 230, 60, 255}, {0, 150, 0, 255}, {220, 255, 200, 255}}},
};

struct Apple {
    Point cell;
    AppleKind kind;
};

struct Game {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::deque<Point> snake;  // front is the head
    Point prevTail{0, 0};     // cell the tail is sliding out of this step
    Point dir{1, 0};
    std::deque<Point> turns;  // queued direction changes, applied one per step
    std::vector<Apple> apples;
    int applesSpawned = 0;  // this run, capped at kMaxApplesPerRun
    Uint32 nextSpawn = 0;
    bool spawnWaiting = false;
    int score = 0;
    bool gameOver = false;
    bool running = true;
    Uint32 lastStep = 0;
};

Game g;

bool OnSnake(const Point& p) {
    for (const Point& s : g.snake) {
        if (s == p) return true;
    }
    return false;
}

int AppleAt(const Point& p) {
    for (size_t i = 0; i < g.apples.size(); ++i) {
        if (g.apples[i].cell == p) return static_cast<int>(i);
    }
    return -1;
}

AppleKind RollAppleKind() {
    int roll = std::rand() % 100;
    if (roll < 1) return kGreen;   // 1 in 100
    if (roll < 3) return kYellow;  // 2 in 100
    return kRed;
}

void SpawnApple() {
    Point p;
    do {
        p = {std::rand() % kCols, std::rand() % kRows};
    } while (OnSnake(p) || AppleAt(p) >= 0);
    g.apples.push_back({p, RollAppleKind()});
    ++g.applesSpawned;
}

void ScheduleNextSpawn(Uint32 now) {
    g.nextSpawn = now + kSpawnMinMs + std::rand() % (kSpawnMaxMs - kSpawnMinMs + 1);
}

// Spawns at most one apple per call; the timer only runs while there is room.
void UpdateSpawns(Uint32 now) {
    if (g.applesSpawned >= kMaxApplesPerRun) return;
    if (g.apples.size() >= kMaxApplesOnField) {
        g.spawnWaiting = true;  // field full: start a fresh 3-6s wait once there's room
        return;
    }
    if (g.spawnWaiting) {
        g.spawnWaiting = false;
        ScheduleNextSpawn(now);
    }
    if (static_cast<Sint32>(now - g.nextSpawn) >= 0) {
        SpawnApple();
        ScheduleNextSpawn(now);
    }
}

void Reset() {
    g.snake.clear();
    for (int i = 0; i < 4; ++i) g.snake.push_back({kCols / 2 - i, kRows / 2});
    g.prevTail = {g.snake.back().x - 1, g.snake.back().y};
    g.dir = {1, 0};
    g.turns.clear();
    g.score = 0;
    g.gameOver = false;
    g.lastStep = SDL_GetTicks();
    g.apples.clear();
    g.applesSpawned = 0;
    g.spawnWaiting = false;
    // Start with a full field; these count toward the run's 50 apples.
    for (size_t i = 0; i < kMaxApplesOnField; ++i) SpawnApple();
    ScheduleNextSpawn(g.lastStep);
}

void QueueTurn(int dx, int dy) {
    if (g.turns.size() >= kMaxQueuedTurns) return;
    Point last = g.turns.empty() ? g.dir : g.turns.back();
    // Ignore repeats and reversing straight into the snake's own neck.
    if ((dx == last.x && dy == last.y) || (dx == -last.x && dy == -last.y)) return;
    g.turns.push_back({dx, dy});
}

void HandleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            g.running = false;
        } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            switch (e.key.keysym.sym) {
                case SDLK_UP: case SDLK_w: QueueTurn(0, -1); break;
                case SDLK_DOWN: case SDLK_s: QueueTurn(0, 1); break;
                case SDLK_LEFT: case SDLK_a: QueueTurn(-1, 0); break;
                case SDLK_RIGHT: case SDLK_d: QueueTurn(1, 0); break;
                case SDLK_SPACE: case SDLK_RETURN:
                    if (g.gameOver) Reset();
                    break;
                case SDLK_ESCAPE:
#ifndef __EMSCRIPTEN__
                    g.running = false;
#endif
                    break;
                default: break;
            }
        }
    }
}

void Step() {
    if (!g.turns.empty()) {
        g.dir = g.turns.front();
        g.turns.pop_front();
    }
    Point head{g.snake.front().x + g.dir.x, g.snake.front().y + g.dir.y};

    bool hitWall = head.x < 0 || head.x >= kCols || head.y < 0 || head.y >= kRows;
    int eaten = AppleAt(head);
    bool eating = eaten >= 0;
    // The tail moves away this step unless we eat, so it is safe to enter.
    Point tail = g.snake.back();
    if (!eating) g.snake.pop_back();
    if (hitWall || OnSnake(head)) {
        if (!eating) g.snake.push_back(tail);
        g.gameOver = true;
        return;
    }

    g.snake.push_front(head);
    g.prevTail = tail;  // when eating the tail stays put, so this is the tail itself
    if (eating) {
        g.score += kAppleTypes[g.apples[eaten].kind].points;
        g.apples.erase(g.apples.begin() + eaten);
        // The run ends once all of its apples have been spawned and eaten.
        if (g.applesSpawned >= kMaxApplesPerRun && g.apples.empty()) g.gameOver = true;
    }
}

// ---- Drawing helpers -------------------------------------------------------

void SetColor(Uint8 r, Uint8 gr, Uint8 b) { SDL_SetRenderDrawColor(g.renderer, r, gr, b, 255); }

void FillRect(float x, float y, float w, float h) {
    SDL_FRect r{x, y, w, h};
    SDL_RenderFillRectF(g.renderer, &r);
}

void FillCircle(Vec c, float radius) {
    for (float dy = -radius + 0.5f; dy < radius; dy += 1.0f) {
        float half = std::sqrt(radius * radius - dy * dy);
        FillRect(c.x - half, c.y + dy - 0.5f, 2 * half, 1.0f);
    }
}

Vec CellCenter(const Point& p) {
    return {p.x * kCellSize + kCellSize / 2.0f, p.y * kCellSize + kCellSize / 2.0f};
}

Vec Lerp(Vec a, Vec b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}; }

// Draws a rounded tube through axis-aligned points. Every snake segment is
// horizontal or vertical, so rectangles plus round joints are enough.
void DrawTube(const std::vector<Vec>& pts, float radius, Vec offset) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        Vec a{pts[i].x + offset.x, pts[i].y + offset.y};
        Vec b{pts[i + 1].x + offset.x, pts[i + 1].y + offset.y};
        float x0 = std::fmin(a.x, b.x), x1 = std::fmax(a.x, b.x);
        float y0 = std::fmin(a.y, b.y), y1 = std::fmax(a.y, b.y);
        if (y0 == y1)
            FillRect(x0, y0 - radius, x1 - x0, 2 * radius);
        else
            FillRect(x0 - radius, y0, 2 * radius, y1 - y0);
    }
    for (const Vec& p : pts) FillCircle({p.x + offset.x, p.y + offset.y}, radius);
}

// Point and unit direction at `dist` pixels along the path from its start.
void PointAlong(const std::vector<Vec>& pts, float dist, Vec* pos, Vec* dir) {
    for (size_t i = 0; i + 1 < pts.size(); ++i) {
        Vec a = pts[i], b = pts[i + 1];
        float len = std::fabs(b.x - a.x) + std::fabs(b.y - a.y);
        if (len <= 0.0f) continue;
        *dir = {(b.x - a.x) / len, (b.y - a.y) / len};
        if (dist <= len || i + 2 == pts.size()) {
            *pos = {a.x + dir->x * dist, a.y + dir->y * dist};
            return;
        }
        dist -= len;
    }
    *pos = pts.front();
    *dir = {1, 0};
}

void DrawSnake(float t) {
    // The head glides from the previous cell into the new one, and the tail
    // glides out of the cell it is leaving, so turns only happen at cell centers.
    std::vector<Vec> path;
    path.push_back(Lerp(CellCenter(g.snake[1]), CellCenter(g.snake[0]), t));
    for (size_t i = 1; i < g.snake.size(); ++i) path.push_back(CellCenter(g.snake[i]));
    path.push_back(Lerp(CellCenter(g.prevTail), CellCenter(g.snake.back()), t));

    // Bright tube underneath, darker tube nudged down-right on top, which
    // leaves a highlight along the top and left edges like the reference art.
    SetColor(0, 255, 0);
    DrawTube(path, kRadius, {0, 0});
    SetColor(0, 195, 0);
    DrawTube(path, kRadius - 1.5f, {1, 1});

    // Tail end: yellow rattle bands across the last stretch of the body.
    std::vector<Vec> fromTail(path.rbegin(), path.rend());
    for (float d : {1.0f, 5.0f}) {
        Vec pos, dir;
        PointAlong(fromTail, d, &pos, &dir);
        Vec perp{-dir.y, dir.x};
        float halfW = std::fabs(perp.x) * (kRadius - 0.5f) + std::fabs(dir.x) * 1;
        float halfH = std::fabs(perp.y) * (kRadius - 0.5f) + std::fabs(dir.y) * 1;
        SetColor(195, 195, 0);
        FillRect(pos.x - halfW, pos.y - halfH, 2 * halfW, 2 * halfH);
        SetColor(255, 255, 0);
        FillRect(pos.x - halfW + (perp.x != 0 ? 1 : 0), pos.y - halfH + (perp.y != 0 ? 1 : 0),
                 perp.x != 0 ? 2 : 2 * halfW, perp.y != 0 ? 2 : 2 * halfH);
    }

    // Head: two red eyes near the front, looking where the snake is going.
    Vec head = path.front();
    Vec dir{static_cast<float>(g.snake[0].x - g.snake[1].x),
            static_cast<float>(g.snake[0].y - g.snake[1].y)};
    Vec perp{-dir.y, dir.x};
    for (float side : {-1.0f, 1.0f}) {
        Vec eye{head.x + dir.x * 2.5f + perp.x * 3.5f * side,
                head.y + dir.y * 2.5f + perp.y * 3.5f * side};
        SetColor(255, 0, 0);
        FillCircle(eye, 2.5f);
        SetColor(20, 0, 0);
        FillCircle({eye.x + dir.x * 0.75f, eye.y + dir.y * 0.75f}, 1.0f);
    }
}

void DrawApple(const Apple& apple) {
    constexpr int kPixel = 2;  // 8x8 sprite drawn at 16x16 inside a 20px cell
    const Point& cell = apple.cell;
    int ox = cell.x * kCellSize + (kCellSize - 8 * kPixel) / 2;
    int oy = cell.y * kCellSize + (kCellSize - 8 * kPixel) / 2;
    for (int y = 0; y < 8; ++y) {
        for (int x = 0; x < 8; ++x) {
            int idx = kApple[y][x] - '0';
            if (idx == 0) continue;
            const SDL_Color& c = kAppleTypes[apple.kind].palette[idx];
            SetColor(c.r, c.g, c.b);
            FillRect(ox + x * kPixel, oy + y * kPixel, kPixel, kPixel);
        }
    }
}

void DrawNumber(int value, int x, int y, int pixel) {
    std::string text = std::to_string(value);
    for (char c : text) {
        unsigned short bits = kDigits[c - '0'];
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (bits & (1 << (14 - (row * 3 + col)))) {
                    FillRect(x + col * pixel, y + row * pixel, pixel, pixel);
                }
            }
        }
        x += 4 * pixel;
    }
}

void Render() {
    SDL_SetRenderDrawColor(g.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g.renderer);

    float t = 1.0f;
    if (!g.gameOver) {
        t = static_cast<float>(SDL_GetTicks() - g.lastStep) / kStepMs;
        t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    }

    for (const Apple& a : g.apples) DrawApple(a);
    DrawSnake(t);

    SetColor(235, 235, 235);
    DrawNumber(g.score, 16, 16, 6);

    if (g.gameOver) {
        SDL_SetRenderDrawBlendMode(g.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g.renderer, 0, 0, 0, 160);
        FillRect(0, 0, kWindowWidth, kWindowHeight);
        // Big final score in the middle; Space or Enter restarts.
        int pixel = 24;
        int width = static_cast<int>(std::to_string(g.score).size()) * 4 * pixel - pixel;
        SetColor(235, 235, 235);
        DrawNumber(g.score, (kWindowWidth - width) / 2, (kWindowHeight - 5 * pixel) / 2, pixel);
    }

    SDL_RenderPresent(g.renderer);
}

void Frame() {
    HandleEvents();
    Uint32 now = SDL_GetTicks();
    if (!g.gameOver) UpdateSpawns(now);
    if (!g.gameOver && now - g.lastStep >= kStepMs) {
        g.lastStep += kStepMs;
        if (now - g.lastStep > kStepMs) g.lastStep = now;  // don't spiral after a stall
        Step();
    }
    Render();
#ifdef __EMSCRIPTEN__
    if (!g.running) emscripten_cancel_main_loop();
#endif
}

}  // namespace

int main(int, char**) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    g.window = SDL_CreateWindow("Snake Slither", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                kWindowWidth, kWindowHeight, 0);
    if (!g.window) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        return 1;
    }
    g.renderer = SDL_CreateRenderer(g.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!g.renderer) g.renderer = SDL_CreateRenderer(g.window, -1, 0);
    if (!g.renderer) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        return 1;
    }

    std::srand(static_cast<unsigned>(std::time(nullptr)));
    Reset();

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(Frame, 0, 1);
#else
    while (g.running) {
        Frame();
        SDL_Delay(1);
    }
#endif

    SDL_DestroyRenderer(g.renderer);
    SDL_DestroyWindow(g.window);
    SDL_Quit();
    return 0;
}
