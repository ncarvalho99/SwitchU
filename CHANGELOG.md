# SwitchU 1.1.0+fork.1

## English

Hello everyone. This is a fork of [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0 — all the original work is his, and it stays credited in the About page.
This build focuses on three things: a bug that was corrupting microSD cards,
the stutter when returning to the menu, and the jagged corners throughout the
interface. Everything below was measured on hardware. Here is the changelog:

**Bug fixes**

- Fixed microSD card corruption when rebooting or shutting down from the power menu
- Fixed game names showing as garbage characters on some titles
- Fixed the app grid briefly going empty while the list refreshed
- Fixed long text running off the edge of the About page

**Performance**

- Returning to the menu after closing a game or homebrew: 1–2 seconds down to ~400 ms
- Opening Settings for the first time: 1.5 seconds down to 135 ms
- Settings now runs at 60 fps instead of 30
- Home screen holds 60 fps while drawing with a quarter of the geometry it used before

**Appearance**

- Smooth, antialiased corners everywhere: game covers, Settings cards, power menu, account picker and the selection outline
- Sharper glass in Settings, the power menu and the account picker

**Other**

- About page now identifies this build as a fork and links both repositories
- Version reads 1.1.0+fork.1
- CI builds release binaries with symbols; build time 21 min down to 3 min
- Shader changes are no longer silently skipped by the build

---

## Português

Olá a todos. Este é um fork do [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU)
1.1.0 — todo o trabalho original é dele, e o crédito continua na página Sobre.
Esta build foca em três coisas: um bug que estava corrompendo cartões microSD, o
engasgo ao voltar para o menu, e os cantos serrilhados por toda a interface. Tudo
abaixo foi medido no console. Segue o changelog:

**Correções de bugs**

- Corrigida a corrupção do cartão microSD ao reiniciar ou desligar pelo menu de energia
- Corrigidos nomes de jogos aparecendo como caracteres estranhos em alguns títulos
- Corrigida a grade de aplicativos ficando vazia por um instante durante a atualização da lista
- Corrigido o texto longo saindo para fora da página Sobre

**Desempenho**

- Voltar ao menu depois de fechar um jogo ou homebrew: de 1–2 segundos para ~400 ms
- Abrir as Configurações pela primeira vez: de 1,5 segundo para 135 ms
- As Configurações agora rodam a 60 fps em vez de 30
- A tela inicial mantém 60 fps desenhando com um quarto da geometria que usava

**Aparência**

- Cantos suavizados em todo o menu: capas de jogos, cards das Configurações, menu de energia, seletor de conta e o contorno de seleção
- Vidro mais nítido nas Configurações, no menu de energia e no seletor de conta

**Outros**

- A página Sobre identifica esta build como fork e mostra os dois repositórios
- A versão passa a ser 1.1.0+fork.1
- O CI compila binários de release com símbolos; tempo de build de 21 min para 3 min
- Mudanças em shaders não são mais ignoradas silenciosamente pela compilação

---

## Installation / Instalação

Extract to the root of the microSD card, replacing the existing files. Requires Atmosphère.

Extraia na raiz do cartão microSD, substituindo os arquivos existentes. Requer Atmosphère.
