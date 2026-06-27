#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#define MAX_TARGETS 12
#define RLIGHTS_IMPLEMENTATION
#include "rlights.h"

int main(void)
{
    const int sw = 1280, sh = 720;

    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(sw, sh, "3D Room - FPS Shooting");
    SetExitKey(0);
    InitAudioDevice();



    unsigned int rate = 44100;
    unsigned int frames = rate * 3 / 20;
    short *samples = malloc(frames * sizeof(short));
    for (unsigned int i = 0; i < frames; i++)
    {
        float t = (float)i / rate;
        float env = expf(-t * 28.0f);
        float noise = (float)GetRandomValue(-32767, 32767) / 32768.0f;
        float punch = sinf(2.0f * PI * 180.0f * t) * 0.6f;
        float s = (noise * 0.7f + punch) * env;
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        samples[i] = (short)(s * 30000.0f);
    }
    Wave w = { frames, rate, 16, 1, samples };
    Sound gunSound = LoadSoundFromWave(w);

    unsigned int hFrames = rate / 5;
    short *hSamples = malloc(hFrames * sizeof(short));
    for (unsigned int i = 0; i < hFrames; i++)
    {
        float t = (float)i / rate;
        float env = expf(-t * 20.0f);
        float ping = sinf(2.0f * PI * 800.0f * t) * expf(-t * 15.0f);
        float crack = sinf(2.0f * PI * 1200.0f * t * (1.0f + t * 5.0f)) * 0.5f;
        float s = (ping + crack) * env;
        if (s > 1.0f) s = 1.0f;
        if (s < -1.0f) s = -1.0f;
        hSamples[i] = (short)(s * 25000.0f);
    }
    Wave hw = { hFrames, rate, 16, 1, hSamples };
    Sound hitSound = LoadSoundFromWave(hw);
    UnloadWave(hw);

    Shader lightShader = LoadShader("shaders/lighting.vs", "shaders/lighting.fs");
    lightShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(lightShader, "viewPos");
    int ambientLoc = GetShaderLocation(lightShader, "ambient");
    float ambientVal[4] = { 0.02f, 0.02f, 0.03f, 1.0f };
    SetShaderValue(lightShader, ambientLoc, ambientVal, SHADER_UNIFORM_VEC4);

    Model pistol = LoadModel("super_simple_pistol.glb");
    for (int i = 0; i < pistol.materialCount; i++)
    {
        pistol.materials[i].shader = lightShader;
        pistol.materials[i].maps[MATERIAL_MAP_DIFFUSE].color = (Color){ 60, 60, 65, 255 };
    }

    BoundingBox pistolBox = GetModelBoundingBox(pistol);
    Vector3 pistolCenter = { (pistolBox.min.x + pistolBox.max.x) * 0.5f,
                             (pistolBox.min.y + pistolBox.max.y) * 0.5f,
                             (pistolBox.min.z + pistolBox.max.z) * 0.5f };

    Shader cubeShader = LoadShader(0, "shaders/cube_glow.fs");
    cubeShader.locs[SHADER_LOC_VECTOR_VIEW] = GetShaderLocation(cubeShader, "viewPos");

    RenderTexture2D target = LoadRenderTexture(sw, sh);
    Shader bloomShader = LoadShader(0, "shaders/bloom.fs");
    int bloomResLoc = GetShaderLocation(bloomShader, "resolution");
    int bloomSizeLoc = GetShaderLocation(bloomShader, "bloomSize");
    float res[2] = { (float)sw, (float)sh };
    SetShaderValue(bloomShader, bloomResLoc, res, SHADER_UNIFORM_VEC2);
    float bloomS = 4.0f;
    SetShaderValue(bloomShader, bloomSizeLoc, &bloomS, SHADER_UNIFORM_FLOAT);

    Light lights[MAX_TARGETS] = { 0 };
    for (int i = 0; i < MAX_TARGETS; i++)
        lights[i] = CreateLight(LIGHT_POINT, (Vector3){ 0, 0, 0 }, Vector3Zero(), WHITE, lightShader);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 1.8f, 0.0f };
    camera.target = (Vector3){ 0.0f, 1.8f, -1.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    float roomW = 24.0f, roomH = 10.0f, roomD = 24.0f;
    float wall = 0.3f;

    float boundX = roomW/2 - wall/2 - 0.3f;
    float boundY_min = 0.3f;
    float boundY_max = roomH - 0.3f;
    float boundZ = roomD/2 - wall/2 - 0.3f;

    typedef struct {
        Vector3 pos;
        Vector3 size;
        Color color;
        bool alive;
        Vector3 vel;
        float rotX, rotY, rotSpeed;
    } Target;

    Target targets[MAX_TARGETS];
    for (int i = 0; i < MAX_TARGETS; i++)
    {
        targets[i].pos = (Vector3){
            (float)GetRandomValue(-(int)(roomW/2 - 2)*10, (int)(roomW/2 - 2)*10) / 10.0f,
            (float)GetRandomValue(8, 28) / 10.0f,
            (float)GetRandomValue(-(int)(roomD/2 - 2)*10, (int)(roomD/2 - 2)*10) / 10.0f
        };
        targets[i].size = (Vector3){ 0.6f, 0.6f, 0.6f };
        targets[i].color = (Color){ GetRandomValue(100, 255), GetRandomValue(100, 255), GetRandomValue(100, 255), 255 };
        targets[i].alive = true;
        float angle = (float)GetRandomValue(0, 3600) / 10.0f * DEG2RAD;
        float angleV = (float)GetRandomValue(-450, 450) / 100.0f;
        float speed = (float)GetRandomValue(80, 300) / 100.0f;
        targets[i].vel = (Vector3){ cosf(angle) * speed, sinf(angleV) * speed * 0.5f, sinf(angle) * speed };
        targets[i].rotX = (float)GetRandomValue(0, 360);
        targets[i].rotY = (float)GetRandomValue(0, 360);
        targets[i].rotSpeed = (float)GetRandomValue(-180, 180) / 100.0f;
    }

    UnloadWave(w);

    bool shooting = false;
    float shootTimer = 0.0f;
    float muzzleTimer = 0.0f;
    int score = 0;
    float playerVy = 0.0f;
    float gravity = -15.0f;
    float jumpSpeed = 8.0f;
    float eyeHeight = 1.6f;
    float pitch = 0.0f, yaw = -90.0f;
    float sensitivity = 0.15f;
    int jumpCount = 0, maxJumps = 2;
    int round = 1;
    float roundTimer = 0.0f;
    bool roundComplete = false;

    bool menuOpen = false;
    int menuSel = 0, menuItems = 6;
    int resOpts[][2] = { { 1280, 720 }, { 1600, 900 }, { 1920, 1080 }, { 2560, 1440 } };
    int resCount = 4, resIdx = 0;
    bool fullscreen = false;
    bool vsyncOn = true;

    DisableCursor();
    SetTargetFPS(0);

    while (!WindowShouldClose())
    {
        float dt = GetFrameTime();

        if (!menuOpen)
        {
            Vector2 md = GetMouseDelta();
            pitch -= md.y * sensitivity;
            yaw -= md.x * sensitivity;
            if (pitch > 89.0f) pitch = 89.0f;
            if (pitch < -89.0f) pitch = -89.0f;

            float cp = cosf(pitch * DEG2RAD), sp = sinf(pitch * DEG2RAD);
            float cy = cosf(yaw * DEG2RAD), sy = sinf(yaw * DEG2RAD);
            Vector3 fwd = Vector3Normalize((Vector3){ cp * sy, sp, cp * cy });
            Vector3 hFwd = Vector3Normalize((Vector3){ fwd.x, 0, fwd.z });
            Vector3 right = Vector3Normalize(Vector3CrossProduct(hFwd, (Vector3){ 0, 1, 0 }));

            float speed = 5.0f;
            Vector3 move = { 0, 0, 0 };
            if (IsKeyDown(KEY_W)) move = Vector3Add(move, Vector3Scale(hFwd, speed * dt));
            if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, Vector3Scale(hFwd, speed * dt));
            if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, Vector3Scale(right, speed * dt));
            if (IsKeyDown(KEY_D)) move = Vector3Add(move, Vector3Scale(right, speed * dt));

            if (IsKeyPressed(KEY_SPACE) && jumpCount < maxJumps)
            {
                playerVy = jumpSpeed;
                jumpCount++;
            }
            playerVy += gravity * dt;
            move.y = playerVy * dt;

            camera.position = Vector3Add(camera.position, move);
            camera.target = Vector3Add(camera.position, fwd);

            if (camera.position.y < eyeHeight) { camera.position.y = eyeHeight; playerVy = 0.0f; jumpCount = 0; }
            if (camera.position.y > roomH - eyeHeight) { camera.position.y = roomH - eyeHeight; playerVy = 0.0f; }

            float lim = roomW / 2 - wall - 0.3f;
            float limz = roomD / 2 - wall - 0.3f;
            if (camera.position.x > lim) camera.position.x = lim;
            if (camera.position.x < -lim) camera.position.x = -lim;
            if (camera.position.z > limz) camera.position.z = limz;
            if (camera.position.z < -limz) camera.position.z = -limz;
        }

        if (IsWindowResized())
        {
            UnloadRenderTexture(target);
            target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());
            res[0] = (float)GetScreenWidth(); res[1] = (float)GetScreenHeight();
            SetShaderValue(bloomShader, bloomResLoc, res, SHADER_UNIFORM_VEC2);
        }

        if (IsKeyPressed(KEY_ESCAPE))
        {
            menuOpen = !menuOpen;
            if (menuOpen) { EnableCursor(); } else { DisableCursor(); }
        }

        if (menuOpen)
        {
            if (IsKeyPressed(KEY_UP)) { menuSel = (menuSel - 1 + menuItems) % menuItems; }
            if (IsKeyPressed(KEY_DOWN)) { menuSel = (menuSel + 1) % menuItems; }
            if (IsKeyPressed(KEY_LEFT))
            {
                if (menuSel == 2) { camera.fovy = fmaxf(50, camera.fovy - 5); }
                if (menuSel == 3) { resIdx = (resIdx - 1 + resCount) % resCount; SetWindowSize(resOpts[resIdx][0], resOpts[resIdx][1]); UnloadRenderTexture(target); target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight()); res[0] = (float)GetScreenWidth(); res[1] = (float)GetScreenHeight(); SetShaderValue(bloomShader, bloomResLoc, res, SHADER_UNIFORM_VEC2); }
            }
            if (IsKeyPressed(KEY_RIGHT))
            {
                if (menuSel == 2) { camera.fovy = fminf(120, camera.fovy + 5); }
                if (menuSel == 3) { resIdx = (resIdx + 1) % resCount; SetWindowSize(resOpts[resIdx][0], resOpts[resIdx][1]); UnloadRenderTexture(target); target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight()); res[0] = (float)GetScreenWidth(); res[1] = (float)GetScreenHeight(); SetShaderValue(bloomShader, bloomResLoc, res, SHADER_UNIFORM_VEC2); }
            }
            if (IsKeyPressed(KEY_ENTER))
            {
                if (menuSel == 0) { fullscreen = !fullscreen; ToggleFullscreen(); }
                if (menuSel == 1) { vsyncOn = !vsyncOn; if (vsyncOn) { SetWindowState(FLAG_VSYNC_HINT); } else { ClearWindowState(FLAG_VSYNC_HINT); } }
                if (menuSel == 2) { camera.fovy = (camera.fovy >= 120) ? 50 : camera.fovy + 5; }
                if (menuSel == 3) { resIdx = (resIdx + 1) % resCount; SetWindowSize(resOpts[resIdx][0], resOpts[resIdx][1]); UnloadRenderTexture(target); target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight()); res[0] = (float)GetScreenWidth(); res[1] = (float)GetScreenHeight(); SetShaderValue(bloomShader, bloomResLoc, res, SHADER_UNIFORM_VEC2); }
                if (menuSel == 4) { camera.position = (Vector3){ 0, 1.8, 0 }; camera.target = (Vector3){ 0, 1.8, -1 }; pitch = 0; yaw = -90; playerVy = 0; jumpCount = 0; shooting = false; shootTimer = 0; muzzleTimer = 0; score = 0; round = 1; roundComplete = false; roundTimer = 0; for (int ri = 0; ri < MAX_TARGETS; ri++) { targets[ri].pos = (Vector3){ (float)GetRandomValue(-(int)(roomW/2 - 2)*10, (int)(roomW/2 - 2)*10) / 10.0f, (float)GetRandomValue(8, 28) / 10.0f, (float)GetRandomValue(-(int)(roomD/2 - 2)*10, (int)(roomD/2 - 2)*10) / 10.0f }; targets[ri].alive = true; float ra = (float)GetRandomValue(0, 3600) / 10.0f * DEG2RAD; float rav = (float)GetRandomValue(-450, 450) / 100.0f; float rspeed = (float)GetRandomValue(80, 300) / 100.0f; targets[ri].vel = (Vector3){ cosf(ra) * rspeed, sinf(rav) * rspeed * 0.5f, sinf(ra) * rspeed }; targets[ri].color = (Color){ GetRandomValue(100, 255), GetRandomValue(100, 255), GetRandomValue(100, 255), 255 }; targets[ri].rotX = (float)GetRandomValue(0, 360); targets[ri].rotY = (float)GetRandomValue(0, 360); targets[ri].rotSpeed = (float)GetRandomValue(-180, 180) / 100.0f; } }
                if (menuSel == 5) { CloseWindow(); }
            }
        }

        if (!menuOpen && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !shooting)
        {
            shooting = true;
            shootTimer = 0.08f;
            muzzleTimer = 0.05f;
            PlaySound(gunSound);

            Vector3 fwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
            Ray ray = { camera.position, fwd };

            float bestDist = 1e9f;
            int bestIdx = -1;
            for (int i = 0; i < MAX_TARGETS; i++)
            {
                if (!targets[i].alive) continue;
                Vector3 half = Vector3Scale(targets[i].size, 0.5f);
                BoundingBox box = {
                    Vector3Subtract(targets[i].pos, half),
                    Vector3Add(targets[i].pos, half)
                };
                RayCollision col = GetRayCollisionBox(ray, box);
                if (col.hit && col.distance < bestDist)
                {
                    bestDist = col.distance;
                    bestIdx = i;
                }
            }
            if (bestIdx >= 0)
            {
                targets[bestIdx].alive = false;
                score++;
                PlaySound(hitSound);
            }
        }

        if (!menuOpen)
        {
            if (shooting)
            {
                shootTimer -= dt;
                if (shootTimer <= 0.0f) shooting = false;
            }
            if (muzzleTimer > 0.0f) muzzleTimer -= dt;

            for (int i = 0; i < MAX_TARGETS; i++)
            {
                if (!targets[i].alive) continue;
                targets[i].pos = Vector3Add(targets[i].pos, Vector3Scale(targets[i].vel, dt));
                if (targets[i].pos.x > boundX) { targets[i].pos.x = boundX; targets[i].vel.x *= -1; }
                if (targets[i].pos.x < -boundX) { targets[i].pos.x = -boundX; targets[i].vel.x *= -1; }
                if (targets[i].pos.y > boundY_max) { targets[i].pos.y = boundY_max; targets[i].vel.y *= -1; }
                if (targets[i].pos.y < boundY_min) { targets[i].pos.y = boundY_min; targets[i].vel.y *= -1; }
                if (targets[i].pos.z > boundZ) { targets[i].pos.z = boundZ; targets[i].vel.z *= -1; }
                if (targets[i].pos.z < -boundZ) { targets[i].pos.z = -boundZ; targets[i].vel.z *= -1; }
                targets[i].rotX += targets[i].rotSpeed * dt * 60.0f;
                targets[i].rotY += targets[i].rotSpeed * dt * 60.0f;
            }

            bool anyAlive = false;
            for (int i = 0; i < MAX_TARGETS; i++) { if (targets[i].alive) { anyAlive = true; break; } }
            if (!anyAlive && !roundComplete)
            {
                roundComplete = true;
                roundTimer = 3.0f;
                round++;
            }
            if (roundComplete)
            {
                roundTimer -= dt;
                if (roundTimer <= 0.0f)
                {
                    for (int ri = 0; ri < MAX_TARGETS; ri++)
                    {
                        targets[ri].pos = (Vector3){ (float)GetRandomValue(-(int)(roomW/2 - 2)*10, (int)(roomW/2 - 2)*10) / 10.0f, (float)GetRandomValue(8, 28) / 10.0f, (float)GetRandomValue(-(int)(roomD/2 - 2)*10, (int)(roomD/2 - 2)*10) / 10.0f };
                        targets[ri].alive = true;
                        float ra = (float)GetRandomValue(0, 3600) / 10.0f * DEG2RAD;
                        float rav = (float)GetRandomValue(-450, 450) / 100.0f;
                        float rspeed = (float)GetRandomValue(80 + round * 20, 300 + round * 30) / 100.0f;
                        targets[ri].vel = (Vector3){ cosf(ra) * rspeed, sinf(rav) * rspeed * 0.5f, sinf(ra) * rspeed };
                        targets[ri].color = (Color){ GetRandomValue(100, 255), GetRandomValue(100, 255), GetRandomValue(100, 255), 255 };
                        targets[ri].rotX = (float)GetRandomValue(0, 360);
                        targets[ri].rotY = (float)GetRandomValue(0, 360);
                        targets[ri].rotSpeed = (float)GetRandomValue(-180, 180) / 100.0f;
                    }
                    roundComplete = false;
                }
            }

            for (int i = 0; i < MAX_TARGETS; i++)
            {
                if (!targets[i].alive) { lights[i].enabled = false; UpdateLightValues(lightShader, lights[i]); continue; }
                lights[i].enabled = true;
                lights[i].position = targets[i].pos;
                lights[i].color = targets[i].color;
                UpdateLightValues(lightShader, lights[i]);
            }
        }
        float camPos[3] = { camera.position.x, camera.position.y, camera.position.z };
        SetShaderValue(lightShader, lightShader.locs[SHADER_LOC_VECTOR_VIEW], camPos, SHADER_UNIFORM_VEC3);
        SetShaderValue(cubeShader, cubeShader.locs[SHADER_LOC_VECTOR_VIEW], camPos, SHADER_UNIFORM_VEC3);

        BeginTextureMode(target);
            ClearBackground((Color){ 20, 18, 16, 255 });

            BeginMode3D(camera);
                BeginShaderMode(lightShader);
                    DrawCube((Vector3){ 0, 0, 0 }, roomW, 0.05f, roomD, WHITE);
                    DrawCube((Vector3){ 0, roomH/2, -roomD/2 }, roomW, roomH, wall, WHITE);
                    DrawCube((Vector3){ 0, roomH/2, roomD/2 }, roomW, roomH, wall, WHITE);
                    DrawCube((Vector3){ -roomW/2, roomH/2, 0 }, wall, roomH, roomD, WHITE);
                    DrawCube((Vector3){ roomW/2, roomH/2, 0 }, wall, roomH, roomD, WHITE);
                    DrawCube((Vector3){ 0, roomH, 0 }, roomW, 0.05f, roomD, WHITE);
                EndShaderMode();

                BeginShaderMode(cubeShader);
                for (int i = 0; i < MAX_TARGETS; i++)
                {
                    if (!targets[i].alive) continue;
                    rlPushMatrix();
                    rlTranslatef(targets[i].pos.x, targets[i].pos.y, targets[i].pos.z);
                    rlRotatef(targets[i].rotX, 1, 0, 0);
                    rlRotatef(targets[i].rotY, 0, 1, 0);
                    DrawCubeV((Vector3){ 0, 0, 0 }, targets[i].size, targets[i].color);
                    rlPopMatrix();
                }
                EndShaderMode();

                Vector3 cfwd = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
                Vector3 cright = Vector3Normalize(Vector3CrossProduct(cfwd, (Vector3){ 0, 1, 0 }));
                if (Vector3Length(cright) < 0.001f) cright = (Vector3){ 1, 0, 0 };
                Vector3 cup = Vector3CrossProduct(cright, cfwd);
                float recoilOff = shooting ? -0.04f : 0.0f;
                Vector3 gPos = Vector3Add(camera.position,
                    Vector3Add(Vector3Scale(cfwd, 0.4f),
                    Vector3Add(Vector3Scale(cright, 0.22f),
                    Vector3Scale(cup, -0.22f + recoilOff))));
                Matrix basis = { cright.x, cright.y, cright.z, 0,
                                 cup.x,    cup.y,    cup.z,    0,
                                 cfwd.x,   cfwd.y,   cfwd.z,   0,
                                 0,        0,        0,        1 };
                rlPushMatrix();
                rlTranslatef(-pistolCenter.x, -pistolCenter.y, -pistolCenter.z);
                rlRotatef(-90.0f, 0.0f, 1.0f, 0.0f);
                rlScalef(0.022f, 0.022f, 0.022f);
                rlMultMatrixf((float *)&basis);
                rlTranslatef(gPos.x, gPos.y, gPos.z);
                DrawModel(pistol, (Vector3){ 0, 0, 0 }, 1.0f, WHITE);
                rlPopMatrix();
                if (muzzleTimer > 0.0f)
                {
                    Vector3 flashPos = Vector3Add(gPos, Vector3Scale(cfwd, 0.08f));
                    DrawSphere(flashPos, 0.02f, (Color){ 255, 220, 50, 200 });
                    DrawSphere(flashPos, 0.01f, (Color){ 255, 255, 200, 255 });
                }
            EndMode3D();
        EndTextureMode();

        BeginDrawing();
            ClearBackground(BLACK);
            BeginShaderMode(bloomShader);
                Rectangle src = { 0, 0, (float)target.texture.width, -(float)target.texture.height };
                DrawTextureRec(target.texture, src, (Vector2){ 0, 0 }, WHITE);
            EndShaderMode();

            // Crosshair
            int chx = GetScreenWidth() / 2, chy = GetScreenHeight() / 2, chGap = 6, chLen = 12;
            Color chCol = WHITE;
            Color chDark = (Color){ 0, 0, 0, 100 };
            DrawLine(chx, chy - chGap - chLen, chx, chy - chGap, chDark);
            DrawLine(chx, chy + chGap, chx, chy + chGap + chLen, chDark);
            DrawLine(chx - chGap - chLen, chy, chx - chGap, chy, chDark);
            DrawLine(chx + chGap, chy, chx + chGap + chLen, chy, chDark);
            DrawLine(chx + 1, chy - chGap - chLen + 1, chx + 1, chy - chGap + 1, chCol);
            DrawLine(chx + 1, chy + chGap + 1, chx + 1, chy + chGap + chLen + 1, chCol);
            DrawLine(chx - chGap - chLen + 1, chy + 1, chx - chGap + 1, chy + 1, chCol);
            DrawLine(chx + chGap + 1, chy + 1, chx + chGap + chLen + 1, chy + 1, chCol);
            DrawCircle(chx, chy, 2, chCol);
            DrawCircle(chx, chy, 1, chDark);

            // HUD
            DrawText(TextFormat("Score: %d  Round: %d", score, round), 10, 10, 22, WHITE);
            DrawFPS(10, 38);

            if (roundComplete)
            {
                const char *msg = TextFormat("Round %d Complete!", round - 1);
                int mw = MeasureText(msg, 40);
                DrawText(msg, (GetScreenWidth() - mw) / 2, GetScreenHeight() / 2 - 40, 40, GREEN);
                DrawText("Get ready...", (GetScreenWidth() - MeasureText("Get ready...", 24)) / 2, GetScreenHeight() / 2 + 10, 24, LIGHTGRAY);
            }

            // Muzzle flash (2D)
            float gx = GetScreenWidth() / 2.0f;
            float gy = GetScreenHeight() * 0.95f;
            if (muzzleTimer > 0.0f)
            {
                DrawCircle(gx, gy - 170, 14, (Color){ 255, 220, 50, 200 });
                DrawCircle(gx, gy - 170, 9, (Color){ 255, 180, 30, 220 });
                DrawCircle(gx, gy - 170, 5, (Color){ 255, 255, 200, 255 });
            }

            if (menuOpen)
            {
                int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
                DrawRectangle(0, 0, sw2, sh2, (Color){ 0, 0, 0, 160 });
                int mx = sw2 / 2, my = sh2 / 3, itemH = 40, itemW = 300;
                DrawText("SETTINGS", mx - 55, my - 55, 30, WHITE);

                const char *labels[] = { "Fullscreen", "VSync", "FOV", "Resolution", "Restart", "Quit" };
                int totalItems = 6;
                char val[64];
                Vector2 mp = GetMousePosition();
                bool clicked = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

                for (int i = 0; i < totalItems; i++)
                {
                    int yy = my + i * itemH;
                    Rectangle rec = { (float)(mx - itemW/2), (float)yy, (float)itemW, (float)itemH - 4 };
                    bool hover = CheckCollisionPointRec(mp, rec);
                    Color bg = (i == menuSel || hover) ? (Color){ 255, 255, 255, 25 } : BLANK;
                    Color tc = (i == 5) ? (Color){ 255, 100, 100, 255 } : (i == 4) ? (Color){ 100, 200, 255, 255 } : (hover ? YELLOW : LIGHTGRAY);
                    DrawRectangleRec(rec, bg);
                    if (hover) DrawRectangleLinesEx(rec, 1, (Color){ 255, 255, 255, 60 });

                    DrawText(labels[i], mx - itemW/2 + 10, yy + 8, 20, tc);
                    if (i < 4)
                    {
                        const char *v = "";
                        if (i == 0) v = fullscreen ? "On" : "Off";
                        if (i == 1) v = vsyncOn ? "On" : "Off";
                        if (i == 2) { snprintf(val, sizeof(val), "%.0f", camera.fovy); v = val; }
                        if (i == 3) { snprintf(val, sizeof(val), "%dx%d", resOpts[resIdx][0], resOpts[resIdx][1]); v = val; }
                        DrawText(v, mx + 60, yy + 8, 20, tc);
                    }

                    if (clicked && hover)
                    {
                        if (i == 0) { fullscreen = !fullscreen; ToggleFullscreen(); }
                        if (i == 1) { vsyncOn = !vsyncOn; if (vsyncOn) SetWindowState(FLAG_VSYNC_HINT); else ClearWindowState(FLAG_VSYNC_HINT); }
                        if (i == 2) camera.fovy = (camera.fovy >= 120) ? 50 : camera.fovy + 5;
                        if (i == 3) { resIdx = (resIdx + 1) % resCount; SetWindowSize(resOpts[resIdx][0], resOpts[resIdx][1]); UnloadRenderTexture(target); target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight()); res[0] = (float)GetScreenWidth(); res[1] = (float)GetScreenHeight(); SetShaderValue(bloomShader, bloomResLoc, res, SHADER_UNIFORM_VEC2); }
                        if (i == 4) { camera.position = (Vector3){ 0, 1.8, 0 }; camera.target = (Vector3){ 0, 1.8, -1 }; pitch = 0; yaw = -90; playerVy = 0; jumpCount = 0; shooting = false; shootTimer = 0; muzzleTimer = 0; score = 0; round = 1; roundComplete = false; roundTimer = 0; for (int ri = 0; ri < MAX_TARGETS; ri++) { targets[ri].pos = (Vector3){ (float)GetRandomValue(-(int)(roomW/2 - 2)*10, (int)(roomW/2 - 2)*10) / 10.0f, (float)GetRandomValue(8, 28) / 10.0f, (float)GetRandomValue(-(int)(roomD/2 - 2)*10, (int)(roomD/2 - 2)*10) / 10.0f }; targets[ri].alive = true; float ra = (float)GetRandomValue(0, 3600) / 10.0f * DEG2RAD; float rav = (float)GetRandomValue(-450, 450) / 100.0f; float rspeed = (float)GetRandomValue(80, 300) / 100.0f; targets[ri].vel = (Vector3){ cosf(ra) * rspeed, sinf(rav) * rspeed * 0.5f, sinf(ra) * rspeed }; targets[ri].color = (Color){ GetRandomValue(100, 255), GetRandomValue(100, 255), GetRandomValue(100, 255), 255 }; targets[ri].rotX = (float)GetRandomValue(0, 360); targets[ri].rotY = (float)GetRandomValue(0, 360); targets[ri].rotSpeed = (float)GetRandomValue(-180, 180) / 100.0f; } }
                        if (i == 5) { CloseWindow(); }
                    }
                }

                DrawText("ESC: Close", mx - 35, my + totalItems * itemH + 12, 16, (Color){ 150, 150, 150, 255 });
            }
        EndDrawing();
    }

    UnloadModel(pistol);
    UnloadRenderTexture(target);
    UnloadShader(bloomShader);
    UnloadShader(lightShader);
    UnloadShader(cubeShader);
    UnloadSound(gunSound);
    UnloadSound(hitSound);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}
