**[Português](#lançar-homebrew-o-que-muda-e-como-desfazer) abaixo.**

# Launching homebrew: what it changes, and how to undo it

Read this before installing a build with homebrew launching enabled. Written
before the feature was, so that the way out exists whether or not the way in
works.

## The short version

Everything this feature adds lives on the microSD card, under `atmosphere/`.
Nothing is written to the console's internal storage. **Deleting the files
listed below restores stock behaviour completely** — there is no state left
behind in NAND to clean up, and no step that cannot be undone with the SD card
in a computer.

## Why an applet has to be taken over at all

SwitchU replaces qlaunch (`0100000000001000`), the home menu. A home menu cannot
run an NRO itself: homebrew runs inside `nx-hbloader`, and `nx-hbloader` runs by
taking the place of a system applet. Atmosphère does this with a file-based
override — the applet's title ID is pointed at `hbl.nsp` on the SD card.

So launching homebrew from the grid means one system applet stops being itself
while the override is in place. Which one is a setting, because the two
reasonable choices break different things.

## What gets taken over, and what that costs

| Setting | Title ID | What stops working while enabled |
| --- | --- | --- |
| **Parental controls** (default) | `0100000000001001` | The parental-control PIN prompt is replaced, so parental controls no longer block anything |
| Album | `010000000000100D` | Opening the Album gives homebrew instead of the screenshot gallery |

Parental controls is the default because on most consoles it is never set up,
and taking it costs those consoles nothing while leaving the Album working.

**If this console has parental controls set up, change this setting before
enabling homebrew launching.** The PIN prompt is what enforces them, and while
it is replaced they stop being enforced at all — the restrictions still appear
configured in Settings, and are no longer applied. Nothing warns about this on
the console itself, which is why it is stated here. Switching the setting to
Album restores enforcement immediately.

Holding **R** while the system raises a PIN prompt gives the real applet instead
of the loader, so the prompt still works for anyone who knows to do it. Do not
hold R while launching homebrew from the grid, though: the menu starts the
applet with no arguments at all, which the loader ignores and the real applet
aborts on. That crash is the applet, not the console, and nothing is left broken
afterwards.

Whichever is chosen, the other applet is untouched.

## Files this feature adds

```
sdmc:/switch/SwitchU/homebrew/hbl.nsp          the loader SwitchU ships
sdmc:/atmosphere/config/override_config.ini    which title ID it takes over,
                                               and which loader answers
```

`override_config.ini` is a file Atmosphère reads and other tools also write to.
If it already exists it is edited, not replaced, and the previous contents are
copied to `override_config.ini.switchu.bak` beside it first.

### This changes the loader for the whole console

Stock nx-hbloader always opens `sdmc:/hbmenu.nro`; there is no way to hand it a
particular NRO when it starts as an applet. Launching an individual homebrew
from the grid therefore needs a modified loader, and SwitchU ships one.

Atmosphère holds **one** loader path for the entire system — `path=` under
`[hbl_config]` is a single global, not a per-title setting. Pointing it at
SwitchU's loader means every homebrew override on the console uses SwitchU's
loader, **including one you set up yourself on the Album**. The previous value
is saved in the `.bak` file and put back when the feature is turned off.

If you rely on a specific loader build of your own, that is the setting that
takes it away, and turning this feature off is what returns it.

## Undoing it

### Normal case — the console boots

1. Turn the setting off in SwitchU, or
2. Power off, put the SD card in a computer, and delete
   `sdmc:/atmosphere/config/override_config.ini`.
   If `override_config.ini.switchu.bak` is there, rename it back over the
   original instead of deleting — that restores whatever the file held before.

Restoring the `.bak` matters more than it used to: it carries the loader path
your console used before, so putting it back returns your own hbl setup along
with everything else.

### The console does not boot, or hangs on the menu

The override is plain text on the SD card, so a computer is all that is needed.
No RCM payload, no NAND restore.

1. Hold **Power** for 12 seconds to force it off.
2. Put the microSD card in a computer.
3. Delete `atmosphere/config/override_config.ini` (or restore the `.bak`).
4. Put the card back and boot.

If it still does not boot, the cause is not this feature — the override only
takes effect when the applet is launched, which is long after boot. Rename
`atmosphere/contents/0100000000001000` to `…1000.off` to fall back to the stock
Nintendo home menu, and SwitchU is out of the picture entirely.

### Getting parental controls back

Nothing is stored, so there is nothing to restore: the moment the override is
gone — either by switching the host to Album or by deleting
`override_config.ini` — the real applet answers again and the existing PIN
works as before. The PIN itself lives in the console's own settings, and this
feature never reads, writes or clears it. The restrictions were configured the
whole time; only the prompt that enforces them was standing aside.

## What this never touches

- The console's internal storage — no title is installed, moved or deleted
- The parental-control PIN, or any other setting held in NAND
- `sdmc:/atmosphere/contents/`, apart from SwitchU's own `0100000000001000`
  that the normal installation already places there
- `sdmc:/atmosphere/hbl.nsp` — SwitchU's loader lives under `switch/SwitchU/`
  and a loader you installed yourself is never overwritten or deleted
- Kernel patches, KIPs, or anything loaded before Atmosphère hands off

## If something goes wrong anyway

`sdmc:/atmosphere/crash_reports/` and `sdmc:/config/SwitchU/` are what make a
report actionable, together with the `SwitchU-symbols-*` artifact from the
**same build** — symbols from a different build resolve to the wrong lines and
send the search in the wrong direction.

---

# Lançar homebrew: o que muda, e como desfazer

Leia isto antes de instalar um build com o lançamento de homebrew habilitado.
Escrito antes da funcionalidade existir, para que o caminho de volta exista
funcione ou não o caminho de ida.

## A versão curta

Tudo que esta funcionalidade adiciona fica no cartão microSD, sob `atmosphere/`.
Nada é escrito na memória interna do console. **Apagar os arquivos listados
abaixo restaura o comportamento original por completo** — não sobra estado
nenhum na NAND para limpar, e não há passo que não possa ser desfeito com o
cartão num computador.

## Por que um applet precisa ser tomado

O SwitchU substitui o qlaunch (`0100000000001000`), o menu principal. Um menu
principal não consegue executar um `.nro` sozinho: homebrew roda dentro do
`nx-hbloader`, e o `nx-hbloader` roda tomando o lugar de um applet do sistema. O
Atmosphère faz isso com um override baseado em arquivo — o título do applet é
apontado para um `hbl.nsp` no cartão.

Então lançar homebrew pela grade significa que um applet do sistema deixa de ser
ele mesmo enquanto o override existir. Qual deles é um ajuste, porque as duas
escolhas razoáveis quebram coisas diferentes.

## O que é tomado, e o que isso custa

| Ajuste | Título | O que deixa de funcionar enquanto ligado |
| --- | --- | --- |
| **Controle parental** (padrão) | `0100000000001001` | O prompt de PIN é substituído, então o controle parental deixa de barrar qualquer coisa |
| Álbum | `010000000000100D` | Abrir o Álbum dá homebrew em vez da galeria de capturas |

O controle parental é o padrão porque na maioria dos consoles ele nunca foi
configurado, e tomá-lo não custa nada a esses consoles, deixando o Álbum
funcionando.

**Se este console tem controle parental configurado, troque este ajuste antes de
habilitar o lançamento de homebrew.** O prompt de PIN é o que o aplica, e
enquanto ele estiver substituído nada é aplicado — as restrições continuam
aparecendo configuradas em Configurações, e deixam de valer. Nada avisa disso no
próprio console, que é o motivo de estar dito aqui. Trocar o ajuste para Álbum
restaura a aplicação imediatamente.

Segurar **R** enquanto o sistema levanta um prompt de PIN dá o applet de verdade
em vez do loader, então o prompt continua funcionando para quem souber. Mas não
segure R ao lançar homebrew pela grade: o menu inicia o applet sem argumento
nenhum, o que o loader ignora e o applet real aborta. Esse crash é do applet,
não do console, e nada fica quebrado depois.

Qualquer que seja a escolha, o outro applet fica intocado.

## Arquivos que esta funcionalidade adiciona

```
sdmc:/switch/SwitchU/homebrew/hbl.nsp          o loader que o SwitchU embarca
sdmc:/atmosphere/config/override_config.ini    qual título é tomado,
                                               e qual loader responde
```

O `override_config.ini` é um arquivo que o Atmosphère lê e que outras
ferramentas também escrevem. Se já existir, ele é editado e não substituído, e o
conteúdo anterior é copiado para `override_config.ini.switchu.bak` ao lado
antes.

### Isto troca o loader do console inteiro

O nx-hbloader original sempre abre `sdmc:/hbmenu.nro`; não há como entregar a
ele um `.nro` específico quando iniciado como applet. Lançar um homebrew
individual pela grade exige portanto um loader modificado, e o SwitchU embarca
um.

O Atmosphère guarda **um** caminho de loader para o sistema inteiro — a chave
`path=` sob `[hbl_config]` é global, não por título. Apontá-la para o loader do
SwitchU significa que todo override de homebrew do console passa a usar o loader
do SwitchU, **inclusive um que você mesmo tenha configurado no Álbum**. O valor
anterior é salvo no arquivo `.bak` e devolvido quando a funcionalidade é
desligada.

Se você depende de um build específico de loader, é esse ajuste que o tira, e
desligar a funcionalidade é o que o devolve.

## Desfazendo

### Caso normal — o console liga

1. Desligue o ajuste no SwitchU, ou
2. Desligue o console, coloque o cartão SD num computador, e apague
   `sdmc:/atmosphere/config/override_config.ini`.
   Se houver um `override_config.ini.switchu.bak`, renomeie-o de volta por cima
   do original em vez de apagar — isso restaura o que o arquivo continha antes.

Restaurar o `.bak` importa mais do que antes: ele carrega o caminho de loader
que o console usava, então devolvê-lo traz de volta o seu próprio hbl junto com
todo o resto.

### O console não liga, ou trava no menu

O override é texto puro no cartão SD, então basta um computador. Sem payload
RCM, sem restauração de NAND.

1. Segure **Power** por 12 segundos para forçar o desligamento.
2. Coloque o cartão microSD num computador.
3. Apague `atmosphere/config/override_config.ini` (ou restaure o `.bak`).
4. Devolva o cartão e ligue.

Se mesmo assim não ligar, a causa não é esta funcionalidade — o override só
entra em ação quando o applet é lançado, muito depois do boot. Renomeie
`atmosphere/contents/0100000000001000` para `…1000.off` para voltar ao menu
principal original da Nintendo, e o SwitchU sai de cena por completo.

### Recuperando o controle parental

Nada é armazenado, então não há o que restaurar: no momento em que o override
sai — seja trocando o applet para Álbum, seja apagando o `override_config.ini` —
o applet de verdade volta a responder e o PIN existente funciona como antes. O
PIN em si vive nas configurações do próprio console, e esta funcionalidade nunca
o lê, escreve ou apaga. As restrições estiveram configuradas o tempo todo; só o
prompt que as aplica é que estava de lado.

## O que isto nunca toca

- A memória interna do console — nenhum título é instalado, movido ou apagado
- O PIN do controle parental, ou qualquer outro ajuste guardado na NAND
- `sdmc:/atmosphere/contents/`, exceto o `0100000000001000` do próprio SwitchU
  que a instalação normal já coloca lá
- `sdmc:/atmosphere/hbl.nsp` — o loader do SwitchU vive em `switch/SwitchU/` e
  um loader que você tenha instalado nunca é sobrescrito nem apagado
- Patches de kernel, KIPs, ou qualquer coisa carregada antes do Atmosphère
  passar o controle adiante

## Se algo der errado mesmo assim

`sdmc:/atmosphere/crash_reports/` e `sdmc:/config/SwitchU/` são o que torna um
relato acionável, junto com o artefato `SwitchU-symbols-*` do **mesmo build** —
símbolos de outro build resolvem para as linhas erradas e mandam a investigação
na direção errada.
