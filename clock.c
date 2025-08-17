#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 600
#define CLOCK_RADIUS 250
#define CENTER_X (WINDOW_WIDTH / 2)
#define CENTER_Y (WINDOW_HEIGHT / 2)

typedef struct {
  SDL_Window *window;
  SDL_Renderer *renderer;
  float scale_factor;
  int running;
} Clock;

void draw_line(SDL_Renderer *renderer, int x1, int y1, int x2, int y2) {
  SDL_RenderLine(renderer, x1, y1, x2, y2);
}

void draw_circle_outline(SDL_Renderer *renderer, int center_x, int center_y,
                         int radius) {
  int x = 0;
  int y = radius;
  int d = 3 - 1 * radius;

  while (y >= x) {
    SDL_RenderPoint(renderer, center_x + x, center_y + y);
    SDL_RenderPoint(renderer, center_x - x, center_y + y);
    SDL_RenderPoint(renderer, center_x + x, center_y - y);
    SDL_RenderPoint(renderer, center_x - x, center_y - y);
    SDL_RenderPoint(renderer, center_x + y, center_y + x);
    SDL_RenderPoint(renderer, center_x - y, center_y + x);
    SDL_RenderPoint(renderer, center_x + y, center_y - x);
    SDL_RenderPoint(renderer, center_x - y, center_y - x);

    if (d < 0) {
      d = d + 4 * x + 6;
    } else {
      d = d + 4 * (x - y) + 10;
      y--;
    }
    x++;
  }
}

void draw_hand(SDL_Renderer *renderer, int center_x, int center_y, double angle,
               int length, int thickness) {
  double radians = angle * M_PI / 180.0;
  int end_x = center_x + (int)(length * sin(radians));
  int end_y = center_y - (int)(length * cos(radians));

  for (int i = -thickness / 2; i <= thickness / 2; i++) {
    for (int j = -thickness / 2; j <= thickness / 2; j++) {
      draw_line(renderer, center_x + i, center_y + j, end_x + i, end_y + j);
    }
  }
}

void draw_hour_markers(SDL_Renderer *renderer, int center_x, int center_y,
                       int radius) {
  int marker_length = radius / 12;
  int marker_thickness = radius / 80;

  for (int hour = 0; hour < 12; hour++) {
    double angle = hour * 30.0 * M_PI / 180.0;
    int outer_x = center_x + (int)(radius * sin(angle));
    int outer_y = center_y - (int)(radius * cos(angle));
    int inner_x = center_x + (int)((radius - marker_length) * sin(angle));
    int inner_y = center_y - (int)((radius - marker_length) * cos(angle));

    for (int i = 0; i < marker_thickness; i++) {
      draw_line(renderer, inner_x, inner_y + i, outer_x, outer_y + i);
      draw_line(renderer, inner_x + i, inner_y, outer_x + i, outer_y);
    }
  }
}

void get_current_time(int *hours, int *minutes, int *seconds) {
  time_t raw_time;
  struct tm *time_info;

  time(&raw_time);
  time_info = localtime(&raw_time);

  *hours = time_info->tm_hour % 12;
  *minutes = time_info->tm_min;
  *seconds = time_info->tm_sec;
}

void render_clock(Clock *clock) {
  SDL_SetRenderDrawColor(clock->renderer, 0, 0, 0, 255);
  SDL_RenderClear(clock->renderer);

  SDL_SetRenderDrawColor(clock->renderer, 255, 255, 255, 255);

  int scaled_center_x = (int)(CENTER_X * clock->scale_factor);
  int scaled_center_y = (int)(CENTER_Y * clock->scale_factor);
  int scaled_radius = (int)(CLOCK_RADIUS * clock->scale_factor);

  draw_circle_outline(clock->renderer, scaled_center_x, scaled_center_y,
                      scaled_radius);
  draw_hour_markers(clock->renderer, scaled_center_x, scaled_center_y,
                    scaled_radius);

  int hours, minutes, seconds;
  get_current_time(&hours, &minutes, &seconds);

  double hour_angle = (hours * 30.0) + (minutes * 0.5);
  double minute_angle = minutes * 6.0;
  double second_angle = seconds * 6.0;

  draw_hand(clock->renderer, scaled_center_x, scaled_center_y, hour_angle,
            (int)(120 * clock->scale_factor), (int)(6 * clock->scale_factor));
  draw_hand(clock->renderer, scaled_center_x, scaled_center_y, minute_angle,
            (int)(180 * clock->scale_factor), (int)(4 * clock->scale_factor));

  SDL_SetRenderDrawColor(clock->renderer, 255, 0, 0, 255);
  draw_hand(clock->renderer, scaled_center_x, scaled_center_y, second_angle,
            (int)(200 * clock->scale_factor), (int)(2 * clock->scale_factor));

  SDL_SetRenderDrawColor(clock->renderer, 255, 255, 255, 255);
  draw_circle_outline(clock->renderer, scaled_center_x, scaled_center_y,
                      (int)(8 * clock->scale_factor));

  SDL_RenderPresent(clock->renderer);
}

int init_clock(Clock *clock) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    printf("SDL initialization failed: %s\n", SDL_GetError());
    return 0;
  }

  clock->window =
      SDL_CreateWindow("Analogue Clock", WINDOW_WIDTH, WINDOW_HEIGHT,
                       SDL_WINDOW_HIGH_PIXEL_DENSITY);

  if (!clock->window) {
    printf("Window creation failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 0;
  }

  clock->renderer = SDL_CreateRenderer(clock->window, NULL);

  if (!clock->renderer) {
    printf("Renderer creation failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(clock->window);
    SDL_Quit();
    return 0;
  }

  clock->scale_factor = SDL_GetWindowPixelDensity(clock->window);
  clock->running = 1;
  return 1;
}

void cleanup_clock(Clock *clock) {
  SDL_DestroyRenderer(clock->renderer);
  SDL_DestroyWindow(clock->window);
  SDL_Quit();
}

void handle_events(Clock *clock) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_EVENT_QUIT) {
      clock->running = 0;
    }
    if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
      clock->running = 0;
    }
  }
}

int main(void) {
  Clock clock = {0};

  if (!init_clock(&clock)) {
    return 1;
  }

  while (clock.running) {
    handle_events(&clock);
    render_clock(&clock);
    SDL_Delay(16);
  }

  cleanup_clock(&clock);
  return 0;
}
