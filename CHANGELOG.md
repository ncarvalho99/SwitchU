# Não publicado / Unreleased

## English

**Themes**

- The theme shop is now just **Themes** in the sidebar, in every language
- Glass sharpness, background animation speed and background blur moved out of
  Settings > Display and into the Themes screen, next to the other controls that
  change how the menu looks. They exist in one place now, not two

**Crash on a fresh install — attempt 1, unconfirmed**

A reporter sees the menu crash twice on a clean install and work from the third
boot on. The focus manager was calling `onFocusLost()` on icons that a refresh
had already destroyed, and the guard written for exactly that case
(`invalidateWidget`) had never been called. It is called now.

This is the first attempt and it is **not confirmed to be the fix**. The crash
has never reproduced here, and the diagnosis comes from reading the code against
one report rather than from a crash report resolved to these lines. It stays open
until someone who could reproduce it says it stopped.

If it still happens, `sdmc:/atmosphere/crash_reports/` plus the
`SwitchU-symbols-sysmodule-release` artifact from the *same* build are what make
the next attempt better than a guess.

---

## Português

**Temas**

- A loja de temas agora é só **Temas** na barra lateral, em todos os idiomas
- Nitidez do vidro, velocidade da animação de fundo e desfoque de fundo saíram de
  Configurações > Tela e foram para a tela de Temas, junto dos outros controles
  que mudam a aparência do menu. Agora existem num lugar só, não em dois

**Crash em instalação limpa — tentativa 1, não confirmada**

Um usuário relata o menu quebrando duas vezes em instalação limpa e funcionando
a partir do terceiro boot. O gerenciador de foco chamava `onFocusLost()` em
ícones que um refresh já havia destruído, e a proteção escrita exatamente para
esse caso (`invalidateWidget`) nunca havia sido chamada. Agora é.

Esta é a primeira tentativa e **não está confirmada como a correção**. O crash
nunca reproduziu aqui, e o diagnóstico vem de ler o código contra um relato, não
de um crash report resolvido até estas linhas. Fica em aberto até alguém que
conseguia reproduzir dizer que parou.

Se continuar acontecendo, `sdmc:/atmosphere/crash_reports/` mais o artefato
`SwitchU-symbols-sysmodule-release` do *mesmo* build são o que faz a próxima
tentativa ser melhor que um chute.

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
