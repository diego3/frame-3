# 18. Revisão do ADR-0002 à luz do GameCode4: indexação de cena reativa para múltiplas views

- Status: Proposed
- Date: 2026-08-06

## Por que este ADR existe

Este documento nasceu de uma pergunta direta durante uma sessão de exploração do `../gamecode4`
(checkout local do código-fonte de *Game Coding Complete, 4th Edition*, usado como referência em
várias ADRs deste projeto): como o `Scene` do GC4 se relaciona com sua `HumanView`, e se essa forma
— mais orientada a eventos — é mais interessante que a que este projeto já decidiu no
[ADR-0002](0002-scene-graph-hierarchy-options.md). Registro aqui os achados dessa comparação,
opções concretas e uma recomendação, para avaliação — **não é uma decisão fechada**, ao contrário
da maioria das ADRs deste repositório que documentam algo já implementado.

**O que este ADR não reabre**: o ADR-0002 decidiu *como o `WorldTransform` de uma entidade é
calculado* — componentes ECS (`Relationship`/`LocalTransform`/`WorldTransform`) mais uma função
livre, `PropagateTransforms`, rodando uma vez por frame em `Engine::Run` antes de qualquer view.
Nada do que segue contradiz essa parte; ela continua sendo, na minha avaliação, a escolha certa (a
seção "Uma ressalva" abaixo explica por quê, comparando diretamente com o que o GC4 faz nesse ponto
específico).

**O que este ADR reabre**: uma camada que o ADR-0002 nunca cobriu — como uma view (ou uma futura
`RemoteView`/`AIView`) *descobre* quais entidades têm o quê para renderizar/observar. Hoje isso é
resolvido caso a caso, por chamada nomeada, dentro de cada `Scene`-como-`IScreenElement` de cada
jogo. É exatamente aí que o `Scene` do GC4 faz algo genuinamente diferente — e vale considerar.

## Contexto: o que existe hoje

Desde o [ADR-0016](0016-screen-element-stack.md) e o [ADR-0017](0017-camera-fps-second-game-module.md),
cada `HumanView`-família tem uma pilha de `IScreenElement`s (`app/human_view_base.h`), e um desses
elementos é a "cena 3D" do jogo — `game/sandbox/human_view.cpp`'s `GameplayScene`,
`game/camera_fps/human_view.cpp`'s `FpsScene`. Cada um desses:

- Abre/fecha seu próprio `BeginMode3D`/`EndMode3D` com a câmera que ele mesmo sabe onde guardar
  (`game/sandbox` num membro `Camera3D`; `game/camera_fps` num componente `FirstPersonCameraRig`).
- Chama, **pelo nome**, uma função que sabe desenhar um tipo específico de componente de render:
  `FpsScene` chama `DrawBoxRenderables(registry_)` (`app/scene_renderer.h:28`), que itera
  `registry.view<WorldTransform, BoxRenderable>()`. `GameplayScene` ainda nem usa isso — desenha um
  wireframe 1×1×1 hardcoded por `WorldTransform`, porque a entidade do sandbox não tem
  `BoxRenderable` (ADR-0017 registrou isso como pendência, não regressão).

Isso funciona bem com **um** tipo de componente de render e **um** consumidor por jogo. Mas repare
no que já está implícito: se amanhã existir `Model`/`Sprite`/`ParticleEmitter` como novos tipos de
"renderable", toda `Scene`-element de todo jogo precisa ganhar uma nova chamada nomeada
(`DrawModels(registry_)`, `DrawSprites(registry_)`, ...) — o conhecimento de "quais tipos de
renderable existem" fica espalhado pelas `Scene`s de cada jogo, não centralizado em um lugar.

Também já existe, sem uso, exatamente o tipo de gancho reativo que faltaria para resolver isso:
`EvtData_EntitySpawned` (`app/level_loader.h:24`) é disparado por `LevelLoader::Load` para *toda*
entidade carregada, e — precedente já registrado no próprio ADR-0009 — **não tem nenhum
subscriber ainda**. É um canal de "uma entidade passou a existir" já pronto, esperando um
consumidor.

## O que o GameCode4 faz (recapitulando a exploração anterior)

- `ScreenElementScene : public IScreenElement, public Scene` (`UserInterface.h:94`) — a `Scene`
  **é** um elemento da `HumanView`, não algo que ela só lê. `HumanView` a cria no construtor e a
  empurra pra pilha (`HumanView.cpp:78-86`, `VLoadGameDelegate`).
- `Scene` mantém um `SceneActorMap` (`ActorId → ISceneNode`, `Scene.h:53,79`) e se popula
  **reagindo a eventos** do próprio sistema de eventos do jogo — não varrendo nada por frame:
  `NewRenderComponentDelegate`, `ModifiedRenderComponentDelegate`, `DestroyActorDelegate`,
  `MoveActorDelegate` (`Scene.cpp:71-74`, registrados como listeners no construtor da `Scene`).
- A câmera é um nó **dentro** do próprio grafo (`CameraNode`, filho da raiz —
  `HumanView.cpp:82-86`), não algo que cada view guarda separadamente.
- O world transform é resolvido **durante** a recursão de render, numa matrix stack
  (`Scene::PushAndSetMatrix`/`PopMatrix`, `Scene.h:107-125`), não pré-computado.

A peça genuinamente interessante aqui — e a que motiva este ADR — é a primeira: **popular/consultar
a cena é orientado a eventos e indexado por ator, desacoplado de qual `View` está perguntando.**
Um `RemoteView` (que não desenha nada) e uma `HumanView` (que desenha) podem, em tese, consultar a
mesma `Scene`/mesmo índice sem duplicar conhecimento de "quais tipos de coisa existem para
mostrar".

## Uma ressalva, antes das opções: nem tudo no GC4 é mais orientado a dados

Vale nomear isso explicitamente, porque é fácil generalizar "GC4 usa eventos, logo é melhor"
longe demais: `MoveActorDelegate` reage a **cada movimento** de **cada ator** recalculando a
matriz daquele nó individualmente. Para um ator que se move todo frame (o jogador do
`camera_fps`, por exemplo), isso é uma notificação por frame por entidade — não é mais "orientado
a dados" que `PropagateTransforms`, é uma versão mais granular (e mais cara) do mesmo trabalho, só
que disparada por um sistema de eventos genérico em vez de uma função dedicada. O `PropagateTransforms`
atual — um array denso de `WorldTransform`, recomputado num passe único, antes de física/lógica/
qualquer view rodar (`engine.cpp:66`) — é estritamente melhor pra esse caso específico: cache-friendly,
sem overhead de despacho por evento, e sem risco de uma entidade renderizar com transform de um
frame atrasado dependendo da ordem dos handlers.

**A parte do GC4 que vale adotar é a indexação/descoberta de "o que existe pra mostrar" (estrutural,
esparsa — spawn/destroy/troca de tipo de renderable) — não recomputar transform via evento por
movimento (denso, todo frame).** São dois problemas diferentes, com cadências de atualização
diferentes; a proposta abaixo trata só do primeiro.

## Opções

### Opção 1 — Manter como está (status quo)

Cada `Scene`-element de cada jogo continua chamando funções nomeadas (`DrawBoxRenderables`, e uma
futura `DrawModels` etc.) diretamente.

| Prós | Contras |
|---|---|
| Zero código novo, zero risco. | Todo novo tipo de renderable = editar toda `Scene`-element de todo jogo que precisar dele. |
| Já funciona para os 2 jogos/1 tipo de renderable atuais. | Nenhum caminho genérico para um futuro `RemoteView` (que não pode chamar `DrawCubeV`) ou `AIView` (percepção "o que existe perto de mim") reaproveitarem — cada um reimplementaria seu próprio conhecimento de tipos de componente. |
| | `EvtData_EntitySpawned` continua sem nenhum uso real, apesar de já ser exatamente o gancho que resolveria isso. |

### Opção 2 — Índice de cena reativo, em cima do ECS (dados, não árvore de objetos) — recomendada

Um `SceneIndex` (nome sugestivo, `app/`) que:

- **Não substitui `PropagateTransforms`** — continua existindo exatamente como está, ADR-0002
  intocado.
- Se populatiza reagindo a mudanças **estruturais** (entidade ganhou/perdeu um componente de
  render), não a movimento. EnTT já oferece isso nativamente, sem precisar rotear pelo
  `EventManager`: `registry.on_construct<BoxRenderable>()`/`on_destroy<BoxRenderable>()` retornam
  um "sink" ao qual um listener se conecta — o mecanismo reativo do próprio ECS já escolhido
  (ADR-0001), não uma peça nova a manter. `EvtData_EntitySpawned` (ADR-0009) continua livre para
  quem quiser saber "uma entidade nova existe" antes de saber *o que* renderizar dela.
- Expõe uma consulta genérica — algo como `ForEachRenderable(registry, visitor)` ou um
  `entt::view` multi-tipo já composto — que qualquer `Scene`-element (de qualquer jogo), e no
  futuro um serializador de `RemoteView` ou uma consulta de percepção de `AIView`, chamam da mesma
  forma, sem precisar saber o nome de cada tipo concreto de renderable.
- Resolve de saída a duplicação que o ADR-0017 já apontou (sandbox quer `BoxRenderable` também,
  ainda não tem): em vez de `game/sandbox`'s `GameplayScene` ganhar sua própria cópia de
  `DrawBoxRenderables`, os dois jogos chamam o mesmo índice.

| Prós | Contras |
|---|---|
| Resolve a duplicação real já existente (2 jogos, mesmo tipo de renderable, chamada duplicada). | Código novo — precisa ser desenhado e testado, não é uma função de 8 linhas como `DrawBoxRenderables`. |
| Caminho genérico pronto para `RemoteView`/`AIView` no dia em que existirem — elas consultam o índice, não reimplementam conhecimento de tipos. | Ganho real só aparece com um segundo consumidor de verdade (mais um tipo de renderable, ou a primeira `RemoteView`/`AIView`) — construir antes disso é uma aposta, não uma correção de duplicação já visível (mitigado: a duplicação sandbox/camera_fps já é real hoje, então a Opção 2 tem gatilho concreto mesmo sem `RemoteView`/`AIView` existirem ainda). |
| Usa um mecanismo do EnTT já documentado (`on_construct`/`on_destroy`) — mesma disciplina do ADR-0002 de "usar o que a biblioteca já oferece antes de inventar". | |
| Mantém `PropagateTransforms` como é — não reabre a parte do ADR-0002 que está certa. | |

### Opção 3 — Adotar a forma do GC4 quase 1:1 (Scene como `IScreenElement`, árvore de `ISceneNode`, câmera como nó, transform resolvido via matrix stack durante o render)

| Prós | Contras |
|---|---|
| É literalmente o que o livro/GC4 faz — zero tradução conceitual. | Reabre o ADR-0002 pelo motivo errado: cria uma **segunda fonte de verdade** para transform (a matriz do `SceneNode` vs. `WorldTransform` do ECS) — exatamente o tipo de duplicação que o ADR-0002 evitou ao fundir a hierarquia nos componentes do EnTT. |
| Câmera-como-nó resolveria split-screen/observer de graça. | Volta a acoplar "computar transform" a "renderizar" (resolvido durante `VRender`, via stack) — pior para qualquer sistema (física, IA) que precise do transform deste frame **antes** do render rodar, problema que `PropagateTransforms` já resolve limpo hoje. |
| | Contradiz a postura já fechada no ADR-0002: EnTT é a camada de dados que o código de gameplay usa diretamente, não algo para envolver atrás de uma árvore de objetos OOP paralela. |
| | `Scene` como dono de estado (mapa ator→nó, câmera) dentro de uma `HumanView` específica é o oposto de "múltiplas views compartilham a mesma leitura" — cada `HumanView` teria sua própria árvore, não uma fonte compartilhada. |

**Não recomendada.** É a opção que mais literalmente responde "como o GC4 faz", mas comparada
diretamente com o que já temos, ela piora exatamente os dois pontos em que o ADR-0002 já tinha
razão (fonte única de verdade para transform; transform computado antes do render, não durante).

### Opção 4 — Câmera como componente genérico, desacoplado do `possessedActor_` (decisão menor, separável)

Hoje a câmera vive presa ao componente do ator possuído (`FirstPersonCameraRig` em `camera_fps`) —
suficiente para uma `HumanView` por jogo. Split-screen (duas `HumanView`s) ou uma view
observadora sem possuir ator nenhum (um `AIView` de debug assistindo a cena, uma `RemoteView`
espectadora) não têm hoje um lugar genérico pra guardar/achar uma câmera. Vale nomear como
componente genérico em `app/` (algo como `CameraComponent`, não necessariamente amarrado a
`possessedActor_`) — mas é uma decisão pequena, independente da Opção 2, que só vale a pena
desenhar quando split-screen ou uma segunda view simultânea forem um caso real, não hipotético.

## Recomendação

**Opção 2, faseada — não tudo de uma vez.**

1. **Agora, se este ADR for aceito**: construir o `SceneIndex` reativo (via `on_construct`/
   `on_destroy` do EnTT) e migrar `game/camera_fps`'s `FpsScene` pra usá-lo em vez de chamar
   `DrawBoxRenderables` direto — e, principalmente, dar a `game/sandbox`'s `GameplayScene` o mesmo
   caminho no dia em que ela ganhar `BoxRenderable` (a duplicação que motivaria isso já está
   prevista, não é hipotética — ADR-0017 já a registrou como pendência). Isso sozinho já paga a
   dívida técnica visível hoje, sem depender de `RemoteView`/`AIView` existirem.
2. **Adiar** a Opção 4 (câmera-como-componente-genérico) e qualquer "feed de diffs para
   `RemoteView`" até que uma dessas views realmente comece a ser construída — mesma disciplina que
   ADR-0015/0016/0017 já aplicaram repetidamente neste projeto ("o segundo consumidor real é quem
   mostra onde a abstração vaza, não adivinhação"). Diferente do caso do próprio ADR-0010 (que
   justificou construir `IGameView`/`BaseGameLogic` **antes** do gatilho, porque toda view futura
   precisaria da mesma costura), aqui a costura estrutural (o `SceneIndex`) já é suficiente
   preparação — a costura de rede/percepção específica de `RemoteView`/`AIView` não precisa existir
   ainda para o `SceneIndex` ser útil hoje.
3. **Não adotar a Opção 3.** Rejeitada pelos motivos acima — reabriria, para pior, uma parte do
   ADR-0002 que está correta.

Isso mantém o espírito do que você pediu (mais orientado a eventos/dados, preparado para
`AIView`/`RemoteView`, mais jogos) sem copiar a parte do GC4 que é objetivamente pior para o
formato ECS+raylib já escolhido.

## Tradeoffs aceitos (se a recomendação for adotada)

- **`SceneIndex` é código novo sem consumidor de produção imediato além de resolver a duplicação
  sandbox/camera_fps** — aceito porque essa duplicação já é real, não hipotética; o ganho para
  `RemoteView`/`AIView` é um bônus da mesma peça, não a justificativa primária.
- **Usar os sinais nativos do EnTT (`on_construct`/`on_destroy`) em vez do `EventManager` do
  projeto para esta camada especificamente** — uma pequena inconsistência de mecanismo (dois
  sistemas de notificação no projeto), aceita porque são sinais *dentro do mesmo processo, mesmo
  frame*, exatamente o caso de uso que o EnTT já resolve sem overhead extra; `EventManager`
  continua sendo o caminho certo para qualquer coisa que precise atravessar sistemas
  desacoplados ou, no futuro, a fronteira de rede (`RemoteView`).
- **Câmera continua presa ao ator possuído por enquanto** (Opção 4 adiada) — split-screen/observer
  continuam não suportados; não é regressão, é o mesmo estado de hoje.

## Consequences / follow-ups

- Se aceito, `docs/roadmap.md` ganha uma entrada em "Proposed", ligando aqui, distinta da entrada
  do ADR-0002 (que continua "Shipped" — este ADR não a substitui, complementa).
- `.claude/skills/engine-architecture/SKILL.md` deveria ganhar uma nota sobre `SceneIndex` uma vez
  implementado, mesma convenção que as outras peças de `app/`.
- Migrar `FpsScene` para o novo índice é a prova de conceito mínima — mesma disciplina de "construir
  contra um consumidor real" que o resto do projeto já segue.
- Este ADR não desbloqueia nem depende de `RemoteView`/`AIView` (ADR-0010) — eles continuam
  "nomeados, não construídos"; este ADR só prepara o terreno para quando isso mudar.

## Open Questions

- **Formato exato da consulta genérica** — `ForEachRenderable(registry, visitor)` com um
  `ISceneVisitor` pequeno, ou simplesmente um `entt::view` multi-componente pré-montado que a
  `Scene`-element itera diretamente? A segunda é mais simples e mais "ECS-idiomática" (menos
  indireção virtual); a primeira generaliza melhor no dia em que existir mais de um *tipo* de
  renderable simultaneamente (`BoxRenderable` + `Model` + ...) e a view quiser um único loop sobre
  ambos. Não decidido aqui.
- **`SceneIndex` é um objeto com estado (índice construído incrementalmente) ou só uma fachada
  sobre `registry.view<>()` recomputada a cada chamada?** Dado o tamanho do projeto (poucas
  entidades por cena, por constraint já registrada no ADR-0002), uma fachada sem estado próprio
  pode ser suficiente e mais simples — evita todo o problema de manter um índice sincronizado.
  Vale medir antes de assumir que precisa de estado.
- **Quando exatamente promover a Opção 4 (câmera genérica)** — quando split-screen ou a primeira
  `RemoteView`/`AIView` observadora virar trabalho real, não antes.
- **`AIView` de fato precisa do `SceneIndex`, ou só de queries ECS normais?** Percepção de IA
  ("o que existe perto de mim") tende a ser melhor servida por `registry.view<WorldTransform,
  AlgumaTag>()` direto — o mesmo idiom que `CameraFpsLogic::VOnUpdate` já usa — do que por um
  índice reativo pensado para descoberta de renderable. Pode ser que `SceneIndex` sirva
  principalmente `HumanView`/`RemoteView` (que precisam saber "o que existe pra mostrar"), e
  `AIView` nunca o use. Não decidido; revisitar quando o primeiro `AIView` real for desenhado.

## References

- [ADR-0002](0002-scene-graph-hierarchy-options.md) — a decisão que este ADR revisita parcialmente
  (mantendo `PropagateTransforms`/componentes ECS intocados, adicionando a camada de indexação que
  aquele ADR nunca cobriu).
- [ADR-0009](0009-level-loading-actor-placement.md) — `EvtData_EntitySpawned`, o canal reativo já
  existente e sem uso que esta proposta se apoiaria em espírito (ainda que a Opção 2 recomendada
  use os sinais nativos do EnTT para a parte estrutural, não este evento).
- [ADR-0010](0010-base-game-logic-and-igameview.md) — `RemoteView`/`AIView`, nomeadas e não
  construídas; o motivador direto de "preparar terreno" deste ADR.
- [ADR-0015](0015-sdk-productization-of-app-gated-on-second-consumer.md),
  [ADR-0016](0016-screen-element-stack.md), [ADR-0017](0017-camera-fps-second-game-module.md) —
  precedente repetido de "esperar o segundo consumidor real antes de generalizar", aplicado aqui à
  Opção 4 (adiada) e usado para justificar por que a Opção 2 tem gatilho suficiente hoje mesmo
  sem `RemoteView`/`AIView` (a duplicação sandbox/camera_fps já é o segundo consumidor).
- `app/scene_renderer.h`, `game/camera_fps/human_view.cpp` (`FpsScene`) — o estado atual que
  motivou a comparação.
- EnTT `docs/md/entity.md`, seção "Signals" — `on_construct`/`on_update`/`on_destroy`, o mecanismo
  citado na Opção 2.
- Local reference: `../gamecode4/Source/GCC4/Graphics3D/Scene.h`/`.cpp`,
  `../gamecode4/Source/GCC4/UserInterface/HumanView.h`/`.cpp`,
  `../gamecode4/Source/GCC4/UserInterface/UserInterface.h` (`ScreenElementScene`) — o código
  comparado nesta ADR.
