#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x68k/iocs.h>

/* ============================================================
 * X68000 CRTC 60 Hz test
 *
 * - Start from IOCS mode 12:
 *     512 x 512 / 65536 colors / 31 kHz
 *
 * - Change vertical timing to:
 *     525 total lines
 *     480 visible lines
 *     approximately 60 Hz
 *
 * - Wait for V-DISP once per frame
 * - Animate a simple rectangle
 * - Measure 600 V-DISP periods using IOCS _ONTIME
 *
 * ESC: abort / exit
 * ============================================================ */


/* ------------------------------------------------------------
 * X68000 hardware addresses
 * ------------------------------------------------------------ */

#define CRTC_R04       ((void *)0x00E80008UL)
#define CRTC_R05       ((void *)0x00E8000AUL)
#define CRTC_R06       ((void *)0x00E8000CUL)
#define CRTC_R07       ((void *)0x00E8000EUL)

#define MFP_GPIP       ((const void *)0x00E88001UL)

/*
 * MFP GPIP bit 4 = V-DISP
 *
 * IMPORTANT:
 *   V-DISP is 0x10, not 0x40.
 *
 *   bit 4 = V-DISP
 *   bit 6 = CIRQ
 */
#define GPIP_VDISP     0x10


/* ------------------------------------------------------------
 * Screen
 * ------------------------------------------------------------ */

#define SCREEN_WIDTH   512

#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF

#define ESC_SCANCODE   0x01


/* ------------------------------------------------------------
 * Measurement
 * ------------------------------------------------------------ */

#define MEASURE_FRAMES 600

/*
 * IOCS _ONTIME:
 *
 * sec = elapsed time within one day,
 *       in 1/100 second units.
 *
 * 24 * 60 * 60 * 100 = 8,640,000
 */
#define CENTISEC_PER_DAY 8640000L


/* ------------------------------------------------------------
 * Saved CRTC state
 * ------------------------------------------------------------ */

static uint16_t saved_r04;
static uint16_t saved_r05;
static uint16_t saved_r06;
static uint16_t saved_r07;


/* ============================================================
 * Low level access
 * ============================================================ */

static uint8_t read_gpip(void)
{
    return (uint8_t)_iocs_b_bpeek(MFP_GPIP);
}


static uint16_t read_crtc(const void *addr)
{
    return (uint16_t)_iocs_b_wpeek(addr);
}


static void write_crtc(void *addr, uint16_t value)
{
    _iocs_b_wpoke(addr, value);
}


/* ============================================================
 * V-DISP synchronization
 *
 * Wait for one complete V-DISP edge sequence.
 *
 * One return from this function corresponds to one frame.
 * ============================================================ */

static void wait_vdisp(void)
{
    /*
     * Wait until V-DISP goes low.
     */
    while ((read_gpip() & GPIP_VDISP) != 0) {
    }

    /*
     * Wait until V-DISP goes high again.
     */
    while ((read_gpip() & GPIP_VDISP) == 0) {
    }
}


/* ============================================================
 * CRTC
 * ============================================================ */

static void save_crtc_vertical(void)
{
    saved_r04 = read_crtc(CRTC_R04);
    saved_r05 = read_crtc(CRTC_R05);
    saved_r06 = read_crtc(CRTC_R06);
    saved_r07 = read_crtc(CRTC_R07);
}


/*
 * Change vertical timing to approximately 60 Hz.
 *
 * Horizontal frequency remains approximately 31.5 kHz.
 *
 * Vertical:
 *
 *   VSYNC        2 lines
 *   back porch  33 lines
 *   display    480 lines
 *   front porch 10 lines
 *               ---------
 *   total      525 lines
 *
 * 31,500 / 525 = 60 Hz
 */
static void set_60hz(void)
{
    wait_vdisp();

    /*
     * Set positions first while the old larger total
     * line count is still active.
     */
    write_crtc(CRTC_R05, 0x0001);
    write_crtc(CRTC_R06, 0x0022);
    write_crtc(CRTC_R07, 0x0202);

    /*
     * R04 = total lines - 1
     *
     * 525 - 1 = 524 = 0x020C
     */
    write_crtc(CRTC_R04, 0x020C);
}


static void restore_crtc(void)
{
    wait_vdisp();

    /*
     * Restore the total period first.
     */
    write_crtc(CRTC_R04, saved_r04);

    write_crtc(CRTC_R05, saved_r05);
    write_crtc(CRTC_R06, saved_r06);
    write_crtc(CRTC_R07, saved_r07);
}


/* ============================================================
 * Graphics
 * ============================================================ */

static void fill_rect(
    int x,
    int y,
    int width,
    int height,
    uint16_t color)
{
    struct iocs_fillptr rect;

    rect.x1 = (short)x;
    rect.y1 = (short)y;
    rect.x2 = (short)(x + width - 1);
    rect.y2 = (short)(y + height - 1);
    rect.color = color;

    _iocs_fill(&rect);
}


/* ============================================================
 * Text output
 * ============================================================ */

static void put_line(int row, const char *text)
{
    char line[61];
    size_t len;

    /*
     * Clear the complete line first.
     */
    memset(line, ' ', 60);
    line[60] = '\0';

    len = strlen(text);

    if (len > 60) {
        len = 60;
    }

    memcpy(line, text, len);

    /*
     * color = 3
     * x     = 2
     * y     = row
     * width = 60 chars
     */
    _iocs_b_putmes(
        3,
        2,
        row,
        59,
        line);
}


/* ============================================================
 * Keyboard
 * ============================================================ */

static int escape_pressed(void)
{
    int key;
    int scancode;

    /*
     * Do not call B_KEYINP unless a key is available.
     */
    if (_iocs_b_keysns() == 0) {
        return 0;
    }

    key = _iocs_b_keyinp();

    scancode = (key >> 8) & 0xFF;

    return scancode == ESC_SCANCODE;
}


/* ============================================================
 * IOCS _ONTIME helpers
 * ============================================================ */

static long ontime_diff_cs(
    struct iocs_time start,
    struct iocs_time end)
{
    long day_diff;
    long diff;

    day_diff =
        (long)end.day - (long)start.day;

    diff =
        day_diff * CENTISEC_PER_DAY
        + (long)end.sec
        - (long)start.sec;

    return diff;
}


/*
 * Return measured frequency * 100.
 *
 * Example:
 *
 *     60.00 Hz -> 6000
 *     59.94 Hz -> 5994
 */
static long calculate_hz_x100(long elapsed_cs)
{
    if (elapsed_cs <= 0) {
        return 0;
    }

    /*
     * frequency =
     *
     *     frames
     * -----------------
     * elapsed_cs / 100
     *
     *
     * Hz * 100 =
     *
     * frames * 10000
     * ----------------
     * elapsed_cs
     *
     * Add elapsed_cs / 2 for rounding.
     */
    return
        ((long)MEASURE_FRAMES * 10000L
         + elapsed_cs / 2)
        / elapsed_cs;
}


/* ============================================================
 * Measurement result
 * ============================================================ */

static void show_result(long elapsed_cs)
{
    char buf[64];

    long sec;
    long cs;

    long hz_x100;

    sec = elapsed_cs / 100;
    cs  = elapsed_cs % 100;

    hz_x100 =
        calculate_hz_x100(elapsed_cs);


    sprintf(
        buf,
        "600 V-DISP : %ld.%02ld sec",
        sec,
        cs);

    put_line(3, buf);


    sprintf(
        buf,
        "Measured   : %ld.%02ld Hz",
        hz_x100 / 100,
        hz_x100 % 100);

    put_line(4, buf);


    put_line(
        5,
        "Target     : 60.00 Hz");


    put_line(
        7,
        "ESC : exit");
}


/* ============================================================
 * Main
 * ============================================================ */

int main(void)
{
    int old_mode;

    int frame;

    int x;
    int dx;

    int phase;

    int heartbeat;

    int aborted;

    struct iocs_time start_time;
    struct iocs_time end_time;

    long elapsed_cs;
    long hz_x100;

    char buf[64];


    /* --------------------------------------------------------
     * Save current IOCS screen mode
     * -------------------------------------------------------- */

    old_mode = _iocs_crtmod(-1);


    /* --------------------------------------------------------
     * IOCS mode 12
     *
     * 512 x 512
     * 65536 colors
     * 31 kHz
     * -------------------------------------------------------- */

    _iocs_crtmod(12);

    _iocs_g_clr_on();


    /* --------------------------------------------------------
     * Save standard vertical timing
     * -------------------------------------------------------- */

    save_crtc_vertical();


    /* --------------------------------------------------------
     * Change to 525-line timing
     * -------------------------------------------------------- */

    set_60hz();


    /* --------------------------------------------------------
     * Initial screen
     * -------------------------------------------------------- */

    put_line(
        1,
        "X68000 60 Hz V-DISP measurement");

    put_line(
        3,
        "Measuring 600 frames...");

    put_line(
        4,
        "Progress   : 0 / 600");

    put_line(
        7,
        "ESC : abort");


    /* --------------------------------------------------------
     * Animation state
     * -------------------------------------------------------- */

    x = 32;
    dx = 2;

    phase = 0;

    heartbeat = 0;

    aborted = 0;

    elapsed_cs = 0;


    /*
     * Moving rectangle.
     */
    fill_rect(
        x,
        180,
        32,
        32,
        COLOR_WHITE);


    /*
     * 60-frame phase indicator.
     */
    fill_rect(
        16,
        80,
        6,
        16,
        COLOR_WHITE);


    /* --------------------------------------------------------
     * Synchronize measurement start
     *
     * First wait for a frame boundary.
     * -------------------------------------------------------- */

    wait_vdisp();


    /*
     * Read IOCS uptime immediately after the boundary.
     */
    start_time = _iocs_ontime();


    /* ========================================================
     * Measure exactly 600 frame periods
     * ======================================================== */

    for (frame = 1;
         frame <= MEASURE_FRAMES;
         ++frame) {

        /*
         * Wait for next frame.
         */
        wait_vdisp();


        /*
         * At frame #600, capture the end time
         * immediately after V-DISP.
         *
         * Do this before drawing the final frame so
         * rendering time after the edge is not included.
         */
        if (frame == MEASURE_FRAMES) {

            end_time = _iocs_ontime();
        }


        /* ----------------------------------------------------
         * Erase previous frame
         * ---------------------------------------------------- */

        fill_rect(
            x,
            180,
            32,
            32,
            COLOR_BLACK);


        fill_rect(
            16 + phase * 8,
            80,
            6,
            16,
            COLOR_BLACK);


        /* ----------------------------------------------------
         * Update moving rectangle
         * ---------------------------------------------------- */

        x += dx;

        if (x >= SCREEN_WIDTH - 32) {

            x = SCREEN_WIDTH - 32;
            dx = -2;

        } else if (x <= 0) {

            x = 0;
            dx = 2;
        }


        /* ----------------------------------------------------
         * 60-frame phase counter
         * ---------------------------------------------------- */

        ++phase;

        if (phase >= 60) {

            phase = 0;

            /*
             * Toggle once per 60 frames.
             */
            heartbeat = !heartbeat;
        }


        /* ----------------------------------------------------
         * Draw current frame
         * ---------------------------------------------------- */

        fill_rect(
            x,
            180,
            32,
            32,
            COLOR_WHITE);


        fill_rect(
            16 + phase * 8,
            80,
            6,
            16,
            COLOR_WHITE);


        /*
         * 1-second heartbeat.
         */
        fill_rect(
            240,
            300,
            32,
            32,
            heartbeat
                ? COLOR_WHITE
                : COLOR_BLACK);


        /* ----------------------------------------------------
         * Progress display
         *
         * Update only once every 60 frames.
         * ---------------------------------------------------- */

        if ((frame % 60) == 0) {

            sprintf(
                buf,
                "Progress   : %d / 600",
                frame);

            put_line(
                4,
                buf);
        }


        /* ----------------------------------------------------
         * ESC = abort
         * ---------------------------------------------------- */

        if (escape_pressed()) {

            aborted = 1;
            break;
        }
    }


    /* ========================================================
     * Result
     * ======================================================== */

    if (!aborted) {

        elapsed_cs =
            ontime_diff_cs(
                start_time,
                end_time);


        show_result(
            elapsed_cs);


        /*
         * Keep result visible until ESC.
         */
        for (;;) {

            wait_vdisp();

            if (escape_pressed()) {
                break;
            }
        }
    }


    /* ========================================================
     * Restore original video state
     * ======================================================== */

    restore_crtc();


    _iocs_crtmod(
        old_mode);


    _iocs_g_clr_on();


    /* ========================================================
     * Print result to Human68k console
     * ======================================================== */

    if (!aborted) {

        hz_x100 =
            calculate_hz_x100(
                elapsed_cs);


        printf(
            "600 V-DISP = %ld.%02ld sec\n",
            elapsed_cs / 100,
            elapsed_cs % 100);


        printf(
            "Measured refresh = %ld.%02ld Hz\n",
            hz_x100 / 100,
            hz_x100 % 100);

    } else {

        printf(
            "Measurement aborted.\n");
    }


    return 0;
}