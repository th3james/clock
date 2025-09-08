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
  float x, y, z;
  float nx, ny, nz; // Normal vector for lighting
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
  Vertex *hand_vertices;
  int hand_vertex_count;
} Clock;

const char *vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "layout (location = 1) in vec3 aNormal;\n"
    "uniform mat4 projection;\n"
    "uniform mat4 view;\n"
    "uniform mat4 model;\n"
    "out vec3 FragPos;\n"
    "out vec3 Normal;\n"
    "void main() {\n"
    "    FragPos = vec3(model * vec4(aPos, 1.0));\n"
    "    Normal = mat3(transpose(inverse(model))) * aNormal;\n"
    "    gl_Position = projection * view * vec4(FragPos, 1.0);\n"
    "}\0";

const char *fragment_shader_source =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "in vec3 FragPos;\n"
    "in vec3 Normal;\n"
    "uniform vec3 color;\n"
    "uniform vec3 lightPos;\n"
    "uniform vec3 lightColor;\n"
    "uniform vec3 viewPos;\n"
    "void main() {\n"
    "    // Ambient\n"
    "    float ambientStrength = 0.15;\n"
    "    vec3 ambient = ambientStrength * lightColor;\n"
    "    \n"
    "    // Diffuse\n"
    "    vec3 norm = normalize(Normal);\n"
    "    vec3 lightDir = normalize(lightPos - FragPos);\n"
    "    float diff = max(dot(norm, lightDir), 0.0);\n"
    "    vec3 diffuse = diff * lightColor;\n"
    "    \n"
    "    // Specular\n"
    "    float specularStrength = 0.5;\n"
    "    vec3 viewDir = normalize(viewPos - FragPos);\n"
    "    vec3 reflectDir = reflect(-lightDir, norm);\n"
    "    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);\n"
    "    vec3 specular = specularStrength * spec * lightColor;\n"
    "    \n"
    "    vec3 result = (ambient + diffuse + specular) * color;\n"
    "    FragColor = vec4(result, 1.0);\n"
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

void create_identity_matrix(float *matrix) {
  memset(matrix, 0, 16 * sizeof(float));
  matrix[0] = matrix[5] = matrix[10] = matrix[15] = 1.0f;
}

void create_perspective_matrix(float *matrix, float fov, float aspect,
                               float near, float far) {
  memset(matrix, 0, 16 * sizeof(float));
  float f = 1.0f / tanf(fov * M_PI / 360.0f); // fov/2 in radians
  matrix[0] = f / aspect;
  matrix[5] = f;
  matrix[10] = (far + near) / (near - far);
  matrix[11] = -1.0f;
  matrix[14] = (2.0f * far * near) / (near - far);
}

void create_view_matrix(float *matrix, float eye_x, float eye_y, float eye_z,
                        float center_x, float center_y, float center_z,
                        float up_x, float up_y, float up_z) {
  // Create lookAt matrix
  float forward_x = center_x - eye_x;
  float forward_y = center_y - eye_y;
  float forward_z = center_z - eye_z;

  // Normalize forward vector
  float f_length = sqrtf(forward_x * forward_x + forward_y * forward_y +
                         forward_z * forward_z);
  forward_x /= f_length;
  forward_y /= f_length;
  forward_z /= f_length;

  // Calculate right vector (forward x up)
  float right_x = forward_y * up_z - forward_z * up_y;
  float right_y = forward_z * up_x - forward_x * up_z;
  float right_z = forward_x * up_y - forward_y * up_x;

  // Normalize right vector
  float r_length =
      sqrtf(right_x * right_x + right_y * right_y + right_z * right_z);
  right_x /= r_length;
  right_y /= r_length;
  right_z /= r_length;

  // Calculate up vector (right x forward)
  float true_up_x = right_y * forward_z - right_z * forward_y;
  float true_up_y = right_z * forward_x - right_x * forward_z;
  float true_up_z = right_x * forward_y - right_y * forward_x;

  memset(matrix, 0, 16 * sizeof(float));
  matrix[0] = right_x;
  matrix[4] = right_y;
  matrix[8] = right_z;
  matrix[1] = true_up_x;
  matrix[5] = true_up_y;
  matrix[9] = true_up_z;
  matrix[2] = -forward_x;
  matrix[6] = -forward_y;
  matrix[10] = -forward_z;
  matrix[15] = 1.0f;

  matrix[12] = -(right_x * eye_x + right_y * eye_y + right_z * eye_z);
  matrix[13] = -(true_up_x * eye_x + true_up_y * eye_y + true_up_z * eye_z);
  matrix[14] = forward_x * eye_x + forward_y * eye_y + forward_z * eye_z;
}

void create_circle_outline_3d(Vertex *vertices, int *vertex_count, float radius,
                              int segments) {
  int idx = 0;

  // Create a simple ring of triangles for the clock outline
  for (int i = 0; i < segments; i++) {
    float angle1 = (float)i * 2.0f * M_PI / segments;
    float angle2 = (float)(i + 1) * 2.0f * M_PI / segments;

    float x1 = radius * cosf(angle1);
    float y1 = radius * sinf(angle1);
    float x2 = radius * cosf(angle2);
    float y2 = radius * sinf(angle2);

    float inner_radius = radius - 10;
    float ix1 = inner_radius * cosf(angle1);
    float iy1 = inner_radius * sinf(angle1);
    float ix2 = inner_radius * cosf(angle2);
    float iy2 = inner_radius * sinf(angle2);

    // Outer triangle
    vertices[idx++] = (Vertex){x1, y1, 0, 0, 0, 1};
    vertices[idx++] = (Vertex){x2, y2, 0, 0, 0, 1};
    vertices[idx++] = (Vertex){ix1, iy1, 0, 0, 0, 1};

    // Inner triangle
    vertices[idx++] = (Vertex){ix1, iy1, 0, 0, 0, 1};
    vertices[idx++] = (Vertex){x2, y2, 0, 0, 0, 1};
    vertices[idx++] = (Vertex){ix2, iy2, 0, 0, 0, 1};
  }

  *vertex_count = idx;
}

void precompute_circle(Clock *clock, int radius) {
  const int segments = 60;
  create_circle_outline_3d(clock->circle_vertices, &clock->circle_vertex_count,
                           radius, segments);
}

void draw_3d_object(Clock *clock, Vertex *vertices, int vertex_count) {
  glBindBuffer(GL_ARRAY_BUFFER, clock->vbo);
  glBufferData(GL_ARRAY_BUFFER, vertex_count * sizeof(Vertex), vertices,
               GL_DYNAMIC_DRAW);

  glBindVertexArray(clock->vao);
  glDrawArrays(GL_TRIANGLES, 0, vertex_count);
}

void create_hand_vertices(Vertex *vertices, int *vertex_count, double angle,
                          float length, float thickness) {
  double radians = angle * M_PI / 180.0;
  float end_x = -length * sin(radians); // Negative to flip X axis
  float end_y = length * cos(radians);  // Positive for correct Y orientation

  float half_thick = thickness * 0.5f;

  // Calculate proper perpendicular vector for consistent thickness
  float perp_x = cos(radians) * half_thick; // Flipped for correct X direction
  float perp_y =
      sin(radians) * half_thick; // Always perpendicular to hand direction

  int idx = 0;

  // Create a simple 3D rectangular hand with proper normals
  // Front face (z = 2)
  vertices[idx++] = (Vertex){-perp_x, -perp_y, 2, 0, 0, 1};
  vertices[idx++] = (Vertex){perp_x, perp_y, 2, 0, 0, 1};
  vertices[idx++] = (Vertex){end_x + perp_x, end_y + perp_y, 2, 0, 0, 1};

  vertices[idx++] = (Vertex){-perp_x, -perp_y, 2, 0, 0, 1};
  vertices[idx++] = (Vertex){end_x + perp_x, end_y + perp_y, 2, 0, 0, 1};
  vertices[idx++] = (Vertex){end_x - perp_x, end_y - perp_y, 2, 0, 0, 1};

  // Back face (z = -2)
  vertices[idx++] = (Vertex){perp_x, perp_y, -2, 0, 0, -1};
  vertices[idx++] = (Vertex){-perp_x, -perp_y, -2, 0, 0, -1};
  vertices[idx++] = (Vertex){end_x - perp_x, end_y - perp_y, -2, 0, 0, -1};

  vertices[idx++] = (Vertex){perp_x, perp_y, -2, 0, 0, -1};
  vertices[idx++] = (Vertex){end_x - perp_x, end_y - perp_y, -2, 0, 0, -1};
  vertices[idx++] = (Vertex){end_x + perp_x, end_y + perp_y, -2, 0, 0, -1};

  *vertex_count = idx;
}

void draw_hand(Clock *clock, double angle, float length, float thickness) {
  create_hand_vertices(clock->hand_vertices, &clock->hand_vertex_count, angle,
                       length, thickness);
  draw_3d_object(clock, clock->hand_vertices, clock->hand_vertex_count);
}

void create_marker_vertices(Vertex *vertices, int *vertex_count, int radius) {
  int marker_length = radius / 12;
  float marker_thickness = radius / 80.0f;
  int idx = 0;

  for (int hour = 0; hour < 12; hour++) {
    double angle = hour * 30.0 * M_PI / 180.0;
    float outer_radius = radius;
    float inner_radius = radius - marker_length;

    float cos_a = cosf(angle), sin_a = sinf(angle);
    float outer_x = -outer_radius * sin_a; // Negative to flip X axis
    float outer_y = outer_radius * cos_a;  // Positive for correct Y orientation
    float inner_x = -inner_radius * sin_a; // Negative to flip X axis
    float inner_y = inner_radius * cos_a;  // Positive for correct Y orientation

    // Calculate perpendicular vector for thickness
    float perp_x = -cos_a * marker_thickness * 0.5f;
    float perp_y = -sin_a * marker_thickness * 0.5f;

    // Front face
    vertices[idx++] = (Vertex){inner_x - perp_x, inner_y - perp_y, 3, 0, 0, 1};
    vertices[idx++] = (Vertex){inner_x + perp_x, inner_y + perp_y, 3, 0, 0, 1};
    vertices[idx++] = (Vertex){outer_x + perp_x, outer_y + perp_y, 3, 0, 0, 1};

    vertices[idx++] = (Vertex){inner_x - perp_x, inner_y - perp_y, 3, 0, 0, 1};
    vertices[idx++] = (Vertex){outer_x + perp_x, outer_y + perp_y, 3, 0, 0, 1};
    vertices[idx++] = (Vertex){outer_x - perp_x, outer_y - perp_y, 3, 0, 0, 1};

    // Back face
    vertices[idx++] =
        (Vertex){inner_x + perp_x, inner_y + perp_y, -3, 0, 0, -1};
    vertices[idx++] =
        (Vertex){inner_x - perp_x, inner_y - perp_y, -3, 0, 0, -1};
    vertices[idx++] =
        (Vertex){outer_x - perp_x, outer_y - perp_y, -3, 0, 0, -1};

    vertices[idx++] =
        (Vertex){inner_x + perp_x, inner_y + perp_y, -3, 0, 0, -1};
    vertices[idx++] =
        (Vertex){outer_x - perp_x, outer_y - perp_y, -3, 0, 0, -1};
    vertices[idx++] =
        (Vertex){outer_x + perp_x, outer_y + perp_y, -3, 0, 0, -1};
  }

  *vertex_count = idx;
}

void draw_hour_markers(Clock *clock, int radius) {
  Vertex marker_vertices[12 * 12]; // 12 markers, 12 triangles per marker
  int vertex_count;
  create_marker_vertices(marker_vertices, &vertex_count, radius);
  draw_3d_object(clock, marker_vertices, vertex_count);
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
  glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glUseProgram(clock->shader_program);

  // Set up 3D matrices
  float projection[16], view[16], model[16];

  // Perspective projection - extended far plane for greater camera distance
  create_perspective_matrix(projection, 45.0f,
                            (float)WINDOW_WIDTH / WINDOW_HEIGHT, 1.0f, 2000.0f);
  GLint proj_loc = glGetUniformLocation(clock->shader_program, "projection");
  glUniformMatrix4fv(proj_loc, 1, GL_FALSE, projection);

  // View matrix (camera position) - moved camera back for full clock view
  create_view_matrix(view, 0, 0, 1300, 0, 0, 0, 0, 1,
                     0); // Moved camera much further back
  GLint view_loc = glGetUniformLocation(clock->shader_program, "view");
  glUniformMatrix4fv(view_loc, 1, GL_FALSE, view);

  // Model matrix (identity for now)
  create_identity_matrix(model);
  GLint model_loc = glGetUniformLocation(clock->shader_program, "model");
  glUniformMatrix4fv(model_loc, 1, GL_FALSE, model);

  // Set up lighting
  GLint light_pos_loc = glGetUniformLocation(clock->shader_program, "lightPos");
  GLint light_color_loc =
      glGetUniformLocation(clock->shader_program, "lightColor");
  GLint view_pos_loc = glGetUniformLocation(clock->shader_program, "viewPos");

  glUniform3f(light_pos_loc, 200.0f, 200.0f, 300.0f);
  glUniform3f(light_color_loc, 1.0f, 1.0f, 1.0f);
  glUniform3f(view_pos_loc, 0.0f, 0.0f,
              1300.0f); // Updated to match camera position

  int scaled_radius = (int)(CLOCK_RADIUS * clock->scale_factor);

  // Set color for clock face and markers
  GLint color_loc = glGetUniformLocation(clock->shader_program, "color");
  glUniform3f(color_loc, 0.8f, 0.8f, 0.9f);

  // Draw clock face (3D cylinder)
  draw_3d_object(clock, clock->circle_vertices, clock->circle_vertex_count);

  // Draw hour markers
  glUniform3f(color_loc, 1.0f, 1.0f, 1.0f);
  draw_hour_markers(clock, scaled_radius);

  int hours, minutes, seconds, milliseconds;
  if (get_current_time(&hours, &minutes, &seconds, &milliseconds) != 0) {
    fprintf(stderr, "Error: Unable to get current time\n");
    exit(EXIT_FAILURE);
  }

  // Calculate angles for clockwise rotation from 12 o'clock position
  double hour_angle = -((hours * 30.0) + (minutes * 0.5));
  double minute_angle = -((minutes * 6.0) + (seconds * 0.1));
  double second_angle = -((seconds * 6.0) + (milliseconds * 0.006));

  // Draw hour hand in dark gray
  glUniform3f(color_loc, 0.3f, 0.3f, 0.3f);
  draw_hand(clock, hour_angle, 120 * clock->scale_factor,
            8 * clock->scale_factor);

  // Draw minute hand in dark gray
  glUniform3f(color_loc, 0.3f, 0.3f, 0.3f);
  draw_hand(clock, minute_angle, 180 * clock->scale_factor,
            6 * clock->scale_factor);

  // Draw second hand in red
  glUniform3f(color_loc, 1.0f, 0.1f, 0.1f);
  draw_hand(clock, second_angle, 200 * clock->scale_factor,
            3 * clock->scale_factor);

  // Draw center circle
  glUniform3f(color_loc, 0.9f, 0.9f, 0.9f);
  Vertex center_vertices[120];
  int center_vertex_count;
  create_circle_outline_3d(center_vertices, &center_vertex_count,
                           12 * clock->scale_factor, 20);
  draw_3d_object(clock, center_vertices, center_vertex_count);

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

  // Enable depth testing for 3D
  glEnable(GL_DEPTH_TEST);

  // Create VAO and VBO
  glGenVertexArrays(1, &clock->vao);
  glGenBuffers(1, &clock->vbo);

  glBindVertexArray(clock->vao);
  glBindBuffer(GL_ARRAY_BUFFER, clock->vbo);

  // Set vertex attribute pointers for 3D position and normal
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);

  // Set viewport
  int width, height;
  SDL_GetWindowSizeInPixels(clock->window, &width, &height);
  glViewport(0, 0, width, height);

  // Precompute 3D geometry for main clock face
  clock->circle_vertices =
      malloc(1000 * sizeof(Vertex)); // Allocate enough for circle outline
  precompute_circle(clock, (int)(CLOCK_RADIUS * clock->scale_factor));

  // Allocate memory for hand vertices
  clock->hand_vertices = malloc(100 * sizeof(Vertex));

  return 1;
}

void cleanup_clock(Clock *clock) {
  free(clock->circle_vertices);
  free(clock->hand_vertices);

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
