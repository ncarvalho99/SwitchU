# SwitchU 1.1.0+fork.6

## English

Sixth release of the [ncarvalho99/SwitchU](https://github.com/ncarvalho99/SwitchU)
fork, based on [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU) 1.1.0.

### Game options redesigned as a dossier

- Pressing **+** on a game or homebrew now opens its software dossier directly.
  It combines the former options with game-specific information, artwork and mod
  management in one controller-friendly screen.
- The dossier shows the active icon, version, installed-mod count, local playtime,
  publisher, release date, genres, themes, game modes and Time to Beat estimates
  for quick, main-story and completionist playthroughs.
- IGDB metadata adds translated synopsis and story, publishers, gameplay captures,
  cover art and duration estimates. Metascore and user score are served through
  the SwitchU metadata service; catalogue responses are cached and never include
  console profile data.
- SteamGridDB gallery integration lets players browse covers and backgrounds,
  filter artwork by dimensions, expand previews, apply artwork, inspect the active
  artwork and restore the default icon. Non-matching cover ratios use a blurred
  supporting fill rather than stretching or cropping the game art.
- Full-screen gallery, artwork, restore and mods views now return with **B** to
  the dossier instead of closing to the home screen. Screenshot navigation and
  selection bounds were also corrected.

### Connectivity, themes and polish

- Added a real **Airplane Mode** toggle under Internet. It synchronizes Wi-Fi and
  other wireless state, correctly refreshes after returning to the tab, and is
  translated in every bundled language.
- Opening the system network applet no longer waits for an in-flight catalogue or
  gallery HTTP request to time out; pending small requests are cancelled safely
  during the handoff.
- Default Dark and Default Light return to the author's lightweight default
  themes, with accurate preview thumbnails. Theme browsing remains available from
  the catalogue, and the background-blur control now responds from its first step.
- The mod manager has a clean, independent card layout and system-style toggles
  in place of Enabled/Disabled chips. It keeps direct enable, disable and remove
  actions with restart guidance.
- The entire new interface, labels and notifications are localized for pt-BR,
  en-US, es-ES, fr-FR, de-DE, it-IT, nl-NL and ru-RU.

### Validation

- The sysmodule release payload was tested successfully on console, including the
  redesigned game options and artwork flow.

---

## Português

Sexta versão da fork [ncarvalho99/SwitchU](https://github.com/ncarvalho99/SwitchU),
baseada no [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU) 1.1.0.

### Opções do jogo redesenhadas como dossiê

- Pressionar **+** sobre um jogo ou homebrew agora abre diretamente o dossiê do
  software. Ele reúne as antigas opções, informações específicas, artes e
  gerenciamento de mods em uma única tela navegável pelo controle.
- O dossiê mostra ícone ativo, versão, quantidade de mods, tempo local jogado,
  publicadoras, lançamento, gêneros, temas, modos de jogo e estimativas do
  Time to Beat para jogo rápido, história principal e completo.
- Os metadados do IGDB acrescentam sinopse e história traduzidas, publicadoras,
  capturas de gameplay, capa e durações. Metascore e nota de usuários são
  fornecidos pelo serviço de metadados do SwitchU; as respostas ficam em cache e
  não incluem dados de perfil do console.
- A galeria SteamGridDB permite navegar por capas e fundos, filtrar as artes por
  dimensões, ampliar a prévia, aplicar a arte, consultar a arte ativa e restaurar
  o ícone padrão. Capas em proporção diferente recebem preenchimento desfocado,
  sem esticar ou recortar a arte do jogo.
- As telas em tela cheia de galeria, arte ativa, restauração e mods agora voltam
  com **B** para o dossiê, em vez de fechar para a tela inicial. A navegação de
  capturas e os limites da seleção também foram corrigidos.

### Conexão, temas e acabamento

- Adicionado um toggle real de **Modo avião** em Internet. Ele sincroniza o estado
  do Wi-Fi e das demais conexões sem fio, atualiza corretamente ao retornar à aba
  e foi traduzido para todos os idiomas incluídos.
- Abrir o applet de rede do sistema não espera mais uma consulta de catálogo ou
  galeria atingir timeout: as requisições pequenas pendentes são canceladas com
  segurança durante a transição.
- Default Dark e Default Light voltam a ser os temas padrão leves do autor, com
  miniaturas fiéis. A navegação de temas continua disponível no catálogo, e o
  controle de desfoque do fundo passou a responder desde o primeiro nível.
- O gerenciador de mods recebeu cards independentes sem linhas sobrepostas e
  toggles no estilo do sistema no lugar de botões Habilitado/Desabilitado. As
  ações de ativar, desativar e remover continuam diretas, com aviso de reinício.
- Toda a interface, rótulos e notificações novos foi localizada para pt-BR,
  en-US, es-ES, fr-FR, de-DE, it-IT, nl-NL e ru-RU.

### Validação

- O payload sysmodule de release foi testado com sucesso no console, incluindo
  as opções redesenhadas de jogo e o fluxo de artes.

<details>
  <summary><b>Capturas da fork.6 / Fork.6 screenshots</b></summary>

![](./screenshots/2.jpg)
![](./screenshots/3.jpg)
![](./screenshots/4.jpg)
![](./screenshots/5.jpg)
![](./screenshots/6.jpg)
![](./screenshots/7.jpg)
![](./screenshots/8.jpg)
![](./screenshots/9.jpg)
![](./screenshots/10.jpg)
![](./screenshots/11.jpg)

</details>

---

# SwitchU 1.1.0+fork.5

## English

Fifth release of this fork of [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0. It builds on fork.4 with animated backgrounds, a package-based theme
catalogue, persistent game sorting, and fixes validated on a real console.

**Animated themes**

- Theme Shop adds an **Animated Themes** tab. The catalogue and packages are
  downloaded directly from [themes.nclabs.dev](https://themes.nclabs.dev), the
  default HTTPS catalogue configured in SwitchU.
- Animated wallpapers are pre-encoded as DDS **BC7** frames at 912×512 and streamed
  to the GPU; no video is decoded while the menu is rendering. This is the single,
  definitive package quality, selected to stay inside the Switch applet memory budget.
- The menu requests a larger applet heap when available, derives a safe image budget
  from it, and uploads background frames incrementally. Returning from a game no
  longer waits for every animation frame to upload at once.
- Theme packages are checked for safe catalogue paths, archive limits and free SD
  space. Installation is transactional and reliably replaces an existing package:
  a failed install preserves the working theme.
- Download and extraction progress now remain monotonic, and the new Theme Shop
  messages are translated in all bundled languages.

**Home screen and stability**

- Press **R** to cycle between **My order**, **A–Z**, and **Recent**. Manual icon
  moves are saved as My order; Recent uses actual launches and uses the personal
  order only to break ties.
- Fixed the sort view so icon artwork moves with its title. Repeated sorting no
  longer exposes freed GPU textures or stale focus pointers, which caused artifacts
  and menu crashes during console testing.
- Community previews are pruned continuously and only visible candidates upload to
  the GPU. The catalogue also avoids unnecessary manifest requests when optional
  screenshots are absent.
- The daemon throttles application-view refreshes after title events. The former
  200 ms scan loop could cause severe launcher lag after a game exit; Zelda: Tears
  of the Kingdom was launched and returned to the menu successfully after this fix.
- The theme website was split into maintainable HTML, CSS and JavaScript assets and
  hardened with restrictive server security headers and safer client-side rendering.

**Validation**

- The definitive 912×512 BC7 animated-background format was validated on both the
  handheld display and TV.

---

## Português

Quinta versão desta fork de [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0. Ela evolui a fork.4 com fundos animados, catálogo de temas por pacote,
ordenação persistente dos jogos e correções validadas em um console real.

**Temas animados**

- A Loja de Temas traz a aba **Temas Animados**. O catálogo e os pacotes são
  baixados diretamente de [themes.nclabs.dev](https://themes.nclabs.dev), o
  catálogo HTTPS padrão configurado no SwitchU.
- Os fundos animados usam quadros DDS em **BC7** a 912×512 enviados em fluxo à GPU;
  não há decodificação de vídeo durante a renderização do menu. Esta é a qualidade
  única e definitiva dos pacotes, escolhida para respeitar a memória de applet.
- O menu pede um heap de applet maior quando o sistema o concede, calcula dele um
  orçamento seguro para imagens e envia os quadros de forma incremental. Voltar de
  um jogo não espera mais o upload de todos os quadros de uma vez.
- Os pacotes verificam caminhos seguros no catálogo, limites do arquivo e espaço
  livre no cartão. A instalação é transacional e substitui corretamente um tema já
  instalado: se falhar, o tema que funcionava é preservado.
- O progresso de download e extração agora é contínuo, e as novas mensagens da Loja
  de Temas foram traduzidas para todos os idiomas incluídos.

**Tela inicial e estabilidade**

- Pressione **R** para alternar entre **Minha ordem**, **A–Z** e **Recentes**.
  Movimentos manuais dos ícones são salvos em Minha ordem; Recentes usa aberturas
  reais dos jogos e usa a ordem pessoal somente para desempates.
- Corrigida a ordenação para que a arte do ícone acompanhe seu jogo. Alternar a
  ordem repetidamente não deixa mais texturas de GPU ou ponteiros de foco obsoletos,
  que causavam artefatos e crashes nos testes no console.
- As texturas de prévia da comunidade são podadas continuamente e somente candidatas
  visíveis chegam à GPU. O catálogo também evita buscar manifestos sem necessidade
  quando capturas opcionais estão ausentes.
- O daemon agora limita a atualização das views de aplicativos após eventos de
  títulos. A varredura anterior a cada 200 ms podia causar lag extremo no launcher
  após sair de um jogo; Zelda: Tears of the Kingdom abriu e retornou ao menu
  normalmente após esta correção.
- O site de temas foi separado em HTML, CSS e JavaScript mais fáceis de manter e
  reforçado com cabeçalhos de segurança restritivos e renderização mais segura no
  cliente.

**Validação**

- O formato definitivo de fundo animado em BC7 a 912×512 foi validado tanto na
  tela portátil quanto na TV.

---

# SwitchU 1.1.0+fork.4

## English

Fourth release of this fork of [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0. All the original work is his, and it stays credited in the About page.

A short one: two things that were wrong on screen, and one more attempt at the
crash people keep reporting.

**The reported crash — still open**

Someone with a 1TB card described it more precisely: it happens with many games
installed, a clean card is fine, it goes wrong while the shortcuts are being
built, and some shortcuts came out blank.

That last detail matters, because nothing found so far explains a blank icon.
Following it led to a real defect: when uploading an icon's texture failed, the
failure was ignored entirely. The icon stayed blank, and the texture slot it had
taken was never given back — so every failure made the next one likelier, which
is exactly the shape of "the more games, the worse it gets". The slot is
returned now, and the failure is written to the log with the pool's state.

**This is not a fix for the crash.** It is a real bug that produces one of the
reported symptoms, and it turns the next report into evidence instead of another
guess.

**Fixed**

- The theme screen ran at 30fps. It covers the home scene completely, and the
  optimisation that stops drawing an occluded scene had only ever been offered
  to the settings overlay — about 16ms a frame spent on pixels nothing could see
- Long setting descriptions ran over the control beside them instead of wrapping

---

## Português

Quarta versão deste fork do [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0. Todo o trabalho original é dele, e o crédito continua na página Sobre.

Uma versão curta: duas coisas que estavam erradas na tela, e mais uma tentativa
no crash que continua sendo relatado.

**O crash relatado — ainda em aberto**

Alguém com um cartão de 1TB descreveu melhor: acontece com muitos jogos
instalados, cartão limpo funciona, dá errado enquanto os atalhos estão sendo
criados, e alguns atalhos saíam em branco.

Esse último detalhe importa, porque nada do que foi encontrado até agora explica
um ícone em branco. Seguir por ele levou a um defeito real: quando o envio da
textura de um ícone falhava, a falha era simplesmente ignorada. O ícone ficava
em branco, e o espaço de textura que ele tinha ocupado nunca era devolvido — de
modo que cada falha tornava a seguinte mais provável, que é exatamente o formato
de "quanto mais jogos, pior fica". O espaço agora é devolvido, e a falha é
registrada no log junto com o estado do conjunto.

**Isto não é a correção do crash.** É um defeito real que produz um dos sintomas
relatados, e transforma o próximo relato em evidência em vez de mais um
chute.

**Corrigido**

- A tela de temas rodava a 30fps. Ela cobre a cena da home por completo, e a
  otimização que para de desenhar uma cena ocluída só havia sido oferecida ao
  painel de configurações — cerca de 16ms por frame gastos em pixels que
  ninguém podia ver
- Descrições longas de ajustes passavam por cima do controle ao lado em vez de
  quebrar linha

---

# SwitchU 1.1.0+fork.3

## English

Third release of this fork of [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0. All the original work is his, and it stays credited in the About page.

This one is about the crash people have been reporting, and about putting the
settings where they are actually looked for.

**The reported crash — attempt one, not confirmed**

A crash report finally arrived with symbols that match the build it came from.
It resolves to `WiiUMenuApp::onUpdate` branching to an address in no module at
all: a call through a pointer to an object that had already been destroyed.

Rebuilding the grid frees every icon, and two places kept using them afterwards.
The focus managers hold raw pointers and call `onFocusLost()` on whatever they
believe is focused; edit mode holds two more and dereferences both without
checking. Both are told now.

That explains the shape of the reports: a title installed while the menu is open
makes the grid genuinely different, so it is rebuilt — and the crash lands during
the rebuild, which is why the new shortcut is missing and only appears after a
restart. On a fresh install the same thing happens repeatedly while the icon
cache is still cold.

**It is not confirmed fixed.** It has never reproduced here, and both changes
come from reading code against crash reports rather than from watching it stop.

### If it still crashes

Two things, and neither needs any technical knowledge:

1. Turn the console off. Put the microSD card in a computer, open the folder
   `atmosphere/crash_reports`, and send whatever is inside — the `.log` files
   there are plain text and contain no personal information.
2. Say which version you were running. It is on the About page in Settings,
   and it should read **1.1.0+fork.3**.

That is everything. The other file needed to read those reports is
`SwitchU-1.1.0-fork.3-symbols.zip`, attached to this release — you do not need
to download it, and it is here rather than left in the build system because
build artifacts are deleted after 90 days and a report that arrives later than
that cannot be read without it.

Without the crash reports there is nothing to go on: the console shows an
error code that says a crash happened and nothing about where.

**Glass**

- Lowering glass sharpness turned the panels into visible squares. The blur
  takes nine samples spaced by its radius, so at the low end they landed 9 to
  36 texels apart with nothing read between them — sampling a grid rather than
  blurring it. Width now comes from repeating the pass instead of spreading it
- The account and power dialogs read sharper than the settings screen with the
  same setting. Refraction was displacing in panel-relative units, so it bent
  proportionally more behind a large panel than a small one. It is measured in
  pixels now, and a small window looks like a large one

**Appearance settings**

- New defaults, chosen after looking at them on a console rather than here:
  glass sharpness 40%, background animation speed 35%, background blur 5%.
  Defaults only apply to a fresh install — an existing one keeps what it has
- Glass sharpness, background animation speed and background blur moved out of
  Settings, Display and into the theme screen, next to the rest of what changes
  how the menu looks. They were in two places at once; now they are in one
- "Theme Shop" is now just **Themes**, in all eight languages

**Accounts**

- The top avatar opens the account list, with a tile for creating a new user
- The accounts dialog can be reached with the controller, not only by touch

**Tutorial**

- Skipping was one line in a corner panel at half size, indistinguishable from
  "skip step". It has its own centred prompt now, at nearly twice the scale,
  translated everywhere

**Under the hood**

- Cached names are terminated on read as well as on write, so a truncated cache
  file cannot walk off the end of one
- The devkitA64 toolchain exposes portlibs, which is what a local build needs to
  find the libraries it links against. No effect on the console

---

## Português

Terceira versão deste fork do [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0. Todo o trabalho original é dele, e o crédito continua na página Sobre.

Esta é sobre o crash que vem sendo relatado, e sobre colocar as configurações
onde as pessoas realmente procuram por elas.

**O crash relatado — tentativa um, não confirmada**

Chegou finalmente um crash report com símbolos que batem com a build que o
gerou. Ele resolve para `WiiUMenuApp::onUpdate` saltando para um endereço que
não existe em módulo nenhum: chamada através de um ponteiro para um objeto já
destruído.

Reconstruir a grade libera todos os ícones, e dois lugares continuavam usando-os
depois disso. Os gerenciadores de foco guardam ponteiros crus e chamam
`onFocusLost()` no que julgam estar focado; o modo de edição guarda mais dois e
desreferencia ambos sem verificar. Os dois passam a ser avisados.

Isso explica o formato dos relatos: um título instalado com o menu aberto torna
a grade de fato diferente, então ela é reconstruída — e o crash acontece durante
a reconstrução, que é por isso que o atalho novo não aparece e só surge depois de
reiniciar. Em instalação limpa a mesma coisa se repete enquanto o cache de
ícones ainda está frio.

**Não está confirmado como corrigido.** Nunca reproduziu aqui, e as duas
mudanças vêm de ler código contra crash reports, não de ver o problema parar.

### Se continuar crashando

Duas coisas, e nenhuma delas exige conhecimento técnico:

1. Desligue o console. Coloque o cartão microSD num computador, abra a pasta
   `atmosphere/crash_reports` e mande o que estiver lá dentro — os arquivos
   `.log` são texto puro e não contêm nenhuma informação pessoal.
2. Diga qual versão você estava usando. Ela aparece na página Sobre, dentro de
   Configurações, e deve estar como **1.1.0+fork.3**.

É só isso. O outro arquivo necessário para ler esses relatórios é o
`SwitchU-1.1.0-fork.3-symbols.zip`, anexado a esta release — você não precisa
baixá-lo, e ele está aqui em vez de ficar no sistema de build porque artefatos
de build são apagados depois de 90 dias, e um relato que chegue depois disso
não teria como ser lido.

Sem os crash reports não há por onde começar: o console mostra um código de
erro que diz que houve um crash e nada sobre onde.

**Vidro**

- Baixar a nitidez do vidro deixava os painéis quadriculados. O desfoque tira
  nove amostras espaçadas pelo seu raio, então no mínimo elas caíam a 9 e 36
  texels de distância sem ler nada entre elas — amostrando uma grade em vez de
  borrá-la. A largura agora vem de repetir o passe, não de espalhá-lo
- As janelas de contas e de energia apareciam mais nítidas que a de
  configurações com o mesmo ajuste. A refração deslocava em unidades relativas
  ao painel, e por isso desviava proporcionalmente mais atrás de um painel
  grande. Agora é medida em pixels, e uma janela pequena fica igual a uma grande

**Configurações de aparência**

- Novos padrões, escolhidos olhando no console e não aqui: nitidez do vidro 40%,
  velocidade da animação de fundo 35%, desfoque de fundo 5%. Padrões só valem
  para instalação nova — quem já tem configuração salva mantém a dele
- Nitidez do vidro, velocidade da animação de fundo e desfoque de fundo saíram de
  Configurações, Tela e foram para a tela de temas, junto do resto do que muda a
  aparência do menu. Estavam em dois lugares ao mesmo tempo; agora estão em um
- "Loja de temas" agora é só **Temas**, nos oito idiomas

**Contas**

- O avatar do topo abre a lista de contas, com um bloco para criar um usuário novo
- O diálogo de contas passa a ser alcançável pelo controle, não só por toque

**Tutorial**

- Pular era uma linha num painel de canto, em metade do tamanho, idêntica a
  "pular passo". Agora tem aviso próprio e centralizado, quase o dobro da escala,
  traduzido em todos os idiomas

**Por baixo**

- Nomes em cache são terminados na leitura além da escrita, para que um arquivo
  de cache truncado não possa passar do fim de um deles
- O toolchain devkitA64 expõe portlibs, que é o que um build local precisa para
  achar as bibliotecas que linka. Sem efeito no console

---

# SwitchU 1.1.0+fork.2

## English

Second release of this fork of [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0. All the original work is his, and it stays credited in the About page.

This one is mostly about themes, and about giving back control over how the
interface looks: a theme catalogue of our own alongside his, and sliders for the
things people said were too sharp, too busy or too fast. Here is the changelog:

**Themes**

- New theme shop catalogue with eight backgrounds, downloaded on demand — none of it is in the package
- PoloNX's catalogue is read alongside ours rather than replaced, so both sets appear in one list
- A catalogue that cannot be reached no longer empties the shop; whatever the other returns is still listed
- The dark theme is now the default on a fresh install, and the first-run tutorial follows it

**New settings**

- **Background blur** — softens the wallpaper and the shapes drifting over it, together
- **Glass sharpness** — how clearly the screen behind menus shows through them. The default matches how it looked before
- **Background animation speed** — from stopped to twice the theme's own pace

**Bug fixes**

- Fixed the settings overlay frosting the entire screen until the first button press
- Fixed the wallpaper and icons disappearing while the glass sharpness slider was moved
- Fixed the button hints in the corner appearing in English in every language — 15 of the 17 had never been translated
- Restored the more detailed sidebar icons

**Known issue**

A crash has been reported when installing a new game or homebrew. It does not
reproduce here and no crash report has reached us yet, so it is not fixed in this
release. If it happens to you, `sdmc:/atmosphere/crash_reports/` and
`sdmc:/config/SwitchU/` are what make it fixable.

---

## Português

Segunda versão deste fork do [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0. Todo o trabalho original é dele, e o crédito continua na página Sobre.

Esta é sobre temas e sobre devolver o controle da aparência: um catálogo de temas
nosso ao lado do dele, e sliders para o que as pessoas acharam nítido demais,
carregado demais ou rápido demais. Segue o changelog:

**Temas**

- Novo catálogo da loja de temas com oito fundos, baixados sob demanda — nada disso vai no pacote
- O catálogo do PoloNX é lido junto com o nosso, não no lugar dele, então os dois conjuntos aparecem numa lista só
- Um catálogo fora do ar não esvazia mais a loja; o que o outro devolver continua listado
- O tema escuro passa a ser o padrão em instalação nova, e o tutorial inicial acompanha

**Novas configurações**

- **Desfoque do fundo** — suaviza o papel de parede e as formas que flutuam sobre ele, juntos
- **Nitidez do vidro** — o quanto a tela atrás dos menus aparece através deles. O padrão reproduz como era antes
- **Velocidade da animação de fundo** — de parado até o dobro do ritmo do próprio tema

**Correções de bugs**

- Corrigido o painel de configurações deixando a tela inteira embaçada até o primeiro toque de botão
- Corrigidos o papel de parede e os ícones sumindo enquanto o slider de nitidez era arrastado
- Corrigidas as dicas de botão do canto aparecendo em inglês em todos os idiomas — 15 das 17 nunca haviam sido traduzidas
- Restaurados os ícones mais detalhados da barra lateral

**Problema conhecido**

Foi relatado um crash ao instalar um jogo ou homebrew novo. Não reproduz aqui e
nenhum relatório de crash chegou até agora, então não está corrigido nesta
versão. Se acontecer contigo, `sdmc:/atmosphere/crash_reports/` e
`sdmc:/config/SwitchU/` são o que torna isso corrigível.

---

## Installation / Instalação

Extract to the root of the microSD card, replacing the existing files. Requires Atmosphère.

Extraia na raiz do cartão microSD, substituindo os arquivos existentes. Requer Atmosphère.

---

## Previous releases / Versões anteriores

**1.1.0+fork.1** — microSD corruption from the power menu, return-to-menu stutter
from 1–2s to ~400ms, Settings from 30 to 60 fps, antialiased corners throughout,
sharper glass, and the About page identifying the fork.
