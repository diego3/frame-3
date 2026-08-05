// extern "C" bridge letting screen_gameplay.c (plain C, per ADR-0001 Decision 2 -- screens.h's
// screen_*.c files stay untouched) reach the C++ BaseGameLogic/HumanView/LevelLoader machinery
// (docs/adr/0010 Sec 3) without screen_gameplay.c itself becoming a C++ translation unit. Mirrors
// the same shape ADR-0009 already anticipated for LevelLoader.
#ifndef GAMEPLAY_BRIDGE_H
#define GAMEPLAY_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

void GameplayBridge_Init(void);
void GameplayBridge_Update(float dt);
void GameplayBridge_Draw(void);
void GameplayBridge_Unload(void);

#ifdef __cplusplus
}
#endif

#endif // GAMEPLAY_BRIDGE_H
