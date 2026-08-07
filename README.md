<div align="center">
    <h1>SwitchU — Homebrew launching</h1>
    <p>Test build. Not a release, not the default.</p>
</div>

---

This branch adds launching homebrew from the grid. It is here to be tried and
reported on. Read [docs/homebrew-recovery.md](docs/homebrew-recovery.md) before
turning it on — it is short, and it is the way back out.

**[Português](#português) below.**

---

## English

### What it does

`.nro` files on the card appear in the grid beside your games. Opening one runs
that homebrew, and leaving it returns to the page you started from.

Everything is under **SwitchU → Homebrew**, in the order you meet it: turn it
on, choose what it costs, restart the menu to see it, then manage the files.

### What it costs, and why

A home menu cannot run an `.nro` by itself. Homebrew runs inside `nx-hbloader`,
and `nx-hbloader` runs by taking the place of a system applet. So one applet
stops being itself while this is on, and which one is yours to pick:

| Setting | What stops working |
| --- | --- |
| **Parental controls** (default) | The PIN prompt is replaced, so parental controls stop being enforced |
| Album | Opening the Album gives homebrew instead of the screenshot gallery |

Parental controls is the default because most consoles never set it up. **If
yours has, switch to Album before enabling this.** The restrictions keep looking
configured in Settings while nothing applies them, and the console says nothing
about it. Holding **R** while the system raises a PIN prompt gives the real
applet, so it stays reachable for anyone who knows.

Atmosphère keeps **one** loader path for the whole system, so while this is on,
SwitchU's loader answers for every homebrew override on the console — including
one you set up on the Album yourself. The previous value is saved and put back
when you turn this off.

### Turning it off is the whole way back

Everything this writes lives on the microSD card under `atmosphere/`. Nothing
touches internal storage. Switching it off in the menu removes what it added; if
the console will not boot, deleting `atmosphere/config/override_config.ini` from
a computer does the same. The override only takes effect when an applet
launches, long after boot, so it cannot stop the console starting. Details,
including how to get parental controls back, are in
[docs/homebrew-recovery.md](docs/homebrew-recovery.md).

### What does not work, and will not

**Ports installed by another homebrew.** PortNX and the like install a shortcut
of their own that passes the port arguments saying which game to load. Launching
the `.nro` from the grid passes only its path — which is all any plain launcher
does — so a port that needs telling reports a missing file and stops. Those
arguments live inside the installed shortcut in a format nobody promises to
keep, so nothing here can recover them. **Open those from the shortcut their
installer made.**

Self-contained homebrew is unaffected. A port that finds its own data by a fixed
path, like Render96, launches fine.

### Hiding duplicates

An installed shortcut and the emulator behind it are two entries for what looks
like one thing — PortNX gives Ocarina of Time its real name and icon, and Ship
of Harkinian then shows up under its own. Which of the two you want is not
something to guess at, so **SwitchU → Homebrew** lets you take any file out of
the grid while leaving it on the card. Leave it on the card: the shortcut needs
it.

Deleting is there too, and removes the `.nro` only. The folder beside it stays,
because those hold saves and settings — JKSV keeps its backups in one.

### How it works

- `lib/switchu-hbloader/` — a vendored copy of
  [nx-hbloader](https://github.com/switchbrew/nx-hbloader) v2.4.5 with one
  change. Upstream always opens `hbmenu.nro` when started as an applet: the path
  it runs is only ever set by homebrew already running, through the
  `envSetNextLoad` ABI, so a menu starting it has no way to name a target. The
  fork reads one line from `sdmc:/config/SwitchU/next_nro.txt`, deletes it, and
  falls back to hbmenu exactly as upstream does when there is no request. Built
  from source by the `switchu-hbloader` xmake target, so the binary that ships
  and the source in this tree cannot drift apart. ISC, notice included.
- `projects/menu/src/launcher/HblOverride.*` — writes and removes the override.
  It adds one entry in the first free slot, leaves every other line as found,
  backs the file up first, and never overwrites anybody's own `hbl.nsp`.
- `SystemMessage::LaunchHomebrew` — the menu does not start applets, it asks the
  daemon. This is the first `Launch*` message carrying an argument, because
  every other one has a fixed applet and this one's is a setting.
- `tests/hbl_override/` — runs the override writer on a PC against a fake SD
  tree. It caught two defects a compiler never would.

### Reporting a problem

Turn the console off, put the card in a computer, and send whatever is in
`atmosphere/crash_reports`. Say which build. The `.log` files there are plain
text with nothing personal in them. Without them there is nothing to go on — the
console shows a code that says a crash happened and nothing about where.

---

## Português

### O que faz

Arquivos `.nro` do cartão aparecem na grade ao lado dos seus jogos. Abrir um
executa aquele homebrew, e sair dele volta para a página de onde você partiu.

Tudo fica em **SwitchU → Homebrew**, na ordem em que você encontra as coisas:
ligar, escolher o que custa, reiniciar o menu para ver, e gerenciar os arquivos.

### O que custa, e por quê

Um menu principal não consegue executar um `.nro` sozinho. Homebrew roda dentro
do `nx-hbloader`, e o `nx-hbloader` roda tomando o lugar de um applet do
sistema. Então um applet deixa de ser ele mesmo enquanto isto estiver ligado, e
qual deles é escolha sua:

| Ajuste | O que deixa de funcionar |
| --- | --- |
| **Controle parental** (padrão) | O prompt de PIN é substituído, então o controle parental deixa de ser aplicado |
| Álbum | Abrir o Álbum dá homebrew em vez da galeria de capturas |

O controle parental é o padrão porque na maioria dos consoles ele nunca foi
configurado. **Se no seu foi, troque para Álbum antes de habilitar.** As
restrições continuam parecendo configuradas em Configurações enquanto nada as
aplica, e o console não avisa nada disso. Segurar **R** enquanto o sistema
levanta um prompt de PIN dá o applet de verdade, então ele continua alcançável
para quem souber.

O Atmosphère guarda **um** caminho de loader para o sistema inteiro, então
enquanto isto estiver ligado o loader do SwitchU responde por todo override de
homebrew do console — inclusive um que você mesmo tenha configurado no Álbum. O
valor anterior é salvo e devolvido quando você desliga.

### Desligar é a rota de volta inteira

Tudo que isto escreve fica no cartão microSD, sob `atmosphere/`. Nada toca a
memória interna. Desligar a opção no menu remove o que foi adicionado; se o
console não bootar, apagar `atmosphere/config/override_config.ini` por um
computador faz o mesmo. O override só entra em ação quando um applet é lançado,
muito depois do boot, então ele não é capaz de impedir o console de ligar. Os
detalhes, inclusive como recuperar o controle parental, estão em
[docs/homebrew-recovery.md](docs/homebrew-recovery.md).

### O que não funciona, e não vai funcionar

**Ports instalados por outro homebrew.** O PortNX e semelhantes instalam um
atalho próprio, que passa ao port os argumentos dizendo qual jogo carregar.
Lançar o `.nro` pela grade passa apenas o caminho dele — que é o que qualquer
lançador comum faz — então um port que precisa ser avisado reporta arquivo
ausente e para. Esses argumentos vivem dentro do atalho instalado, num formato
que ninguém promete manter, então nada aqui consegue recuperá-los. **Abra esses
pelo atalho que o instalador deles criou.**

Homebrew autossuficiente não é afetado. Um port que acha os próprios dados por
caminho fixo, como o Render96, abre normalmente.

### Ocultar duplicatas

Um atalho instalado e o emulador por trás dele são duas entradas para o que
parece ser uma coisa só — o PortNX dá ao Ocarina of Time o nome e o ícone
corretos, e o Ship of Harkinian aparece em seguida sob o nome dele. Qual dos
dois você quer não é coisa para adivinhar, então **SwitchU → Homebrew** permite
tirar qualquer arquivo da grade deixando-o no cartão. Deixe no cartão: o atalho
precisa dele.

Apagar também está lá, e remove apenas o `.nro`. A pasta ao lado permanece,
porque elas guardam saves e configurações — o JKSV mantém os backups dele numa.

### Como funciona

- `lib/switchu-hbloader/` — uma cópia vendorizada do
  [nx-hbloader](https://github.com/switchbrew/nx-hbloader) v2.4.5 com uma
  modificação. O original sempre abre o `hbmenu.nro` quando iniciado como
  applet: o caminho que ele executa só é escrito por homebrew que já está
  rodando, pelo ABI do `envSetNextLoad`, então um menu que o inicia não tem como
  nomear um alvo. O fork lê uma linha de
  `sdmc:/config/SwitchU/next_nro.txt`, apaga o arquivo, e cai no hbmenu
  exatamente como o original quando não há pedido. Compilado a partir da fonte
  pelo alvo `switchu-hbloader` do xmake, para que o binário que embarca e a
  fonte desta árvore não possam divergir. ISC, com o aviso junto.
- `projects/menu/src/launcher/HblOverride.*` — escreve e remove o override.
  Adiciona uma entrada no primeiro slot livre, deixa as demais linhas como
  estavam, faz backup do arquivo antes, e nunca sobrescreve o `hbl.nsp` de
  ninguém.
- `SystemMessage::LaunchHomebrew` — o menu não inicia applets, ele pede ao
  daemon. Esta é a primeira mensagem `Launch*` que carrega argumento, porque
  todas as outras têm applet fixo e o desta é um ajuste.
- `tests/hbl_override/` — roda o escritor do override num PC, contra uma árvore
  de cartão falsa. Ele encontrou dois defeitos que compilador nenhum acharia.

### Reportando um problema

Desligue o console, coloque o cartão num computador, e mande o que estiver em
`atmosphere/crash_reports`. Diga qual build. Os arquivos `.log` de lá são texto
puro e não contêm nada pessoal. Sem eles não há por onde começar — o console
mostra um código que diz que houve um crash e nada sobre onde.
