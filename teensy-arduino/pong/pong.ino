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
#include <OctoWS2811.h>
#include <SparkFun_MMA8452Q.h>

#define SCR_W 60
#define SCR_H 30
#define SCR_H_MEM 32
#define LED_NUM(x, y) (((y) * SCR_W) + (x))

#define SCORE_MAX 5

#define RGB(r, g, b) (((r) << 16) | ((g) << 8) | (b))
#define COL_BORDER RGB(255, 255, 255)
#define COL_BALL RGB(255, 0, 255)
#define COL_LBAT RGB(255, 0, 0)
#define COL_RBAT RGB(0, 255, 0)

// FIXME: Needs calibration
#define ACCEL_DEADSPOT 10

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
struct bat lbat;
struct bat rbat;
struct ball ball;
bool last_score_l;
const uint32_t leds_per_strip = SCR_W * ((SCR_H_MEM + 8 - 1) / 8);
DMAMEM int leds_mem[leds_per_strip * 6];
OctoWS2811 leds(leds_per_strip, leds_mem, NULL, WS2811_GRB | WS2811_800kHz);
MMA8452Q accel_l(0x1d);
MMA8452Q accel_r(0x1c);

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

    // Background
    // HACK: We should call leds.clear() but it doesn't exist
    memset(leds_mem, 0, sizeof(leds_mem));

    // Border
    for (int x = 0; x < SCR_W; x++) {
        leds.setPixel(LED_NUM(x, 1), COL_BORDER);
        leds.setPixel(LED_NUM(x, SCR_H - 1), COL_BORDER);
    }
    for (int y = 1; y < SCR_H; y++) {
        leds.setPixel(LED_NUM(0, y), COL_BORDER);
        leds.setPixel(LED_NUM((SCR_W / 2) - 1, y), COL_BORDER);
        leds.setPixel(LED_NUM(SCR_W - 2, y), COL_BORDER);
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
        leds.setPixel(LED_NUM(1, y), c);

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
        leds.setPixel(LED_NUM(x + 1, 0), c);

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
        leds.setPixel(LED_NUM(SCR_W - 3, y), c);

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
        leds.setPixel(LED_NUM(SCR_W - (3 + x), 0), c);

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
    leds.setPixel(LED_NUM(ball.xp, ball.yp), c);
}

void keys_accel_init(void) {
    accel_l.read();
    if (accel_l.x < -ACCEL_DEADSPOT)
        lbat.ypd = -1;
    else if (accel_l.x > ACCEL_DEADSPOT)
        lbat.ypd = 1;
    accel_r.read();
    if (accel_r.x < -ACCEL_DEADSPOT)
        rbat.ypd = -1;
    else if (accel_r.x > ACCEL_DEADSPOT)
        rbat.ypd = 1;
}

void keys_accel_read(void) {
    accel_l.read();
    accel_r.read();
}

void leds_init(void) {
    leds.begin();
    leds.show();
}

void leds_display_screen(void) {
    leds.show();
}

void setup(void) {
    leds_init();
    keys_accel_init();
    init_game();
}

void loop(void) {
    keys_accel_read();
    iter();
    render();
    leds_display_screen();
    delay(50);
}
