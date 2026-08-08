/*******************************************************************************************
*
*   raylib game template
*
*
*   Code licensed under an unmodified zlib/libpng license, which is an OSI-certified,
*   BSD-like license that allows static linking with closed source software
*
*   Copyright (c) 2021-2026 Ramon Santamaria (@raysan5)
*
********************************************************************************************/

#include "raylib.h"
#include "screens.h"    // NOTE: Declares global (extern) variables and screens functions
#include "game_config.h"
#include "app/view/debug_overlay.h"
#include "app/core/engine.h"
#include "app/core/engine_config.h"

#include <memory>                           // Required for: std::shared_ptr (font/sound handles)
#include <stdio.h>                          // Required for: printf()
#include <stdlib.h>                         // Required for:
#include <string.h>                         // Required for:

//----------------------------------------------------------------------------------
// Defines and Macros
//----------------------------------------------------------------------------------
// Simple log system to avoid printf() calls if required
// NOTE: Avoiding those calls, also avoids const strings memory usage
#define SUPPORT_LOG_INFO
#if defined(SUPPORT_LOG_INFO)
    #define LOG(...) printf(__VA_ARGS__)
#else
    #define LOG(...)
#endif

//----------------------------------------------------------------------------------
// Shared Variables Definition (global)
// NOTE: Those variables are shared between modules through screens.h
//----------------------------------------------------------------------------------
GameScreen currentScreen = LOGO;
Font font = { 0 };
Music music = { 0 };
Sound fxCoin = { 0 };

//----------------------------------------------------------------------------------
// Global Variables Definition (local to this module)
//----------------------------------------------------------------------------------
// screenWidth/screenHeight moved into EngineConfig (docs/adr/0011), loaded below in main() --
// no longer hardcoded constants here.

// Required variables to manage screen transitions (fade-in, fade-out)
static float transAlpha = 0.0f;
static bool onTransition = false;
static bool transFadeOut = false;
static int transFromScreen = -1;
static GameScreen transToScreen = UNKNOWN;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
static void TransitionToScreen(GameScreen screen); // Request transition to next screen
static void UpdateTransition(void);         // Update transition effect
static void DrawTransition(void);           // Draw transition effect (full-screen rectangle)

static void UpdateDrawFrame(void);          // Update and draw one frame

//----------------------------------------------------------------------------------
// Program main entry point
//----------------------------------------------------------------------------------
int main(void)
{
    // Initialization
    //---------------------------------------------------------
    Engine engine;
    if (!engine.Init(LoadOrCreateEngineConfig(), "raylib game template")) return 1;

    // Load resources shared across all screens (font, fxCoin -- see screens.h) through Engine's
    // caches (Ch. 8, ADR-0004). Which specific asset to load is a game choice, not an engine one
    // (docs/adr/0014) -- read from GameConfig (docs/adr/0011) rather than hardcoded here, so a
    // different sandbox build could point at different assets without a recompile; GameConfig's
    // first real fields and caller. This game's own screen_*.cpp files still read font/fxCoin as
    // plain extern globals (ADR-0001 Decision 2), not through a shared_ptr -- so the cache handle
    // is kept alive here (as long as main()'s own stack frame is, i.e. the whole app's run) and the
    // plain globals get a copy of the raylib value type, which is how raylib itself expects
    // Font/Sound to be passed around (a lightweight handle to GPU/audio-resident data, not the data
    // itself).
    GameConfig gameConfig = LoadOrCreateGameConfig();

    std::shared_ptr<Font> fontHandle = engine.Fonts().GetHandle(gameConfig.characterTexturePath);
    font = *fontHandle;
    //music = LoadMusicStream("resources/audio/music/ambient.ogg"); // TODO: Load music
    std::shared_ptr<Sound> soundHandle = engine.Sounds().GetHandle(gameConfig.coinSoundPath);
    fxCoin = *soundHandle;

    SetMusicVolume(music, 1.0f);
    PlayMusicStream(music);

    // Setup and init first screen
    currentScreen = LOGO;
    InitLogoScreen();

    engine.Run(UpdateDrawFrame);

    // De-Initialization
    //--------------------------------------------------------------------------------------
    // Unload current screen data before closing
    switch (currentScreen)
    {
        case LOGO: UnloadLogoScreen(); break;
        case TITLE: UnloadTitleScreen(); break;
        case OPTIONS: UnloadOptionsScreen(); break;
        case GAMEPLAY: UnloadGameplayScreen(); break;
        case ENDING: UnloadEndingScreen(); break;
        default: break;
    }

    // Release the cache handles (running UnloadFont/UnloadSound, via ResourceCache's own deleter)
    // and stop the (never-actually-loaded, see TODO above) music stream before engine.Shutdown()
    // below closes the GL/audio context those Unload* calls need -- same ordering requirement
    // Engine::Shutdown() documents for every other resource-cache handle a caller holds.
    fontHandle.reset();
    soundHandle.reset();
    UnloadMusicStream(music);

    engine.Shutdown();
    //--------------------------------------------------------------------------------------

    return 0;
}

//----------------------------------------------------------------------------------
// Module Functions Definition
//----------------------------------------------------------------------------------
// Request transition to next screen
static void TransitionToScreen(GameScreen screen)
{
    onTransition = true;
    transFadeOut = false;
    transFromScreen = currentScreen;
    transToScreen = screen;
    transAlpha = 0.0f;
}

// Update transition effect (fade-in, fade-out)
static void UpdateTransition(void)
{
    if (!transFadeOut)
    {
        transAlpha += 0.05f;

        // NOTE: Due to float internal representation, condition jumps on 1.0f instead of 1.05f
        // For that reason we compare against 1.01f, to avoid last frame loading stop
        if (transAlpha > 1.01f)
        {
            transAlpha = 1.0f;

            // Unload current screen
            switch (transFromScreen)
            {
                case LOGO: UnloadLogoScreen(); break;
                case TITLE: UnloadTitleScreen(); break;
                case OPTIONS: UnloadOptionsScreen(); break;
                case GAMEPLAY: UnloadGameplayScreen(); break;
                case ENDING: UnloadEndingScreen(); break;
                default: break;
            }

            // Load next screen
            switch (transToScreen)
            {
                case LOGO: InitLogoScreen(); break;
                case TITLE: InitTitleScreen(); break;
                case OPTIONS: InitOptionsScreen(); break;
                case GAMEPLAY: InitGameplayScreen(); break;
                case ENDING: InitEndingScreen(); break;
                default: break;
            }

            currentScreen = transToScreen;

            // Activate fade out effect to next loaded screen
            transFadeOut = true;
        }
    }
    else  // Transition fade out logic
    {
        transAlpha -= 0.02f;

        if (transAlpha < -0.01f)
        {
            transAlpha = 0.0f;
            transFadeOut = false;
            onTransition = false;
            transFromScreen = -1;
            transToScreen = UNKNOWN;
        }
    }
}

// Draw transition effect (full-screen rectangle)
static void DrawTransition(void)
{
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, transAlpha));
}

// Update and draw game frame
static void UpdateDrawFrame(void)
{
    // Update
    //----------------------------------------------------------------------------------
    //UpdateMusicStream(music);       // NOTE: Music keeps playing between screens

    UpdateDebugOverlay(GetFrameTime());   // NOTE: F3 toggles a /proc/self stats HUD (Linux desktop only)

    if (!onTransition)
    {
        switch(currentScreen)
        {
            case LOGO:
            {
                UpdateLogoScreen();

                if (FinishLogoScreen()) TransitionToScreen(TITLE);

            } break;
            case TITLE:
            {
                UpdateTitleScreen();

                if (FinishTitleScreen() == 1) TransitionToScreen(OPTIONS);
                else if (FinishTitleScreen() == 2) TransitionToScreen(GAMEPLAY);

            } break;
            case OPTIONS:
            {
                UpdateOptionsScreen();

                if (FinishOptionsScreen()) TransitionToScreen(TITLE);

            } break;
            case GAMEPLAY:
            {
                UpdateGameplayScreen();

                if (FinishGameplayScreen() == 1) TransitionToScreen(ENDING);
                //else if (FinishGameplayScreen() == 2) TransitionToScreen(TITLE);

            } break;
            case ENDING:
            {
                UpdateEndingScreen();

                if (FinishEndingScreen() == 1) TransitionToScreen(TITLE);

            } break;
            default: break;
        }
    }
    else UpdateTransition();    // Update transition (fade-in, fade-out)
    //----------------------------------------------------------------------------------

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

        ClearBackground(RAYWHITE);

        switch(currentScreen)
        {
            case LOGO: DrawLogoScreen(); break;
            case TITLE: DrawTitleScreen(); break;
            case OPTIONS: DrawOptionsScreen(); break;
            case GAMEPLAY: DrawGameplayScreen(); break;
            case ENDING: DrawEndingScreen(); break;
            default: break;
        }

        // Draw full screen rectangle in front of everything
        if (onTransition) DrawTransition();

        DrawDebugOverlay();

        //DrawFPS(10, 10);

    EndDrawing();
    //----------------------------------------------------------------------------------
}
