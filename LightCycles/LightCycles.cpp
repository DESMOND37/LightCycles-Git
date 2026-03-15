/*
 * ████████╗██████╗  ██████╗ ███╗   ██╗
 *    ██╔══╝██╔══██╗██╔═══██╗████╗  ██║
 *    ██║   ██████╔╝██║   ██║██╔██╗ ██║
 *    ██║   ██╔══██╗██║   ██║██║╚██╗██║
 *    ██║   ██║  ██║╚██████╔╝██║ ╚████║
 *    ╚═╝   ╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 *  LIGHT CYCLES — Classic Tron Arena
 *  Cross-platform: Windows (PDCurses) / Linux (ncursesw - for wide char)/ macOS (ncurses - already includes wide char support on Apple platforms)
 *
 *  Player 1 (CYAN) : W A S D
 *  Player 2 (GOLD) : Arrow keys  (PvP)  /  CPU  (PvC)
 *  Pause: P   Quit: Q
 *
 *  © Not Yet Copyright.
 *  DSMND Software. All Rights Not Reserved.
 *  All Rights Not Reserved Yet.
 */

 // ─── Platform headers ─────────────────────────────────────────────────────────

#ifdef _WIN32
#  define PDC_WIDE        // PDCurses — wide char support is enabled by default in vcpkg build
#  include <curses.h>
#else
// ncursesw — wide character ncurses (apt install libncursesw5-dev)
#  define _XOPEN_SOURCE_EXTENDED 1
#  ifdef __APPLE__
#    include <ncurses.h>    // macOS ncurses already includes wide char
#  else
#    include <ncursesw/ncurses.h>
#  endif
#endif

#include <array>
#include <chrono>
#include <cstring>
#include <queue>
#include <string>
#include <thread>

// ─── Constants ────────────────────────────────────────────────────────────────

static const int ARENA_W = 78;
static const int ARENA_H = 22;
static const int PANEL_W = 20;
static const int WIN_SCORE = 5;
static const int FPS = 13;

static const int CELL_EMPTY = 0;
static const int CELL_P1 = 1;
static const int CELL_P2 = 2;

enum CP {
    CP_BORDER = 1,
    CP_P1,
    CP_P2,
    CP_TITLE,
    CP_WIN,
    CP_INFO,
    CP_DEAD,
    CP_BG
};

enum Dir { UP, DOWN, LEFT, RIGHT };
enum GameMode { MODE_PVP, MODE_PVC };

// ─── Structs ──────────────────────────────────────────────────────────────────

struct Player {
    int  x, y;
    Dir  dir;
    bool alive;
    int  score;

    bool canTurn(Dir d) const {
        if (dir == UP && d == DOWN)  return false;
        if (dir == DOWN && d == UP)    return false;
        if (dir == LEFT && d == RIGHT) return false;
        if (dir == RIGHT && d == LEFT)  return false;
        return true;
    }
};

// ─── Grid ─────────────────────────────────────────────────────────────────────

static int grid[ARENA_H][ARENA_W];

static void clearGrid() {
    for (int r = 0; r < ARENA_H; ++r)
        for (int c = 0; c < ARENA_W; ++c)
            grid[r][c] = CELL_EMPTY;
}

// ─── Screen coordinate helpers ────────────────────────────────────────────────

static int sc(int x) { return x + 1; }
static int sr(int y) { return y + 1; }

// ─── Drawing ──────────────────────────────────────────────────────────────────

static void drawBorder(WINDOW* w) {
    wattron(w, COLOR_PAIR(CP_BORDER) | A_BOLD);
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER) | A_BOLD);
}

static void drawGrid(WINDOW* w) {
    for (int r = 0; r < ARENA_H; ++r) {
        for (int c = 0; c < ARENA_W; ++c) {
            int cell = grid[r][c];
            if (cell == CELL_P1) {
                wattron(w, COLOR_PAIR(CP_P1) | A_BOLD);
                mvwaddch(w, sr(r), sc(c), ACS_CKBOARD);
                wattroff(w, COLOR_PAIR(CP_P1) | A_BOLD);
            }
            else if (cell == CELL_P2) {
                wattron(w, COLOR_PAIR(CP_P2) | A_BOLD);
                mvwaddch(w, sr(r), sc(c), ACS_CKBOARD);
                wattroff(w, COLOR_PAIR(CP_P2) | A_BOLD);
            }
            else {
                wattron(w, COLOR_PAIR(CP_BG));
                mvwaddch(w, sr(r), sc(c), ' ');
                wattroff(w, COLOR_PAIR(CP_BG));
            }
        }
    }
}

static void drawHead(WINDOW* w, const Player& p, CP cp) {
    if (!p.alive) return;
    wattron(w, COLOR_PAIR(cp) | A_REVERSE | A_BOLD);
    mvwaddch(w, sr(p.y), sc(p.x), '@');
    wattroff(w, COLOR_PAIR(cp) | A_REVERSE | A_BOLD);
}

static void centerMsg(WINDOW* w, int row, const char* msg, CP cp, bool bold) {
    int attr = COLOR_PAIR(cp) | (bold ? A_BOLD : 0);
    wattron(w, attr);
    int len = (int)strlen(msg);
    int x = ((ARENA_W + 2) - len) / 2;
    if (x < 1) x = 1;
    mvwprintw(w, row, x, "%s", msg);
    wattroff(w, attr);
}

// ─── Info panel ───────────────────────────────────────────────────────────────

static void drawPanel(WINDOW* p, const Player& p1, const Player& p2,
    int round, bool paused, GameMode mode)
{
    wclear(p);

    wattron(p, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(p, 0, 2, " T R O N ");
    mvwprintw(p, 1, 1, "LIGHT CYCLES");
    wattroff(p, COLOR_PAIR(CP_TITLE) | A_BOLD);

    wattron(p, COLOR_PAIR(CP_INFO));
    mvwprintw(p, 3, 1, "ROUND  %d", round);
    mvwprintw(p, 3, 11, mode == MODE_PVC ? "[PvC]" : "[PvP]");
    wattroff(p, COLOR_PAIR(CP_INFO));

    wattron(p, COLOR_PAIR(CP_P1) | A_BOLD);
    mvwprintw(p, 5, 1, "P1  [CYAN]");
    wattroff(p, COLOR_PAIR(CP_P1) | A_BOLD);
    wattron(p, COLOR_PAIR(CP_INFO));
    mvwprintw(p, 6, 1, "Score : %d / %d", p1.score, WIN_SCORE);
    mvwprintw(p, 7, 1, "Status: %s", p1.alive ? "ALIVE" : "DEAD ");
    wattroff(p, COLOR_PAIR(CP_INFO));

    wattron(p, COLOR_PAIR(CP_P2) | A_BOLD);
    if (mode == MODE_PVC)
        mvwprintw(p, 9, 1, "CPU [GOLD]");
    else
        mvwprintw(p, 9, 1, "P2  [GOLD]");
    wattroff(p, COLOR_PAIR(CP_P2) | A_BOLD);
    wattron(p, COLOR_PAIR(CP_INFO));
    mvwprintw(p, 10, 1, "Score : %d / %d", p2.score, WIN_SCORE);
    mvwprintw(p, 11, 1, "Status: %s", p2.alive ? "ALIVE" : "DEAD ");
    wattroff(p, COLOR_PAIR(CP_INFO));

    wattron(p, COLOR_PAIR(CP_INFO) | A_DIM);
    mvwprintw(p, 14, 1, "P1 : W A S D");
    if (mode == MODE_PVP)
        mvwprintw(p, 15, 1, "P2 : Arrows");
    else
        mvwprintw(p, 15, 1, "P2 : CPU AI");
    mvwprintw(p, 17, 1, "P  - pause");
    mvwprintw(p, 18, 1, "Q  - quit");
    wattroff(p, COLOR_PAIR(CP_INFO) | A_DIM);

    if (paused) {
        wattron(p, COLOR_PAIR(CP_WIN) | A_BOLD);
        mvwprintw(p, 20, 2, "* PAUSED *");
        wattroff(p, COLOR_PAIR(CP_WIN) | A_BOLD);
    }

    wrefresh(p);
}

// ─── Overlay helpers ──────────────────────────────────────────────────────────

static void waitKey(WINDOW* w, const char* prompt) {
    centerMsg(w, ARENA_H / 2 + 3, prompt, CP_INFO, false);
    wrefresh(w);
    nodelay(w, FALSE);
    wint_t ch;
    while (wget_wch(w, &ch) != ERR
        && ch != L' ' && ch != L'\n'
        && ch != L'q' && ch != L'Q') {
    }
    nodelay(w, TRUE);
}

static void showCountdown(WINDOW* w) {
    char buf[8];
    for (int i = 3; i >= 1; --i) {
        drawBorder(w);
        snprintf(buf, sizeof(buf), " %d ", i);
        centerMsg(w, ARENA_H / 2, buf, CP_WIN, true);
        wrefresh(w);
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
    }
    drawBorder(w);
    centerMsg(w, ARENA_H / 2, "  G O !  ", CP_WIN, true);
    wrefresh(w);
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
}

static void showRoundResult(WINDOW* w, int winner, GameMode mode) {
    drawGrid(w);
    drawBorder(w);
    if (winner == 0) {
        centerMsg(w, ARENA_H / 2 - 1, "*** D R A W ***", CP_INFO, true);
        centerMsg(w, ARENA_H / 2 + 1, "Both cycles destroyed.", CP_INFO, false);
    }
    else if (winner == 1) {
        centerMsg(w, ARENA_H / 2 - 1, "PLAYER 1 WINS THE ROUND!", CP_P1, true);
        centerMsg(w, ARENA_H / 2 + 1, "Cyan cycle survives.", CP_P1, false);
    }
    else {
        if (mode == MODE_PVC) {
            centerMsg(w, ARENA_H / 2 - 1, "CPU WINS THE ROUND!", CP_P2, true);
            centerMsg(w, ARENA_H / 2 + 1, "The machine prevails.", CP_P2, false);
        }
        else {
            centerMsg(w, ARENA_H / 2 - 1, "PLAYER 2 WINS THE ROUND!", CP_P2, true);
            centerMsg(w, ARENA_H / 2 + 1, "Gold cycle survives.", CP_P2, false);
        }
    }
    waitKey(w, "--- Press SPACE or ENTER to continue ---");
}

static bool showMatchResult(WINDOW* w, int winner, GameMode mode) {
    wclear(w);
    drawBorder(w);
    CP cp = (winner == 1) ? CP_P1 : CP_P2;
    centerMsg(w, ARENA_H / 2 - 2, "==========================", cp, true);
    if (winner == 1)
        centerMsg(w, ARENA_H / 2, " PLAYER 1 WINS THE MATCH! ", cp, true);
    else if (mode == MODE_PVC)
        centerMsg(w, ARENA_H / 2, "   CPU WINS THE MATCH!    ", cp, true);
    else
        centerMsg(w, ARENA_H / 2, " PLAYER 2 WINS THE MATCH! ", cp, true);
    centerMsg(w, ARENA_H / 2 + 2, "==========================", cp, true);
    centerMsg(w, ARENA_H / 2 + 4, "R - rematch      Q - quit", CP_INFO, false);
    wrefresh(w);

    nodelay(w, FALSE);
    wint_t ch;
    do { wget_wch(w, &ch); } while (ch != L'r' && ch != L'R'
        && ch != L'q' && ch != L'Q');
    nodelay(w, TRUE);
    return (ch == L'r' || ch == L'R');
}

static void showSplash(WINDOW* w) {
    wclear(w);
    drawBorder(w);
    centerMsg(w, 3, " T R O N  :  L I G H T  C Y C L E S ", CP_TITLE, true);
    centerMsg(w, 4, " ver. 0.0.0.3 - AI Update ", CP_INFO, true);
    centerMsg(w, 5, "Two programs enter. One program leaves.", CP_INFO, false);
    centerMsg(w, 7, "Player 1  (CYAN)  :  W  A  S  D", CP_P1, true);
    centerMsg(w, 8, "Player 2  (GOLD)  :  Arrow Keys  (PvP)", CP_P2, true);
    centerMsg(w, 10, "Ride your light cycle across the Grid.", CP_INFO, false);
    centerMsg(w, 11, "Force your opponent into a wall or trail.", CP_INFO, false);
    centerMsg(w, 13, "First to 5 round wins takes the match.", CP_INFO, false);
    centerMsg(w, 15, "P - pause                Q - quit", CP_INFO, false);
    centerMsg(w, 17, ">>> Press SPACE to enter the Grid <<<", CP_WIN, true);
    
    centerMsg(w, ARENA_H - 2, "© Not Yet Copyright. DSMND Software.", CP_INFO, false);
    centerMsg(w, ARENA_H - 1, "All Rights Not Reserved Yet.", CP_INFO, false);
    wrefresh(w);

    nodelay(w, FALSE);
    wint_t ch = 0;
    while (wget_wch(w, &ch) != ERR && ch != L' ' && ch != L'\n') {}
    nodelay(w, TRUE);
}

// ─── Mode selection menu ──────────────────────────────────────────────────────

static GameMode showModeMenu(WINDOW* w) {
    int sel = 0; // 0 = PvP, 1 = PvC

    nodelay(w, FALSE);
    while (true) {
        wclear(w);
        drawBorder(w);

        centerMsg(w, 3, " T R O N  :  L I G H T  C Y C L E S ", CP_TITLE, true);
        centerMsg(w, 5, "Select game mode:", CP_INFO, false);

        // Option 0: PvP
        if (sel == 0) {
            wattron(w, COLOR_PAIR(CP_WIN) | A_BOLD | A_REVERSE);
            centerMsg(w, 8, "  [ Player vs Player ]  ", CP_WIN, true);
            wattroff(w, COLOR_PAIR(CP_WIN) | A_BOLD | A_REVERSE);
        }
        else {
            centerMsg(w, 8, "    Player vs Player    ", CP_INFO, false);
        }

        // Option 1: PvC
        if (sel == 1) {
            wattron(w, COLOR_PAIR(CP_WIN) | A_BOLD | A_REVERSE);
            centerMsg(w, 10, "  [ Player vs CPU   ]  ", CP_WIN, true);
            wattroff(w, COLOR_PAIR(CP_WIN) | A_BOLD | A_REVERSE);
        }
        else {
            centerMsg(w, 10, "    Player vs CPU       ", CP_INFO, false);
        }

        centerMsg(w, 13, "UP / DOWN  - select", CP_INFO, false);
        centerMsg(w, 14, "ENTER / SPACE - confirm", CP_INFO, false);

        centerMsg(w, ARENA_H - 2, "(c) Not Yet Copyright. DSMND Software.", CP_INFO, false);
        centerMsg(w, ARENA_H - 1, "All Rights Not Reserved Yet.", CP_INFO, false);

        wrefresh(w);

        wint_t ch = 0;
        wget_wch(w, &ch);
        switch (ch) {
        case KEY_UP:
        case L'w': case L'W':
        case L'\u0446': case L'\u0426': // ц / Ц
            sel = 0; break;
        case KEY_DOWN:
        case L's': case L'S':
        case L'\u044b': case L'\u042b': // ы / Ы
            sel = 1; break;
        case L'\n': case L'\r': case L' ':
            nodelay(w, TRUE);
            return (sel == 0) ? MODE_PVP : MODE_PVC;
        case L'q': case L'Q':
            nodelay(w, TRUE);
            return MODE_PVP;
        }
    }
}

// ─── AI (flood fill) ──────────────────────────────────────────────────────────
//
// For each candidate direction, simulate a BFS from the resulting cell and
// count how many empty cells are reachable (= "space score").
// Pick the direction with the maximum space score.
// Tie-break: prefer continuing straight over turning.
// Extra: if the opponent is reachable and we can cut them off, prefer that.

static int nextX(int x, Dir d) {
    if (d == LEFT)  return x - 1;
    if (d == RIGHT) return x + 1;
    return x;
}
static int nextY(int y, Dir d) {
    if (d == UP)   return y - 1;
    if (d == DOWN) return y + 1;
    return y;
}

static bool oob(int x, int y) {
    return x < 0 || x >= ARENA_W || y < 0 || y >= ARENA_H;
}

// BFS flood fill: count reachable empty cells from (sx, sy).
// Treats the given extra blocked cell (bx, by) as occupied
// (used to pretend the player's current head is already a trail).
static int floodFill(int sx, int sy, int bx, int by) {
    if (oob(sx, sy)) return 0;
    if (grid[sy][sx] != CELL_EMPTY) return 0;

    // Visited array on stack — small enough for ARENA_H×ARENA_W = 78×22
    static bool visited[ARENA_H][ARENA_W];
    for (int r = 0; r < ARENA_H; ++r)
        for (int c = 0; c < ARENA_W; ++c)
            visited[r][c] = false;

    // Mark the extra blocked cell
    bool hadExtra = false;
    int savedExtra = 0;
    if (!oob(bx, by)) {
        savedExtra = grid[by][bx];
        grid[by][bx] = CELL_P2; // treat as occupied
        hadExtra = true;
    }

    std::queue<std::pair<int, int>> q;
    q.push({ sx, sy });
    visited[sy][sx] = true;
    int count = 0;

    const int dx[] = { 0, 0, -1, 1 };
    const int dy[] = { -1, 1, 0, 0 };

    while (!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        ++count;
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i];
            int ny = cy + dy[i];
            if (!oob(nx, ny) && !visited[ny][nx] && grid[ny][nx] == CELL_EMPTY) {
                visited[ny][nx] = true;
                q.push({ nx, ny });
            }
        }
    }

    // Restore extra blocked cell
    if (hadExtra) grid[by][bx] = savedExtra;

    return count;
}

// Choose best direction for the AI (controls Player p2, chases Player p1)
static Dir aiChooseDir(const Player& ai, const Player& player) {
    const Dir allDirs[4] = { UP, DOWN, LEFT, RIGHT };

    Dir bestDir = ai.dir;
    int bestScore = -1;

    for (Dir d : allDirs) {
        if (!ai.canTurn(d)) continue;

        int nx = nextX(ai.x, d);
        int ny = nextY(ai.y, d);

        // Skip immediately fatal moves
        if (oob(nx, ny) || grid[ny][nx] != CELL_EMPTY) continue;

        // Flood fill score — how much space we'd have
        int space = floodFill(nx, ny, ai.x, ai.y);

        // Small bonus for going straight (avoid unnecessary zigzag)
        if (d == ai.dir) space += 2;

        // Aggression bonus: prefer directions that get closer to the player
        // (only when we have plenty of space, so we don't suicide)
        if (space > 10 && player.alive) {
            int distNow = abs(ai.x - player.x) + abs(ai.y - player.y);
            int distNext = abs(nx - player.x) + abs(ny - player.y);
            if (distNext < distNow) space += 3;
        }

        if (space > bestScore) {
            bestScore = space;
            bestDir = d;
        }
    }

    return bestDir;
}

// ─── Game logic ───────────────────────────────────────────────────────────────

static void stepDir(int& x, int& y, Dir d) {
    switch (d) {
    case UP:    y--; break;
    case DOWN:  y++; break;
    case LEFT:  x--; break;
    case RIGHT: x++; break;
    }
}

static void resetRound(Player& p1, Player& p2) {
    clearGrid();
    p1.x = ARENA_W / 4;       p1.y = ARENA_H / 2;
    p1.dir = RIGHT;            p1.alive = true;
    p2.x = (ARENA_W * 3) / 4; p2.y = ARENA_H / 2;
    p2.dir = LEFT;             p2.alive = true;
}

// Returns 0=ongoing, 1=P1 wins, 2=P2 wins, 3=tie
static int tick(Player& p1, Player& p2) {
    if (p1.alive) grid[p1.y][p1.x] = CELL_P1;
    if (p2.alive) grid[p2.y][p2.x] = CELL_P2;

    int nx1 = p1.x, ny1 = p1.y;
    int nx2 = p2.x, ny2 = p2.y;
    if (p1.alive) stepDir(nx1, ny1, p1.dir);
    if (p2.alive) stepDir(nx2, ny2, p2.dir);

    bool die1 = p1.alive && (oob(nx1, ny1) || grid[ny1][nx1] != CELL_EMPTY);
    bool die2 = p2.alive && (oob(nx2, ny2) || grid[ny2][nx2] != CELL_EMPTY);

    if (p1.alive && p2.alive && nx1 == nx2 && ny1 == ny2)
        die1 = die2 = true;

    if (die1) p1.alive = false;
    if (die2) p2.alive = false;
    if (p1.alive) { p1.x = nx1; p1.y = ny1; }
    if (p2.alive) { p2.x = nx2; p2.y = ny2; }

    if (!p1.alive && !p2.alive) return 3;
    if (!p1.alive)               return 2;
    if (!p2.alive)               return 1;
    return 0;
}

// ─── Main ─────────────────────────────────────────────────────────────────────

int main() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (!has_colors()) {
        endwin();
        fprintf(stderr, "Your terminal does not support colors.\n");
        return 1;
    }
    start_color();

#ifdef _WIN32
    init_pair(CP_BORDER, COLOR_CYAN, COLOR_BLACK);
    init_pair(CP_P1, COLOR_CYAN, COLOR_BLACK);
    init_pair(CP_P2, COLOR_YELLOW, COLOR_BLACK);
    init_pair(CP_TITLE, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(CP_WIN, COLOR_GREEN, COLOR_BLACK);
    init_pair(CP_INFO, COLOR_WHITE, COLOR_BLACK);
    init_pair(CP_DEAD, COLOR_RED, COLOR_BLACK);
    init_pair(CP_BG, COLOR_WHITE, COLOR_BLACK);
#else
    use_default_colors();
    init_pair(CP_BORDER, COLOR_CYAN, -1);
    init_pair(CP_P1, COLOR_CYAN, -1);
    init_pair(CP_P2, COLOR_YELLOW, -1);
    init_pair(CP_TITLE, COLOR_MAGENTA, -1);
    init_pair(CP_WIN, COLOR_GREEN, -1);
    init_pair(CP_INFO, COLOR_WHITE, -1);
    init_pair(CP_DEAD, COLOR_RED, -1);
    init_pair(CP_BG, -1, -1);
#endif

    WINDOW* arena = newwin(ARENA_H + 2, ARENA_W + 2, 0, 0);
    WINDOW* panel = newwin(ARENA_H + 2, PANEL_W, 0, ARENA_W + 2);
    keypad(arena, TRUE);
    nodelay(arena, TRUE);

    Player p1{}, p2{};
    p1.score = p2.score = 0;

    bool running = true;

    showSplash(arena);

    while (running) {
        // ── Mode selection ────────────────────────────────────────────────
        GameMode mode = showModeMenu(arena);

        // ── New match ─────────────────────────────────────────────────────
        p1.score = p2.score = 0;
        int round = 1;

        while (running) {
            // ── New round ────────────────────────────────────────────────
            resetRound(p1, p2);
            drawBorder(arena);
            drawGrid(arena);
            drawHead(arena, p1, CP_P1);
            drawHead(arena, p2, CP_P2);
            wrefresh(arena);
            drawPanel(panel, p1, p2, round, false, mode);
            showCountdown(arena);

            bool paused = false;
            int  winner = 0;
            auto frameDur = std::chrono::milliseconds(1000 / FPS);

            while (winner == 0 && running) {
                auto t0 = std::chrono::steady_clock::now();

                // ── Input ────────────────────────────────────────────────
                wint_t ch = 0;
                wget_wch(arena, &ch);
                switch (ch) {
                    // Player 1 — W A S D (EN) + Ц Ф Ы В (RU) + any layout
                case L'w': case L'W':
                case L'\u0446': case L'\u0426': // ц / Ц
                    if (p1.canTurn(UP)) { p1.dir = UP; } break;
                case L's': case L'S':
                case L'\u044b': case L'\u042b': // ы / Ы
                    if (p1.canTurn(DOWN)) { p1.dir = DOWN; } break;
                case L'a': case L'A':
                case L'\u0444': case L'\u0424': // ф / Ф
                    if (p1.canTurn(LEFT)) { p1.dir = LEFT; } break;
                case L'd': case L'D':
                case L'\u0432': case L'\u0412': // в / В
                    if (p1.canTurn(RIGHT)) { p1.dir = RIGHT; } break;
                    // Player 2 — arrow keys (layout-independent)
                case KEY_UP:
                    if (mode == MODE_PVP && p2.canTurn(UP)) { p2.dir = UP; } break;
                case KEY_DOWN:
                    if (mode == MODE_PVP && p2.canTurn(DOWN)) { p2.dir = DOWN; } break;
                case KEY_LEFT:
                    if (mode == MODE_PVP && p2.canTurn(LEFT)) { p2.dir = LEFT; } break;
                case KEY_RIGHT:
                    if (mode == MODE_PVP && p2.canTurn(RIGHT)) { p2.dir = RIGHT; } break;
                case L'p': case L'P':
                case L'\u0437': case L'\u0417': // з / З (P в русской раскладке)
                    paused = !paused;
                    drawPanel(panel, p1, p2, round, paused, mode);
                    break;
                case L'q': case L'Q':
                case L'\u0439': case L'\u0419': // й / Й (Q в русской раскладке)
                    running = false;
                    break;
                }

                if (!paused && running) {
                    // ── AI decision (before tick) ─────────────────────────
                    if (mode == MODE_PVC && p2.alive)
                        p2.dir = aiChooseDir(p2, p1);

                    winner = tick(p1, p2);
                    drawGrid(arena);
                    drawHead(arena, p1, CP_P1);
                    drawHead(arena, p2, CP_P2);
                    drawBorder(arena);
                    wrefresh(arena);
                    drawPanel(panel, p1, p2, round, false, mode);
                }

                auto elapsed = std::chrono::steady_clock::now() - t0;
                if (elapsed < frameDur)
                    std::this_thread::sleep_for(frameDur - elapsed);
            }

            if (!running) break;

            if (winner == 1) p1.score++;
            else if (winner == 2) p2.score++;

            drawPanel(panel, p1, p2, round, false, mode);
            showRoundResult(arena, winner == 3 ? 0 : winner, mode);
            round++;

            if (p1.score >= WIN_SCORE || p2.score >= WIN_SCORE) break;
        }

        if (!running) break;

        int matchWinner = (p1.score >= WIN_SCORE) ? 1 : 2;
        if (!showMatchResult(arena, matchWinner, mode))
            running = false;
        // rematch → outer loop shows mode menu again
    }

    delwin(arena);
    delwin(panel);
    endwin();
    return 0;
}
