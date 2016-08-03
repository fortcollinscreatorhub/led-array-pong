/*
 * Copyright (c) 2016, Stephen Warren.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name Stephen Warren, the name Fort Collins Creator Hub, nor the
 * names of this software's contributors may be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SCR_W 60
#define SCR_H 30
#define SCORE_MAX 5

#define R(col) (((col) >> 16) & 0xff)
#define G(col) (((col) >>  8) & 0xff)
#define B(col) (((col) >>  0) & 0xff)
#define RGB(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define COL_BORDER RGB(255, 255, 255)
#define COL_BALL RGB(255, 0, 255)
#define COL_LBAT RGB(255, 0, 0)
#define COL_RBAT RGB(0, 255, 0)

enum state {
    STATE_WAIT_TURN_START,
    STATE_PLAYING,
    STATE_SCORED,
    STATE_GAME_OVER,
};

struct bat {
    int score;
    int yp;
    int ypd;
    int len;
};

struct ball {
    int xp;
    int yp;
    int xpd;
    int ypd;
};

enum state state;
int frame;
uint32_t screen[SCR_H][SCR_W];
struct bat lbat;
struct bat rbat;
struct ball ball;
bool last_score_l;

void enter_state_wait_turn_start(void);
void enter_state_playing(void);
void enter_state_scored(bool l_scored);
void enter_state_game_over(void);
void init_game(void);

void enter_state(enum state new_state) {
    state = new_state;
    frame = 0;
}

void enter_state_wait_turn_start(void) {
    enter_state(STATE_WAIT_TURN_START);
    ball.xp = (SCR_W / 2) - 1;
    ball.yp = SCR_H / 2;
    ball.ypd = 1 + (-2 * (rand() & 1));
    ball.xpd = 1 + (-2 * (rand() & 1));
    lbat.len = 5;
    lbat.yp = (SCR_H / 2) - (lbat.len / 2);
    rbat.len = 5;
    rbat.yp = (SCR_H / 2) - (lbat.len / 2);
}

void iter_wait_turn_start(void) {
    // FIXME: Wait for trigger instead?
    if (frame < 35)
        return;

    enter_state_playing();
}

void enter_state_playing(void) {
    enter_state(STATE_PLAYING);
}

void iter_playing(void) {
    // Move left bat
    lbat.yp += lbat.ypd;
    if (lbat.yp < 2)
        lbat.yp = 2;
    if ((lbat.yp + lbat.len) > SCR_H - 1)
        lbat.yp = SCR_H - (1 + lbat.len);

    // Move right bat
    // FIXME: Share bat move code...
    rbat.yp += rbat.ypd;
    if (rbat.yp < 2)
        rbat.yp = 2;
    if ((rbat.yp + rbat.len) > SCR_H - 1)
        rbat.yp = SCR_H - (1 + rbat.len);

    // Check ball at left gutter or bat
    if (ball.xp == 2) {
        if ((ball.yp < lbat.yp) || (ball.yp > (lbat.yp + lbat.len - 1))) {
            rbat.score++;
            if (rbat.score == SCORE_MAX)
                enter_state_game_over();
            else
                enter_state_scored(false);
            return;
        }
        ball.xpd *= -1;
    }
    // Check ball at right gutter or bat
    if (ball.xp == SCR_W - 4) {
        if ((ball.yp < rbat.yp) || (ball.yp > (rbat.yp + rbat.len - 1))){
            lbat.score++;
            if (lbat.score == SCORE_MAX)
                enter_state_game_over();
            else
                enter_state_scored(true);
            frame = 0;
            return;
        }
        ball.xpd *= -1;
    }
    // Check ball at top/bottom
    if (ball.yp == 2)
        ball.ypd *= -1;
    if (ball.yp == SCR_H - 2)
        ball.ypd *= -1;

    // Ball movement
    ball.xp += ball.xpd;
    ball.yp += ball.ypd;
}

void enter_state_scored(bool l_scored) {
    enter_state(STATE_SCORED);
    last_score_l = l_scored;
}

void iter_scored(void) {
    if (frame < 30)
        return;

    enter_state_wait_turn_start();
    frame = 0;
}

void enter_state_game_over(void) {
    enter_state(STATE_GAME_OVER);
}

void iter_game_over(void) {
    if (frame < 100)
        return;

    init_game();
    enter_state_wait_turn_start();
}

void iter(void) {
    frame++;
    switch (state) {
    case STATE_WAIT_TURN_START:
        iter_wait_turn_start();
        break;
    case STATE_PLAYING:
        iter_playing();
        break;
    case STATE_SCORED:
        iter_scored();
        break;
    case STATE_GAME_OVER:
        iter_game_over();
        break;
    }
}

void init_game(void) {
    lbat.score = 0;
    lbat.ypd = 0;
    rbat.score = 0;
    rbat.ypd = 0;
    enter_state_wait_turn_start();
}

void render(void) {
    uint32_t c;
    bool blink_off = !((frame / 5) & 1);

    // Border
    memset(screen, 0, sizeof(screen));
    memset(&(screen[1][0]), COL_BORDER, (SCR_W - 1) * 4);
    memset(&(screen[SCR_H - 1][0]), COL_BORDER, (SCR_W - 1) * 4);
    for (int y = 1; y < SCR_H; y++) {
        screen[y][0] = COL_BORDER;
        screen[y][(SCR_W / 2) - 1] = COL_BORDER;
        screen[y][SCR_W - 2] = COL_BORDER;
    }

    // Left bat
    c = COL_LBAT;
    switch(state) {
    case STATE_SCORED:
        if (blink_off && !last_score_l)
            c = 0;
        break;
    default:
        break;
    }
    for (int y = lbat.yp; y < lbat.yp + lbat.len; y++)
        screen[y][1] = c;

    // Left score
    c = COL_LBAT;
    switch(state) {
    case STATE_SCORED:
        if (blink_off && last_score_l)
            c = 0;
        break;
    case STATE_GAME_OVER:
        if ((lbat.score == SCORE_MAX) && blink_off)
            c = 0;
        break;
    default:
        break;
    }
    for (int x = 0; x < lbat.score; x++)
        screen[0][x + 1] = c;

    // Right bat
    c = COL_RBAT;
    switch(state) {
    case STATE_SCORED:
        if (blink_off && last_score_l)
            c = 0;
        break;
    default:
        break;
    }
    for (int y = rbat.yp; y < rbat.yp + rbat.len; y++)
        screen[y][SCR_W - 3] = c;

    // Right score
    c = COL_RBAT;
    switch(state) {
    case STATE_SCORED:
        if (blink_off && !last_score_l)
            c = 0;
        break;
    case STATE_GAME_OVER:
        if ((rbat.score == SCORE_MAX) && blink_off)
            c = 0;
        break;
    default:
        break;
    }
    for (int x = 0; x < rbat.score; x++)
        screen[0][SCR_W - (3 + x)] = c;

    // Ball
    c = COL_BALL;
    switch (state) {
    case STATE_WAIT_TURN_START:
    case STATE_SCORED:
        if (blink_off)
            c = 0;
        break;
    default:
        break;
    }
    screen[ball.yp][ball.xp] = c;
}

void stdout_display_screen(void) {
    uint32_t last_col = 0x1000000;

    printf("\033c");
    for (int y = 0; y < SCR_H; y++) {
        for (int x = 0; x < SCR_W; x++) {
            uint32_t col = screen[y][x];
            if (col != last_col)
                printf("\033[38;2;%d;%d;%dm", R(col), G(col), B(col));
            putchar('*');
        }
        putchar('\n');
    }
    printf("\033[38;2;0;255;0m");
    fflush(stdout);
}

void keys_random_read(void) {
    lbat.ypd = (rand() % 3) - 1;
    rbat.ypd = (rand() % 3) - 1;
}

void keys_i2c_init(void) {
    // FIXME: Open I2C bus device
}

void keys_i2c_read(void) {
    // FIXME: Read I2C devices
}

void leds_init(void) {
    // FIXME: Connect to /dev/tty*
}

void leds_display_screen(void) {
    // FIXME: Write to serial fds.
}

int main(int argc, char **argv) {
    leds_init();
    init_game();
    for (;;) {
        keys_random_read();
        iter();
        render();
        stdout_display_screen();
        leds_display_screen();
        usleep(50 * 1000);
    }
    return 0;
}
