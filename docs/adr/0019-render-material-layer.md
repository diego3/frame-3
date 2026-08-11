# 19. Camada de Material: onde moram os uniforms nomeados de um shader (Mesh/Material/Renderer)

- Status: Accepted
- Date: 2026-08-08 (implemented 2026-08-10 -- see "What actually shipped" below)

## Por que este ADR existe

Nasceu de um problema concreto: implementar fresnel rim glow (`game/flare_reactor/lighting.cpp`)
e depois querer restringi-lo só ao reator, não aos `Renderable`s sólidos genéricos (Box/Sphere,
usados pelo Sentinel). Isso expôs uma lacuna real — não existe conceito de "Material" por-objeto
neste projeto. `SetupLights`/`SetupRim` (`lighting.cpp`) empurram constantes fixas em cima de
qualquer `Shader` que recebam, sem nenhuma forma de dizer "este material quer rim, aquele não" sem
enfiar um parâmetro (bool/enum) na função compartilhada que compila os shaders.

Discutido ao vivo; escalou pra pergunta geral via um desenho do usuário:

```
Mesh     -> (vertices, indices, UVs)
Material -> (Shader, Texturas, Uniforms, Propriedades (roughness, metallic, ...))
Renderer -> (Bind(Material), Bind(Mesh), Draw())
```

Isso bate com um item **já nomeado e nunca desenhado** em `docs/roadmap.md`, seção "Not started":
*"Material/shader-per-entity component (e.g. emission/glow) — `ResourceCache<Shader>` (ADR-0004)
only caches a shader by path; nothing binds a shader/material to a specific entity for
rendering."* — sinalizado desde a Phase 4 do RFC-0001 (a "emissão de luz" do núcleo do reator),
nunca desenhado até agora. Este ADR é esse desenho.

**O que este ADR não reabre**: o [ADR-0002](0002-scene-graph-hierarchy-options.md) (transform/
hierarquia) — eixo ortogonal, `PropagateTransforms`/componentes ECS continuam intocados. Também não
resolve a duplicação `Renderable` (`app/scene/renderable.h`) vs `BoxRenderable`
(`app/scene/render_components.h`) como mecanismo de **descoberta de cena** — isso já está marcado
no próprio código como "belongs in its own ADR, not a side effect", e o
[ADR-0018](0018-scene-graph-event-driven-revisit.md) (Proposed) já cobre "como uma view descobre o
que existe pra desenhar". Este ADR é mais estreito: dado um mesh e seus dados de shading, como ele
é **desenhado**, e onde moram os valores de uniform customizados de um shader (intensidade do rim,
uma cor emissiva, uma futura velocidade de scroll) por-objeto. Tanto `Renderable` quanto
`BoxRenderable` poderiam, cada um, rotear seu próprio draw call pelo que quer que este ADR defina,
sem precisar convergir suas definições de componente.

## Contexto: o que existe hoje

- O raylib já dá `Mesh` (vértices/índices/normais/UVs — `GenMeshCube`/`GenMeshSphere`, ou
  `Model.meshes[]` pra um glTF carregado) e `Material` (`Shader` + `MaterialMap[12]`, cada um com
  textura/cor/valor, + `params[4]`) como structs reais. Duas instâncias de "material" já existem
  neste código: `renderable_detail::PrimitiveGeometry::material` (uma só, compartilhada e mutada in-
  place a cada draw call de Box/Sphere sólido, `app/scene/renderable.h`), e cada submesh do `Model`
  do reator tem seu próprio `model.materials[i]` (13, cada um compilado independentemente — ver o
  próprio raciocínio de `lighting.h` sobre por que não compartilhar uma instância).
- Valores de uniform **customizados e nomeados**, além dos slots fixos `maps[]`/`params[4]` do
  raylib (`ambient`, `lights[]`, e agora `rimColor`/`rimPower`/`rimIntensity`), não têm casa em
  nenhum dos dois acima — são empurrados à mão, via funções livres (`SetupLights`, `SetupRim`,
  `game/flare_reactor/lighting.cpp`) direto em cima de qualquer `Shader` que o chamador entregar,
  sem identidade por-objeto. Foi exatamente isso que tornou "rim só no reator" estranho: a única
  alavanca disponível era um parâmetro (bool/enum) atravessando a função compartilhada
  `LoadLightingShaderInstance()`, não uma propriedade de um valor de `Material` que o chamador
  pudesse simplesmente... setar diferente por objeto.
- Necessidade concreta e próxima, além do rim glow: o próprio roadmap de
  [`docs/learning/rendering.html`](../learning/rendering.html) já lista scrolling core texture e
  emissive pulse como próximos — ambos são "esta instância de material específica precisa do seu
  próprio valor de uniform", o mesmo formato de problema, não casos isolados.

## Opções

### Opção 1 — Mínima, restrita a `game/flare_reactor/lighting.*`

```cpp
// lighting.h
struct RimGlowConfig { Color color; float power; float intensity; };

struct LightingMaterial {
    Shader shader;
    std::optional<RimGlowConfig> rim;   // nullopt = sem rim glow
};

LightingMaterial CompilePrimitivesMaterial();   // Box/Sphere -- sem rim
LightingMaterial CompileReactorMaterial();      // Model, por-material -- com rim
```

| Prós | Contras |
|---|---|
| Resolve "rim só no reator" e já dá casa pros próximos 2 itens do roadmap (scroll, pulse), sem arquivo novo fora de `game/flare_reactor`. | Não resolve o item do roadmap pro projeto inteiro — um segundo módulo de jogo que quisesse seu próprio efeito de shader por-material começaria do zero. |
| Espelha o precedente que `skybox.h`/`lighting.h` já estabeleceram pra si mesmos ("single-consumer experiment... promote it alongside app/scene/renderable.h once a second game module actually wants one"). | O formato do "saco de uniforms" é exercitado por um único consumidor — sua forma ideal continua não comprovada. |
| Zero risco pro `Renderable`/`BoxRenderable`/`sandbox`/`camera_fps`. | |

### Opção 2 — Mesh/Material/Renderer completo, em `app/`, agora

```cpp
// app/scene/material.h
struct UniformValue { std::string name; /* variant: float, Vector3, int, Color */ };

struct Material {
    Shader shader;
    ::Material raylibMaterial;         // maps[]/params[4] do raylib, quando fizer sentido
    std::vector<UniformValue> extras;  // rim, scroll, pulse, o que vier
};
void ApplyExtras(Shader& shader, const std::vector<UniformValue>& extras);

// app/scene/renderer.h
void Bind(const Material& material);
void Draw(const Mesh& mesh, const Material& material, Matrix transform);
```

Substituiria a lógica de bind inline que `DrawRenderables` (`Renderable`) e `DrawBoxRenderables`
(`BoxRenderable`) hoje têm cada uma a sua própria cópia.

| Prós | Contras |
|---|---|
| Uma casa genérica real pra "valores de uniform de um material", reutilizável por qualquer módulo de jogo ou tipo de componente de render futuro. | Quebra a disciplina muito consistente deste projeto de "esperar o segundo consumidor real antes de generalizar" — nomeada nas ADR-0004, 0007, 0015, 0016, 0017, e reafirmada de novo na recomendação da própria ADR-0018. Hoje só `game/flare_reactor` precisa de uniforms customizados por material; `sandbox`/`camera_fps` não. |
| Resolve o item do roadmap de vez, não só pro flare_reactor. | Toca dois tipos de componente (`Renderable`, `BoxRenderable`) cuja convergência já está explicitamente adiada pra uma ADR própria ainda não escrita — risco real de decidir isso "de brinde", sem querer. |
| Se bem feito, `DrawRenderables`/`DrawBoxRenderables` ficam mais simples (delegam o bind ao `Renderer`). | Superfície grande pra acertar de primeira com só um caso de uso real exercitando (mesma cautela que a ADR-0004 levantou sobre eviction do resource cache, e a ADR-0018 sobre a forma exata da query do `SceneIndex`) — a forma ideal do "saco de Uniforms" (variant tipado? só nome+float? split estático vs por-frame?) é genuinamente incerta com amostra de tamanho um. |

### Opção 3 — Faseada (recomendada)

Mesmo idioma da própria [ADR-0018](0018-scene-graph-event-driven-revisit.md) ("Opção 2, faseada"):

1. **Agora**: construir a Opção 1 (restrita a `game/flare_reactor/lighting.*`), mas desenhar o
   "saco de extras" do `LightingMaterial` deliberadamente perto do que um futuro `Material` em
   `app/` pareceria (pares nome+valor, aplicados de uma vez via um único loop
   `ApplyExtras(shader, extras)`) — pra promoção depois ser um "lift-and-shift" (mover o struct +
   renomear + ajustar call sites), não uma reescrita. Espelha a própria trajetória que
   `skybox.h`/`lighting.h` já declaram pra si mesmos.
2. **Adiar**: `app/scene/material.h` + um `Renderer::Bind/Draw` genérico, até um segundo consumidor
   real aparecer — ou um segundo módulo de jogo querendo seus próprios uniforms customizados por-
   material, ou a ADR de convergência `Renderable`/`BoxRenderable` sendo escrita (o que vier
   primeiro; essa ADR de convergência é um momento natural pra também incorporar o `Renderer`, já
   que ela decidiria como os dois caminhos de desenho se relacionam de qualquer forma).
3. **Não escolher a Opção 2 agora**, sem um segundo consumidor real — mesma disciplina repetida nas
   ADR-0004/0007/0015/0016/0017/0018.

## Revisão da recomendação (mesma sessão, 2026-08-08)

A recomendação inicial acima (Opção 3) foi contestada pelo usuário, com três argumentos — registro
os três porque cada um pesa numa parte diferente do raciocínio:

1. **"Já temos o sandbox, que pode ser refatorado só pra gente adequar e termos um segundo
   consumidor."** Aceito, com uma condição: pra isso validar a generalização de verdade (não só
   "contar" como segundo consumidor), o sandbox precisa exercitar a **mesma API** que o reator usa
   — não precisa de um uniform diferente pra ser válido; precisa provar que a API do `Material`/
   `Renderer` funciona pra um módulo de jogo construído independentemente, sem gambiarra local. É
   isso que o usuário confirmou (ver `## Contexto adicional` abaixo).
2. **"Quero ver como a arquitetura da engine evolui usando o reator como cobaia; deixar ele com
   código diferente do que eu quero não ajuda no aprendizado."** Este é o argumento que mais mudou
   minha avaliação. A disciplina "espera o segundo consumidor real" (ADR-0004/0007/0015/0016/0017)
   existe pra evitar trabalho jogado fora **num contexto de produção**, onde generalizar errado
   custa retrabalho caro. O objetivo declarado deste projeto não é esse — é aprendizado deliberado
   sobre como a arquitetura evolui (o próprio skill `engine-architecture` se descreve assim). Contra
   esse objetivo, "esperar o gatilho orgânico" otimiza pro critério errado.
3. **"Vou levar o reator a um ponto de complexidade interessante — UI, networking, mais mecânicas,
   outros shaders, câmeras, level design."** Isso muda o cálculo de "generalizar numa aposta" pra
   "antecipar algo quase certo" — o mesmo raciocínio que a própria [ADR-0010](0010-base-game-logic-and-igameview.md)
   usou pra construir `IGameView`/`BaseGameLogic` **antes** do gatilho, porque toda view futura ia
   precisar da mesma costura.

## Contexto adicional (a pedido, sobre o segundo consumidor)

Perguntei que forma o segundo consumidor (sandbox) deveria ter — se um efeito genuinamente
diferente, ou algo que reusa a mesma peça. Resposta: **sandbox deve reusar a mesma estrutura do
reator**, pra manter consistência com o que a API da engine oferece. Ou seja, o teste real não é
"o saco de uniforms generaliza pra formatos diferentes" — é "dois módulos de jogo construídos
independentemente conseguem usar a mesma API de `Material`/`Renderer` sem fricção". Isso é
inclusive a validação mais importante pra uma API de engine (o mesmo espírito da
[ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md), "productização gated no
segundo consumidor" — a API sendo reusável de verdade É o teste, não a forma dos dados).

Isso também expõe um gap já conhecido e documentado (`app/scene/renderable.h`'s próprio header,
[ADR-0018](0018-scene-graph-event-driven-revisit.md)): `game/sandbox`'s `GameplayScene`
(`src/game/sandbox/human_view.cpp:33`) **nunca foi migrada** pro componente `Renderable` — ainda
desenha um `DrawCubeWires(position, 1.0f, 1.0f, 1.0f, MAROON)` hardcoded por `WorldTransform`,
o código de antes do `Renderable` existir (RFC-0001 Phase 1). Fazer do sandbox um segundo
consumidor real do `Material`/`Renderer` resolve esse gap de tabela, não só serve de cobaia.

## Recomendação

**Opção 2 — Mesh/Material/Renderer completo, em `app/`, agora.** Revisado a partir da Opção 3
original: o propósito de aprendizado deste projeto e o plano concreto de aumentar a complexidade
do reator (item 2 e 3 acima) pesam mais do que a disciplina "espera o segundo consumidor real"
neste caso específico — e o sandbox, migrado deliberadamente pra usar a mesma API, supre esse
segundo consumidor de forma real, não fabricada. **Limite de escopo que continua valendo** (não
muda com a revisão): este ADR não resolve a convergência `Renderable`/`BoxRenderable` — o
`Renderer` deve ser algo que ambos os tipos de componente conseguem chamar por baixo, sem forçar a
fusão de suas definições agora. Isso continua pra sua própria ADR (referenciada pela ADR-0018).

## Plano de implementação

1. **`app/scene/material.h`**: `Material` (Shader + raylib `Material` quando fizer sentido + um
   "saco de extras" nome→valor) e `ApplyExtras(Shader&, extras)`. Desenhado contra as necessidades
   já conhecidas do reator (rim glow hoje; scroll/pulse a seguir, per
   [`docs/learning/rendering.html`](../learning/rendering.html)).
2. **`app/scene/renderer.h`**: `Bind(const Material&)`/`Draw(const Mesh&, const Material&, Matrix)`
   — substitui a lógica de bind inline que `DrawRenderables` hoje tem pra Box/Sphere sólidos.
3. **`game/flare_reactor`**: migra `Lighting`/o caminho de desenho do reator pra usar
   `app/scene/material.h`/`renderer.h`, substituindo `SetupRim`/o parâmetro bool-ou-enum discutido
   antes desta ADR.
4. **`game/sandbox`**: migra `GameplayScene` do `DrawCubeWires` hardcoded pra um `Renderable` de
   verdade (componente "Renderable" via `EntityFactory`, mesmo padrão que `flare_reactor`'s
   `main.cpp` já usa) desenhado através do mesmo `Material`/`Renderer` — incluindo configurar o
   mesmo tipo de extra (rim glow) que o reator usa, pela mesma API, provando que ela é reusável por
   um consumidor construído de forma independente. Fecha de quebra o gap já registrado no header de
   `renderable.h` e na ADR-0018 (sandbox nunca adotou `Renderable`).
5. **Fora de escopo por ora** (nomeado, não bloqueante): dobrar `game/camera_fps`'s
   `BoxRenderable`/`scene_renderer.h` pro mesmo `Renderer` — terceiro consumidor, bem-vindo depois,
   não necessário pra validar a API com dois.

## Tradeoffs aceitos

- **Isso é generalizar antes do "segundo consumidor orgânico" de verdade** — o sandbox é migrado
  *de propósito* pra servir esse papel, não porque ele já precisava disso por conta própria.
  Aceito explicitamente (não silenciosamente) porque o objetivo deste projeto pesa aprendizado e
  planejamento de arquitetura tanto quanto (ou mais que) o custo usual de generalizar cedo — mesma
  categoria de exceção documentada que a própria ADR-0010 já registrou pra si.
- **Toca dois caminhos de componente (`Renderable`, `BoxRenderable`)** sem resolver a convergência
  entre eles — aceito, com o limite de escopo explícito acima (o `Renderer` serve os dois sem
  fundi-los).
- **A forma do "saco de extras" ainda é exercitada por um uso repetido (rim glow no reator e no
  sandbox), não por um formato de uniform genuinamente diferente** — o teste real aqui é a API, não
  a forma dos dados (ver `## Contexto adicional`); a forma dos dados em si continua com uma amostra
  efetivamente pequena até um terceiro efeito (scroll, pulse) ou um terceiro consumidor
  (`camera_fps`) aparecer.

## Consequences / follow-ups

- `docs/roadmap.md`: mover a entrada existente "Material/shader-per-entity component" de "Not
  started" pra "Proposed", linkando aqui. **(feito nesta sessão.)**
- `.claude/skills/engine-architecture/SKILL.md` deveria ganhar uma nota (§ nova) sobre `Material`/
  `Renderer` uma vez implementado, mesma convenção que as outras peças de `app/` já seguem lá.
- `game/sandbox/human_view.cpp`'s `GameplayScene` ganha, pela primeira vez, um `Renderable` de
  verdade — vale conferir se algo mais em `game/sandbox` (testes, outros arquivos) assume o
  `DrawCubeWires` hardcoded atual antes de remover.
- Terceiro consumidor natural a observar depois: `game/camera_fps` (dobrar `BoxRenderable` pro
  mesmo `Renderer`) — não bloqueante, mencionado no Plano de implementação como fora de escopo por
  ora.

## Open Questions

- Forma exata do "saco de extras": um pequeno variant tipado (`Float`/`Vec3`/`Int`/`Color`) chaveado
  por nome, ou campos tipados por efeito (`RimGlowConfig`, um futuro `ScrollConfig`)? Como o
  sandbox vai reusar exatamente o mesmo efeito (rim glow) que o reator, essa pergunta ainda não
  ganha um segundo formato real de uniform pra decidir — só um segundo *consumidor* da mesma forma.
  Resolver na implementação; revisitar quando scroll/pulse (efeitos com forma diferente) chegarem.
- Se `Renderer::Bind/Draw` acaba sendo código novo de verdade ou um rename/wrapper fino em cima do
  que `DrawRenderables` já faz inline — só fica claro implementando.
- Um `Model` do raylib (desenho multi-material de um submesh, via `DrawModelEx`) passa pelo mesmo
  `Renderer` que primitivos Box/Sphere, ou continua um caminho separado (como hoje,
  `Lighting::ApplyToModel` vs. o bind inline de primitivos)? Deixado em aberto pra implementação —
  o reator (`Model`) e o sandbox/sentinel (`Box`/`Sphere`) precisam concordar nisso pra ambos
  passarem pelo mesmo `Material`.
- Quando (se) `game/camera_fps` entra como terceiro consumidor — não antes de precisar de verdade,
  mesma disciplina já aplicada ao resto do projeto (essa parte da disciplina original continua de
  pé; só o "segundo consumidor" foi antecipado deliberadamente, não o terceiro).

## What actually shipped, alongside this ADR

Implemented 2026-08-10, following the Plano de implementação above with a few deviations forced by
compiling against raylib's actual headers rather than the ADR's illustrative sketch:

- **Named `RenderMaterial`, not `Material`.** raylib's own `raylib.h` already defines a global
  `struct Material { Shader shader; MaterialMap *maps; float params[4]; } Material;` (C-style
  typedef, no namespace). A second global `struct Material` is a redefinition conflict, not a
  shadow -- doesn't compile. The ADR's own sketch (`::Material raylibMaterial`) assumed a namespace
  wrapper this project's `app/scene/` headers don't otherwise use (`Renderable`, `BoxRenderable`,
  `WorldTransform` all sit in the global namespace uncontested). `app/scene/material.h`'s
  `RenderMaterial { Shader shader; ::Material raylibMaterial; std::vector<UniformValue> extras; }`
  is the same shape, just renamed to actually compile.
- **`BindMaterial`/`DrawWithMaterial`, not the sketch's bare `Bind`/`Draw`.** `app/scene/
  renderer.h`. A one-word global function name is too easy to collide with unrelated code later;
  every other draw-adjacent free function in this directory already pairs a verb with a noun
  (`DrawRenderables`, `DrawBoxRenderables`).
- **The Model-vs-primitives "same Renderer?" Open Question is resolved: yes.**
  `app/scene/renderable.h`'s `DrawRenderables` now reimplements raylib's own `DrawModelEx` loop
  (confirmed against `vendor/raylib/src/rmodels.c`: one `DrawMesh` call per submesh, tint
  premultiplied onto each submesh's own diffuse color) one submesh at a time through
  `DrawWithMaterial`, instead of calling `DrawModelEx` directly. Each submesh's `RenderMaterial`
  carries an empty `extras` list -- the reactor's rim glow is still baked once per material at
  `Lighting::ApplyToModel` time (uniform values are static, no need to re-push every frame/draw),
  not re-applied through this call's `extras`.
- **`Lighting::GetShader()` → `GetPrimitivesMaterial() -> const RenderMaterial&`.** Its
  `SetupRim` free function is gone; rim glow is now `RenderMaterial::extras` built once
  (`BuildRimExtras()`) and pushed via the generic `ApplyExtras` (`material.h`), applied only inside
  `ApplyToModel` -- **not** on the primitives shader anymore. This is the actual fix for the bug
  that opened this ADR: before this change, `LoadLightingShaderInstance()` called `SetupRim`
  unconditionally on every shader instance it compiled, including the one shared by every solid
  Box/Sphere `Renderable` (the Sentinel) -- so the Sentinel picked up rim glow it was never meant to
  have. That's fixed now, not just designed around.
- **`game/sandbox`**: `player.yaml` gained a `Renderable` (`shape: box, wireframe: false`),
  `screen_gameplay.cpp` gained a `"Renderable"` component loader mirroring `game/flare_reactor/
  main.cpp`'s own (minus its `model`/`Lighting::ApplyToModel` branch -- no Model-shaped entity
  exists in sandbox to exercise it), and `HumanView` builds its own `RenderMaterial` (`
  LoadSandboxMaterial()`, `human_view.cpp`) independently of `game/flare_reactor/lighting.h`'s
  `Lighting` class -- same rim-glow-extras technique, amber instead of cyan, its own ambient/one-
  light setup -- rather than reusing that class as-is. `GameplayScene` now calls `DrawRenderables`
  instead of hardcoding `DrawCubeWires`, closing the gap `renderable.h`'s own header comment and
  ADR-0018 both flagged. `HumanView` gained a destructor (`UnloadShader(material_.shader)`) --
  necessary here in a way it wasn't for `game/flare_reactor`'s single-shot `main()`, since GAMEPLAY
  can be Init/Unload'd repeatedly within one run via `main.cpp`'s screen state machine.
- **`game/camera_fps`** untouched, as scoped (third consumer, not needed to validate the API with
  two) -- confirmed it still builds clean against the changed `app/scene/renderable.h`.
- Verified: all three game modules (`flare_reactor`, `sandbox`, `camera_fps`) build clean via
  `build.sh`; full `test.sh` suite (97 cases / 235 assertions) passes unchanged.

## References

- `docs/roadmap.md`, "Material/shader-per-entity component" (agora em "Proposed") — o item nomeado
  e nunca desenhado que este ADR responde.
- [`docs/learning/rendering.html`](../learning/rendering.html), "Roadmap de efeitos do reator" — os
  uniforms concretos e próximos (scroll, pulse, especular) contra os quais isso está escopado.
- `src/game/flare_reactor/lighting.h`/`.cpp`, `skybox.h` — o precedente "single-consumer
  experiment, promove quando um segundo módulo de jogo quiser" que motivou a Opção 3 original
  (superseded pela revisão acima).
- `src/game/sandbox/human_view.cpp:33` (`GameplayScene`) — o `DrawCubeWires` hardcoded que a
  migração do sandbox (Plano de implementação, item 4) substitui; gap já registrado em
  `app/scene/renderable.h`'s header e na ADR-0018.
- `src/app/scene/renderable.h`, `src/app/scene/render_components.h` (`BoxRenderable`),
  `src/app/scene/scene_renderer.h` — os dois caminhos de componente/desenho já existentes que o
  `Renderer` deste ADR precisa conviver ao lado, sem convergi-los (limite de escopo explícito).
- [ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md),
  [ADR-0007](0007-terraform-gated-on-authoritative-server.md),
  [ADR-0010](0010-base-game-logic-and-igameview.md),
  [ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md),
  [ADR-0016](0016-screen-element-stack.md), [ADR-0017](0017-camera-fps-second-game-module.md) —
  precedente de "esperar o segundo consumidor real" (0004/0007/0015/0016/0017) e seu contraponto
  documentado, "generalizar antes do gatilho quando toda peça futura vai precisar da mesma costura"
  (0010) — a categoria de exceção que a revisão desta ADR usa.
- [ADR-0018](0018-scene-graph-event-driven-revisit.md) (Proposed) — ADR irmã de arquitetura de
  render (descoberta/indexação de cena), eixo ortogonal (como uma view acha o que desenhar vs. como
  uma coisa achada é desenhada).
- Conversa 2026-08-08: fresnel rim glow (implementado, `game/flare_reactor/lighting.cpp`) expôs
  essa lacuna ao ser restringido só ao reator; discussão subsequente revisou a recomendação inicial
  (Opção 3) pra Opção 2, com sandbox como segundo consumidor deliberado.
