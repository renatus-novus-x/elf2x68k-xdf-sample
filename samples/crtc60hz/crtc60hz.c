#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <x68k/iocs.h>


/*
 * X68000 CRTC 60 Hz V-DISP measurement.
 *
 * Starts from IOCS mode 12, changes only the vertical timing,
 * and measures 600 frames with IOCS _ONTIME.
 */

#define CRTC_R04       ((void *)0x00E80008UL)
#define CRTC_R05       ((void *)0x00E8000AUL)
#define CRTC_R06       ((void *)0x00E8000CUL)
#define CRTC_R07       ((void *)0x00E8000EUL)

#define MFP_GPIP       ((const void *)0x00E88001UL)

/* MFP GPIP bit 4 is V-DISP. Bit 6 (0x40) is CIRQ. */
#define GPIP_VDISP     0x10

#define SCREEN_WIDTH   512
#define SCREEN_HEIGHT  512

#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF

#define ESC_SCANCODE   0x01

#define MEASURE_FRAMES 600

/* Update FPS every 0.2 seconds and the flow line every two frames. */
#define FPS_UPDATE_INTERVAL_CS        20
#define FLOW_UPDATE_EVERY_FRAMES      2
#define FLOW_SWEEP_STEPS              30
#define FLOW_LINE_HEIGHT               1

/* _ONTIME sec uses centiseconds and wraps once per day. */
#define CENTISEC_PER_DAY 8640000L

#define WAIT_VDISP_TIMEOUT_CS 100


static uint16_t saved_r04;
static uint16_t saved_r05;
static uint16_t saved_r06;
static uint16_t saved_r07;


static long ontime_diff_cs(
  struct iocs_time start,
  struct iocs_time end);


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


/* Wait for one complete V-DISP edge sequence. */
static int wait_vdisp_with_timeout(long timeout_cs)
{
  struct iocs_time start;
  struct iocs_time now;
  uint8_t gpip;

  start = _iocs_ontime();
  gpip = read_gpip();

  while ((gpip & GPIP_VDISP) != 0) {
    gpip = read_gpip();
    now = _iocs_ontime();

    if (ontime_diff_cs(start, now) > timeout_cs) {
      return -1;
    }
  }

  while ((gpip & GPIP_VDISP) == 0) {
    gpip = read_gpip();
    now = _iocs_ontime();

    if (ontime_diff_cs(start, now) > timeout_cs) {
      return -1;
    }
  }

  return 0;
}


static int wait_vdisp(void)
{
  return wait_vdisp_with_timeout(WAIT_VDISP_TIMEOUT_CS);
}


static void save_crtc_vertical(void)
{
  saved_r04 = read_crtc(CRTC_R04);
  saved_r05 = read_crtc(CRTC_R05);
  saved_r06 = read_crtc(CRTC_R06);
  saved_r07 = read_crtc(CRTC_R07);
}


/*
 * Set 525 total lines with 480 visible lines.
 * 31.5 kHz / 525 lines is approximately 60 Hz.
 */
static int set_60hz(void)
{
  if (wait_vdisp() != 0) {
    return -1;
  }

  /* Set display positions before reducing the total line count. */
  write_crtc(CRTC_R05, 0x0001);
  write_crtc(CRTC_R06, 0x0022);
  write_crtc(CRTC_R07, 0x0202);

  /* R04 is total lines minus one: 525 - 1 = 0x020c. */
  write_crtc(CRTC_R04, 0x020C);

  return 0;
}


static void restore_crtc_now(void)
{
  /* Restore the total period before the display positions. */
  write_crtc(CRTC_R04, saved_r04);
  write_crtc(CRTC_R05, saved_r05);
  write_crtc(CRTC_R06, saved_r06);
  write_crtc(CRTC_R07, saved_r07);
}


static int is_reasonable_mode(int mode)
{
  return (mode >= 0) && (mode <= 0x7f);
}


static int restore_mode_and_crtc(int old_mode)
{
  int mode;

  if (!is_reasonable_mode(old_mode)) {
    mode = 12;
  } else {
    mode = old_mode;
  }

  _iocs_crtmod(mode);

  if (mode == 12) {
    restore_crtc_now();
  }

  _iocs_g_clr_on();

  return 0;
}


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


static void put_line(int row, const char *text)
{
  char line[61];
  size_t len;

  memset(line, ' ', 60);
  line[60] = '\0';

  len = strlen(text);

  if (len > 60) {
    len = 60;
  }

  memcpy(line, text, len);

  _iocs_b_putmes(
    3,
    2,
    row,
    59,
    line);
}


static void hide_console_cursor(void)
{
  _iocs_b_curoff();
  _iocs_os_curof();
}


static void restore_console_cursor(void)
{
  _iocs_os_curon();
  _iocs_b_curon();
}


static int escape_pressed(void)
{
  int key;
  int scancode;

  if (_iocs_b_keysns() == 0) {
    return 0;
  }

  key = _iocs_b_keyinp();
  scancode = (key >> 8) & 0xFF;

  return scancode == ESC_SCANCODE;
}


static long ontime_diff_cs(
  struct iocs_time start,
  struct iocs_time end)
{
  long day_diff;
  long diff;

  day_diff = (long)end.day - (long)start.day;
  diff = day_diff * CENTISEC_PER_DAY
    + (long)end.sec
    - (long)start.sec;

  return diff;
}


/* Return the measured frequency multiplied by 100. */
static long calculate_hz_x100(long elapsed_cs)
{
  if (elapsed_cs <= 0) {
    return 0;
  }

  return ((long)MEASURE_FRAMES * 10000L + elapsed_cs / 2)
    / elapsed_cs;
}


static void show_result(long elapsed_cs)
{
  char buf[64];
  long sec;
  long cs;
  long hz_x100;

  sec = elapsed_cs / 100;
  cs = elapsed_cs % 100;
  hz_x100 = calculate_hz_x100(elapsed_cs);

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

  put_line(5, "Target     : 60.00 Hz");
  put_line(7, "ESC : exit");
}


int main(void)
{
  int old_mode;
  int frame;
  int aborted;

  struct iocs_time start_time;
  struct iocs_time end_time;
  struct iocs_time fps_time;
  struct iocs_time now_time;

  int fps_count;
  long fps_elapsed_cs;
  int flow_prev_y;
  int flow_step;
  long last_fps_x100;

  long elapsed_cs;
  long hz_x100;

  char buf[64];

  old_mode = _iocs_crtmod(-1);
  hide_console_cursor();

  /* Start from the standard 512 x 512, 65536-color, 31 kHz mode. */
  _iocs_crtmod(12);
  _iocs_g_clr_on();

  save_crtc_vertical();

  aborted = 0;

  if (set_60hz() != 0) {
    aborted = 1;
    put_line(3, "V-DISP timeout at 60Hz setup");
  }

  put_line(1, "X68000 60 Hz V-DISP measurement");
  put_line(3, "Measuring 600 frames...");
  put_line(4, "Progress   : 0 / 600");
  put_line(5, "FPS   : --");
  put_line(7, "ESC : abort");

  fps_count = 0;
  flow_step = 0;
  flow_prev_y = -1;
  last_fps_x100 = -1;
  elapsed_cs = 0;

  /* Start timing immediately after a V-DISP boundary. */
  if (!aborted) {
    if (wait_vdisp() != 0) {
      aborted = 1;
      put_line(3, "V-DISP timeout at start");
    } else {
      start_time = _iocs_ontime();
      fps_time = start_time;
    }
  }

  for (frame = 1;
       (frame <= MEASURE_FRAMES) && (aborted == 0);
       ++frame) {
    if (wait_vdisp() != 0) {
      aborted = 1;
      put_line(7, "V-DISP timeout during measurement");
      break;
    }

    /* Check ESC before drawing the next frame. */
    if (escape_pressed()) {
      aborted = 1;
      break;
    }

    /* Capture frame 600 before doing its drawing work. */
    if (frame == MEASURE_FRAMES) {
      end_time = _iocs_ontime();
    }

    if ((frame % FLOW_UPDATE_EVERY_FRAMES) == 0) {
      int flow_phase;
      int flow_y;

      ++flow_step;
      flow_phase = flow_step % FLOW_SWEEP_STEPS;
      flow_y = (flow_phase * SCREEN_HEIGHT) / FLOW_SWEEP_STEPS;

      if (flow_prev_y >= 0 && flow_prev_y != flow_y) {
        fill_rect(
          0,
          flow_prev_y,
          SCREEN_WIDTH,
          FLOW_LINE_HEIGHT,
          COLOR_BLACK);
      }

      fill_rect(
        0,
        flow_y,
        SCREEN_WIDTH,
        FLOW_LINE_HEIGHT,
        COLOR_WHITE);

      flow_prev_y = flow_y;
    }

    now_time = _iocs_ontime();
    ++fps_count;
    fps_elapsed_cs = ontime_diff_cs(fps_time, now_time);

    if (fps_elapsed_cs >= FPS_UPDATE_INTERVAL_CS) {
      long fps_x100;

      fps_x100 =
        ((long)fps_count * 10000L + fps_elapsed_cs / 2L)
        / fps_elapsed_cs;

      if (fps_x100 != last_fps_x100) {
        sprintf(
          buf,
          "FPS : %ld.%02ld",
          fps_x100 / 100,
          fps_x100 % 100);
        put_line(5, buf);
        last_fps_x100 = fps_x100;
      }

      fps_time = now_time;
      fps_count = 0;
    }

    if ((frame % 60) == 0) {
      sprintf(buf, "Progress   : %d / 600", frame);
      put_line(4, buf);
    }
  }

  /* Remove the animation before displaying the result. */
  if (flow_prev_y >= 0) {
    fill_rect(
      0,
      flow_prev_y,
      SCREEN_WIDTH,
      FLOW_LINE_HEIGHT,
      COLOR_BLACK);

    flow_prev_y = -1;
    (void)wait_vdisp();
  }

  if (!aborted) {
    elapsed_cs = ontime_diff_cs(start_time, end_time);
    show_result(elapsed_cs);

    while (1) {
      if (wait_vdisp() != 0) {
        break;
      }

      if (escape_pressed()) {
        break;
      }
    }
  }

  /* Let emulators display the cleared frame before restoring CRTC. */
  if (flow_prev_y >= 0) {
    fill_rect(
      0,
      flow_prev_y,
      SCREEN_WIDTH,
      FLOW_LINE_HEIGHT,
      COLOR_BLACK);
  }

  _iocs_g_clr_on();
  (void)wait_vdisp();

  restore_mode_and_crtc(old_mode);
  restore_console_cursor();

  if (!aborted) {
    hz_x100 = calculate_hz_x100(elapsed_cs);

    printf(
      "600 V-DISP = %ld.%02ld sec\n",
      elapsed_cs / 100,
      elapsed_cs % 100);

    printf(
      "Measured refresh = %ld.%02ld Hz\n",
      hz_x100 / 100,
      hz_x100 % 100);
  } else {
    printf("Measurement aborted.\n");
  }

  return 0;
}
