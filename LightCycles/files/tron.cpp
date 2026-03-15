/*
 * ████████╗██████╗  ██████╗ ███╗   ██╗
 *    ██╔══╝██╔══██╗██╔═══██╗████╗  ██║
 *    ██║   ██████╔╝██║   ██║██╔██╗ ██║
 *    ██║   ██╔══██╗██║   ██║██║╚██╗██║
 *    ██║   ██║  ██║╚██████╔╝██║ ╚████║
 *    ╚═╝   ╚═╝  ╚═╝ ╚═════╝ ╚═╝  ╚═══╝
 *
 *  LIGHT CYCLES — Classic Tron Arena
 *  Cross-platform: Windows (PDCurses) / Linux / macOS (ncurses)
 *
 *  Player 1 (CYAN) : W A S D
 *  Player 2 (GOLD) : Arrow keys
 *  Pause: P   Quit: Q
 */

// ─── Platform headers ─────────────────────────────────────────────────────────

#ifdef _WIN32
#  include <curses.h>       // PDCurses (install via vcpkg: pdcurses)
#else
#  include <ncurses.h>      // ncurses (Linux / macOS)
#endif

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

// ─── Constants ────────────────────────────────────────────────────────────────

static const int ARENA_W  = 78;
static const int ARENA_H  = 22;
static const int PANEL_W  = 20;
static const int WIN_SCORE = 5;
static const int FPS      = 13;

static const int CELL_EMPTY = 0;
static const int CELL_P1    = 1;
static const int CELL_P2    = 2;

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

// ─── Structs ──────────────────────────────────────────────────────────────────

struct Player {
    int  x, y;
    Dir  dir;
    bool alive;
    int  score;

    bool canTurn(Dir d) const {
        if (dir == UP    && d == DOWN)  return false;
        if (dir == DOWN  && d == UP)    return false;
        if (dir == LEFT  && d == RIGHT) return false;
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

static int sc(int x) { return x + 1; }  // arena x -> screen col (inside border)
static int sr(int y) { return y + 1; }  // arena y -> screen row (inside border)

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
            } else if (cell == CELL_P2) {
                wattron(w, COLOR_PAIR(CP_P2) | A_BOLD);
                mvwaddch(w, sr(r), sc(c), ACS_CKBOARD);
                wattroff(w, COLOR_PAIR(CP_P2) | A_BOLD);
            } else {
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
    int x   = ((ARENA_W + 2) - len) / 2;
    if (x < 1) x = 1;
    mvwprintw(w, row, x, "%s", msg);
    wattroff(w, attr);
}

// ─── Info panel ───────────────────────────────────────────────────────────────

static void drawPanel(WINDOW* p, const Player& p1, const Player& p2,
                      int round, bool paused)
{
    wclear(p);

    wattron(p, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(p, 0, 2, " T R O N ");
    mvwprintw(p, 1, 1, "LIGHT CYCLES");
    wattroff(p, COLOR_PAIR(CP_TITLE) | A_BOLD);

    wattron(p, COLOR_PAIR(CP_INFO));
    mvwprintw(p, 3, 1, "ROUND  %d", round);
    wattroff(p, COLOR_PAIR(CP_INFO));

    wattron(p, COLOR_PAIR(CP_P1) | A_BOLD);
    mvwprintw(p, 5, 1, "P1  [CYAN]");
    wattroff(p, COLOR_PAIR(CP_P1) | A_BOLD);
    wattron(p, COLOR_PAIR(CP_INFO));
    mvwprintw(p, 6, 1, "Score : %d / %d", p1.score, WIN_SCORE);
    mvwprintw(p, 7, 1, "Status: %s", p1.alive ? "ALIVE" : "DEAD ");
    wattroff(p, COLOR_PAIR(CP_INFO));

    wattron(p, COLOR_PAIR(CP_P2) | A_BOLD);
    mvwprintw(p, 9,  1, "P2  [GOLD]");
    wattroff(p, COLOR_PAIR(CP_P2) | A_BOLD);
    wattron(p, COLOR_PAIR(CP_INFO));
    mvwprintw(p, 10, 1, "Score : %d / %d", p2.score, WIN_SCORE);
    mvwprintw(p, 11, 1, "Status: %s", p2.alive ? "ALIVE" : "DEAD ");
    wattroff(p, COLOR_PAIR(CP_INFO));

    wattron(p, COLOR_PAIR(CP_INFO) | A_DIM);
    mvwprintw(p, 14, 1, "P1 : W A S D");
    mvwprintw(p, 15, 1, "P2 : Arrows");
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
    int ch;
    while ((ch = wgetch(w)) != ' ' && ch != '\n'
           && ch != 'q' && ch != 'Q') {}
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

static void showRoundResult(WINDOW* w, int winner) {
    drawGrid(w);
    drawBorder(w);
    if (winner == 0) {
        centerMsg(w, ARENA_H/2 - 1, "*** D R A W ***",          CP_INFO, true);
        centerMsg(w, ARENA_H/2 + 1, "Both cycles destroyed.",    CP_INFO, false);
    } else if (winner == 1) {
        centerMsg(w, ARENA_H/2 - 1, "PLAYER 1 WINS THE ROUND!", CP_P1, true);
        centerMsg(w, ARENA_H/2 + 1, "Cyan cycle survives.",      CP_P1, false);
    } else {
        centerMsg(w, ARENA_H/2 - 1, "PLAYER 2 WINS THE ROUND!", CP_P2, true);
        centerMsg(w, ARENA_H/2 + 1, "Gold cycle survives.",      CP_P2, false);
    }
    waitKey(w, "--- Press SPACE or ENTER to continue ---");
}

// Returns true if rematch requested
static bool showMatchResult(WINDOW* w, int winner) {
    wclear(w);
    drawBorder(w);
    CP cp = (winner == 1) ? CP_P1 : CP_P2;
    centerMsg(w, ARENA_H/2 - 2, "==========================",      cp,     true);
    if (winner == 1)
        centerMsg(w, ARENA_H/2,  " PLAYER 1 WINS THE MATCH! ",     cp,     true);
    else
        centerMsg(w, ARENA_H/2,  " PLAYER 2 WINS THE MATCH! ",     cp,     true);
    centerMsg(w, ARENA_H/2 + 2, "==========================",      cp,     true);
    centerMsg(w, ARENA_H/2 + 4, "R - rematch      Q - quit",       CP_INFO, false);
    wrefresh(w);

    nodelay(w, FALSE);
    int ch;
    do { ch = wgetch(w); } while (ch != 'r' && ch != 'R'
                                && ch != 'q' && ch != 'Q');
    nodelay(w, TRUE);
    return (ch == 'r' || ch == 'R');
}

static void showSplash(WINDOW* w) {
    wclear(w);
    drawBorder(w);
    centerMsg(w,  3, " T R O N  :  L I G H T  C Y C L E S ",  CP_TITLE, true);
    centerMsg(w,  5, "Two programs enter. One program leaves.", CP_INFO,  false);
    centerMsg(w,  8, "Player 1  (CYAN)  :  W  A  S  D",        CP_P1,    true);
    centerMsg(w,  9, "Player 2  (GOLD)  :  Arrow Keys",         CP_P2,    true);
    centerMsg(w, 11, "Ride your light cycle across the Grid.",  CP_INFO,  false);
    centerMsg(w, 12, "Force your opponent into a wall or trail.",CP_INFO, false);
    centerMsg(w, 14, "First to 5 round wins takes the match.",  CP_INFO,  false);
    centerMsg(w, 17, "P - pause                Q - quit",       CP_INFO,  false);
    centerMsg(w, 19, ">>> Press SPACE to enter the Grid <<<",   CP_WIN,   true);
    wrefresh(w);

    nodelay(w, FALSE);
    int ch;
    while ((ch = wgetch(w)) != ' ' && ch != '\n') {}
    nodelay(w, TRUE);
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

static bool oob(int x, int y) {
    return x < 0 || x >= ARENA_W || y < 0 || y >= ARENA_H;
}

static void resetRound(Player& p1, Player& p2) {
    clearGrid();
    p1.x = ARENA_W / 4;      p1.y = ARENA_H / 2;
    p1.dir = RIGHT;           p1.alive = true;
    p2.x = (ARENA_W * 3) / 4; p2.y = ARENA_H / 2;
    p2.dir = LEFT;            p2.alive = true;
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

    // Head-on collision
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
    // PDCurses: COLOR_BLACK is the safe background on Windows consoles
    init_pair(CP_BORDER, COLOR_CYAN,    COLOR_BLACK);
    init_pair(CP_P1,     COLOR_CYAN,    COLOR_BLACK);
    init_pair(CP_P2,     COLOR_YELLOW,  COLOR_BLACK);
    init_pair(CP_TITLE,  COLOR_MAGENTA, COLOR_BLACK);
    init_pair(CP_WIN,    COLOR_GREEN,   COLOR_BLACK);
    init_pair(CP_INFO,   COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_DEAD,   COLOR_RED,     COLOR_BLACK);
    init_pair(CP_BG,     COLOR_WHITE,   COLOR_BLACK);
#else
    // ncurses: -1 = transparent terminal background
    use_default_colors();
    init_pair(CP_BORDER, COLOR_CYAN,    -1);
    init_pair(CP_P1,     COLOR_CYAN,    -1);
    init_pair(CP_P2,     COLOR_YELLOW,  -1);
    init_pair(CP_TITLE,  COLOR_MAGENTA, -1);
    init_pair(CP_WIN,    COLOR_GREEN,   -1);
    init_pair(CP_INFO,   COLOR_WHITE,   -1);
    init_pair(CP_DEAD,   COLOR_RED,     -1);
    init_pair(CP_BG,     -1,            -1);
#endif

    // Arena window includes 1-cell border → (ARENA_H+2) × (ARENA_W+2)
    WINDOW* arena = newwin(ARENA_H + 2, ARENA_W + 2, 0, 0);
    WINDOW* panel = newwin(ARENA_H + 2, PANEL_W,     0, ARENA_W + 2);
    keypad(arena, TRUE);
    nodelay(arena, TRUE);

    Player p1{}, p2{};
    p1.score = p2.score = 0;

    bool running = true;
    showSplash(arena);

    while (running) {
        // ── New match ──────────────────────────────────────────────────────
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
            drawPanel(panel, p1, p2, round, false);
            showCountdown(arena);

            bool paused = false;
            int  winner = 0;
            auto frameDur = std::chrono::milliseconds(1000 / FPS);

            while (winner == 0 && running) {
                auto t0 = std::chrono::steady_clock::now();

                int ch = wgetch(arena);
                switch (ch) {
                    case 'w': case 'W': if (p1.canTurn(UP))    p1.dir = UP;    break;
                    case 's': case 'S': if (p1.canTurn(DOWN))  p1.dir = DOWN;  break;
                    case 'a': case 'A': if (p1.canTurn(LEFT))  p1.dir = LEFT;  break;
                    case 'd': case 'D': if (p1.canTurn(RIGHT)) p1.dir = RIGHT; break;
                    case KEY_UP:    if (p2.canTurn(UP))    p2.dir = UP;    break;
                    case KEY_DOWN:  if (p2.canTurn(DOWN))  p2.dir = DOWN;  break;
                    case KEY_LEFT:  if (p2.canTurn(LEFT))  p2.dir = LEFT;  break;
                    case KEY_RIGHT: if (p2.canTurn(RIGHT)) p2.dir = RIGHT; break;
                    case 'p': case 'P':
                        paused = !paused;
                        drawPanel(panel, p1, p2, round, paused);
                        break;
                    case 'q': case 'Q':
                        running = false;
                        break;
                }

                if (!paused && running) {
                    winner = tick(p1, p2);
                    drawGrid(arena);
                    drawHead(arena, p1, CP_P1);
                    drawHead(arena, p2, CP_P2);
                    drawBorder(arena);
                    wrefresh(arena);
                    drawPanel(panel, p1, p2, round, false);
                }

                auto elapsed = std::chrono::steady_clock::now() - t0;
                if (elapsed < frameDur)
                    std::this_thread::sleep_for(frameDur - elapsed);
            }

            if (!running) break;

            if      (winner == 1) p1.score++;
            else if (winner == 2) p2.score++;

            drawPanel(panel, p1, p2, round, false);
            showRoundResult(arena, winner == 3 ? 0 : winner);
            round++;

            if (p1.score >= WIN_SCORE || p2.score >= WIN_SCORE) break;
        }

        if (!running) break;

        int matchWinner = (p1.score >= WIN_SCORE) ? 1 : 2;
        if (!showMatchResult(arena, matchWinner))
            running = false;
        // rematch: outer loop continues
    }

    delwin(arena);
    delwin(panel);
    endwin();
    return 0;
}
