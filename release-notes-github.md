# SwitchU 2.6.3

Targeted fix release addressing user-reported issues with game update title name resolution (Super Mario Bros. Wonder), WaraWara Plaza Mii avatar rendering and "no name" labels, and initial focus bounds on new game detected modal dialogs.

## English

### Title Name Resolution & Updates
- **Super Mario Bros. Wonder Name Display**: Fixed an issue where Super Mario Bros. Wonder (and games with modern update title blocks) displayed raw numeric Title IDs (`010015100B514000`) instead of the game's actual title name.
- **Enhanced NACP Decompression**: Upgraded `decompressNacpTitles` in the control cache to handle modern Deflate-compressed NACP title formats across variable buffer sizes and multiple windowBits modes (raw Deflate, zlib headers, auto-detect), verifying UTF-8 name validity.
- **Expanded Language Mapping**: Extended system language table to 18 entries, ensuring Brazilian Portuguese (`SetLanguage_PTBR = 17`) and other localized variants correctly resolve preferred language strings.
- **Built-in Title ID Fallback**: Added robust title resolution fallbacks for major first-party and popular Switch titles so that games with empty update NACP strings always display their authentic titles on the grid and in SteamGridDB searches.
- **Cache Logging & Flushing**: Added explicit log flushing in the daemon control-cache worker to ensure all title discovery and metadata caching operations are written to disk.

### WaraWara Plaza & Miis
- **Restored Bundled Guest Avatars**: Fixed asset packaging in the toolchain to ensure all 16 bundled high-resolution guest Mii avatar textures (`guest_01.png` – `guest_16.png`) and `plaza_dialogues.json` are installed into `sdmc:/switch/SwitchU/`.
- **Friendly Mii Names**: System Mii database records with default or empty names (`"no name"`, `"Mii"`, or blank) are now assigned distinct friendly guest names on the Plaza rather than showing `"no name"`.
- **Database Mii Disambiguation**: Resolved duplicate name handling that previously discarded valid database Miis, allowing all available system Miis to be placed across community pedestals with assigned head textures and favorite shirt colors.
- **Plaza Community Dialogues**: Restored authentic community game tips and Miiverse dialogue speech bubbles.

### Modal Dialogs & Focus
- **Button-Scoped Focus Rect**: Fixed `OverlayDialog` focus rect calculation so the focus target accurately matches the active button rather than inheriting the entire 1280x720 screen bounding box.
- **Dialog Pre-Layout**: Added immediate layout calculation on dialog show so button coordinates are positioned prior to cursor animation, ensuring the selection cursor frames the primary action button ("Download" / "Baixar") on the very first frame.
- **Global Cursor Suppression**: Suppressed the main menu global selection ring when modal dialogs, progress dialogs, or user selectors are active, eliminating unwanted screen-wide focus framing.

## Português (Brasil)

### Resolução de Nomes e Atualizações
- **Exibição do Nome de Super Mario Bros. Wonder**: Corrigida a exibição do Title ID numérico (`010015100B514000`) no lugar do nome oficial do jogo ao instalar atualizações.
- **Descompressão Aprimorada de NACP**: Suporte aprimorado no `control_cache` para descompressão de blocos de títulos NACP modernos em formato Deflate em múltiplos modos, validando nomes em UTF-8.
- **Mapeamento Completo de Idiomas**: Tabela de idiomas expandida para 18 entradas, garantindo resolução correta para Português Brasileiro (`SetLanguage_PTBR = 17`) e demais variações regionais.
- **Resolução de Títulos Conhecidos**: Adicionada tabela de mapeamento para grandes títulos da Nintendo, garantindo que atualizações com NACP sem strings de idiomas continuem exibindo seus nomes corretos na grade e no SteamGridDB.
- **Gravação em Disco no Daemon**: Adicionada gravação forçada (`flush`) no worker de cache de controle do daemon para persistência imediata dos registros de metadados no cartão SD.

### WaraWara Plaza e Miis
- **Avatares de Convidados Restaurados**: Corrigido o instalador da toolchain para incluir os 16 avatares de convidados em alta resolução (`guest_01.png` – `guest_16.png`) e `plaza_dialogues.json` na pasta `sdmc:/switch/SwitchU/`.
- **Nomes Amigáveis para Miis**: Miis do banco de dados do sistema que possuíam nomes padrão ou vazios (`"no name"`, `"Mii"`) agora recebem nomes variados e amigáveis na praça.
- **Desambiguação de Miis do Banco**: Corrigido o descarte de Miis com nomes duplicados, permitindo que todos os Miis cadastrados no console apareçam na praça com texturas faciais e camisetas coloridas variadas.
- **Diálogos do Miiverse**: Restaurados os balões de fala com dicas autênticas de jogos e postagens do Miiverse na praça.

### Janelas Modais e Foco
- **Retângulo de Foco nos Botões**: Corrigido o `focusRect` do `OverlayDialog` para corresponder exatamente ao botão selecionado em vez de cobrir a tela inteira de 1280x720.
- **Pré-Layout da Janela Modal**: Adicionado cálculo de layout imediato ao exibir a janela para que as coordenadas dos botões estejam prontas antes de posicionar o cursor, selecionando diretamente o botão principal ("Baixar" / "Download") no primeiro quadro.
- **Ocultação do Cursor Global**: O anel de seleção do menu principal agora é ocultado enquanto caixas de diálogo, telas de progresso ou seletor de usuários estiverem ativos.
