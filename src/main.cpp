// Snake Slither: a minimal grid-based Snake game.
// One codebase for desktop (SDL2) and browser (Emscripten + SDL2).

#include <SDL.h>

#include <cstdlib>
#include <ctime>
#include <deque>
#include <string>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr int kCellSize = 40;
constexpr int kCols = kWindowWidth / kCellSize;   // 32
constexpr int kRows = kWindowHeight / kCellSize;  // 18
constexpr Uint32 kStepMs = 110;                   // snake moves one cell per step

struct Point {
    int x;
    int y;
    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
};

// 3x5 bitmap digits, one row per 3 bits, so the score needs no font library.
constexpr unsigned short kDigits[10] = {
    0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF,
};

struct Game {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::deque<Point> snake;  // front is the head
    Point dir{1, 0};
    Point nextDir{1, 0};
    Point food{0, 0};
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

void PlaceFood() {
    do {
        g.food = {std::rand() % kCols, std::rand() % kRows};
    } while (OnSnake(g.food));
}

void Reset() {
    g.snake.clear();
    for (int i = 0; i < 4; ++i) g.snake.push_back({kCols / 2 - i, kRows / 2});
    g.dir = g.nextDir = {1, 0};
    g.score = 0;
    g.gameOver = false;
    g.lastStep = SDL_GetTicks();
    PlaceFood();
}

void SetDirection(int dx, int dy) {
    // Ignore reversing straight into the snake's own neck.
    if (dx == -g.dir.x && dy == -g.dir.y) return;
    g.nextDir = {dx, dy};
}

void HandleEvents() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            g.running = false;
        } else if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_UP: case SDLK_w: SetDirection(0, -1); break;
                case SDLK_DOWN: case SDLK_s: SetDirection(0, 1); break;
                case SDLK_LEFT: case SDLK_a: SetDirection(-1, 0); break;
                case SDLK_RIGHT: case SDLK_d: SetDirection(1, 0); break;
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
    g.dir = g.nextDir;
    Point head{g.snake.front().x + g.dir.x, g.snake.front().y + g.dir.y};

    bool hitWall = head.x < 0 || head.x >= kCols || head.y < 0 || head.y >= kRows;
    bool eating = head == g.food;
    // The tail moves away this step unless we eat, so it is safe to enter.
    Point tail = g.snake.back();
    if (!eating) g.snake.pop_back();
    if (hitWall || OnSnake(head)) {
        if (!eating) g.snake.push_back(tail);
        g.gameOver = true;
        return;
    }

    g.snake.push_front(head);
    if (eating) {
        ++g.score;
        if (static_cast<int>(g.snake.size()) == kCols * kRows) {
            g.gameOver = true;  // board full: you win
            return;
        }
        PlaceFood();
    }
}

void FillCell(const Point& p, int inset) {
    SDL_Rect r{p.x * kCellSize + inset, p.y * kCellSize + inset, kCellSize - 2 * inset,
               kCellSize - 2 * inset};
    SDL_RenderFillRect(g.renderer, &r);
}

void DrawNumber(int value, int x, int y, int pixel) {
    std::string text = std::to_string(value);
    for (char c : text) {
        unsigned short bits = kDigits[c - '0'];
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (bits & (1 << (14 - (row * 3 + col)))) {
                    SDL_Rect r{x + col * pixel, y + row * pixel, pixel, pixel};
                    SDL_RenderFillRect(g.renderer, &r);
                }
            }
        }
        x += 4 * pixel;
    }
}

void Render() {
    SDL_SetRenderDrawColor(g.renderer, 18, 22, 28, 255);
    SDL_RenderClear(g.renderer);

    // Subtle checkerboard so the grid is readable.
    SDL_SetRenderDrawColor(g.renderer, 24, 29, 37, 255);
    for (int y = 0; y < kRows; ++y)
        for (int x = 0; x < kCols; ++x)
            if ((x + y) % 2 == 0) FillCell({x, y}, 0);

    SDL_SetRenderDrawColor(g.renderer, 230, 70, 70, 255);
    FillCell(g.food, 8);

    for (size_t i = 0; i < g.snake.size(); ++i) {
        if (i == 0)
            SDL_SetRenderDrawColor(g.renderer, 140, 230, 120, 255);
        else
            SDL_SetRenderDrawColor(g.renderer, 80, 190, 90, 255);
        FillCell(g.snake[i], 2);
    }

    SDL_SetRenderDrawColor(g.renderer, 235, 235, 235, 255);
    DrawNumber(g.score, 16, 16, 6);

    if (g.gameOver) {
        SDL_SetRenderDrawBlendMode(g.renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(g.renderer, 0, 0, 0, 160);
        SDL_Rect full{0, 0, kWindowWidth, kWindowHeight};
        SDL_RenderFillRect(g.renderer, &full);
        // Big final score in the middle; Space or Enter restarts.
        int pixel = 24;
        int width = static_cast<int>(std::to_string(g.score).size()) * 4 * pixel - pixel;
        SDL_SetRenderDrawColor(g.renderer, 235, 235, 235, 255);
        DrawNumber(g.score, (kWindowWidth - width) / 2, (kWindowHeight - 5 * pixel) / 2, pixel);
    }

    SDL_RenderPresent(g.renderer);
}

void Frame() {
    HandleEvents();
    Uint32 now = SDL_GetTicks();
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
