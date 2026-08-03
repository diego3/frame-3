// Declares EcsSmokeTest() with C linkage so raylib_game.c (plain C) can call into the C++
// translation unit that exercises EnTT. See ecs_smoke_test.cpp for what it actually does and why
// it exists — this is a temporary build/link proof, not real ECS usage; delete it once real
// components/systems land (see .claude/skills/engine-architecture's Open Questions).
#ifndef ECS_SMOKE_TEST_H
#define ECS_SMOKE_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

void EcsSmokeTest(void);

#ifdef __cplusplus
}
#endif

#endif // ECS_SMOKE_TEST_H
