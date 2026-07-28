#include "raylib.h"
#include <vector>
#include <cmath>
#include <string>
#include <algorithm>
#include <iostream> // Para imprimir diagnóstico en consola

//----------------------------------------------------------------------
// Constantes globales
//----------------------------------------------------------------------
const float GROUND_Y     = 480.0f;
const float PLAYER_W     = 90.0f;
const float PLAYER_H     = 110.0f;
const float LEVEL_LENGTH = 3400.0f; // largo de CADA nivel (mapas separados)
const int   SCREEN_W = 1280;
const int   SCREEN_H = 720;
const int   MAX_BULLETS = 100;
const int   LAST_LEVEL = 3;

// Video de introduccion: se reproduce como un sprite-sheet (atlas) de
// cuadros extraidos del mp4, ya que raylib base no decodifica video.
const int   INTRO_COLS = 10;
const int   INTRO_ROWS = 8;
const int   INTRO_FRAME_COUNT = 76;
const float INTRO_FPS = 15.0f;

//----------------------------------------------------------------------
// Estructuras de datos
//----------------------------------------------------------------------
struct Bullet {
    float x, y;
    float speed;
    int dir;
    bool active;
};

struct Zombie {
    float x, y;
    int health;
    bool alive;
    float speed;
    float width = 90.0f;
    float height = 110.0f;
    bool dying = false;
    float deathTimer = 0.0f;
};

struct Tank {
    float x = 0, y = 0;
    int health = 180, maxHealth = 180;
    bool alive = false;
    bool dying = false;
    float deathTimer = 0.0f;
    float speed = 0.35f;
    float shootCooldown = 2.0f;
    float width = 140.0f;
    float height = 70.0f;
};

struct Shell {
    float x, y;
    float speed;
    int dir;
    bool active;
};

struct FinalBoss {
    float x = 0, y = 0;
    int health = 420, maxHealth = 420;
    bool alive = false;
    bool dying = false;
    bool triggered = false; // se activa cuando el jugador se acerca
    float deathTimer = 0.0f;
    float speed = 0.55f;
    float attackCooldown = 1.6f;
    float width = 110.0f;
    float height = 150.0f;
};

struct BossOrb {
    float x, y;
    float vx, vy;
    bool active;
    float radius = 13.0f;
};

struct Box { // Cofre
    float x, y;
    bool opened;
    Rectangle GetRect() const { return { x - 5, y - 20, 70, 58 }; }
};

struct WeaponPickup {
    float x, y;
    int type; // 0 = pistola, 1 = escopeta
    bool taken;
    float width = 70.0f;
    float height = 35.0f;
};

struct Grenade {
    float x, y;
    float vx, vy;
    float timer;
    bool active;
    float width = 30.0f;
    float height = 40.0f;
};

enum WeaponType { WEAPON_PISTOL = 0, WEAPON_SHOTGUN = 1 };
enum Screen { SCREEN_INTRO, SCREEN_MENU, SCREEN_PLAYING };
enum PickupType { PICKUP_AMMO = 0, PICKUP_HEALTH = 1, PICKUP_COIN = 2 };

struct ItemPickup {
    float x, y;
    PickupType type;
    bool taken;
};

struct GameState {
    float x, y;
    float velocityY;
    bool isJumping;
    bool isCrouching;
    bool facingRight;

    int maxHealth, health;
    float hurtCooldown;

    int currentWeapon;
    int coins;
    int grenadeCount;
    int bulletCount;
    float shootCooldown;

    float animTimer;

    bool isDead;
    float deathTimer;
    bool gameComplete;

    int level;
    bool showLevelBanner;
    float levelBannerTimer;

    std::vector<Bullet> bullets;
    std::vector<Zombie> zombies;
    std::vector<Box> boxes;
    std::vector<WeaponPickup> weaponPickups;
    std::vector<Grenade> thrownGrenades;
    std::vector<ItemPickup> pickups;
    std::vector<Shell> shells;
    Tank tank;
    FinalBoss finalBoss;
    std::vector<BossOrb> bossOrbs;

    Camera2D camera;
};

//----------------------------------------------------------------------
// Utilidades de texturas
//----------------------------------------------------------------------
bool TexturaEstaLista(Texture2D tex) {
    return tex.id > 0;
}

Texture2D CargarTexturaInteligente(const std::string& basePath) {
    std::vector<std::string> extensions = { ".png", ".jpeg", ".jpg", ".PNG", ".JPEG", ".JPG" };
    for (const auto& ext : extensions) {
        std::string fullPath = basePath + ext;
        if (FileExists(fullPath.c_str())) {
            std::cout << "[DEBUG] Intentando cargar archivo encontrado: " << fullPath << std::endl;
            Texture2D tex = LoadTexture(fullPath.c_str());
            if (TexturaEstaLista(tex)) {
                std::cout << "[EXITO] Cargado correctamente: " << fullPath << std::endl;
                return tex;
            } else {
                std::cout << "[ERROR] El archivo existe pero Raylib no pudo procesarlo: " << fullPath << std::endl;
            }
        }
    }
    std::cout << "[ERROR] No se encontro ninguna version de: " << basePath << std::endl;
    return { 0, 0, 0, 0, 0 };
}

//----------------------------------------------------------------------
// Utilidades de musica (streaming)
//----------------------------------------------------------------------
bool MusicaEstaLista(Music m) {
    return m.stream.buffer != NULL;
}

Music CargarMusicaInteligente(const std::string& basePath) {
    std::vector<std::string> extensions = { ".mp3", ".ogg", ".wav", ".MP3", ".OGG", ".WAV" };
    for (const auto& ext : extensions) {
        std::string fullPath = basePath + ext;
        if (FileExists(fullPath.c_str())) {
            std::cout << "[DEBUG] Intentando cargar musica: " << fullPath << std::endl;
            Music m = LoadMusicStream(fullPath.c_str());
            if (MusicaEstaLista(m)) {
                std::cout << "[EXITO] Musica cargada correctamente: " << fullPath << std::endl;
                return m;
            } else {
                std::cout << "[ERROR] El archivo existe pero Raylib no pudo procesar la musica: " << fullPath << std::endl;
            }
        }
    }
    std::cout << "[ERROR] No se encontro ninguna version de musica: " << basePath << std::endl;
    Music empty = { 0 };
    return empty;
}

//----------------------------------------------------------------------
// Generador de efectos de sonido sinteticos (sin archivos externos)
//----------------------------------------------------------------------
Wave GenerateToneWave(float freq, float durationSec, float volume, int type)
{
    unsigned int sampleRate = 44100;
    unsigned int frameCount = (unsigned int)(durationSec * sampleRate);
    if (frameCount < 1) frameCount = 1;
    short *data = (short *)malloc(frameCount * sizeof(short));

    for (unsigned int i = 0; i < frameCount; i++)
    {
        float t = (float)i / (float)sampleRate;
        float envelope = 1.0f - ((float)i / (float)frameCount);
        float sample = 0.0f;

        if (type == 0)
            sample = sinf(6.2831853f * freq * t);
        else if (type == 1)
            sample = (sinf(6.2831853f * freq * t) >= 0.0f) ? 1.0f : -1.0f;
        else
            sample = ((float)GetRandomValue(-1000, 1000)) / 1000.0f;

        data[i] = (short)(sample * envelope * volume * 32000.0f);
    }

    Wave wave = { 0 };
    wave.frameCount = frameCount;
    wave.sampleRate = sampleRate;
    wave.sampleSize = 16;
    wave.channels = 1;
    wave.data = data;
    return wave;
}

Sound MakeTone(float freq, float durationSec, float volume, int type)
{
    Wave wave = GenerateToneWave(freq, durationSec, volume, type);
    Sound snd = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return snd;
}

//----------------------------------------------------------------------
// Dibuja un cofre de madera (cerrado o abierto)
//----------------------------------------------------------------------
void DrawChest(float x, float y, bool opened, float t)
{
    Color wood     = { 120, 72, 40, 255 };
    Color woodDark = { 70, 42, 24, 255 };
    Color metal    = { 190, 190, 200, 255 };
    Color gold     = { 255, 215, 0, 255 };

    float w = 60.0f, h = 38.0f;

    DrawRectangleRounded({ x, y, w, h }, 0.15f, 6, wood);
    DrawRectangleLines((int)x, (int)y, (int)w, (int)h, woodDark);
    DrawRectangle((int)(x + 8), (int)y, 6, (int)h, metal);
    DrawRectangle((int)(x + w - 14), (int)y, 6, (int)h, metal);

    if (!opened)
    {
        Rectangle lid = { x - 3, y - 20, w + 6, 24 };
        DrawRectangleRounded(lid, 0.5f, 6, wood);
        DrawRectangleLines((int)lid.x, (int)lid.y, (int)lid.width, (int)lid.height, woodDark);
        DrawRectangle((int)(x + 8), (int)(y - 20), 6, 24, metal);
        DrawRectangle((int)(x + w - 14), (int)(y - 20), 6, 24, metal);
        DrawRectangle((int)(x + w / 2 - 6), (int)(y - 6), 12, 12, gold);
    }
    else
    {
        Rectangle lid = { x - 3, y, w + 6, 24 };
        Vector2 origin = { 0, 24 };
        DrawRectanglePro(lid, origin, -95.0f, wood);
        DrawRectangle((int)(x + w / 2 - 6), (int)(y - 4), 12, 12, gold);

        float bob = sinf(t * 4.0f) * 3.0f;
        DrawCircle((int)(x + w / 2 - 6), (int)(y - 30 + bob), 4, gold);
        DrawCircle((int)(x + w / 2 + 10), (int)(y - 38 + bob), 3, gold);
    }
}

//----------------------------------------------------------------------
// Dibuja un tanque con formas simples (respaldo si no hay textura)
//----------------------------------------------------------------------
void DrawTankShapes(float x, float y, float w, float h, float alpha, bool faceLeft)
{
    Color body     = Fade((Color){ 74, 84, 58, 255 }, alpha);
    Color bodyDark = Fade((Color){ 42, 48, 32, 255 }, alpha);
    Color turret   = Fade((Color){ 94, 104, 76, 255 }, alpha);
    Color track    = Fade((Color){ 30, 30, 30, 255 }, alpha);
    Color barrel   = Fade((Color){ 55, 55, 55, 255 }, alpha);

    // Orugas
    DrawRectangle((int)x, (int)(y + h - 18), (int)w, 18, track);
    for (int i = 0; i < 7; i++)
        DrawCircle((int)(x + 10 + i * (w - 20) / 6), (int)(y + h - 9), 8, Fade(BLACK, alpha));

    // Cuerpo
    Rectangle bodyRect = { x, y + h - 50, w, 40 };
    DrawRectangleRounded(bodyRect, 0.2f, 6, body);
    DrawRectangleLines((int)bodyRect.x, (int)bodyRect.y, (int)bodyRect.width, (int)bodyRect.height, bodyDark);

    // Torreta
    Rectangle turretRect = { x + w / 2 - 25, y + h - 72, 50, 26 };
    DrawRectangleRounded(turretRect, 0.3f, 6, turret);

    // Cañon (apunta segun direccion)
    if (faceLeft)
        DrawRectangle((int)(x + w / 2 - 45), (int)(y + h - 63), 45, 8, barrel);
    else
        DrawRectangle((int)(x + w / 2), (int)(y + h - 63), 45, 8, barrel);
}

//----------------------------------------------------------------------
// Dibuja el tanque: usa la textura assets/tank si esta disponible,
// y si no, cae de vuelta a las formas simples (DrawTankShapes).
// La textura del tanque original mira hacia la IZQUIERDA (cañon a la izq),
// por eso el flip se aplica cuando el tanque debe mirar a la derecha.
//----------------------------------------------------------------------
void DrawTank(Texture2D texTank, float x, float y, float w, float h, float alpha, bool faceLeft)
{
    if (TexturaEstaLista(texTank))
    {
        Rectangle src = { 0, 0, (float)texTank.width, (float)texTank.height };
        // La imagen fuente mira hacia la izquierda de forma nativa.
        if (!faceLeft) src.width = -src.width;

        Rectangle dst = { x, y + h, w, h + 30.0f }; // un poco más alto para incluir el cañón
        Vector2 origin = { 0, h + 30.0f };
        DrawTexturePro(texTank, src, dst, origin, 0.0f, Fade(WHITE, alpha));
    }
    else
    {
        DrawTankShapes(x, y, w, h, alpha, faceLeft);
    }
}

//----------------------------------------------------------------------
// Dibuja al jefe final con formas simples (respaldo si no hay textura)
// Un mago/caballero elemental flotante con un orbe sobre un baston.
//----------------------------------------------------------------------
void DrawFinalBossShapes(float x, float y, float w, float h, float alpha, bool faceLeft, float t)
{
    Color robe     = Fade((Color){ 20, 20, 25, 255 }, alpha);
    Color trim     = Fade((Color){ 230, 140, 30, 255 }, alpha);
    Color trimGold = Fade((Color){ 255, 210, 60, 255 }, alpha);
    Color skinBlue = Fade((Color){ 40, 90, 200, 255 }, alpha);
    Color orbGreen = Fade((Color){ 130, 220, 60, 255 }, alpha);

    float hover = sinf(t * 2.5f) * 6.0f;
    float cx = x + w / 2.0f;
    float baseY = y + h + hover;

    // Capa / cuerpo
    Rectangle body = { cx - w * 0.28f, baseY - h * 0.55f, w * 0.56f, h * 0.55f };
    DrawRectangleRounded(body, 0.25f, 8, robe);
    DrawRectangleLines((int)body.x, (int)body.y, (int)body.width, (int)body.height, trim);

    // Cabeza / casco
    DrawCircle((int)cx, (int)(baseY - h * 0.62f), w * 0.16f, robe);
    DrawCircleLines((int)cx, (int)(baseY - h * 0.62f), w * 0.16f, trimGold);

    // Brazo con baston (izquierda o derecha segun direccion)
    float armDir = faceLeft ? -1.0f : 1.0f;
    float armX = cx + armDir * w * 0.30f;
    DrawLineEx({ cx + armDir * w * 0.10f, baseY - h * 0.45f }, { armX, baseY - h * 0.75f }, 8.0f, skinBlue);

    // Baston
    Vector2 staffBase = { armX, baseY - h * 0.75f };
    Vector2 staffTop  = { armX, baseY - h * 1.15f };
    DrawLineEx(staffBase, staffTop, 6.0f, trim);

    // Orbe flotante en la punta del baston
    float orbPulse = 1.0f + sinf(t * 4.0f) * 0.15f;
    DrawCircle((int)staffTop.x, (int)staffTop.y, 14.0f * orbPulse, Fade(orbGreen, 0.35f));
    DrawCircle((int)staffTop.x, (int)staffTop.y, 9.0f, orbGreen);
    DrawCircleLines((int)staffTop.x, (int)staffTop.y, 9.0f, trimGold);

    // Piernas / base flotante
    DrawTriangle({ cx - w * 0.2f, baseY }, { cx + w * 0.2f, baseY }, { cx, baseY - h * 0.15f }, robe);
}

//----------------------------------------------------------------------
// Dibuja el jefe final: usa la textura assets/boss_final si esta
// disponible, y si no, cae de vuelta a las formas simples.
//----------------------------------------------------------------------
void DrawFinalBoss(Texture2D texBoss, float x, float y, float w, float h, float alpha, bool faceLeft, float t)
{
    if (TexturaEstaLista(texBoss))
    {
        float hover = sinf(t * 2.5f) * 6.0f;
        Rectangle src = { 0, 0, (float)texBoss.width, (float)texBoss.height };
        if (faceLeft) src.width = -src.width; // la imagen mira hacia la derecha de forma nativa

        Rectangle dst = { x + w / 2.0f, y + h + hover, w, h };
        Vector2 origin = { w / 2.0f, h };
        DrawTexturePro(texBoss, src, dst, origin, 0.0f, Fade(WHITE, alpha));
    }
    else
    {
        DrawFinalBossShapes(x, y, w, h, alpha, faceLeft, t);
    }
}

//----------------------------------------------------------------------
// Dibuja objetos recogibles (municion / botiquin / bolsa de monedas)
//----------------------------------------------------------------------
void DrawPickup(const ItemPickup &p, float t)
{
    float bob = sinf(t * 3.0f + p.x) * 4.0f;
    float py = p.y + bob;

    if (p.type == PICKUP_AMMO)
    {
        DrawRectangle((int)p.x, (int)py, 30, 20, (Color){ 90, 90, 70, 255 });
        DrawRectangleLines((int)p.x, (int)py, 30, 20, BLACK);
        DrawRectangle((int)p.x + 6, (int)py + 4, 4, 12, GOLD);
        DrawRectangle((int)p.x + 13, (int)py + 4, 4, 12, GOLD);
        DrawRectangle((int)p.x + 20, (int)py + 4, 4, 12, GOLD);
    }
    else if (p.type == PICKUP_HEALTH)
    {
        DrawRectangle((int)p.x, (int)py, 26, 22, WHITE);
        DrawRectangleLines((int)p.x, (int)py, 26, 22, (Color){ 180, 30, 30, 255 });
        DrawRectangle((int)p.x + 11, (int)py + 4, 4, 14, RED);
        DrawRectangle((int)p.x + 5, (int)py + 9, 16, 4, RED);
    }
    else // PICKUP_COIN
    {
        DrawCircle((int)p.x + 6, (int)py + 6, 9, Fade(GOLD, 0.9f));
        DrawCircle((int)p.x + 14, (int)py + 2, 9, GOLD);
        DrawCircle((int)p.x + 22, (int)py + 6, 9, Fade(GOLD, 0.9f));
    }
}

//----------------------------------------------------------------------
// Carga el contenido de un nivel (enemigos, tanque, cofres, objetos...)
// No reinicia monedas, granadas, balas ni arma (eso persiste entre niveles).
//----------------------------------------------------------------------
void LoadLevel(GameState &s, int levelNum)
{
    s.level = levelNum;
    s.x = 100.0f; s.y = GROUND_Y;
    s.velocityY = 0; s.isJumping = false; s.isCrouching = false; s.facingRight = true;
    s.health = s.maxHealth; // curacion completa al entrar a un nivel nuevo
    s.hurtCooldown = 0.0f;
    s.isDead = false; s.deathTimer = 0.0f;
    s.gameComplete = false;
    s.showLevelBanner = true; s.levelBannerTimer = 3.0f;

    s.bullets.clear();
    s.thrownGrenades.clear();
    s.shells.clear();

    float diff = (levelNum == 2) ? 1.4f : (levelNum == 3) ? 1.6f : 1.0f;

    s.bossOrbs.clear();

    if (levelNum == 3)
    {
        // ---------------- Nivel 3: pocos zombies, sin tanque, jefe final ----------------
        s.zombies.clear();
        float zX3[] = { 500, 1100, 1750, 2350 };
        for (int i = 0; i < 4; i++)
        {
            float sp = (1.0f + i * 0.12f) * diff;
            int hp = (int)((35 + i * 4) * diff);
            s.zombies.push_back({ zX3[i], GROUND_Y, hp, true, sp });
        }

        // Sin tanque en este nivel: el jefe final ocupa su lugar
        s.tank = Tank();
        s.tank.alive = false;

        s.finalBoss = FinalBoss();
        s.finalBoss.x = 3100.0f;
        s.finalBoss.y = GROUND_Y + PLAYER_H - s.finalBoss.height + 40.0f;
        s.finalBoss.maxHealth = (int)(420 * (diff / 1.6f));
        s.finalBoss.health = s.finalBoss.maxHealth;
        s.finalBoss.alive = true;
        s.finalBoss.dying = false;
        s.finalBoss.triggered = false;
        s.finalBoss.deathTimer = 0.0f;
        s.finalBoss.speed = 0.55f;
        s.finalBoss.attackCooldown = 1.6f;
    }
    else
    {
        // ---------------- Zombies ----------------
        s.zombies.clear();
        float zX[] = { 400, 750, 1150, 1550, 1950, 2350, 2750 };
        for (int i = 0; i < 7; i++)
        {
            float sp = (0.9f + i * 0.1f) * diff;
            int hp = (int)((30 + i * 3) * diff);
            s.zombies.push_back({ zX[i], GROUND_Y, hp, true, sp });
        }

        // ---------------- Tanque (mini-jefe, uno por nivel) ----------------
        s.tank = Tank();
        s.tank.x = 3100.0f;
        s.tank.y = GROUND_Y + PLAYER_H - 70.0f;
        s.tank.maxHealth = (int)(180 * diff);
        s.tank.health = s.tank.maxHealth;
        s.tank.alive = true;
        s.tank.dying = false;
        s.tank.deathTimer = 0.0f;
        s.tank.speed = 0.35f * diff;
        s.tank.shootCooldown = 2.0f;

        // No hay jefe final en niveles 1 y 2
        s.finalBoss = FinalBoss();
        s.finalBoss.alive = false;
    }

    // ---------------- Cofres (5 por nivel) ----------------
    s.boxes.clear();
    float chestX[] = { 500, 1200, 1900, 2600, 3150 };
    for (float cx : chestX)
        s.boxes.push_back({ cx, GROUND_Y + PLAYER_H - 62.0f, false });

    // ---------------- Armas para recoger ----------------
    s.weaponPickups.clear();
    float wpX[] = { 800, 1800, 2800 };
    for (int i = 0; i < 3; i++)
        s.weaponPickups.push_back({ wpX[i], GROUND_Y + PLAYER_H - 35.0f, (i % 2 == 0) ? WEAPON_SHOTGUN : WEAPON_PISTOL, false });

    // ---------------- Objetos automaticos: municion / vida / monedas ----------------
    s.pickups.clear();
    float ammoX[]   = { 300, 1400, 2450 };
    float healthX[] = { 650, 1700, 2900 };
    float coinX[]   = { 1000, 2200 };
    for (float ax : ammoX)   s.pickups.push_back({ ax,  GROUND_Y + PLAYER_H - 25.0f, PICKUP_AMMO,   false });
    for (float hx : healthX) s.pickups.push_back({ hx,  GROUND_Y + PLAYER_H - 25.0f, PICKUP_HEALTH, false });
    for (float cx : coinX)   s.pickups.push_back({ cx,  GROUND_Y + PLAYER_H - 25.0f, PICKUP_COIN,   false });

    // ---------------- Camara ----------------
    s.camera.target = { std::max(0.0f, s.x - 300.0f), 0 };
    s.camera.offset = { 300, 0 };
    s.camera.rotation = 0.0f;
    s.camera.zoom = 1.0f;
}

//----------------------------------------------------------------------
// Empieza una partida desde cero (reinicia monedas, granadas, balas, arma)
//----------------------------------------------------------------------
void StartNewGame(GameState &s, int startLevel)
{
    s.maxHealth = 100;
    s.currentWeapon = WEAPON_PISTOL;
    s.coins = 0;
    s.grenadeCount = 3;
    s.bulletCount = 30;
    s.animTimer = 0.0f;
    LoadLevel(s, startLevel);
}

int main()
{
    InitWindow(SCREEN_W, SCREEN_H, "Metal Mini");
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    InitAudioDevice();

    std::cout << "=== DIAGNOSTICO DE INICIO ===" << std::endl;
    std::cout << "Directorio actual al iniciar: " << GetWorkingDirectory() << std::endl;
    if (DirectoryExists("assets")) {
        std::cout << "[INFO] Se detecto la carpeta 'assets' en el directorio actual." << std::endl;
    } else if (DirectoryExists("../assets")) {
        std::cout << "[INFO] Se detecto 'assets' un nivel arriba. Cambiando directorio..." << std::endl;
        ChangeDirectory("..");
        std::cout << "Nuevo directorio de trabajo: " << GetWorkingDirectory() << std::endl;
    } else {
        std::cout << "[ALERTA] No se detecta la carpeta 'assets' ni en el directorio actual ni un nivel arriba." << std::endl;
    }
    std::cout << "=============================" << std::endl;

    Texture2D texPlayer     = CargarTexturaInteligente("assets/player");
    Texture2D texZombie     = CargarTexturaInteligente("assets/zombie");
    Texture2D texPistol     = CargarTexturaInteligente("assets/weapon_pistol");
    Texture2D texShotgun    = CargarTexturaInteligente("assets/weapon_shotgun");
    Texture2D texGrenade    = CargarTexturaInteligente("assets/grenade");
    Texture2D texBackground = CargarTexturaInteligente("assets/background");
    Texture2D texTank       = CargarTexturaInteligente("assets/tank");
    Texture2D texBossFinal  = CargarTexturaInteligente("assets/boss_final");
    Texture2D texIntro      = CargarTexturaInteligente("assets/intro_atlas");

    Music musicBackground = CargarMusicaInteligente("assets/music_background");
    Music musicBossFight  = CargarMusicaInteligente("assets/music_boss");
    Music musicIntro      = CargarMusicaInteligente("assets/intro_audio");
    if (MusicaEstaLista(musicBackground)) { musicBackground.looping = true; SetMusicVolume(musicBackground, 0.5f); }
    if (MusicaEstaLista(musicBossFight))  { musicBossFight.looping = true;  SetMusicVolume(musicBossFight, 0.6f); }
    if (MusicaEstaLista(musicIntro))      { musicIntro.looping = false;    SetMusicVolume(musicIntro, 0.8f); }

    Sound sndShoot       = MakeTone(950.0f, 0.07f, 0.35f, 1);
    Sound sndShotgun     = MakeTone(300.0f, 0.15f, 0.45f, 2);
    Sound sndExplosion   = MakeTone(110.0f, 0.45f, 0.6f, 2);
    Sound sndCoin        = MakeTone(1300.0f, 0.10f, 0.35f, 0);
    Sound sndHurt        = MakeTone(160.0f, 0.20f, 0.45f, 1);
    Sound sndJump        = MakeTone(500.0f, 0.12f, 0.30f, 0);
    Sound sndMenuMove    = MakeTone(700.0f, 0.05f, 0.25f, 0);
    Sound sndMenuSelect  = MakeTone(1000.0f, 0.10f, 0.35f, 0);
    Sound sndZombieDeath = MakeTone(150.0f, 0.25f, 0.40f, 2);
    Sound sndPickup      = MakeTone(850.0f, 0.08f, 0.30f, 0);
    Sound sndEmpty       = MakeTone(200.0f, 0.05f, 0.20f, 1);
    Sound sndVictory     = MakeTone(1046.5f, 0.5f, 0.5f, 0);

    float speed = 5.0f;

    GameState s;
    StartNewGame(s, 1);

    Screen screen = SCREEN_INTRO;
    float introTimer = 0.0f;
    if (TexturaEstaLista(texIntro))
    {
        if (MusicaEstaLista(musicIntro)) PlayMusicStream(musicIntro);
    }
    else
    {
        // No hay video de intro disponible: saltamos directo al menu
        screen = SCREEN_MENU;
        if (MusicaEstaLista(musicBackground)) PlayMusicStream(musicBackground);
    }
    int menuIndex = 0;
    const char *menuOptions[] = { "Nivel 1", "Nivel 2", "Nivel 3 (Jefe Final)", "Salir" };
    const int MENU_OPTION_COUNT = 4;
    bool shouldExit = false;
    bool victorySoundPlayed = false;

    while (!WindowShouldClose() && !shouldExit)
    {
        float dt = GetFrameTime();
        bool isMoving = false;

        if (MusicaEstaLista(musicBackground)) UpdateMusicStream(musicBackground);
        if (MusicaEstaLista(musicBossFight))  UpdateMusicStream(musicBossFight);
        if (MusicaEstaLista(musicIntro))      UpdateMusicStream(musicIntro);

        // ================================================================
        // INTRO (video de bienvenida)
        // ================================================================
        if (screen == SCREEN_INTRO)
        {
            introTimer += dt;
            bool skipPressed = IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_Z) || IsKeyPressed(KEY_SPACE) ||
                                IsKeyPressed(KEY_ESCAPE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
            bool finished = introTimer >= (float)INTRO_FRAME_COUNT / INTRO_FPS;

            if (skipPressed || finished)
            {
                screen = SCREEN_MENU;
                if (MusicaEstaLista(musicIntro)) StopMusicStream(musicIntro);
                if (MusicaEstaLista(musicBackground)) PlayMusicStream(musicBackground);
            }
        }
        // ================================================================
        // MENU
        // ================================================================
        else if (screen == SCREEN_MENU)
        {
            if (IsKeyPressed(KEY_DOWN)) { menuIndex = (menuIndex + 1) % MENU_OPTION_COUNT; PlaySound(sndMenuMove); }
            if (IsKeyPressed(KEY_UP))   { menuIndex = (menuIndex + MENU_OPTION_COUNT - 1) % MENU_OPTION_COUNT; PlaySound(sndMenuMove); }

            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_Z))
            {
                PlaySound(sndMenuSelect);
                if (menuIndex == 0) { StartNewGame(s, 1); screen = SCREEN_PLAYING; victorySoundPlayed = false; }
                else if (menuIndex == 1) { StartNewGame(s, 2); screen = SCREEN_PLAYING; victorySoundPlayed = false; }
                else if (menuIndex == 2)
                {
                    StartNewGame(s, 3); screen = SCREEN_PLAYING; victorySoundPlayed = false;
                    // Al entrar directo al nivel del jefe, aseguramos que suene la musica normal
                    if (MusicaEstaLista(musicBossFight)) StopMusicStream(musicBossFight);
                    if (MusicaEstaLista(musicBackground) && !IsMusicStreamPlaying(musicBackground)) PlayMusicStream(musicBackground);
                }
                else { shouldExit = true; }
            }
        }
        // ================================================================
        // JUGANDO
        // ================================================================
        else if (screen == SCREEN_PLAYING)
        {
            if (s.gameComplete)
            {
                if (!victorySoundPlayed) { PlaySound(sndVictory); victorySoundPlayed = true; }
                if (IsKeyPressed(KEY_M))
                {
                    screen = SCREEN_MENU;
                    if (MusicaEstaLista(musicBossFight)) StopMusicStream(musicBossFight);
                    if (MusicaEstaLista(musicBackground) && !IsMusicStreamPlaying(musicBackground)) PlayMusicStream(musicBackground);
                }
            }
            else if (s.isDead)
            {
                s.deathTimer += dt;
                if (IsKeyPressed(KEY_R))
                {
                    LoadLevel(s, s.level);
                    if (MusicaEstaLista(musicBossFight)) StopMusicStream(musicBossFight);
                    if (MusicaEstaLista(musicBackground) && !IsMusicStreamPlaying(musicBackground)) PlayMusicStream(musicBackground);
                }
                if (IsKeyPressed(KEY_M))
                {
                    screen = SCREEN_MENU;
                    if (MusicaEstaLista(musicBossFight)) StopMusicStream(musicBossFight);
                    if (MusicaEstaLista(musicBackground) && !IsMusicStreamPlaying(musicBackground)) PlayMusicStream(musicBackground);
                }
            }
            else
            {
                if (IsKeyPressed(KEY_ESCAPE))
                {
                    screen = SCREEN_MENU;
                    if (MusicaEstaLista(musicBossFight)) StopMusicStream(musicBossFight);
                    if (MusicaEstaLista(musicBackground) && !IsMusicStreamPlaying(musicBackground)) PlayMusicStream(musicBackground);
                }

                bool downHeld = IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S);
                s.isCrouching = downHeld && !s.isJumping;

                if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_SPACE)) && !s.isJumping && !s.isCrouching)
                {
                    s.velocityY = -12;
                    s.isJumping = true;
                    PlaySound(sndJump);
                }

                if (s.shootCooldown > 0) s.shootCooldown -= dt;
                if (IsKeyPressed(KEY_Z) && s.shootCooldown <= 0)
                {
                    int cost = (s.currentWeapon == WEAPON_SHOTGUN) ? 3 : 1;
                    if (s.bulletCount >= cost)
                    {
                        int dir = s.facingRight ? 1 : -1;
                        float shootY = s.isCrouching ? s.y + 70 : s.y + 45;
                        if (s.currentWeapon == WEAPON_SHOTGUN)
                        {
                            for (int i = -1; i <= 1; i++)
                                s.bullets.push_back({ s.x + (s.facingRight ? 80.0f : 10.0f), shootY + i * 8, 10.0f, dir, true });
                            s.shootCooldown = 0.6f;
                            PlaySound(sndShotgun);
                        }
                        else
                        {
                            s.bullets.push_back({ s.x + (s.facingRight ? 80.0f : 10.0f), shootY, 13.0f, dir, true });
                            s.shootCooldown = 0.25f;
                            PlaySound(sndShoot);
                        }
                        s.bulletCount -= cost;
                    }
                    else
                    {
                        s.shootCooldown = 0.3f;
                        PlaySound(sndEmpty);
                    }
                }

                if (IsKeyPressed(KEY_G) && s.grenadeCount > 0)
                {
                    s.thrownGrenades.push_back({ s.x + 45, s.y + 20, s.facingRight ? 7.0f : -7.0f, -8.0f, 1.5f, true });
                    s.grenadeCount--;
                }

                if (IsKeyPressed(KEY_E))
                {
                    for (auto &w : s.weaponPickups)
                    {
                        if (!w.taken && fabsf(w.x - s.x) < 60)
                        {
                            w.taken = true;
                            s.currentWeapon = w.type;
                            PlaySound(sndPickup);
                        }
                    }
                }

                // Recoleccion automatica: municion, vida, monedas
                for (auto &p : s.pickups)
                {
                    if (!p.taken && fabsf(p.x - s.x) < 40 && fabsf(p.y - s.y) < 90)
                    {
                        p.taken = true;
                        if (p.type == PICKUP_AMMO)        { s.bulletCount = std::min(MAX_BULLETS, s.bulletCount + 25); PlaySound(sndPickup); }
                        else if (p.type == PICKUP_HEALTH) { s.health = std::min(s.maxHealth, s.health + 10); PlaySound(sndPickup); }
                        else                               { s.coins += 40; PlaySound(sndCoin); }
                    }
                }

                float moveSpeed = s.isCrouching ? speed * 0.5f : speed;
                if (IsKeyDown(KEY_RIGHT)) { s.x += moveSpeed; s.facingRight = true; isMoving = true; }
                if (IsKeyDown(KEY_LEFT))  { s.x -= moveSpeed; s.facingRight = false; isMoving = true; }
                if (s.x < 0) s.x = 0;
                if (s.x > LEVEL_LENGTH - PLAYER_W) s.x = LEVEL_LENGTH - PLAYER_W;
                if (s.level == 3 && s.finalBoss.alive && !s.finalBoss.dying)
                {
                    float bossGateX = s.finalBoss.x - 130.0f;
                    if (s.x > bossGateX) s.x = bossGateX;
                }

                s.y += s.velocityY;
                s.velocityY += 0.5f;
                if (s.y >= GROUND_Y) { s.y = GROUND_Y; s.velocityY = 0; s.isJumping = false; }

                float animSpeed = isMoving && !s.isCrouching ? 10.0f
                                 : isMoving && s.isCrouching  ? 5.0f
                                 : 2.5f;
                s.animTimer += dt * animSpeed;

                // ---------------- Balas ----------------
                for (auto &b : s.bullets)
                {
                    if (!b.active) continue;
                    b.x += b.speed * b.dir;
                    if (b.x < s.camera.target.x - 400 || b.x > s.camera.target.x + 1600) b.active = false;

                    for (auto &z : s.zombies)
                    {
                        if (z.alive && !z.dying && fabsf(b.x - z.x) < 45 && fabsf(b.y - (z.y + 50)) < 60)
                        {
                            z.health -= (s.currentWeapon == WEAPON_SHOTGUN) ? 20 : 15;
                            b.active = false;
                            if (z.health <= 0) { z.dying = true; z.deathTimer = 0.0f; PlaySound(sndZombieDeath); }
                        }
                    }

                    if (s.tank.alive && !s.tank.dying && b.active &&
                        fabsf(b.x - s.tank.x) < 75 && fabsf(b.y - (s.tank.y + 35)) < 40)
                    {
                        s.tank.health -= (s.currentWeapon == WEAPON_SHOTGUN) ? 20 : 15;
                        b.active = false;
                        if (s.tank.health <= 0) { s.tank.dying = true; s.tank.deathTimer = 0.0f; PlaySound(sndZombieDeath); }
                    }

                    if (s.finalBoss.alive && !s.finalBoss.dying && b.active &&
                        fabsf(b.x - s.finalBoss.x) < 70 && fabsf(b.y - (s.finalBoss.y + 80)) < 100)
                    {
                        s.finalBoss.health -= (s.currentWeapon == WEAPON_SHOTGUN) ? 20 : 15;
                        b.active = false;
                        if (s.finalBoss.health <= 0) { s.finalBoss.dying = true; s.finalBoss.deathTimer = 0.0f; PlaySound(sndZombieDeath); }
                    }

                    for (auto &box : s.boxes)
                    {
                        if (!box.opened && CheckCollisionCircleRec({ b.x, b.y }, 5, box.GetRect()))
                        {
                            box.opened = true;
                            s.coins += 10;
                            s.grenadeCount += 1;
                            b.active = false;
                            PlaySound(sndCoin);
                        }
                    }
                }

                // ---------------- Zombies ----------------
                if (s.hurtCooldown > 0) s.hurtCooldown -= dt;
                for (auto &z : s.zombies)
                {
                    if (!z.alive) continue;
                    if (z.dying)
                    {
                        z.deathTimer += dt;
                        if (z.deathTimer > 0.5f) z.alive = false;
                        continue;
                    }
                    if (z.x > s.x) z.x -= z.speed; else z.x += z.speed;
                    if (fabsf(z.x - s.x) < 50 && s.hurtCooldown <= 0)
                    {
                        s.health -= 10;
                        s.hurtCooldown = 1.0f;
                        if (s.health < 0) s.health = 0;
                        PlaySound(sndHurt);
                    }
                }

                // ---------------- Tanque ----------------
                if (s.tank.alive)
                {
                    if (s.tank.dying)
                    {
                        s.tank.deathTimer += dt;
                        if (s.tank.deathTimer > 1.0f) s.tank.alive = false;
                    }
                    else
                    {
                        if (s.tank.x > s.x) s.tank.x -= s.tank.speed; else s.tank.x += s.tank.speed;

                        if (fabsf(s.tank.x - s.x) < 80 && s.hurtCooldown <= 0)
                        {
                            s.health -= 20;
                            s.hurtCooldown = 1.0f;
                            if (s.health < 0) s.health = 0;
                            PlaySound(sndHurt);
                        }

                        s.tank.shootCooldown -= dt;
                        if (s.tank.shootCooldown <= 0 && fabsf(s.tank.x - s.x) < 900)
                        {
                            int dir = (s.x < s.tank.x) ? -1 : 1;
                            s.shells.push_back({ s.tank.x, s.tank.y + 25, 6.0f, dir, true });
                            s.tank.shootCooldown = 2.2f;
                        }
                    }
                }

                // ---------------- Jefe final (nivel 3) ----------------
                if (s.level == 3 && s.finalBoss.alive)
                {
                    // Activa el combate y cambia la musica cuando el jugador se acerca
                    if (!s.finalBoss.triggered && s.x > 2650.0f)
                    {
                        s.finalBoss.triggered = true;
                        s.showLevelBanner = true;
                        s.levelBannerTimer = 3.0f;
                        if (MusicaEstaLista(musicBackground)) PauseMusicStream(musicBackground);
                        if (MusicaEstaLista(musicBossFight)) PlayMusicStream(musicBossFight);
                    }

                    if (s.finalBoss.dying)
                    {
                        s.finalBoss.deathTimer += dt;
                        if (s.finalBoss.deathTimer > 1.2f)
                        {
                            s.finalBoss.alive = false;
                            if (MusicaEstaLista(musicBossFight)) StopMusicStream(musicBossFight);
                            if (MusicaEstaLista(musicBackground)) ResumeMusicStream(musicBackground);
                        }
                    }
                    else if (s.finalBoss.triggered)
                    {
                        float dx = s.x - s.finalBoss.x;
                        if (fabsf(dx) > 260.0f) s.finalBoss.x += (dx > 0 ? 1.0f : -1.0f) * s.finalBoss.speed;

                        s.finalBoss.attackCooldown -= dt;
                        if (s.finalBoss.attackCooldown <= 0 && fabsf(dx) < 950.0f)
                        {
                            int dir = (dx > 0) ? 1 : -1;
                            s.bossOrbs.push_back({ s.finalBoss.x, s.finalBoss.y + 40.0f, dir * 5.5f, -3.0f, true });
                            s.finalBoss.attackCooldown = 1.5f;
                        }

                        if (fabsf(dx) < 90.0f && s.hurtCooldown <= 0)
                        {
                            s.health -= 15;
                            s.hurtCooldown = 1.0f;
                            if (s.health < 0) s.health = 0;
                            PlaySound(sndHurt);
                        }
                    }
                }

                // ---------------- Orbes magicos del jefe final ----------------
                for (auto &o : s.bossOrbs)
                {
                    if (!o.active) continue;
                    o.x += o.vx;
                    o.y += o.vy;
                    o.vy += 0.18f; // leve arco de gravedad
                    if (o.x < s.camera.target.x - 400 || o.x > s.camera.target.x + 1600 || o.y > GROUND_Y + 150)
                        o.active = false;

                    if (o.active && fabsf(o.x - s.x) < 35 && fabsf(o.y - (s.y + 45)) < 55)
                    {
                        s.health -= 18;
                        if (s.health < 0) s.health = 0;
                        o.active = false;
                        PlaySound(sndHurt);
                    }
                }

                // ---------------- Proyectiles del tanque ----------------
                for (auto &sh : s.shells)
                {
                    if (!sh.active) continue;
                    sh.x += sh.speed * sh.dir;
                    if (sh.x < s.camera.target.x - 400 || sh.x > s.camera.target.x + 1600) sh.active = false;

                    if (sh.active && fabsf(sh.x - s.x) < 30 && fabsf(sh.y - (s.y + 45)) < 50)
                    {
                        s.health -= 20;
                        if (s.health < 0) s.health = 0;
                        sh.active = false;
                        PlaySound(sndHurt);
                    }
                }

                // ---------------- Granadas ----------------
                for (auto &gr : s.thrownGrenades)
                {
                    if (!gr.active) continue;
                    gr.x += gr.vx;
                    gr.y += gr.vy;
                    gr.vy += 0.5f;
                    if (gr.y >= GROUND_Y + 70) { gr.y = GROUND_Y + 70; gr.vy = 0; gr.vx = 0; }
                    gr.timer -= dt;
                    if (gr.timer <= 0)
                    {
                        gr.active = false;
                        PlaySound(sndExplosion);
                        for (auto &z : s.zombies)
                        {
                            if (z.alive && !z.dying && fabsf(z.x - gr.x) < 120)
                            {
                                z.health -= 100;
                                if (z.health <= 0) { z.dying = true; z.deathTimer = 0.0f; }
                            }
                        }
                        if (s.tank.alive && !s.tank.dying && fabsf(s.tank.x - gr.x) < 160)
                        {
                            s.tank.health -= 80;
                            if (s.tank.health <= 0) { s.tank.dying = true; s.tank.deathTimer = 0.0f; }
                        }
                        if (s.finalBoss.alive && !s.finalBoss.dying && fabsf(s.finalBoss.x - gr.x) < 180)
                        {
                            s.finalBoss.health -= 90;
                            if (s.finalBoss.health <= 0) { s.finalBoss.dying = true; s.finalBoss.deathTimer = 0.0f; }
                        }
                    }
                }

                // ---------------- Camara ----------------
                s.camera.target.x = s.x - 300;
                if (s.camera.target.x < 0) s.camera.target.x = 0;
                if (s.camera.target.x > LEVEL_LENGTH - SCREEN_W) s.camera.target.x = LEVEL_LENGTH - SCREEN_W;

                if (s.showLevelBanner)
                {
                    s.levelBannerTimer -= dt;
                    if (s.levelBannerTimer <= 0) s.showLevelBanner = false;
                }

                // ---------------- Fin del nivel ----------------
                if (s.x >= LEVEL_LENGTH - PLAYER_W - 5)
                {
                    if (s.level < LAST_LEVEL)
                    {
                        LoadLevel(s, s.level + 1);
                    }
                    else
                    {
                        s.gameComplete = true;
                        victorySoundPlayed = false;
                    }
                }

                if (s.health <= 0)
                {
                    s.isDead = true;
                    s.deathTimer = 0.0f;
                }
            }
        }

        // ================================================================
        // RENDERING
        // ================================================================
        BeginDrawing();
        ClearBackground(DARKGRAY);

        if (screen == SCREEN_INTRO)
        {
            ClearBackground(BLACK);

            if (TexturaEstaLista(texIntro))
            {
                int frame = (int)(introTimer * INTRO_FPS);
                if (frame >= INTRO_FRAME_COUNT) frame = INTRO_FRAME_COUNT - 1;
                if (frame < 0) frame = 0;

                int col = frame % INTRO_COLS;
                int row = frame / INTRO_COLS;
                float cellW = (float)texIntro.width / INTRO_COLS;
                float cellH = (float)texIntro.height / INTRO_ROWS;

                Rectangle src = { col * cellW, row * cellH, cellW, cellH };
                Rectangle dst = { 0, 0, (float)SCREEN_W, (float)SCREEN_H };
                DrawTexturePro(texIntro, src, dst, { 0, 0 }, 0.0f, WHITE);
            }

            if (introTimer > 1.0f)
            {
                const char *skipMsg = "Presiona cualquier tecla para saltar";
                int smw = MeasureText(skipMsg, 20);
                float fade = std::min(1.0f, (introTimer - 1.0f) * 2.0f);
                DrawText(skipMsg, SCREEN_W / 2 - smw / 2, SCREEN_H - 45, 20, Fade(WHITE, fade * 0.85f));
            }
        }
        else if (screen == SCREEN_MENU)
        {
            if (TexturaEstaLista(texBackground))
            {
                DrawTexturePro(texBackground,
                    { 0, 0, (float)texBackground.width, (float)texBackground.height },
                    { 0, 0, (float)SCREEN_W, (float)SCREEN_H }, { 0, 0 }, 0, WHITE);
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 0, 0, 0, 140 });
            }

            const char *title = "METAL MINI";
            int titleSize = 70;
            int tw = MeasureText(title, titleSize);
            DrawText(title, SCREEN_W / 2 - tw / 2, 120, titleSize, GOLD);

            for (int i = 0; i < MENU_OPTION_COUNT; i++)
            {
                bool sel = (i == menuIndex);
                int fs = sel ? 34 : 28;
                Color c = sel ? YELLOW : LIGHTGRAY;
                std::string label = (sel ? "> " : "  ") + std::string(menuOptions[i]) + (sel ? " <" : "");
                int w = MeasureText(label.c_str(), fs);
                DrawText(label.c_str(), SCREEN_W / 2 - w / 2, 290 + i * 55, fs, c);
            }

            const char *help = "Flechas ARRIBA/ABAJO: elegir  |  ENTER: confirmar";
            int hw = MeasureText(help, 20);
            DrawText(help, SCREEN_W / 2 - hw / 2, 600, 20, LIGHTGRAY);
        }
        else // SCREEN_PLAYING
        {
            if (TexturaEstaLista(texBackground))
            {
                float bgTile = (float)SCREEN_W;
                float bgX = -s.camera.target.x * 0.4f;
                float startTile = fmodf(bgX, bgTile);
                if (startTile > 0) startTile -= bgTile;
                for (float tx = startTile; tx < SCREEN_W; tx += bgTile)
                {
                    DrawTexturePro(texBackground,
                        { 0, 0, (float)texBackground.width, (float)texBackground.height },
                        { tx, 0, bgTile, (float)SCREEN_H }, { 0, 0 }, 0, WHITE);
                }
            }
            else
            {
                DrawRectangle(-1000, (int)GROUND_Y + 110, (int)LEVEL_LENGTH + 2000, 150, (Color){ 80, 60, 40, 255 });
            }

            if (s.level == 2)
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 140, 20, 20, 45 });
            else if (s.level == 3)
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 60, 20, 110, 55 });

            BeginMode2D(s.camera);

            DrawRectangle(-500, (int)GROUND_Y + 110, (int)LEVEL_LENGTH + 1000, 150, (Color){ 60, 45, 30, 200 });

            for (auto &box : s.boxes)
                DrawChest(box.x, box.y, box.opened, (float)GetTime());

            for (auto &p : s.pickups)
                if (!p.taken) DrawPickup(p, (float)GetTime());

            for (auto &w : s.weaponPickups)
            {
                if (!w.taken)
                {
                    Texture2D &t = (w.type == WEAPON_SHOTGUN) ? texShotgun : texPistol;
                    if (TexturaEstaLista(t))
                        DrawTexturePro(t, { 0, 0, (float)t.width, (float)t.height },
                            { w.x, w.y, w.width, w.height }, { 0, 0 }, 0, WHITE);
                    else
                        DrawRectangle((int)w.x, (int)w.y + 15, 40, 15, ORANGE);
                }
            }

            for (auto &z : s.zombies)
            {
                if (!z.alive) continue;
                bool zFacingRight = !(z.x > s.x);
                float zRot = 0.0f, zAlpha = 1.0f, zBob = 0.0f;

                if (z.dying)
                {
                    zRot = std::min(z.deathTimer * 300.0f, 90.0f) * (zFacingRight ? 1.0f : -1.0f);
                    zAlpha = std::max(0.0f, 1.0f - z.deathTimer * 2.0f);
                }
                else
                {
                    zBob = sinf((float)GetTime() * 4.0f + z.x) * 3.0f;
                }

                if (TexturaEstaLista(texZombie))
                {
                    Rectangle src = { 0, 0, (float)texZombie.width, (float)texZombie.height };
                    if (!zFacingRight) src.width = -src.width;
                    Rectangle dst = { z.x + z.width / 2, z.y + z.height + zBob, z.width, z.height };
                    Vector2 origin = { z.width / 2, z.height };
                    DrawTexturePro(texZombie, src, dst, origin, zRot, Fade(WHITE, zAlpha));
                }
                else
                {
                    DrawRectangle((int)z.x, (int)(z.y + zBob), (int)z.width, (int)z.height, Fade(LIME, zAlpha));
                }
            }

            // Tanque
            if (s.tank.alive)
            {
                float tAlpha = s.tank.dying ? std::max(0.0f, 1.0f - s.tank.deathTimer) : 1.0f;
                bool faceLeft = (s.tank.x > s.x);
                DrawTank(texTank, s.tank.x, s.tank.y, s.tank.width, s.tank.height, tAlpha, faceLeft);

                if (!s.tank.dying)
                {
                    float barW = s.tank.width;
                    DrawRectangle((int)s.tank.x, (int)(s.tank.y - 14), (int)barW, 8, BLACK);
                    DrawRectangle((int)s.tank.x + 1, (int)(s.tank.y - 13), (int)((barW - 2) * ((float)s.tank.health / s.tank.maxHealth)), 6, RED);
                }
            }

            // Proyectiles del tanque
            for (auto &sh : s.shells)
                if (sh.active) DrawCircle((int)sh.x, (int)sh.y, 6, (Color){ 40, 40, 40, 255 });

            // Jefe final
            if (s.finalBoss.alive)
            {
                float bAlpha = s.finalBoss.dying ? std::max(0.0f, 1.0f - s.finalBoss.deathTimer) : 1.0f;
                bool faceLeft = (s.finalBoss.x > s.x);
                DrawFinalBoss(texBossFinal, s.finalBoss.x, s.finalBoss.y, s.finalBoss.width, s.finalBoss.height, bAlpha, faceLeft, (float)GetTime());

                if (!s.finalBoss.dying)
                {
                    float barW = s.finalBoss.width + 20.0f;
                    float barX = s.finalBoss.x - 10.0f;
                    float barY = s.finalBoss.y - 24.0f;
                    DrawRectangle((int)barX, (int)barY, (int)barW, 10, BLACK);
                    DrawRectangle((int)barX + 1, (int)barY + 1, (int)((barW - 2) * ((float)s.finalBoss.health / s.finalBoss.maxHealth)), 8, (Color){ 190, 60, 220, 255 });
                    const char *bossLabel = "JEFE FINAL";
                    int blw = MeasureText(bossLabel, 16);
                    DrawText(bossLabel, (int)(s.finalBoss.x + s.finalBoss.width / 2 - blw / 2), (int)(barY - 20), 16, (Color){ 220, 150, 255, 255 });
                }
            }

            // Orbes magicos del jefe final
            for (auto &o : s.bossOrbs)
            {
                if (!o.active) continue;
                DrawCircle((int)o.x, (int)o.y, o.radius + 4.0f, Fade((Color){ 130, 220, 60, 255 }, 0.3f));
                DrawCircle((int)o.x, (int)o.y, o.radius, (Color){ 130, 220, 60, 255 });
            }

            // Jugador
            {
                float drawH = s.isCrouching ? PLAYER_H * 0.6f : PLAYER_H;
                float feetY = s.y + PLAYER_H;
                float bob = 0.0f, lean = 0.0f, alpha = 1.0f;

                if (s.isDead)
                {
                    lean = std::min(s.deathTimer * 140.0f, 90.0f) * (s.facingRight ? 1.0f : -1.0f);
                    alpha = std::max(0.0f, 1.0f - s.deathTimer * 0.5f);
                    feetY += std::min(s.deathTimer * 20.0f, 15.0f);
                }
                else if (!s.isJumping && !s.isCrouching)
                {
                    bob = isMoving ? sinf(s.animTimer) * 5.0f : sinf(s.animTimer) * 2.0f;
                    lean = isMoving ? sinf(s.animTimer) * 4.0f : 0.0f;
                }

                if (TexturaEstaLista(texPlayer))
                {
                    Rectangle src = { 0, 0, (float)texPlayer.width, (float)texPlayer.height };
                    if (!s.facingRight) src.width = -src.width;
                    Rectangle dst = { s.x + PLAYER_W / 2, feetY + bob, PLAYER_W, drawH };
                    Vector2 origin = { PLAYER_W / 2, drawH };
                    DrawTexturePro(texPlayer, src, dst, origin, lean, Fade(WHITE, alpha));
                }
                else
                {
                    Rectangle dst = { s.x + PLAYER_W / 2, feetY + bob, PLAYER_W, drawH };
                    Vector2 origin = { PLAYER_W / 2, drawH };
                    DrawRectanglePro(dst, origin, lean, Fade(BLUE, alpha));
                }

                if (!s.isDead)
                {
                    Texture2D &t = (s.currentWeapon == WEAPON_SHOTGUN) ? texShotgun : texPistol;
                    if (TexturaEstaLista(t))
                    {
                        float wx = s.facingRight ? s.x + 45 : s.x - 15;
                        float wy = s.isCrouching ? s.y + 70 : s.y + 45;
                        Rectangle src = { 0, 0, (float)t.width, (float)t.height };
                        if (!s.facingRight) src.width = -src.width;
                        DrawTexturePro(t, src, { wx, wy, 60, 30 }, { 0, 0 }, 0, WHITE);
                    }
                }
            }

            for (auto &b : s.bullets)
                if (b.active) DrawRectangle((int)b.x, (int)b.y, 12, 6, GOLD);

            for (auto &gr : s.thrownGrenades)
            {
                if (!gr.active) continue;
                if (TexturaEstaLista(texGrenade))
                    DrawTexturePro(texGrenade, { 0, 0, (float)texGrenade.width, (float)texGrenade.height },
                        { gr.x, gr.y, gr.width, gr.height }, { 0, 0 }, 0, WHITE);
                else
                    DrawCircle((int)gr.x, (int)gr.y, 10, RED);
            }

            EndMode2D();

            // ---------------- HUD ----------------
            DrawRectangle(20, 20, 204, 24, BLACK);
            DrawRectangle(22, 22, (int)(200 * ((float)s.health / s.maxHealth)), 20, RED);
            DrawText(TextFormat("VIDA: %d", s.health), 30, 24, 16, WHITE);
            DrawText(TextFormat("Monedas: %d  |  Granadas: %d", s.coins, s.grenadeCount), 20, 55, 20, WHITE);
            DrawText(TextFormat("Balas: %d/%d", s.bulletCount, MAX_BULLETS), 20, 80, 20, YELLOW);
            DrawText(TextFormat("Nivel: %d", s.level), 20, 105, 20, SKYBLUE);

            int errorY = 130;
            if (!TexturaEstaLista(texBackground)) { DrawText("FALTA: assets/background (png/jpeg)", 20, errorY, 20, RED); errorY += 25; }
            if (!TexturaEstaLista(texPlayer))     { DrawText("FALTA: assets/player (png/jpeg)", 20, errorY, 20, RED);     errorY += 25; }
            if (!TexturaEstaLista(texZombie))     { DrawText("FALTA: assets/zombie (png/jpeg)", 20, errorY, 20, RED);     errorY += 25; }
            if (!TexturaEstaLista(texPistol))     { DrawText("FALTA: assets/weapon_pistol (png/jpeg)", 20, errorY, 20, RED); errorY += 25; }
            if (!TexturaEstaLista(texShotgun))    { DrawText("FALTA: assets/weapon_shotgun (png/jpeg)", 20, errorY, 20, RED); errorY += 25; }
            if (!TexturaEstaLista(texGrenade))    { DrawText("FALTA: assets/grenade (png/jpeg)", 20, errorY, 20, RED);    errorY += 25; }
            if (!TexturaEstaLista(texTank))       { DrawText("FALTA: assets/tank (png/jpeg)", 20, errorY, 20, RED);       errorY += 25; }
            if (!TexturaEstaLista(texBossFinal))  { DrawText("FALTA: assets/boss_final (png/jpeg)", 20, errorY, 20, RED); errorY += 25; }
            if (!MusicaEstaLista(musicBackground)){ DrawText("FALTA: assets/music_background (mp3/ogg/wav)", 20, errorY, 20, RED); errorY += 25; }
            if (!MusicaEstaLista(musicBossFight)) { DrawText("FALTA: assets/music_boss (mp3/ogg/wav)", 20, errorY, 20, RED); errorY += 25; }
            if (!TexturaEstaLista(texIntro))      { DrawText("FALTA: assets/intro_atlas (jpg/png)", 20, errorY, 20, RED); errorY += 25; }
            if (!MusicaEstaLista(musicIntro))     { DrawText("FALTA: assets/intro_audio (mp3/ogg/wav)", 20, errorY, 20, RED); errorY += 25; }

            if (s.showLevelBanner && !s.gameComplete)
            {
                const char *txt = (s.level == 3 && s.finalBoss.triggered) ? "JEFE FINAL" : TextFormat("NIVEL %d", s.level);
                int fontSize = 50;
                int tw = MeasureText(txt, fontSize);
                unsigned char a = (unsigned char)(std::min(255.0f, s.levelBannerTimer * 200.0f));
                DrawText(txt, SCREEN_W / 2 - tw / 2, 140, fontSize, Fade(GOLD, a / 255.0f));
            }

            if (s.isDead)
            {
                DrawText("HAS MUERTO", SCREEN_W / 2 - MeasureText("HAS MUERTO", 50) / 2, 260, 50, RED);
                if (s.deathTimer > 1.2f)
                {
                    const char *msg = "R: reintentar   |   M: menu de niveles";
                    DrawText(msg, SCREEN_W / 2 - MeasureText(msg, 24) / 2, 330, 24, WHITE);
                }
            }

            // ---------------- Pantalla de VICTORIA ----------------
            if (s.gameComplete)
            {
                DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){ 0, 0, 0, 160 });

                float pulse = 1.0f + sinf((float)GetTime() * 3.0f) * 0.06f;
                int baseSize = 90;
                int fs = (int)(baseSize * pulse);
                const char *txt = "FELICIDADES";
                int tw = MeasureText(txt, fs);
                DrawText(txt, SCREEN_W / 2 - tw / 2, SCREEN_H / 2 - 140, fs, GOLD);

                const char *sub = "Has completado Metal Mini";
                int sw = MeasureText(sub, 28);
                DrawText(sub, SCREEN_W / 2 - sw / 2, SCREEN_H / 2 - 20, 28, WHITE);

                const char *msg = "M: volver al menu";
                int mw = MeasureText(msg, 22);
                DrawText(msg, SCREEN_W / 2 - mw / 2, SCREEN_H / 2 + 30, 22, LIGHTGRAY);

                for (int i = 0; i < 10; i++)
                {
                    float t = (float)GetTime() * 1.5f + i * 0.7f;
                    float sx = 100 + i * 110.0f;
                    float sy = 100 + fmodf(t * 60.0f, (float)(SCREEN_H - 150));
                    DrawCircle((int)sx, (int)sy, 4, Fade(GOLD, 0.8f));
                }
            }

            if (!s.isDead && !s.gameComplete)
            {
                DrawText("Flechas: mover | ABAJO: agachar | ARRIBA: saltar | Z: disparar | G: granada | E: recoger | ESC: menu",
                          20, SCREEN_H - 26, 15, LIGHTGRAY);
            }
        }

        EndDrawing();
    }

    UnloadSound(sndShoot);
    UnloadSound(sndShotgun);
    UnloadSound(sndExplosion);
    UnloadSound(sndCoin);
    UnloadSound(sndHurt);
    UnloadSound(sndJump);
    UnloadSound(sndMenuMove);
    UnloadSound(sndMenuSelect);
    UnloadSound(sndZombieDeath);
    UnloadSound(sndPickup);
    UnloadSound(sndEmpty);
    UnloadSound(sndVictory);
    if (MusicaEstaLista(musicBackground)) UnloadMusicStream(musicBackground);
    if (MusicaEstaLista(musicBossFight))  UnloadMusicStream(musicBossFight);
    CloseAudioDevice();

    if (TexturaEstaLista(texPlayer)) UnloadTexture(texPlayer);
    if (TexturaEstaLista(texZombie)) UnloadTexture(texZombie);
    if (TexturaEstaLista(texPistol)) UnloadTexture(texPistol);
    if (TexturaEstaLista(texShotgun)) UnloadTexture(texShotgun);
    if (TexturaEstaLista(texGrenade)) UnloadTexture(texGrenade);
    if (TexturaEstaLista(texBackground)) UnloadTexture(texBackground);
    if (TexturaEstaLista(texTank)) UnloadTexture(texTank);
    if (TexturaEstaLista(texBossFinal)) UnloadTexture(texBossFinal);

    CloseWindow();
    return 0;
}