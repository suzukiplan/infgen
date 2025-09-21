#include <vgs.h>

// Index of Pattern
#define P_FONT 0
#define P_PLAYER 128

#define MAX_CHILD 256

// Index of OAM
#define O_PLAYER 0
#define O_CHILD (O_PLAYER + 1)

typedef struct {
    int32_t exist;
    int32_t x;
    int32_t y;
    int32_t* ax;
    int32_t* ay;
    int32_t degree;
    uint32_t scale;
    int oi;
} Child;

struct GlobalVariables {
    BOOL gameover;
    uint32_t child_count;

    struct Player {
        int32_t x;
        int32_t y;
        int32_t degree;
        int32_t speed;
        int32_t max_speed;
        BOOL collision;
    } player;

    Child child[MAX_CHILD];
} g;

BOOL hitchk(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2)
{
    return y1 < y2 + h2 && y2 < y1 + h1 && x1 < x2 + w2 && x2 < x1 + w1 ? TRUE : FALSE;
}

void game_init(void)
{
    VGS_VREG_CLSA = 0;
    VGS_VREG_BMP0 = TRUE;
    VGS_VREG_BMP1 = TRUE;
    VGS_VREG_SPOS = 0;

    for (int i = 0; i < 8; i++) {
        uint32_t c1 = 0x0F * i + 0x0F;
        uint32_t c2 = c1;
        c2 <<= 8;
        c2 |= c1;
        c2 <<= 8;
        c2 |= c1;
        vgs_draw_box(1, i, i, VRAM_WIDTH - i - 1, VRAM_HEIGHT - i - 1, c2);
    }
    vgs_draw_boxf(0, 8, 8, VRAM_WIDTH - 9, VRAM_HEIGHT - 9, 0x0F2F60);

    for (int i = 0; i < OAM_MAX; i++) {
        OAM[i].visible = FALSE;
    }

    vgs_memset(&g, 0, sizeof(g));
    g.player.x = ((VRAM_WIDTH - 16) / 2) << 8;
    g.player.y = ((VRAM_HEIGHT - 16) / 2) << 8;
    vgs_sprite(O_PLAYER, FALSE, g.player.x >> 8, g.player.y >> 8, 1, 0, P_PLAYER);
}

void add_child(int32_t x, int32_t y, int32_t* ax, int32_t* ay, int32_t degree)
{
    if (MAX_CHILD <= g.child_count) {
        return;
    }
    g.child[g.child_count].exist = 30;
    g.child[g.child_count].x = x;
    g.child[g.child_count].y = y;
    g.child[g.child_count].ax = ax;
    g.child[g.child_count].ay = ay;
    g.child[g.child_count].degree = degree;
    g.child[g.child_count].scale = 1;
    g.child[g.child_count].oi = O_CHILD + g.child_count;
    vgs_sprite(g.child[g.child_count].oi, TRUE, x >> 8, y >> 8, 1, 0, P_PLAYER);
    OAM[g.child[g.child_count].oi].scale = 1;
    g.child_count++;
}

void move_player(void)
{
    // Turn left or right
    if (VGS_KEY_LEFT) {
        g.player.degree -= 5;
        OAM[O_PLAYER].rotate = g.player.degree;
    } else if (VGS_KEY_RIGHT) {
        g.player.degree += 5;
        OAM[O_PLAYER].rotate = g.player.degree;
    }
    g.player.degree %= 360;
    if (g.player.degree < 0) {
        g.player.degree += 360;
    }

    // Set max speed
    if (VGS_KEY_A) {
        g.player.max_speed = 300;
    } else if (VGS_KEY_B) {
        g.player.max_speed = 80;
    } else {
        g.player.max_speed = 160;
    }

    // Update current speed
    g.player.speed += g.player.speed < g.player.max_speed ? 4 : -2;

    // Move player
    int32_t vx = vgs_cos(g.player.degree);
    vx *= g.player.speed;
    vx /= 100;
    g.player.x += vx;
    int32_t vy = vgs_sin(g.player.degree);
    vy *= g.player.speed;
    vy /= 100;
    g.player.y += vy;

    int32_t ox = g.player.x >> 8;
    int32_t oy = g.player.y >> 8;
    OAM[O_PLAYER].x = ox;
    OAM[O_PLAYER].y = oy;

    // Collision check
    if (!g.player.collision) {
        g.player.collision = ox < 4 || oy < 4 || VRAM_WIDTH - 20 < ox || VRAM_HEIGHT - 20 < oy;
    }

    if (0 == g.child_count) {
        add_child(g.player.x, g.player.y, &g.player.x, &g.player.y, g.player.degree);
    }
}

void move_child(Child* c)
{
    int32_t td = vgs_degree(c->x, c->y, *c->ax, *c->ay);
    if (c->degree != td) {
        if (c->degree < td && td - c->degree < 180) {
            c->degree += 2;
        } else {
            c->degree -= 2;
        }
        c->degree %= 360;
        if (c->degree < 0) {
            c->degree += 360;
        }
        OAM[c->oi].rotate = c->degree;
    }
    if (1 < c->exist) {
        c->exist--;
        return;
    }
    // Move player
    int32_t vx = vgs_cos(c->degree);
    vx *= g.player.speed;
    vx /= 100;
    c->x += vx;
    int32_t vy = vgs_sin(c->degree);
    vy *= g.player.speed;
    vy /= 100;
    c->y += vy;

    int32_t ox = c->x >> 8;
    int32_t oy = c->y >> 8;
    OAM[c->oi].x = ox;
    OAM[c->oi].y = oy;

    if (c->scale) {
        c->scale++;
        if (100 == c->scale) {
            add_child(c->x, c->y, &c->x, &c->y, c->degree);
            c->scale = 0;
        }
        OAM[c->oi].scale = c->scale;
    } else {
        if (hitchk((g.player.x >> 8) + 4, (g.player.y >> 8) + 4, 8, 8, (c->x >> 8) + 4, (c->y >> 8) + 4, 8, 8)) {
            g.player.collision = TRUE;
        }
    }
}

BOOL game_main(void)
{
    vgs_vsync();

    // Game Over
    if (g.player.collision) {
        if (!g.gameover) {
            vgs_print_bg(2, 15, 11, 0, "GAME  OVER");
            vgs_print_bg(2, 11, 13, 0, "PRESS START BUTTON");
            g.gameover = TRUE;
        }
        return VGS_KEY_START ? FALSE : TRUE;
    }

    move_player();
    for (int i = 0; i < g.child_count; i++) {
        move_child(&g.child[i]);
    }
    return TRUE;
}

int main(int argc, char* argv[])
{
    BOOL first = TRUE;
    while (TRUE) {
        game_init();
        if (first) {
            vgs_print_bg(2, 10, 11, 0, "INFINITE GENERATIONS");
            vgs_print_bg(2, 11, 13, 0, "PRESS START BUTTON");
            while (!VGS_KEY_START) {
                vgs_vsync();
            }
            vgs_cls_bg(2, 0);
            first = FALSE;
        }
        OAM[O_PLAYER].visible = TRUE;
        while (game_main()) {
            char score[11];
            vgs_u32str(score, g.child_count);
            vgs_print_bg(2, 1, 1, 0, score);
        }
    }
    return 0;
}
