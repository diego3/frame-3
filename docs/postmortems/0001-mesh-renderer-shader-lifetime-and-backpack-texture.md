# Postmortem 0001: MeshRenderer (ADR-0020) — shader descarregado cedo demais + textura JPEG não suportada

- Status: Resolvido
- Data: 2026-08-11
- Impacto: só em ambiente de desenvolvimento (`game/flare_reactor`, ainda não commitado/lançado) —
  nenhum usuário final afetado. Registrado mesmo assim porque o processo de achar a causa raiz é o
  valor real aqui (ver "Lições aprendidas").
- Primeiro postmortem escrito neste projeto — ver `.claude/skills/engine-sre/SKILL.md` pro contexto
  de por que esse formato existe e quando usar.

## Resumo

Implementação inicial do ADR-0020 Fase A (`MeshRenderer`/`Light`/`.mat` YAML assets) parecia
funcionar (build limpo, 113 testes passando), mas o primeiro teste visual real do usuário revelou
três problemas: (1) o reator renderizava incompleto, (2) o backpack renderizava "gigante" mesmo
após uma correção de escala, e depois de corrigir (1), dois problemas novos apareceram: (3) o
backpack renderizava cinza sólido, sem sua textura, e (4) o tamanho do backpack lia como pequeno
demais. (1) e (2) tinham a MESMA causa raiz (um bug de lifetime de `shared_ptr`); (3) era um
problema de configuração do raylib vendorizado (JPEG desabilitado); (4) não era bug nenhum — era a
escala matematicamente correta, só pequena demais pro gosto do usuário.

## Linha do tempo

1. ADR-0020 Fase A implementado: `MeshRenderer`/`MeshRendererRoot`/`Light`/`.mat` YAML, reator e
   `survival_guitar_backpack` migrados. Build + suite de testes passando.
2. Refatoração pedida pelo usuário ("esses loaders na main.cpp tao muito grandes") — lógica movida
   pra `app/scene/{mesh_renderer,light,renderable}.h`. Sem mudança de comportamento.
3. Usuário reporta: "algum objeto ficou super gigante". Diagnóstico (sem rodar o jogo): o `size` do
   backpack (`0.15`) foi calculado a partir do `min`/`max` bruto dos *accessors* do glTF, que é
   espaço local PRÉ-transformação de nó — o glTF real bakea uma escala de ~100x por nó de mesh
   (`scene.gltf`'s `nodes[].matrix`). Corrigido pra `size: 0.0015` calculando a bounding box real
   (pós-transformação, via walk manual da árvore de nós). Nesta mesma resposta, usuário pediu um
   campo `active: bool` genérico em qualquer entity YAML, pra facilitar esse tipo de teste no
   futuro — implementado em `LevelLoader::Load` (`app/entity/level_loader.h`/`.cpp`).
4. Usuário reporta (texto, sem screenshot ainda): "1) o backpack ta super gigante ainda 2) o reator
   ficou incompleto, renderizando só um pedaço". Investigação por leitura de código não achou nada
   óbvio (a matemática de transform conferia no papel). Instrumentado com `TraceLog` temporário +
   `Xvfb` (só pra capturar log, não screenshot) — achou a causa raiz real: ver "Causa raiz 1"
   abaixo. Corrigido, log confirmou (nenhum "Unloaded shader" mais durante o load).
5. Usuário confirma com um screenshot real: reator está completo e correto (rim glow, núcleo
   emissivo, iluminação — tudo funcionando). Mas reporta dois problemas novos, específicos do
   backpack: muito pequeno, e totalmente cinza. Ver "Causa raiz 2" e "Não-bug" abaixo.
6. Ambos corrigidos; usuário pede que esse processo de correção de bug vire um postmortem escrito
   — este documento.

## Causa raiz 1: `RenderMaterial` cacheado descarregado antes do primeiro frame

**Sintoma**: reator renderizava incompleto (a maioria dos submeshes sem luz/rim/scroll corretos);
backpack aparentava "gigante" mesmo com a escala já corrigida.

**Causa raiz**: em `SpawnMeshRendererComponent` (`app/scene/mesh_renderer.h`), `frameMaterial`/
`coreMaterial` (`shared_ptr<RenderMaterial>` obtidos via `ResourceCache<RenderMaterial>::GetHandle`,
ver `app/resource/resource_cache.h`, ADR-0004) eram variáveis **locais** à função. `MergeSubmeshMaterial`
(`app/scene/material_loader.cpp`) só copia o `Shader` (struct `{id, locs}`) **por valor** pra dentro
de cada `RenderMaterial` por-índice que constrói — não estende a posse (`shared_ptr`) do handle
original. Quando `SpawnMeshRendererComponent` retornava, `frameMaterial`/`coreMaterial` saíam de
escopo; se nada mais segurasse uma referência viva àquele mesmo cache entry, o refcount zerava ali
mesmo, o deleter do cache (`UnloadShader`) disparava **imediatamente**, e todo `MeshRenderer` filho
ficava com um `Shader.id` apontando pra um programa GL já destruído — ou, pior, reciclado por
outra coisa (o log mostrou literalmente o skybox reutilizando o mesmo ID logo em seguida).

## Detecção

Não por screenshot — por leitura de código (não achou nada) e depois por instrumentação:
`TraceLog(LOG_WARNING, ...)` temporário em pontos-chave (`SpawnMeshRendererComponent`,
`DrawMeshRenderers`, `PushFrameUniforms`) + `Xvfb` rodando o binário real por alguns segundos só
pra capturar a saída de log (não uma imagem) via `DISPLAY=:99 timeout 5 ./flare_reactor > log.txt`.
O log do raylib (`SHADER: [ID N] ...`) mostrou a sequência decisiva:

```
SHADER: [ID 7] Program shader loaded successfully   <- reactor_frame.mat.yaml
SHADER: [ID 8] Program shader loaded successfully   <- reactor_core.mat.yaml
SHADER: [ID 8] Unloaded shader program data from VRAM (GPU)
SHADER: [ID 7] Unloaded shader program data from VRAM (GPU)
```
— os dois shaders descarregados **durante o carregamento do nível**, antes de qualquer frame ser
desenhado. Isso por si só provou a causa raiz sem precisar adivinhar mais.

## Resolução

`MeshRendererRoot` (o componente-âncora de lifetime no entity raiz de cada subtree) ganhou um novo
campo:
```cpp
std::vector<std::shared_ptr<RenderMaterial>> materialTemplates;
```
`SpawnMeshRendererComponent` agora guarda `{frameMaterial, coreMaterial}` ali, mantendo os handles
originais (e portanto o `Shader` compilado) vivos pelo tempo de vida do root — e, por extensão, de
todos os seus filhos. Confirmado via o mesmo método de log: nenhum "Unloaded shader" mais aparece
durante o carregamento do nível.

## Causa raiz 2: textura JPEG não suportada pelo raylib vendorizado

**Sintoma**: backpack renderizava com geometria/escala corretas, mas totalmente cinza sólido — sem
a textura base.

**Causa raiz**: `assets/models/survival_guitar_backpack/textures/Scene_-_Root_baseColor.jpeg` é um
JPEG real (confirmado via `file`, 4096x4096). `vendor/raylib/src/config.h` define
`SUPPORT_FILEFORMAT_JPG 0` ("Disabled by default") — o suporte a JPEG do `stb_image` embutido
(`STBI_NO_JPEG`) fica compilado fora do binário. `LoadImage` nesse arquivo falha silenciosamente
(bem, não tão silenciosamente — loga, mas não é um erro fatal): `IMAGE: Data format not supported`
/ `IMAGE: Failed to load image data`. O material resultante fica com a textura diffuse em branco,
lida como cinza sob a iluminação da cena.

## Detecção

Mesmo método: log do raylib sob `Xvfb`, sem screenshot — a linha `WARNING: IMAGE: Data format not
supported` logo após `Scene_-_Root_baseColor.jpeg` carregar deixou a causa óbvia.

## Resolução

Convertido pra PNG (`convert Scene_-_Root_baseColor.jpeg Scene_-_Root_baseColor.png`, ImageMagick;
PNG já é o único formato de textura usado no resto do projeto) e `scene.gltf`'s `images[0].uri`
reapontado pro novo arquivo. `.jpeg` original removido (inútil neste engine, mantê-lo só confundiria
o próximo a mexer no asset). Sem mudança de código C++ — o pipeline de "preservar o diffuse map
original do glTF" (`MergeSubmeshMaterial`) já fazia a coisa certa; só precisava de um arquivo que o
raylib conseguisse decodificar.

## Não-bug: escala do backpack "pequena demais"

O `size: 0.0015` calculado na sessão anterior estava matematicamente correto — confirmado
empiricamente lendo o `Mesh.vertices` real já carregado em memória (não só recalculando no papel):
bounding box real ~416x515x386 unidades, dando ~0.62x0.77x0.58 unidades de mundo com aquele fator.
O que a screenshot do usuário mostrou como "muito pequeno" era uma questão real de proporção visual
(o reator, ~3.4 unidades de altura, faz qualquer prop de ~0.7 unidades parecer minúsculo do lado),
não um erro de cálculo. Ajustado pra `size: 0.003` (~1.25x1.54x1.16 unidades) — maior que o
`box` de 1 unidade do player, lê como um prop de verdade na cena.

## Lições aprendidas / itens de ação

- **Um `shared_ptr<RenderMaterial>` do cache precisa de um dono explícito por todo o tempo em que
  qualquer cópia derivada do seu `Shader` (por valor) ainda existe** — não basta o `Shader` em si
  "parecer" independente (é só um `{id, locs}`, sem RAII próprio). Esse padrão (extrair um valor
  raylib de um handle gerenciado e assumir que ele sobrevive por conta própria) é fácil de repetir
  em qualquer lugar que combine `ResourceCache<T>` com "construir N objetos derivados a partir de
  1 handle" — vale grep por esse padrão se `MaterialManager`/`ShaderManager` (Fase B) for
  implementado.
- **`TraceLog` + `Xvfb` capturando só *log*, não screenshot, é uma ferramenta de debug válida e
  proporcional quando o usuário já reportou um bug concreto e pediu ajuda pra investigar** — não é
  o mesmo tipo de "auto-validação visual" que a convenção do projeto pede pra evitar (essa
  continua sendo do usuário). A distinção que importa: dado numérico/log pra confirmar uma
  hipótese vs. julgamento estético ("ficou bonito?").
- **Bounding box de um glTF não é o `min`/`max` dos accessors** quando o arquivo tem transformação
  de nó não-trivial (comum em exports do Sketchfab/Blender) — sempre confirmar contra o
  `Mesh.vertices` real pós-`LoadModel`, não só recalcular a mesma fórmula com mais cuidado.
- **Formato de asset (JPEG vs. PNG) é uma dependência de build tão real quanto uma lib vendorizada**
  — o raylib vendorizado aqui não suporta todo formato que um asset de terceiros (Sketchfab, etc.)
  pode trazer; checar `vendor/raylib/src/config.h`'s `SUPPORT_FILEFORMAT_*` antes de assumir que
  "é só uma imagem, vai carregar".

## Related

- [ADR-0020](../adr/0020-mesh-renderer-material-shader-layers.md) — o design cuja Fase A gerou os
  bugs acima; a seção de status do ADR também documenta a Causa raiz 1 resumidamente.
- `app/scene/mesh_renderer.h`'s `MeshRendererRoot` — comentário completo da Causa raiz 1 no código.
- `.claude/skills/engine-sre/SKILL.md` — onde esse formato de postmortem passa a ser referenciado.
