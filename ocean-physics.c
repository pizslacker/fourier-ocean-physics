#include <SDL2/SDL.h>
#include <math.h>
#include <complex.h>
#include <stdlib.h>
#include <time.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 1024
#define N 256
#define L 200.0
#define G 9.81
#define AMPLITUDE 12000.0

#define BOX_SIZE 240.0
#define BOX_DEPTH 120.0
#define ISO_SCALE 1.5

// --- Camera State ---
float cam_yaw = M_PI / 4.0;
float cam_pitch = M_PI / 6.0;
int is_dragging = 0;
int last_mouse_x = 0;
int last_mouse_y = 0;

double complex h0[N];
double complex h_tilde[N];
double complex h_spatial[N];

void idft(double complex in[], double complex out[], int n) {
    for (int x = 0; x < n; x++) {
        out[x] = 0;
        for (int k = 0; k < n; k++) {
            out[x] += in[k] * cexp(I * 2.0 * M_PI * k * x / n);
        }
        out[x] /= n;
    }
}

void init_ocean() {
    srand(time(NULL));
    h0[0] = 0; 
    for (int i = 1; i < N; i++) {
        int k_index = (i <= N / 2) ? i : (i - N);
        double P = 1.0 / pow(fabs((double)k_index), 2.5);
        if (k_index < 0) P *= 0.05; 

        double r1 = (double)rand() / RAND_MAX + 1e-6;
        double r2 = (double)rand() / RAND_MAX;
        double u1 = sqrt(-2.0 * log(r1)) * cos(2.0 * M_PI * r2);
        double u2 = sqrt(-2.0 * log(r1)) * sin(2.0 * M_PI * r2);
        h0[i] = (u1 + I * u2) * sqrt(P) * AMPLITUDE;
    }
}

void update_ocean(double t) {
    h_tilde[0] = 0;
    for (int i = 1; i < N; i++) {
        int k_index = (i <= N / 2) ? i : (i - N);
        double omega = sqrt(G * fabs(2.0 * M_PI * k_index / L));
        int neg_i = N - i;
        h_tilde[i] = h0[i] * cexp(-I * omega * t) + conj(h0[neg_i]) * cexp(I * omega * t);
    }
}

// 3D Rotation Matrix Projection
void project_3d(float x, float y, float z, float *sx, float *sy) {
    // 1. Yaw (Rotate around Y axis)
    float rx = x * cos(cam_yaw) - z * sin(cam_yaw);
    float rz = x * sin(cam_yaw) + z * cos(cam_yaw);
    
    // 2. Pitch (Rotate around X axis)
    float ry = y * cos(cam_pitch) - rz * sin(cam_pitch);

    // 3. Orthographic projection to screen
    *sx = WINDOW_WIDTH / 2.0 + rx * ISO_SCALE;
    *sy = WINDOW_HEIGHT / 2.0 - ry * ISO_SCALE; // Screen Y goes down
}

SDL_Vertex create_vertex(float x, float y, float z, SDL_Color color) {
    float sx, sy;
    project_3d(x, y, z, &sx, &sy);
    return (SDL_Vertex){ {sx, sy}, color, {0, 0} };
}

void draw_quad(SDL_Renderer* renderer, 
               float x1, float y1, float z1, float x2, float y2, float z2,
               float x3, float y3, float z3, float x4, float y4, float z4, 
               SDL_Color c1, SDL_Color c2, SDL_Color c3, SDL_Color c4) {
    SDL_Vertex verts[4] = {
        create_vertex(x1, y1, z1, c1), create_vertex(x2, y2, z2, c2),
        create_vertex(x3, y3, z3, c3), create_vertex(x4, y4, z4, c4)
    };
    int indices[6] = {0, 1, 2, 0, 2, 3};
    SDL_RenderGeometry(renderer, NULL, verts, 4, indices, 6);
}

// Draw static flat side walls
void draw_x_wall(SDL_Renderer* renderer, float x_val, float y_top, float half_size, SDL_Color bot, SDL_Color top) {
    draw_quad(renderer, x_val, -BOX_DEPTH, -half_size, x_val, -BOX_DEPTH, half_size,
              x_val, y_top, half_size, x_val, y_top, -half_size, bot, bot, top, top);
}

// Draw dynamic wave-profile walls
void draw_z_wall(SDL_Renderer* renderer, float z_val, float half_size, SDL_Color bot, SDL_Color top) {
    for (int i = 0; i < N - 1; i++) {
        float x1 = -half_size + ((float)i / (N - 1)) * BOX_SIZE;
        float x2 = -half_size + ((float)(i + 1) / (N - 1)) * BOX_SIZE;
        float y1 = creal(h_spatial[i]);
        float y2 = creal(h_spatial[i + 1]);
        draw_quad(renderer, x1, -BOX_DEPTH, z_val, x2, -BOX_DEPTH, z_val,
                  x2, y2, z_val, x1, y1, z_val, bot, bot, top, top);
    }
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_Window* window = SDL_CreateWindow("Fourier Ocean - Drag to Rotate", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    init_ocean();
    Uint32 start_time = SDL_GetTicks();
    int running = 1;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
            
            // Mouse Drag Controls
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                is_dragging = 1;
                last_mouse_x = event.button.x;
                last_mouse_y = event.button.y;
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                is_dragging = 0;
            } else if (event.type == SDL_MOUSEMOTION && is_dragging) {
                cam_yaw += (event.motion.x - last_mouse_x) * 0.01f;
                cam_pitch += (event.motion.y - last_mouse_y) * 0.01f;
                
                // Restrict pitch so we never look completely upside down or from underneath
                if (cam_pitch < 0.1f) cam_pitch = 0.1f;
                if (cam_pitch > M_PI / 2.0f - 0.1f) cam_pitch = M_PI / 2.0f - 0.1f;
                
                last_mouse_x = event.motion.x;
                last_mouse_y = event.motion.y;
            }
        }

        update_ocean((SDL_GetTicks() - start_time) / 1000.0);
        idft(h_tilde, h_spatial, N);

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        float half_size = BOX_SIZE / 2.0;
        SDL_Color c_bot = {10, 40, 80, 255};
        SDL_Color c_side = {20, 110, 160, 255};

        // Determine which direction the walls are facing relative to the camera
        float sy = sin(cam_yaw), cy = cos(cam_yaw);

        // 1. Draw BACK walls (Faces pointing away from the camera, drawn before water)
        if (sy < 0)  draw_x_wall(renderer, half_size, creal(h_spatial[N-1]), half_size, c_bot, c_side); // Right
        if (-sy < 0) draw_x_wall(renderer, -half_size, creal(h_spatial[0]), half_size, c_bot, c_side);  // Left
        if (cy < 0)  draw_z_wall(renderer, half_size, half_size, c_bot, c_side);                        // Front
        if (-cy < 0) draw_z_wall(renderer, -half_size, half_size, c_bot, c_side);                       // Back

        // 2. Draw Top Water Surface
        for (int i = 0; i < N - 1; i++) {
            float x1 = -half_size + ((float)i / (N - 1)) * BOX_SIZE;
            float x2 = -half_size + ((float)(i + 1) / (N - 1)) * BOX_SIZE;
            float y1 = creal(h_spatial[i]);
            float y2 = creal(h_spatial[i + 1]);

            float slope = y2 - y1;
            int b = 160 - (int)(slope * 6.0);
            if (b > 255) b = 255; if (b < 60) b = 60;
            SDL_Color surf_c = {0, b, b + 50, 255};

            draw_quad(renderer, x1, y1, -half_size, x2, y2, -half_size,
                      x2, y2, half_size, x1, y1, half_size, surf_c, surf_c, surf_c, surf_c);
        }

        // 3. Draw Wireframe Grid
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 50);
        for (float z = -half_size; z <= half_size; z += BOX_SIZE / 8.0) {
            for (int i = 0; i < N - 1; i++) {
                float sx1, sy1, sx2, sy2;
                project_3d(-half_size + ((float)i / (N - 1)) * BOX_SIZE, creal(h_spatial[i]), z, &sx1, &sy1);
                project_3d(-half_size + ((float)(i + 1) / (N - 1)) * BOX_SIZE, creal(h_spatial[i + 1]), z, &sx2, &sy2);
                SDL_RenderDrawLine(renderer, sx1, sy1, sx2, sy2);
            }
        }

        // 4. Draw FRONT walls (Faces pointing towards the camera, drawn after water to cover edges)
        if (sy >= 0)  draw_x_wall(renderer, half_size, creal(h_spatial[N-1]), half_size, c_bot, c_side);
        if (-sy >= 0) draw_x_wall(renderer, -half_size, creal(h_spatial[0]), half_size, c_bot, c_side);
        if (cy >= 0)  draw_z_wall(renderer, half_size, half_size, c_bot, c_side);
        if (-cy >= 0) draw_z_wall(renderer, -half_size, half_size, c_bot, c_side);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}