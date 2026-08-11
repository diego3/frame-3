# 21. Spawn frequente (projéteis): cache de prototype sobre `EntityFactory`, não pooling ainda

- Status: Proposed — **rascunho pra não perder a ideia**, não é uma decisão fechada nem um plano de
  implementação. Registrado a pedido do usuário durante a discussão da [ADR-0020](
  0020-mesh-renderer-material-shader-layers.md), stress-testando aquele desenho contra um caso de
  uso diferente (spawn frequente, ciclo de vida curto) antes de comprometer nada.
- Date: 2026-08-10

## Por que este ADR existe

Discutindo a [ADR-0020](0020-mesh-renderer-material-shader-layers.md) (MeshRenderer/RenderMaterial/
Shader), o usuário trouxe um caso de uso ainda não exercitado: **projéteis** — entidades de ciclo de
vida curto, spawnadas com frequência (a cada tiro), perguntando se isso exigiria repensar o desenho
proposto, citando dois conceitos de outras engines: *prototype* e *object pool*.

## O que já existe hoje (grounded, não suposição)

- **ECS via EnTT já resolve a metade "clonar um molde de objeto" do problema de Prototype** -- não é
  uma decisão nova deste ADR, é consequência de [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md)
  já registrada na skill `engine-architecture` (§3, "Prototype / archetype spawning"), que usa
  **"projectile" como o próprio exemplo do texto**: "don't hand-construct an entity's full component
  set at every spawn site... the registry's own sparse-set storage *is* the pool/arena". `entt::
  registry::create()`/`destroy()` recicla IDs, sem alocação de heap por entidade -- não existe (nem
  precisa existir) um scheme de `unique_ptr`-por-entidade pra desenhar.
- **O gap real está em `LevelLoader::Load`, não no ECS.** Lido em `level_loader.cpp`:
  ```cpp
  for (const EntityDefNode &placement : level.Get("actors").AsList()) {
      std::string resourcePath = placement.Get("resource").AsString();
      EntityDefNode entityDef = parser_.Parse(readFile_(resourcePath));   // lê disco + parseia, toda vez
      ...
  }
  ```
  Ótimo pra carregar um level (dezenas de entidades, uma vez). Péssimo se chamado a cada tiro --
  leitura de disco + parse de texto YAML por spawn, várias vezes por segundo.
- **`EntityFactory::Create(registry, def)` já aceita um `EntityDefNode` qualquer**, parseado de onde
  for -- nada o acopla ao `LevelLoader`. Um cache do `EntityDefNode` já parseado (`projectile.yaml`,
  uma vez) + chamadas repetidas a `entityFactory_.Create(registry, cachedDef)` já resolveria o custo
  de spawn, sem tocar `LevelLoader` nem `EntityFactory`.
- **`ProcessManager` (ADR-0003) já existe** e já tem um precedente real de "envelhece e se destrói"
  (`BeaconPulseProcess`) -- ciclo de vida curto de um projétil é o mesmo formato, ortogonal ao
  spawn/render.
- **Frame budget é um SLO real, checado em runtime hoje** (`~16.67ms/frame`, overlay F3 -- skill
  `engine-sre`) -- a ferramenta certa pra decidir *se* pooling é necessário, com dado, não achismo.

## Compatibilidade com a ADR-0020 (MeshRenderer/RenderMaterial)

**Nenhum rework necessário.** `MeshRenderer.material` já foi desenhado como
`std::shared_ptr<RenderMaterial>`, não uma instância própria por entidade -- muitos projéteis do
mesmo tipo apontam pro **mesmo** `RenderMaterial`/`Shader` já compilado uma vez (spawnar nunca
compila shader novo). Mesmo padrão que `renderable_detail::PrimitiveGeometry` (`renderable.h`) já
usa pra Box/Sphere hoje (malha compartilhada, processo inteiro).

**Guardrail explícito**: projétil deve ser entidade **única** (`Renderable::Shape::Box`/`Sphere` ou
mesh simples) -- nunca passar pelo caminho "Model multi-material vira subtree de entidades filhas"
que a ADR-0020 desenha pro reator/backpack. Aquele padrão é pra "um asset com partes visualmente
diferentes de verdade"; pra um projétil seria N entidades-filhas nascendo/morrendo por spawn, sem
necessidade nenhuma.

## Direção candidata (não decidida)

1. Um pequeno cache de `EntityDefNode` já parseado -- mesmo desenho do `ResourceCache<T>` (ADR-0004:
   `GetHandle(path)`, cache-miss faz o trabalho caro uma vez), só que pra definição de entidade em
   vez de recurso de GPU. Vive ao lado do `LevelLoader`, não dentro dele (`LevelLoader` continua
   sendo "placement em bulk no load do level", não spawn em runtime).
2. `ProjectileLifetimeProcess`-equivalente (via `ProcessManager`, mesmo formato de
   `BeaconPulseProcess`) pro envelhecimento/destruição.
3. **Pooling de verdade (pré-alocar N entidades, reativar em vez de destruir/recriar): explicitamente
   adiado**, gated no frame budget SLO mostrar que `create()`/`destroy()` do EnTT (já barato por
   construção) é de fato o gargalo -- mesma disciplina "espera o gatilho real" já usada no resto do
   projeto (ADR-0004/0007/0015/0016/0017).

## Open Questions

- Nome/forma exata do cache de `EntityDefNode` (`EntityDefCache`? método novo em `LevelLoader`?
  algo totalmente separado?).
- Se/quando um projétil precisa de efeito visual próprio (trail, glow) -- reusa o mesmo
  `RenderMaterial` compartilhado entre instâncias, ou precisa de estado por-projétil (ex.: fade-out
  perto do fim da vida) que não cabe num `RenderMaterial` totalmente compartilhado?
- Colisão/dano -- fora de escopo aqui (ADR-0012, physics, ainda `Proposed`, não bloqueante pra este
  rascunho).

## References

- [ADR-0020](0020-mesh-renderer-material-shader-layers.md) -- discussão que motivou este ADR;
  `MeshRenderer`/`RenderMaterial` compartilhado é o que garante compatibilidade aqui.
- [ADR-0001](0001-ecs-via-entt-and-cpp-engine-init.md) -- decisão ECS via EnTT, base de por que
  "clonar objeto" não é mais o problema.
- [ADR-0003](0003-event-manager-and-process-manager-game-loop.md) -- `ProcessManager`, pro ciclo de
  vida curto.
- [ADR-0004](0004-resource-cache-thin-vs-full-book-rescache.md) -- precedente de forma pro cache de
  `EntityDefNode` (`GetHandle(path)`-style).
- [ADR-0008](0008-data-driven-entity-loading-yaml.md)/[ADR-0009](
  0009-level-loading-actor-placement.md) -- `EntityFactory`/`LevelLoader`, onde o gap real está.
- `.claude/skills/engine-architecture/SKILL.md` §3 -- "Prototype / archetype spawning", já usa
  "projectile" como exemplo, já decidido que EnTT resolve a parte de object-cloning.
- `.claude/skills/engine-sre/SKILL.md` -- frame budget SLO, a ferramenta pra decidir se pooling
  algum dia vira necessário.
- Conversa 2026-08-10: pergunta do usuário sobre prototype/pool, stress-testando a ADR-0020 contra
  spawn frequente de projéteis.
