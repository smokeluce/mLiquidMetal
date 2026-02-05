#include "raylib.h"
#include "raymath.h"
#include <vector>
#include <cmath>

struct LiquidSim {
    int width;
    int height;
    float stiffness;
    float damping;
    std::vector<float> heightField;
    std::vector<float> velocityField;

    LiquidSim(int w, int h)
        : width(w), height(h),
          stiffness(0.2f), damping(0.985f),
          heightField(w * h, 0.0f),
          velocityField(w * h, 0.0f) {}

    int idx(int x, int y) const { return y * width + x; }

    void AddImpulse(int x, int y, float amount, int radius = 3) {
        for (int j = -radius; j <= radius; ++j) {
            for (int i = -radius; i <= radius; ++i) {
                int nx = x + i;
                int ny = y + j;
                if (nx > 0 && nx < width - 1 && ny > 0 && ny < height - 1) {
                    float dist2 = float(i * i + j * j);
                    float falloff = std::exp(-dist2 * 0.5f);
                    heightField[idx(nx, ny)] += amount * falloff;
                }
            }
        }
    }

    void Step() {
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                float center = heightField[idx(x, y)];
                float sumNeighbors =
                    heightField[idx(x - 1, y)] +
                    heightField[idx(x + 1, y)] +
                    heightField[idx(x, y - 1)] +
                    heightField[idx(x, y + 1)];

                float force = (sumNeighbors - 4.0f * center) * stiffness;
                velocityField[idx(x, y)] += force;
            }
        }

        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                //THIS IS WHERE DAMPING LIVES
                
                //Originally:
                //velocityField[idx(x, y)] *= damping;
                velocityField[idx(x, y)] *= 0.94f;
                heightField[idx(x, y)] += velocityField[idx(x, y)];
            }
        }
    }

    // --- Fake cubemap reflection ---
    Color SampleCubemap(const Vector3 &n) {
        // Define 6 cubemap face colors
        Color envRight  = { 200, 180, 160, 255 }; // +X
        Color envLeft   = { 160, 180, 200, 255 }; // -X
        Color envUp     = { 180, 200, 255, 255 }; // +Y
        Color envDown   = { 40, 40, 50, 255 };    // -Y
        Color envFront  = { 120, 130, 150, 255 }; // +Z
        Color envBack   = { 80, 70, 60, 255 };    // -Z

        Color env;

        // Pick dominant axis of normal
        float ax = fabs(n.x);
        float ay = fabs(n.y);
        float az = fabs(n.z);

        if (ax > ay && ax > az)
            env = (n.x > 0) ? envRight : envLeft;
        else if (ay > az)
            env = (n.y > 0) ? envUp : envDown;
        else
            env = (n.z > 0) ? envFront : envBack;

        return env;
    }

    void RenderToImage(Image &img, Vector2 lightDir) {
        Color *pixels = (Color *)img.data;

        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                float hL = heightField[idx(x - 1, y)];
                float hR = heightField[idx(x + 1, y)];
                float hU = heightField[idx(x, y - 1)];
                float hD = heightField[idx(x, y + 1)];

                float dx = hR - hL;
                float dy = hD - hU;

                Vector3 n = { -dx, -dy, 1.0f };
                float len = std::sqrt(n.x*n.x + n.y*n.y + n.z*n.z);
                if (len > 0.0f) {
                    n.x /= len;
                    n.y /= len;
                    n.z /= len;
                }

                float ndotl = n.x * lightDir.x + n.y * lightDir.y + n.z * 1.0f;
                float base = 0.4f;
                float intensity = base + ndotl * 0.6f;
                intensity = Clamp(intensity, 0.0f, 1.0f);

                // Chrome brightness curve
                float boosted = powf(intensity, 0.6f);
                unsigned char chrome = (unsigned char)(boosted * 255.0f);

                // Sample cubemap
                Color env = SampleCubemap(n);

                // Blend chrome with cubemap
                unsigned char finalR = (unsigned char)(chrome * 0.4f + env.r * 0.6f);
                unsigned char finalG = (unsigned char)(chrome * 0.4f + env.g * 0.6f);
                unsigned char finalB = (unsigned char)(chrome * 0.4f + env.b * 0.6f);

                // Semi-transparent chrome
                pixels[idx(x, y)] = { finalR, finalG, finalB, 180 };
            }
        }
    }
};

int main() {

    const int startWidth = 960;
    const int startHeight = 540;

    const int simWidth = 200;
    const int simHeight = 200;

    InitWindow(startWidth, startHeight, "mLiquidMetal by Paul Swonger (covidinsane@gmail.com 02.03.2026)");
    SetTargetFPS(60);

    // Dark background for chrome contrast
    Color rayBlue = { 20, 40, 60, 255 };

    LiquidSim sim(simWidth, simHeight);

    Image img = GenImageColor(simWidth, simHeight, BLACK);
    Texture2D tex = LoadTextureFromImage(img);

    Vector2 lightDir = { -0.4f, -0.6f };
    float len = std::sqrt(lightDir.x*lightDir.x + lightDir.y*lightDir.y + 1.0f);
    lightDir.x /= len;
    lightDir.y /= len;

    float idleTime = 0.0f;
    unsigned char statusAlpha = 0;
    unsigned char targetAlpha = 0;
    Vector2 lastMouse = GetMousePosition();

    bool wasFullscreen = false;

    while (!WindowShouldClose()) {

        // --- FULLSCREEN TOGGLE ---
        if (IsKeyPressed(KEY_F)) {
            if (!IsWindowFullscreen()) {
                int monitor = GetCurrentMonitor();
                SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
                ToggleFullscreen();
            } else {
                ToggleFullscreen();
                SetWindowSize(startWidth, startHeight);
            }
            wasFullscreen = IsWindowFullscreen();
        }

        // --- IDLE DETECTION ---
        Vector2 curMouse = GetMousePosition();
        if (curMouse.x != lastMouse.x || curMouse.y != lastMouse.y) {
            idleTime = 0.0f;
            targetAlpha = 0;
        } else {
            idleTime += GetFrameTime();
            if (idleTime > 1.0f) {
                targetAlpha = 255;
            }
        }
        lastMouse = curMouse;

        // --- FADE ANIMATION ---
        if (statusAlpha < targetAlpha)
            statusAlpha = (unsigned char)Clamp(statusAlpha + 5, 0, 255);
        else if (statusAlpha > targetAlpha)
            statusAlpha = (unsigned char)Clamp(statusAlpha - 5, 0, 255);

        int drawW = GetRenderWidth();
        int drawH = GetRenderHeight();

        // --- MOUSE INTERACTION ---
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            float sx = curMouse.x * (float)simWidth / (float)drawW;
            float sy = curMouse.y * (float)simHeight / (float)drawH;

            int ix = (int)sx;
            int iy = (int)sy;

            if (ix > 1 && ix < simWidth - 1 && iy > 1 && iy < simHeight - 1) {
                sim.AddImpulse(ix, iy, -1.5f);
            }
        }

        sim.Step();

        ImageClearBackground(&img, BLACK);

        sim.RenderToImage(img, lightDir);
        UpdateTexture(tex, img.data);

        BeginDrawing();
        ClearBackground(rayBlue);

        Rectangle src = { 0, 0, (float)simWidth, (float)simHeight };
        Rectangle dst = { 0, 0, (float)drawW, (float)drawH };

        BeginBlendMode(BLEND_ALPHA);
        DrawTexturePro(tex, src, dst, {0,0}, 0.0f, WHITE);
        EndBlendMode();

        if (statusAlpha > 0) {
            Color bar = { 50, 50, 50, statusAlpha };
            Color text = { 255, 255, 255, statusAlpha };
            Color line = { 180, 180, 180, statusAlpha };

            DrawRectangle(0, drawH - 24, drawW, 24, bar);
            DrawLine(0, drawH - 24, drawW, drawH - 24, line);

            DrawText("Click and drag your mouse. \"F\" toggles fullscreen.",
                     8, drawH - 20, 16, text);
        }

        EndDrawing();
    }

    UnloadTexture(tex);
    UnloadImage(img);
    CloseWindow();
    return 0;
}