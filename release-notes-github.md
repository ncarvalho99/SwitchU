# SwitchU 2.6.0

Major release introducing the initial implementation of the authentic Wii U WaraWara Plaza, Most Played sort mode with playtime badges, custom title renaming in the dossier, minimalist dossier layout refinements, and integrated downstream system/storage fixes.

## English

> **⚠️ WaraWara Plaza Notice**: This is an initial implementation of the WaraWara Plaza. It currently runs in offline mode using local community posts and system Mii avatars. Online network integration (Miiverse / Pretendo community services) along with further animations and polish will be introduced in future updates.

### WaraWara Plaza (Phase 5)

- Added the authentic Wii U WaraWara Plaza accessible via the screen swap button in the top bar.
- 3D-perspective radial plaza featuring authentic blue pedestals, carousel rotation, and ambient wandering Miis with dynamic pacing and idle behaviors.
- Speech bubbles displaying community commentary, tips, and thoughts around installed game pedestals.
- Interactive Wii U pointer hand cursor supporting analog stick, touch, and motion navigation.
- Smooth camera zoom controls (`ZL` / `ZR`) and extended panning with the Right Stick.
- Direct game launching and Software Information dossier access directly from community pedestals.
- Seamless, locked 60 FPS transitions between the Home Grid and WaraWara Plaza.

### Most Played Sort Mode & Playtime Badges

- Added "Most played" as sort mode 4 in the `R` shoulder button cycle, sorting games by total play duration.
- Compact playtime badges (e.g. "8 h", "28 min") rendered directly on game icons when in Most played mode.
- Asynchronous playtime queries via `pdm:qry` that cache durations to keep navigation smooth and instantaneous.

### Game Details Dossier Polish & Custom Renaming

- In-dossier game renaming (`Rename`): customize titles for games and homebrew; leaving the name empty restores the original NACP catalogue title.
- Streamlined left rail to 6 clean options, preventing vertical overflow and ensuring Version, Play time, and Mods are always visible.
- Removed "Mark as game port" from official Nintendo Switch titles.
- Nested "Restore default" directly inside the "Active artwork" modal dialog, allowing one-step restoration of custom covers and backgrounds.
- Consolidated game port management ("Edit search title" and "Unmark port") into a single "Port options" dialog.
- Fixed vertical navigation clamp so controller focus reaches all actions smoothly down to Delete software.

### Storage, Grid Synchronization & System Settings

- Fixed software deletion grid sync: automatic icon streamer title remapping in projected sort modes prevents stuck loading spinners and stale selection pills.
- Full SD footprint deletion removing LayeredFS contents and community port folders with real-time progress.
- Settings > System: live memory pool breakdown and active sysmodule enumeration, plus in-menu nickname editing.
- Settings > Display & Internet: live DNS display, USB 3.0 restart notification toast, complete resolution picker, and reliable brightness persistence across reboots.
- Automatic log rotation (`menu-*.log`, `daemon-*.log`) and "Save logs for copying" in System settings for simple troubleshooting.

---

## Português

> **⚠️ Aviso sobre a WaraWara Plaza**: Esta é uma implementação inicial da WaraWara Plaza. Atualmente ela funciona em modo offline utilizando postagens comunitárias locais e avatares Mii do sistema. A integração online (serviços de comunidade Miiverse / Pretendo) e refinamentos visuais adicionais serão introduzidos em atualizações futuras.

### WaraWara Plaza (Fase 5)

- Adicionada a autêntica WaraWara Plaza do Wii U, acessível através do botão de troca de tela na barra superior.
- Praça radial em perspectiva 3D com pedestais azuis autênticos, rotação de carrossel e Miis caminhando com ritmo e comportamentos naturais.
- Balões de fala exibindo comentários da comunidade, dicas e pensamentos ao redor dos pedestais de jogos instalados.
- Cursor de mão apontadora autêntico do Wii U com suporte a analógico, toque e movimento.
- Controles suaves de zoom (`ZL` / `ZR`) e visão panorâmica com o analógico direito.
- Inicialização direta de jogos e acesso à ficha de informações a partir dos pedestais da praça.
- Transições fluidas e estáveis a 60 FPS entre a Grade Principal e a WaraWara Plaza.

### Modo de Ordenação Mais Jogados e Indicadores de Tempo

- Adicionado o modo "Mais jogados" como modo 4 no ciclo do botão `R`, ordenando títulos pelo tempo total de jogo.
- Indicadores compactos de tempo de jogo (ex.: "8 h", "28 min") exibidos nos ícones dos jogos no modo Mais jogados.
- Consultas assíncronas de tempo via `pdm:qry` com cache local para manter a navegação instantânea e sem travamentos.

### Ficha de Detalhes do Jogo e Renomeação Personalizada

- Renomeação de jogos na ficha (`Renomear`): personalize títulos de jogos e homebrews; deixar o campo vazio restaura o nome original do catálogo NACP.
- Menu lateral simplificado com 6 opções limpas, evitando sobreposição vertical e garantindo que Versão, Tempo de jogo e Mods estejam sempre visíveis.
- Removida a opção "Marcar como port de jogo" de títulos oficiais do Nintendo Switch.
- Opção "Restaurar padrão" integrada diretamente dentro da janela de "Arte ativa", permitindo restaurar capa e fundo personalizados em um só lugar.
- Consolidadas as ações de port ("Editar título de busca" e "Desmarcar port") em um único diálogo de "Opções de port".
- Corrigida a trava de navegação vertical para permitir alcançar todas as opções com o controle até "Excluir software".

### Gerenciamento de Armazenamento, Sincronização da Grade e Configurações

- Corrigida a sincronização da grade pós-exclusão: o remapeamento automático do streamer em modos de ordenação projetados evita ícones travados em carregamento e títulos desalinhados.
- Exclusão completa de arquivos no SD, removendo pastas de mods LayeredFS e ports com barra de progresso em tempo real.
- Configurações > Sistema: exibição detalhada dos pools de memória, lista de sysmodules ativos e edição de apelido no menu.
- Configurações > Tela e Internet: exibição de servidores DNS ativos, aviso de reinicialização para USB 3.0, seletor de resolução e persistência de brilho após reinicialização.
- Rotação automática de logs (`menu-*.log`, `daemon-*.log`) e opção "Salvar logs para cópia" em Configurações do Sistema.
