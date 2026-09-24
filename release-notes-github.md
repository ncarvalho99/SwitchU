# SwitchU 2.6.4

Major feature release introducing **Phase 6.1: YouTube Music Store & In-Console Downloader**, the **Multimedia Center (`Central Multimídia`)** with live audio visualizer and hardware volume synchronization, custom soundtrack BGM management, and virtual keyboard touch isolation.

<details>
<summary>📸 Click here to expand screenshots & previews 🔽</summary>
<br>

### 🎵 Installed Music Library (ThemeShop)
![Installed Music Library](https://raw.githubusercontent.com/ncarvalho99/SwitchU/master/screenshots/40.jpg)

### 🔍 YouTube Music Online Search & Previews
![YouTube Music Search](https://raw.githubusercontent.com/ncarvalho99/SwitchU/master/screenshots/41.jpg)

### 🎛️ Central Multimídia / Multimedia Center
![Multimedia Center](https://raw.githubusercontent.com/ncarvalho99/SwitchU/master/screenshots/39.jpg)

</details>

---

## English

### 🎵 YouTube Music Store & In-Console Downloader (BGM Shop)
- **New Music Tab in ThemeShop**: Added dedicated "Music (YouTube)" tab alongside Theme tabs.
- **Installed Music Library View**: By default, displays all local custom soundtracks installed on `sdmc:/config/SwitchU/music/`, showing track names, file sizes, and total storage space in the header (e.g. `2 songs - 18.3 MB on the SD card`).
- **Direct YouTube Search**: Press Search (or X) to search YouTube for any video, soundtrack, or artist, rendering authentic 16:9 video thumbnails and duration badges in a clean 2x2 card grid.
- **Pre-Download Size Estimation**: Accurately calculates estimated SD card storage before downloading (e.g., `Duration: 3:21 • Est. Size: ~4.8 MB`) based on duration and 192kbps stereo MP3 encoding.
- **True 0% – 100% Progress Bar**: Displays a real-time progress dialog during audio conversion and transfer with downloaded MB counter, smoothly reaching 100% on completion.
- **Automatic Thumbnail Downloads**: Video thumbnails are automatically saved alongside the audio as `<Title>.jpg`, displaying authentic artwork for your downloaded tracks in the shop and multimedia center.
- **Download Complete Modal Popup**: When a download finishes, an interactive dialog confirms the installation and offers immediate playback with `[ Play Now ]` or `[ OK ]`.
- **In-Shop Track Deletion**: Delete downloaded songs directly from the detail sheet (`[ Delete Track ]`) with immediate SD cleanup and playlist synchronization.
- **Clean ASCII Filename Sanitizer**: Automatically cleans non-ASCII symbols, emojis, and musical note characters (`♫`) to safe FAT32 filenames, preventing filesystem errors on Nintendo Switch.

### 🎛️ Multimedia Center (`Central Multimídia`)
- **HUD Quick Launcher**: Opened via top HUD button (`media_center.png`) with pulsing active playback glow or shoulder shortcuts.
- **Live Visualizer & Vinyl Graphic**: Real-time animated 4-band audio equalizer visualizer bars over a spinning vinyl record graphic.
- **Hardware Master Volume Sync**: Direct integration with Horizon OS `audctl` (`audctlGetActiveOutputTarget` & `audctlGetTargetVolume`), dynamically synchronizing on-screen volume with the console's physical Volume +/- buttons in real time.
- **Playlist Quick Delete**: When browsing the playlist in Row 2, press **`X`** to instantly delete the track from SD card with real-time list updating and contextual legend (`A: Play • X: Delete`).
- **Minus Button Isolation**: Pressing `-` (Minus) inside the Multimedia Center is strictly isolated, preventing background layout toggling.
- **Marquee Text Scrolling**: Automatic ping-pong scrolling for long song titles and audio mode descriptions.
- **Audio Precedence Modes**: Cycle between `Custom SD First`, `Theme Music First`, `Custom SD Only`, and `Theme & Preset Only`.

### 🛡️ Input Focus & Security
- **Touch Event Isolation**: While the on-screen keyboard (`TextEntryScreen`) or modal dialogs are active, touch inputs are consumed exclusively by the active layer, completely preventing touches from leaking through to background tabs and buttons.
- **Backend Architecture & Hardening**: Deployed hardened backend service (`switchu-ytdl.service`) with constant-time client key authentication (`X-SwitchU-Key`), strict regex input validation, SSRF domain whitelisting, concurrency limits, and live MP3 proxy streaming.

---

## Português (Brasil)

### 🎵 Loja de Músicas & Downloader do YouTube
- **Nova Aba de Músicas na Loja**: Adicionada a aba "Músicas (YouTube)" dentro da ThemeShop.
- **Biblioteca de Músicas Instaladas**: Por padrão, exibe todas as faixas locais salvas em `sdmc:/config/SwitchU/music/`, com tamanho dos arquivos e espaço total ocupado no cabeçalho (ex: `2 músicas - 18,3 MB no cartão SD`).
- **Busca Direta no YouTube**: Pressione Buscar (ou X) para procurar qualquer música, trilha sonora ou artista, exibindo miniaturas 16:9 em alta resolução e duração das faixas.
- **Estimativa de Tamanho Pré-Download**: Calcula com precisão o espaço necessário no cartão SD antes de baixar (ex: `Duração: 3:21 • Tam. Est.: ~4.8 MB`).
- **Barra de Progresso Real (0% – 100%)**: Exibe barra de progresso contínua com contagem de MBs baixados e avanço visual suave até a conclusão.
- **Download Automático de Miniaturas**: Salva a miniatura do vídeo junto com o áudio (`<Nome>.jpg`), exibindo a capa autêntica na biblioteca da loja e na central.
- **Janela de Confirmação Pós-Download**: Ao concluir o download, uma janela modal confirma o sucesso e permite iniciar a reprodução imediatamente com `[ Tocar Agora ]` ou `[ OK ]`.
- **Exclusão de Faixas na Loja**: Exclua músicas baixadas diretamente na janela de detalhes (`[ Excluir Música ]`) com atualização imediata da lista.
- **Sanitização de Nomes FAT32**: Limpa automaticamente caracteres especiais, emojis e notas musicais (`♫`), garantindo compatibilidade total com o sistema de arquivos do Switch.

### 🎛️ Central Multimídia
- **Acesso Rápido no Topo**: Acesse pelo botão no HUD superior (`media_center.png`) com animação pulsante durante a reprodução.
- **Equalizador Visual & Disco de Vinil**: Barras de equalização de áudio em 4 bandas animadas em tempo real sobre gráfico de vinil.
- **Sincronização de Volume de Hardware**: Integração direta com o serviço `audctl` do Horizon OS, acompanhando e ajustando o volume físico do console em tempo real.
- **Atalho de Exclusão Rápida**: Na lista de faixas (Linha 2), pressione **`X`** para excluir a faixa e miniatura do cartão SD instantaneamente com legenda no rodapé (`A: Tocar • X: Excluir`).
- **Isolamento do Botão Menos (-)**: Pressionar `-` dentro da Central Multimídia não altera mais o layout da tela inicial em segundo plano.
- **Texto Deslizante (Marquee)**: Rolagem horizontal suave para títulos longos e descrições de modos de áudio.
- **4 Modos de Precedência de Áudio**: `Músicas do SD Primeiro`, `Músicas do Tema Primeiro`, `Somente Músicas do SD` e `Somente Tema e Padrão`.

### 🛡️ Isolamento de Toque & Segurança
- **Isolamento de Entrada Touch**: Ao digitar no teclado virtual (`TextEntryScreen`), os toques são consumidos exclusivamente pelo teclado, impedindo qualquer clique acidental nas abas ou botões em segundo plano.
- **Serviço de Backend Seguro**: Serviço backend (`switchu-ytdl.service`) com autenticação por chave de cliente em tempo constante (`X-SwitchU-Key`), validação estrita por regex, proteção contra SSRF e streaming direto de MP3.
