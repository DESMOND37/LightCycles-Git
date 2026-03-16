/*
 * ████████╗██████╗  ██████╗ ███╗   ██╗
 *    ██╔══╝██╔══██╗██╔═══██╗████╗  ██║
 *    ██║   ██████╔╝██║   ██║██╔██╗ ██║
 *    ██║   ██╔══██╗██║   ██║██║╚██╗██║
 *    ██║   ██║  ██║╚██████╔╝██║ ╚████║
 *    ╚═╝   ╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 *  LIGHT CYCLES — Classic Tron Arena
 *  ver. 0.0.0.4 — Color & Navigation Update
 *  Cross-platform: Windows (PDCurses) / Linux (ncursesw) / macOS (ncurses)
 *
 *  Player 1 : W A S D  (EN) / Ц Ф Ы В (RU)
 *  Player 2 : Arrow keys (PvP) / CPU (PvC)
 *  Pause: P   Quit: Q   Back: ESC
 *
 *  © Not Yet Copyright.
 *  DSMND Software. All Rights Not Reserved.
 *  All Rights Not Reserved Yet.
 */

 // ─── Platform headers ─────────────────────────────────────────────────────────

#ifdef _WIN32
#  define PDC_WIDE
#  include <curses.h>
#else
#  define _XOPEN_SOURCE_EXTENDED 1
#  ifdef __APPLE__
#    include <ncurses.h>
#  else
#    include <ncursesw/ncurses.h>
#  endif
#endif

#include <array>
#include <chrono>
#include <cstring>
#include <cstdio>
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

// ─── Color palette ────────────────────────────────────────────────────────────
//
// Six selectable colors, each paired with its "opposite".
// Pairs: CYAN↔RED, GREEN↔MAGENTA, YELLOW↔BLUE

struct ColorEntry {
    const char* name;   // display name
    int         color;  // ncurses COLOR_* constant
    int         opp;    // index of the opposite color in COLOR_OPTIONS[]
};

static const ColorEntry COLOR_OPTIONS[] = {
    { "CYAN",    COLOR_CYAN,    4 },   // 0  ↔ RED
    { "BLUE",    COLOR_BLUE,    3 },   // 1  ↔ YELLOW
    { "GREEN",   COLOR_GREEN,   5 },   // 2  ↔ MAGENTA
    { "YELLOW",  COLOR_YELLOW,  1 },   // 3  ↔ BLUE
    { "RED",     COLOR_RED,     0 },   // 4  ↔ CYAN
    { "MAGENTA", COLOR_MAGENTA, 2 },   // 5  ↔ GREEN
};
static const int NUM_COLORS = 6;

// ─── Color pair IDs ───────────────────────────────────────────────────────────

enum CP {
    CP_BORDER = 1,
    CP_P1,       // 2 — player 1 (dynamic)
    CP_P2,       // 3 — player 2 / CPU (dynamic)
    CP_TITLE,    // 4
    CP_WIN,      // 5
    CP_INFO,     // 6
    CP_DEAD,     // 7
    CP_BG,       // 8
    // Static preview pairs for the color selection menu
    CP_COL0,     // 9  CYAN
    CP_COL1,     // 10 RED
    CP_COL2,     // 11 GREEN
    CP_COL3,     // 12 MAGENTA
    CP_COL4,     // 13 YELLOW
    CP_COL5,     // 14 BLUE
};

enum Dir { UP, DOWN, LEFT, RIGHT };
enum GameMode { MODE_PVP, MODE_PVC };

// Application state machine
enum AppState {
    ST_SPLASH,
    ST_MODE,
    ST_COLOR,
    ST_MATCH,
    ST_QUIT
};

// ─── Player ───────────────────────────────────────────────────────────────────

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

// ─── Key classification helpers ───────────────────────────────────────────────

static bool isConfirm(wint_t ch) {
    return ch == L'\n' || ch == L'\r' || ch == L' ' || (int)ch == KEY_ENTER;
}
static bool isBack(wint_t ch) {
    // ESC=27, Backspace (KEY_BACKSPACE, \b=8, DEL=127)
    return (int)ch == 27 || (int)ch == KEY_BACKSPACE
        || (int)ch == 8 || (int)ch == 127;
}
static bool isQuit(wint_t ch) {
    return ch == L'q' || ch == L'Q'
        || ch == L'\u0439' || ch == L'\u0419'; // й / Й
}
static bool isUp(wint_t ch) {
    return (int)ch == KEY_UP
        || ch == L'w' || ch == L'W'
        || ch == L'\u0446' || ch == L'\u0426'; // ц / Ц
}
static bool isDown(wint_t ch) {
    return (int)ch == KEY_DOWN
        || ch == L's' || ch == L'S'
        || ch == L'\u044b' || ch == L'\u042b'; // ы / Ы
}

// ─── Apply dynamic color pairs for P1 / P2 ───────────────────────────────────

static void applyPlayerColors(int p1Idx, int p2Idx) {
#ifdef _WIN32
    init_pair(CP_P1, COLOR_OPTIONS[p1Idx].color, COLOR_BLACK);
    init_pair(CP_P2, COLOR_OPTIONS[p2Idx].color, COLOR_BLACK);
#else
    init_pair(CP_P1, COLOR_OPTIONS[p1Idx].color, -1);
    init_pair(CP_P2, COLOR_OPTIONS[p2Idx].color, -1);
#endif
}

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

// Center a string inside the arena window (width = ARENA_W + 2)
static void centerMsg(WINDOW* w, int row, const char* msg, CP cp, bool bold) {
    int attr = COLOR_PAIR(cp) | (bold ? A_BOLD : 0);
    wattron(w, attr);
    int len = (int)strlen(msg);
    int x = ((ARENA_W + 2) - len) / 2;
    if (x < 1) x = 1;
    mvwprintw(w, row, x, "%s", msg);
    wattroff(w, attr);
}

// Copyright footer (reused in every menu)
static void drawFooter(WINDOW* w) {
    centerMsg(w, ARENA_H - 2, "(c) Not Yet Copyright. DSMND Software.", CP_INFO, false);
    centerMsg(w, ARENA_H - 1, "All Rights Not Reserved Yet.", CP_INFO, false);
}

// ─── Info panel ───────────────────────────────────────────────────────────────

static void drawPanel(WINDOW* p, const Player& p1, const Player& p2,
    int round, bool paused, GameMode mode,
    int p1ColorIdx, int p2ColorIdx)
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
    mvwprintw(p, 5, 1, "P1 [%-7s]", COLOR_OPTIONS[p1ColorIdx].name);
    wattroff(p, COLOR_PAIR(CP_P1) | A_BOLD);
    wattron(p, COLOR_PAIR(CP_INFO));
    mvwprintw(p, 6, 1, "Score : %d / %d", p1.score, WIN_SCORE);
    mvwprintw(p, 7, 1, "Status: %s", p1.alive ? "ALIVE" : "DEAD ");
    wattroff(p, COLOR_PAIR(CP_INFO));

    wattron(p, COLOR_PAIR(CP_P2) | A_BOLD);
    if (mode == MODE_PVC)
        mvwprintw(p, 9, 1, "CPU[%-7s]", COLOR_OPTIONS[p2ColorIdx].name);
    else
        mvwprintw(p, 9, 1, "P2 [%-7s]", COLOR_OPTIONS[p2ColorIdx].name);
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
    mvwprintw(p, 17, 1, "P/ESC- pause");
    mvwprintw(p, 18, 1, "Q  - quit");
    wattroff(p, COLOR_PAIR(CP_INFO) | A_DIM);

    if (paused) {
        wattron(p, COLOR_PAIR(CP_WIN) | A_BOLD);
        mvwprintw(p, 20, 2, "* PAUSED *");
        wattroff(p, COLOR_PAIR(CP_WIN) | A_BOLD);
        wattron(p, COLOR_PAIR(CP_INFO) | A_DIM);
        mvwprintw(p, 21, 1, "BS  - main menu");
        wattroff(p, COLOR_PAIR(CP_INFO) | A_DIM);
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
        && !isConfirm(ch) && !isQuit(ch)) {
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

// Returns true = rematch (go to mode menu), false = quit
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
    do { wget_wch(w, &ch); } while (!isQuit(ch) && ch != L'r' && ch != L'R');
    nodelay(w, TRUE);
    return (ch == L'r' || ch == L'R');
}

// ─── Splash screen ────────────────────────────────────────────────────────────
// Returns false if user pressed Q (quit)

static bool showSplash(WINDOW* w) {
    wclear(w);
    drawBorder(w);
    centerMsg(w, 2, " T R O N  :  L I G H T  C Y C L E S ", CP_TITLE, true);
    centerMsg(w, 3, " ver. 0.0.0.4 - Color & Navigation Update ", CP_INFO, false);
    centerMsg(w, 5, "Two programs enter. One program leaves.", CP_INFO, false);
    centerMsg(w, 7, "Player 1  (CYAN)  :  W  A  S  D", CP_P1, true);
    centerMsg(w, 8, "Player 2  (GOLD)  :  Arrow Keys  (PvP)", CP_P2, true);
    centerMsg(w, 10, "Ride your light cycle across the Grid.", CP_INFO, false);
    centerMsg(w, 11, "Force your opponent into a wall or trail.", CP_INFO, false);
    centerMsg(w, 13, "First to 5 round wins takes the match.", CP_INFO, false);
    centerMsg(w, 16, "P - pause                Q - quit", CP_INFO, false);
    centerMsg(w, 18, ">>> Press SPACE or ENTER to continue <<<", CP_WIN, true);
    drawFooter(w);
    wrefresh(w);

    nodelay(w, FALSE);
    wint_t ch = 0;
    while (true) {
        wget_wch(w, &ch);
        if (isConfirm(ch)) { nodelay(w, TRUE); return true; }
        if (isQuit(ch)) { nodelay(w, TRUE); return false; }
    }
}

// ─── Mode selection menu ──────────────────────────────────────────────────────
// Returns: -1 = back (ESC), -2 = quit, 0 = PvP, 1 = PvC

static int showModeMenu(WINDOW* w) {
    int sel = 0;
    nodelay(w, FALSE);

    while (true) {
        wclear(w);
        drawBorder(w);
        centerMsg(w, 2, " T R O N  :  L I G H T  C Y C L E S ", CP_TITLE, true);
        centerMsg(w, 4, "Select game mode:", CP_INFO, false);

        // PvP option
        if (sel == 0) {
            wattron(w, COLOR_PAIR(CP_WIN) | A_BOLD | A_REVERSE);
            centerMsg(w, 7, "  [ Player vs Player ]  ", CP_WIN, true);
            wattroff(w, COLOR_PAIR(CP_WIN) | A_BOLD | A_REVERSE);
        }
        else {
            centerMsg(w, 7, "    Player vs Player    ", CP_INFO, false);
        }

        // PvC option
        if (sel == 1) {
            wattron(w, COLOR_PAIR(CP_WIN) | A_BOLD | A_REVERSE);
            centerMsg(w, 9, "  [ Player vs CPU    ]  ", CP_WIN, true);
            wattroff(w, COLOR_PAIR(CP_WIN) | A_BOLD | A_REVERSE);
        }
        else {
            centerMsg(w, 9, "    Player vs CPU       ", CP_INFO, false);
        }

        centerMsg(w, 12, "W/S  or  Up/Down  -  select", CP_INFO, false);
        centerMsg(w, 13, "ENTER / SPACE     -  confirm", CP_INFO, false);
        centerMsg(w, 14, "ESC / Backspace   -  back", CP_INFO, false);
        centerMsg(w, 15, "Q                 -  quit", CP_INFO, false);
        drawFooter(w);
        wrefresh(w);

        wint_t ch = 0;
        wget_wch(w, &ch);
        if (isUp(ch))      sel = 0;
        else if (isDown(ch))    sel = 1;
        else if (isConfirm(ch)) { nodelay(w, TRUE); return sel; }
        else if (isBack(ch)) { nodelay(w, TRUE); return -1; }
        else if (isQuit(ch)) { nodelay(w, TRUE); return -2; }
    }
}

// ─── Color selection menu ─────────────────────────────────────────────────────
// Returns: -1 = back (ESC), -2 = quit, 0-5 = selected color index

static int showColorMenu(WINDOW* w) {
    int sel = 0;
    nodelay(w, FALSE);

    // Fixed layout positions (inside 80-wide arena window)
    const int xArrow = 13;   // selector arrow column
    const int xBox = 16;   // start of player color box
    const int xSep = 29;   // separator arrow
    const int xOpp = 33;   // opponent color name start

    while (true) {
        wclear(w);
        drawBorder(w);
        centerMsg(w, 2, " T R O N  :  L I G H T  C Y C L E S ", CP_TITLE, true);
        centerMsg(w, 4, "Choose your cycle color:", CP_INFO, false);
        centerMsg(w, 5, "(opponent gets the opposite color)", CP_INFO, false);

        for (int i = 0; i < NUM_COLORS; ++i) {
            int row = 7 + i;
            int oppIdx = COLOR_OPTIONS[i].opp;
            CP  cpI = (CP)(CP_COL0 + i);
            CP  cpO = (CP)(CP_COL0 + oppIdx);

            // Selector arrow
            if (i == sel) {
                wattron(w, COLOR_PAIR(CP_WIN) | A_BOLD);
                mvwprintw(w, row, xArrow, ">");
                wattroff(w, COLOR_PAIR(CP_WIN) | A_BOLD);
            }
            else {
                mvwprintw(w, row, xArrow, " ");
            }

            // Player color box — highlighted if selected
            char pBox[16];
            snprintf(pBox, sizeof(pBox), "[%-7s]", COLOR_OPTIONS[i].name);
            if (i == sel)
                wattron(w, COLOR_PAIR(cpI) | A_BOLD | A_REVERSE);
            else
                wattron(w, COLOR_PAIR(cpI) | A_BOLD);
            mvwprintw(w, row, xBox, "%s", pBox);
            if (i == sel)
                wattroff(w, COLOR_PAIR(cpI) | A_BOLD | A_REVERSE);
            else
                wattroff(w, COLOR_PAIR(cpI) | A_BOLD);

            // Separator
            wattron(w, COLOR_PAIR(CP_INFO));
            mvwprintw(w, row, xSep, "->");
            wattroff(w, COLOR_PAIR(CP_INFO));

            // Opponent color box
            char oBox[16];
            snprintf(oBox, sizeof(oBox), "[%-7s]", COLOR_OPTIONS[oppIdx].name);
            wattron(w, COLOR_PAIR(cpO) | A_BOLD);
            mvwprintw(w, row, xOpp, "%s", oBox);
            wattroff(w, COLOR_PAIR(cpO) | A_BOLD);
        }

        // Legend
        centerMsg(w, 15, "W/S  or  Up/Down  -  select", CP_INFO, false);
        centerMsg(w, 16, "ENTER / SPACE     -  confirm", CP_INFO, false);
        centerMsg(w, 17, "ESC / Backspace   -  back", CP_INFO, false);
        centerMsg(w, 18, "Q                 -  quit", CP_INFO, false);
        drawFooter(w);
        wrefresh(w);

        wint_t ch = 0;
        wget_wch(w, &ch);
        if (isUp(ch)) { sel = (sel - 1 + NUM_COLORS) % NUM_COLORS; }
        else if (isDown(ch)) { sel = (sel + 1) % NUM_COLORS; }
        else if (isConfirm(ch)) { nodelay(w, TRUE); return sel; }
        else if (isBack(ch)) { nodelay(w, TRUE); return -1; }
        else if (isQuit(ch)) { nodelay(w, TRUE); return -2; }
    }
}

// ─── AI (flood fill) ──────────────────────────────────────────────────────────

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

static int floodFill(int sx, int sy, int bx, int by) {
    if (oob(sx, sy) || grid[sy][sx] != CELL_EMPTY) return 0;

    static bool visited[ARENA_H][ARENA_W];
    for (int r = 0; r < ARENA_H; ++r)
        for (int c = 0; c < ARENA_W; ++c)
            visited[r][c] = false;

    int savedExtra = 0;
    bool hadExtra = false;
    if (!oob(bx, by)) {
        savedExtra = grid[by][bx];
        grid[by][bx] = CELL_P2;
        hadExtra = true;
    }

    std::queue<std::pair<int, int>> q;
    q.push({ sx, sy });
    visited[sy][sx] = true;
    int count = 0;

    const int dx[] = { 0, 0, -1, 1 };
    const int dy[] = { -1, 1,  0, 0 };

    while (!q.empty()) {
        auto [cx, cy] = q.front(); q.pop();
        ++count;
        for (int i = 0; i < 4; ++i) {
            int nx = cx + dx[i], ny = cy + dy[i];
            if (!oob(nx, ny) && !visited[ny][nx] && grid[ny][nx] == CELL_EMPTY) {
                visited[ny][nx] = true;
                q.push({ nx, ny });
            }
        }
    }

    if (hadExtra) grid[by][bx] = savedExtra;
    return count;
}

static Dir aiChooseDir(const Player& ai, const Player& player) {
    const Dir allDirs[4] = { UP, DOWN, LEFT, RIGHT };
    Dir bestDir = ai.dir;
    int bestScore = -1;

    for (Dir d : allDirs) {
        if (!ai.canTurn(d)) continue;
        int nx = nextX(ai.x, d);
        int ny = nextY(ai.y, d);
        if (oob(nx, ny) || grid[ny][nx] != CELL_EMPTY) continue;

        int space = floodFill(nx, ny, ai.x, ai.y);
        if (d == ai.dir) space += 2;
        if (space > 10 && player.alive) {
            int distNow = abs(ai.x - player.x) + abs(ai.y - player.y);
            int distNext = abs(nx - player.x) + abs(ny - player.y);
            if (distNext < distNow) space += 3;
        }
        if (space > bestScore) { bestScore = space; bestDir = d; }
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

    // Background value differs per platform
#ifdef _WIN32
    const int BG = COLOR_BLACK;
#else
    use_default_colors();
    const int BG = -1;
#endif

    // Static pairs (never change)
    init_pair(CP_BORDER, COLOR_CYAN, BG);
    init_pair(CP_P1, COLOR_CYAN, BG); // overwritten before each match
    init_pair(CP_P2, COLOR_YELLOW, BG); // overwritten before each match
    init_pair(CP_TITLE, COLOR_MAGENTA, BG);
    init_pair(CP_WIN, COLOR_GREEN, BG);
    init_pair(CP_INFO, COLOR_WHITE, BG);
    init_pair(CP_DEAD, COLOR_RED, BG);
    init_pair(CP_BG, BG == -1 ? -1 : COLOR_WHITE, BG);

    // Preview pairs for color selection menu (always fixed)
    init_pair(CP_COL0, COLOR_CYAN, BG);  // 0 CYAN
    init_pair(CP_COL1, COLOR_BLUE, BG);  // 1 BLUE
    init_pair(CP_COL2, COLOR_GREEN, BG);  // 2 GREEN
    init_pair(CP_COL3, COLOR_YELLOW, BG);  // 3 YELLOW
    init_pair(CP_COL4, COLOR_RED, BG);  // 4 RED
    init_pair(CP_COL5, COLOR_MAGENTA, BG);  // 5 MAGENTA

    WINDOW* arena = newwin(ARENA_H + 2, ARENA_W + 2, 0, 0);
    WINDOW* panel = newwin(ARENA_H + 2, PANEL_W, 0, ARENA_W + 2);
    keypad(arena, TRUE);
    nodelay(arena, TRUE);

    Player p1{}, p2{};
    p1.score = p2.score = 0;

    AppState state = ST_SPLASH;
    GameMode mode = MODE_PVP;
    int      p1ColorIdx = 0;   // CYAN default
    int      p2ColorIdx = 1;   // RED (opposite of CYAN)

    // ── State machine ─────────────────────────────────────────────────────────
    while (state != ST_QUIT) {

        // ── Splash ────────────────────────────────────────────────────────────
        if (state == ST_SPLASH) {
            bool cont = showSplash(arena);
            state = cont ? ST_MODE : ST_QUIT;
        }

        // ── Mode menu ─────────────────────────────────────────────────────────
        else if (state == ST_MODE) {
            int r = showModeMenu(arena);
            if (r == -2) state = ST_QUIT;
            else if (r == -1) state = ST_SPLASH;
            else { mode = (GameMode)r; state = ST_COLOR; }
        }

        // ── Color menu ────────────────────────────────────────────────────────
        else if (state == ST_COLOR) {
            int r = showColorMenu(arena);
            if (r == -2) state = ST_QUIT;
            else if (r == -1) state = ST_MODE;
            else {
                p1ColorIdx = r;
                p2ColorIdx = COLOR_OPTIONS[r].opp;
                applyPlayerColors(p1ColorIdx, p2ColorIdx);
                state = ST_MATCH;
            }
        }

        // ── Match ─────────────────────────────────────────────────────────────
        else if (state == ST_MATCH) {
            p1.score = p2.score = 0;
            int  round = 1;
            bool matchOn = true;

            while (matchOn && state == ST_MATCH) {
                // ── Round ─────────────────────────────────────────────────
                resetRound(p1, p2);
                drawBorder(arena);
                drawGrid(arena);
                drawHead(arena, p1, CP_P1);
                drawHead(arena, p2, CP_P2);
                wrefresh(arena);
                drawPanel(panel, p1, p2, round, false, mode, p1ColorIdx, p2ColorIdx);
                showCountdown(arena);

                bool paused = false;
                int  winner = 0;
                auto frameDur = std::chrono::milliseconds(1000 / FPS);

                while (winner == 0 && state == ST_MATCH) {
                    auto t0 = std::chrono::steady_clock::now();

                    wint_t ch = 0;
                    wget_wch(arena, &ch);
                    switch ((int)ch) {
                        // P1 — W A S D (EN) + Ц Ф Ы В (RU)
                    case L'w': case L'W':
                    case L'\u0446': case L'\u0426':
                        if (p1.canTurn(UP)) { p1.dir = UP; } break;
                    case L's': case L'S':
                    case L'\u044b': case L'\u042b':
                        if (p1.canTurn(DOWN)) { p1.dir = DOWN; } break;
                    case L'a': case L'A':
                    case L'\u0444': case L'\u0424':
                        if (p1.canTurn(LEFT)) { p1.dir = LEFT; } break;
                    case L'd': case L'D':
                    case L'\u0432': case L'\u0412':
                        if (p1.canTurn(RIGHT)) { p1.dir = RIGHT; } break;
                        // P2 arrows (PvP only)
                    case KEY_UP:
                        if (mode == MODE_PVP && p2.canTurn(UP)) { p2.dir = UP; } break;
                    case KEY_DOWN:
                        if (mode == MODE_PVP && p2.canTurn(DOWN)) { p2.dir = DOWN; } break;
                    case KEY_LEFT:
                        if (mode == MODE_PVP && p2.canTurn(LEFT)) { p2.dir = LEFT; } break;
                    case KEY_RIGHT:
                        if (mode == MODE_PVP && p2.canTurn(RIGHT)) { p2.dir = RIGHT; } break;
                        // Pause — P или ESC
                    case L'p': case L'P':
                    case L'\u0437': case L'\u0417': // з / З
                    case 27:           // ESC
                        paused = !paused;
                        drawPanel(panel, p1, p2, round, paused, mode, p1ColorIdx, p2ColorIdx);
                        break;
                        // Backspace — выход в главное меню (только если на паузе)
                    case KEY_BACKSPACE:
                    case 8:            // ^H
                    case 127:          // DEL
                        if (paused) state = ST_MODE;
                        break;
                        // Quit
                    case L'q': case L'Q':
                    case L'\u0439': case L'\u0419': // й / Й
                        state = ST_QUIT;
                        break;
                    }

                    if (!paused && state == ST_MATCH) {
                        if (mode == MODE_PVC && p2.alive)
                            p2.dir = aiChooseDir(p2, p1);

                        winner = tick(p1, p2);
                        drawGrid(arena);
                        drawHead(arena, p1, CP_P1);
                        drawHead(arena, p2, CP_P2);
                        drawBorder(arena);
                        wrefresh(arena);
                        drawPanel(panel, p1, p2, round, false, mode, p1ColorIdx, p2ColorIdx);
                    }

                    auto elapsed = std::chrono::steady_clock::now() - t0;
                    if (elapsed < frameDur)
                        std::this_thread::sleep_for(frameDur - elapsed);
                }

                if (state == ST_QUIT) break;

                if (winner == 1) p1.score++;
                else if (winner == 2) p2.score++;

                drawPanel(panel, p1, p2, round, false, mode, p1ColorIdx, p2ColorIdx);
                showRoundResult(arena, winner == 3 ? 0 : winner, mode);
                round++;

                if (p1.score >= WIN_SCORE || p2.score >= WIN_SCORE) {
                    int mw = (p1.score >= WIN_SCORE) ? 1 : 2;
                    bool rematch = showMatchResult(arena, mw, mode);
                    // Rematch → back to mode menu so player can change everything
                    state = rematch ? ST_MODE : ST_QUIT;
                    matchOn = false;
                }
            }
        }
    }

    delwin(arena);
    delwin(panel);
    endwin();
    return 0;
}
