#include <OpenGL/gl3.h>
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 600
#define CLOCK_RADIUS 250
#define CENTER_X (WINDOW_WIDTH / 2)
#define CENTER_Y (WINDOW_HEIGHT / 2)

typedef struct {
  float x, y;
} Vertex;

typedef struct {
  SDL_Window *window;
  SDL_GLContext gl_context;
  float scale_factor;
  int running;
  GLuint shader_program;
  GLuint vao, vbo;
  Vertex *circle_vertices;
  int circle_vertex_count;
} Clock;

const char *vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "uniform mat4 projection;\n"
    "void main() {\n"
    "    gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
    "}\0";

const char *fragment_shader_source = "#version 330 core\n"
                                     "out vec4 FragColor;\n"
                                     "uniform vec3 color;\n"
                                     "void main() {\n"
                                     "    FragColor = vec4(color, 1.0f);\n"
                                     "}\0";

GLuint compile_shader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);

  GLint success;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetShaderInfoLog(shader, 512, NULL, info_log);
    printf("Shader compilation failed: %s\n", info_log);
  }

  return shader;
}

GLuint create_shader_program(void) {
  GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_shader_source);
  GLuint fragment_shader =
      compile_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

  GLuint program = glCreateProgram();
  glAttachShader(program, vertex_shader);
  glAttachShader(program, fragment_shader);
  glLinkProgram(program);

  GLint success;
  glGetProgramiv(program, GL_LINK_STATUS, &success);
  if (!success) {
    char info_log[512];
    glGetProgramInfoLog(program, 512, NULL, info_log);
    printf("Shader program linking failed: %s\n", info_log);
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  return program;
}

void create_projection_matrix(float *matrix, int width, int height) {
  // Orthographic projection matrix for 2D rendering
  memset(matrix, 0, 16 * sizeof(float));
  matrix[0] = 2.0f / width;   // Scale X
  matrix[5] = -2.0f / height; // Scale Y (flipped for screen coords)
  matrix[10] = -1.0f;         // Z scale
  matrix[12] = -1.0f;         // X offset
  matrix[13] = 1.0f;          // Y offset
  matrix[15] = 1.0f;          // W component
}

void precompute_circle(Clock *clock, int radius) {
  const int segments = 720;
  clock->circle_vertex_count = segments + 1;
  clock->circle_vertices = malloc(clock->circle_vertex_count * sizeof(Vertex));

  for (int i = 0; i <= segments; i++) {
    float angle = (float)i * 2.0f * M_PI / segments;
    clock->circle_vertices[i].x = CENTER_X + radius * cosf(angle);
    clock->circle_vertices[i].y = CENTER_Y + radius * sinf(angle);
  }
}

void draw_circle_outline(Clock *clock) {
  glBindBuffer(GL_ARRAY_BUFFER, clock->vbo);
  glBufferData(GL_ARRAY_BUFFER, clock->circle_vertex_count * sizeof(Vertex),
               clock->circle_vertices, GL_DYNAMIC_DRAW);

  glBindVertexArray(clock->vao);
  glDrawArrays(GL_LINE_STRIP, 0, clock->circle_vertex_count);
}

void draw_hand(Clock *clock, int center_x, int center_y, double angle,
               int length, int thickness) {
  double radians = angle * M_PI / 180.0;
  float end_x = center_x + length * sin(radians);
  float end_y = center_y - length * cos(radians);

  // Calculate perpendicular vector for thickness
  float perp_x = -cos(radians) * thickness * 0.5f;
  float perp_y = -sin(radians) * thickness * 0.5f;

  // Create quad vertices for thick hand
  Vertex hand_vertices[6]; // Two triangles

  // Triangle 1
  hand_vertices[0] = (Vertex){center_x - perp_x, center_y - perp_y};
  hand_vertices[1] = (Vertex){center_x + perp_x, center_y + perp_y};
  hand_vertices[2] = (Vertex){end_x + perp_x, end_y + perp_y};

  // Triangle 2
  hand_vertices[3] = (Vertex){center_x - perp_x, center_y - perp_y};
  hand_vertices[4] = (Vertex){end_x + perp_x, end_y + perp_y};
  hand_vertices[5] = (Vertex){end_x - perp_x, end_y - perp_y};

  glBindBuffer(GL_ARRAY_BUFFER, clock->vbo);
  glBufferData(GL_ARRAY_BUFFER, 6 * sizeof(Vertex), hand_vertices,
               GL_DYNAMIC_DRAW);

  glBindVertexArray(clock->vao);
  glDrawArrays(GL_TRIANGLES, 0, 6);
}

void draw_hour_markers(Clock *clock, int center_x, int center_y, int radius) {
  int marker_length = radius / 12;
  int marker_thickness = radius / 80;

  Vertex marker_vertices[12 * 6]; // 12 markers, 6 vertices each (2 triangles)
  int vertex_index = 0;

  for (int hour = 0; hour < 12; hour++) {
    double angle = hour * 30.0 * M_PI / 180.0;
    float outer_x = center_x + radius * sin(angle);
    float outer_y = center_y - radius * cos(angle);
    float inner_x = center_x + (radius - marker_length) * sin(angle);
    float inner_y = center_y - (radius - marker_length) * cos(angle);

    // Calculate perpendicular vector for thickness
    float perp_x = -cos(angle) * marker_thickness * 0.5f;
    float perp_y = -sin(angle) * marker_thickness * 0.5f;

    // Triangle 1
    marker_vertices[vertex_index++] =
        (Vertex){inner_x - perp_x, inner_y - perp_y};
    marker_vertices[vertex_index++] =
        (Vertex){inner_x + perp_x, inner_y + perp_y};
    marker_vertices[vertex_index++] =
        (Vertex){outer_x + perp_x, outer_y + perp_y};

    // Triangle 2
    marker_vertices[vertex_index++] =
        (Vertex){inner_x - perp_x, inner_y - perp_y};
    marker_vertices[vertex_index++] =
        (Vertex){outer_x + perp_x, outer_y + perp_y};
    marker_vertices[vertex_index++] =
        (Vertex){outer_x - perp_x, outer_y - perp_y};
  }

  glBindBuffer(GL_ARRAY_BUFFER, clock->vbo);
  glBufferData(GL_ARRAY_BUFFER, vertex_index * sizeof(Vertex), marker_vertices,
               GL_DYNAMIC_DRAW);

  glBindVertexArray(clock->vao);
  glDrawArrays(GL_TRIANGLES, 0, vertex_index);
}

int get_current_time(int *hours, int *minutes, int *seconds,
                     int *milliseconds) {
  struct timespec ts;

  if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
    return -1;
  }

  struct tm *time_info = localtime(&ts.tv_sec);
  if (time_info == NULL) {
    return -1;
  }

  *hours = time_info->tm_hour % 12;
  *minutes = time_info->tm_min;
  *seconds = time_info->tm_sec;
  // printf("tv_sec: %03li\n", ts.tv_sec);
  *milliseconds = (ts.tv_nsec / 1000000);

  return 0;
}

void render_clock(Clock *clock) {
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  glUseProgram(clock->shader_program);

  // Set projection matrix
  float projection[16];
  create_projection_matrix(projection, WINDOW_WIDTH * clock->scale_factor,
                           WINDOW_HEIGHT * clock->scale_factor);
  GLint proj_loc = glGetUniformLocation(clock->shader_program, "projection");
  glUniformMatrix4fv(proj_loc, 1, GL_FALSE, projection);

  int scaled_center_x = (int)(CENTER_X * clock->scale_factor);
  int scaled_center_y = (int)(CENTER_Y * clock->scale_factor);
  int scaled_radius = (int)(CLOCK_RADIUS * clock->scale_factor);

  // Update circle vertices for current scale
  // TODO - this looks like duplication
  const int segments = 720;
  for (int i = 0; i <= segments; i++) {
    float angle = (float)i * 2.0f * M_PI / segments;
    clock->circle_vertices[i].x = scaled_center_x + scaled_radius * cosf(angle);
    clock->circle_vertices[i].y = scaled_center_y + scaled_radius * sinf(angle);
  }

  // Set white color for clock outline and markers
  GLint color_loc = glGetUniformLocation(clock->shader_program, "color");
  glUniform3f(color_loc, 1.0f, 1.0f, 1.0f);

  draw_circle_outline(clock);
  draw_hour_markers(clock, scaled_center_x, scaled_center_y, scaled_radius);

  int hours, minutes, seconds, milliseconds;
  if (get_current_time(&hours, &minutes, &seconds, &milliseconds) != 0) {
    fprintf(stderr, "Error: Unable to get current time\n");
    exit(EXIT_FAILURE);
  }

  double hour_angle = (hours * 30.0) + (minutes * 0.5);
  double minute_angle = (minutes * 6.0) + (seconds * 0.1);
  double second_angle = (seconds * 6.0) + (milliseconds * 0.006);

  // Draw hour and minute hands in white
  glUniform3f(color_loc, 1.0f, 1.0f, 1.0f);
  draw_hand(clock, scaled_center_x, scaled_center_y, hour_angle,
            (int)(120 * clock->scale_factor), (int)(6 * clock->scale_factor));
  draw_hand(clock, scaled_center_x, scaled_center_y, minute_angle,
            (int)(180 * clock->scale_factor), (int)(4 * clock->scale_factor));

  // Draw second hand in red
  glUniform3f(color_loc, 1.0f, 0.0f, 0.0f);
  draw_hand(clock, scaled_center_x, scaled_center_y, second_angle,
            (int)(200 * clock->scale_factor), (int)(2 * clock->scale_factor));

  // Draw center circle in white
  glUniform3f(color_loc, 1.0f, 1.0f, 1.0f);
  const int center_segments = 32;
  Vertex center_vertices[center_segments + 1];
  int small_radius = (int)(8 * clock->scale_factor);

  for (int i = 0; i <= center_segments; i++) {
    float angle = (float)i * 2.0f * M_PI / center_segments;
    center_vertices[i].x = scaled_center_x + small_radius * cosf(angle);
    center_vertices[i].y = scaled_center_y + small_radius * sinf(angle);
  }

  glBindBuffer(GL_ARRAY_BUFFER, clock->vbo);
  glBufferData(GL_ARRAY_BUFFER, (center_segments + 1) * sizeof(Vertex),
               center_vertices, GL_DYNAMIC_DRAW);
  glBindVertexArray(clock->vao);
  glDrawArrays(GL_LINE_STRIP, 0, center_segments + 1);

  SDL_GL_SwapWindow(clock->window);
}

int init_clock(Clock *clock) {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    printf("SDL initialization failed: %s\n", SDL_GetError());
    return 0;
  }

  // Set OpenGL attributes
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  clock->window =
      SDL_CreateWindow("Analogue Clock", WINDOW_WIDTH, WINDOW_HEIGHT,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIGH_PIXEL_DENSITY);

  if (!clock->window) {
    printf("Window creation failed: %s\n", SDL_GetError());
    SDL_Quit();
    return 0;
  }

  clock->gl_context = SDL_GL_CreateContext(clock->window);
  if (!clock->gl_context) {
    printf("OpenGL context creation failed: %s\n", SDL_GetError());
    SDL_DestroyWindow(clock->window);
    SDL_Quit();
    return 0;
  }

  // Enable vsync
  SDL_GL_SetSwapInterval(1);

  clock->scale_factor = SDL_GetWindowPixelDensity(clock->window);
  clock->running = 1;

  // Create shader program
  clock->shader_program = create_shader_program();

  // Create VAO and VBO
  glGenVertexArrays(1, &clock->vao);
  glGenBuffers(1, &clock->vbo);

  glBindVertexArray(clock->vao);
  glBindBuffer(GL_ARRAY_BUFFER, clock->vbo);

  // Set vertex attribute pointers
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
  glEnableVertexAttribArray(0);

  // Set viewport
  int width, height;
  SDL_GetWindowSizeInPixels(clock->window, &width, &height);
  glViewport(0, 0, width, height);

  // Precompute circle points for main clock face
  precompute_circle(clock, (int)(CLOCK_RADIUS * clock->scale_factor));

  return 1;
}

void cleanup_clock(Clock *clock) {
  free(clock->circle_vertices);

  if (clock->vao)
    glDeleteVertexArrays(1, &clock->vao);
  if (clock->vbo)
    glDeleteBuffers(1, &clock->vbo);
  if (clock->shader_program)
    glDeleteProgram(clock->shader_program);

  if (clock->gl_context)
    SDL_GL_DestroyContext(clock->gl_context);
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
    SDL_Delay(100);
  }

  cleanup_clock(&clock);
  return 0;
}
