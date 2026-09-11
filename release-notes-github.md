# SwitchU 2.5.0

First release of the [robsonbaioco/SwitchU](https://github.com/robsonbaioco/SwitchU) fork, continuing from [ncarvalho99/SwitchU](https://github.com/ncarvalho99/SwitchU) 2.4.3. Adds a most-played view to the home grid, with each game's hours on its icon.

## English

### Most played

- **R** now cycles a fourth view, **Most played**, after My order, A–Z and Recent. Games are ordered by the play time the console itself records, most played first; games never played go to the end, and ties keep your own arrangement.
- In this view every icon shows its hours in a small pill in the bottom-left corner. The other views stay clean.
- Play time is read from the system in a single batch, off the interface thread, whenever the menu opens in this view (including every return from a game), when you switch to it, and when a game is closed or suspended. The grid opens in the right order straight away from the last known values and re-sorts only if the fresh numbers change it.

### Play time everywhere else

- The game details panel and the Recent playtime widget now read play time the same way. The widget used a system call that libnx documents as available to games only, instead of the menu's own play data service.

### Updates and credits

- The Update tab and the manager now check this fork's releases. About names the fork's maintainer and credits ncarvalho99's fork alongside PoloNX's original.

### Build

- The homebrew (.nro) build links again: a stub for the self-uninstall request was missing outside the sysmodule build.
- CI installs `zstd`, and the SDL2 package falls back to an archived copy of the same file (same checksum) when its mirror is down.

---

## Português

Primeira versão do fork [robsonbaioco/SwitchU](https://github.com/robsonbaioco/SwitchU), a partir do [ncarvalho99/SwitchU](https://github.com/ncarvalho99/SwitchU) 2.4.3. Adiciona a ordenação por mais jogados na grade inicial, com as horas de cada jogo no ícone.

### Mais jogados

- O **R** agora alterna uma quarta ordenação, **Mais jogados**, depois de Minha ordem, A–Z e Recentes. Os jogos são ordenados pelo tempo de jogo que o próprio console registra, do mais jogado para o menos; jogos nunca jogados vão para o fim e empates mantêm a sua arrumação.
- Nessa ordenação cada ícone mostra as horas jogadas numa pequena etiqueta no canto inferior esquerdo. As outras ordenações continuam limpas.
- O tempo de jogo é lido do sistema de uma só vez, fora da thread da interface, sempre que o menu abre nessa ordenação (inclusive a cada volta de um jogo), quando você muda para ela e quando um jogo é fechado ou suspenso. A grade já abre na ordem certa com os últimos valores conhecidos e só reordena se os números novos mudarem a ordem.

### Tempo de jogo nos outros lugares

- O painel de detalhes do jogo e o widget de tempo de jogo recente agora leem o tempo de jogo do mesmo jeito. O widget usava uma chamada do sistema que a libnx documenta como disponível só para jogos, em vez do serviço de dados de jogo usado pelo menu.

### Atualizações e créditos

- A aba Atualizar e o gerenciador agora consultam as releases deste fork. A tela Sobre mostra o mantenedor do fork e credita o fork do ncarvalho99 junto com o original do PoloNX.

### Build

- O build homebrew (.nro) volta a linkar: faltava um stub para o pedido de desinstalação fora do build de sysmodule.
- O CI instala o `zstd`, e o pacote do SDL2 usa uma cópia arquivada do mesmo arquivo (mesmo checksum) quando o mirror está fora do ar.
