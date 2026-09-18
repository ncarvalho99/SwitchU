# SwitchU 2.6.1

Maintenance release bringing dynamic page sizing to the Home menu and folders, seamless icon drag-and-drop into folders, dedicated Plaza icon caching, and critical UI/focus polish.

## English

### Dynamic Pages & Page Management
- **Dynamic Launcher Pages**: Added a toggle in Theme Shop > Options to dynamically size Home launcher pages to fit installed games and folders, rather than keeping 8 fixed pages.
- **Hold-to-Create Animation**: Holding `ZR` (or touch-holding the `+` button) on the last page of the Home menu or open folder smoothly fills a radial progress arc; the new page is created once the animation reaches 100%. Releasing early cleanly cancels without creating a page.
- **Empty Page Deletion**: Pressing `(X) Delete page` on any empty page removes it from the Home grid or folder, accompanied by confirmation audio and screen transition.
- **Folder Page Cleanup**: Added "Delete empty pages" in Folder Options (Management tab) to quickly trim trailing empty pages.

### Folders & Drag-and-Drop Polish
- **Drag & Drop into Folders**: Fixed moving icons with `Y` into and out of folders. The ghost icon texture, scale, and placement anchors are preserved across folder boundaries, fixing the issue where icons collapsed into a dot in the top-left corner.
- **Streamlined Dossier**: Removed the redundant "Add to folder" tab from the Game Details (`+`) screen, keeping folder organization purely drag-and-drop.

### WaraWara Plaza & Performance
- **Dedicated Plaza Icon Cache**: Fixed community pedestal icon flickering and texture eviction thrashing in WaraWara Plaza by decoupling Plaza pedestal textures from the Home grid `IconStreamer`.
- **Large Icon Loading**: Expanded the `control_cache` buffer limit from 256 KB to 1 MB, ensuring large homebrew and port icons load reliably without falling back to letter tiles.
- **Ambient Miis Restricted to Plaza**: Removed wandering Miis from the Home menu background, keeping them exclusively in WaraWara Plaza.

### UI & Shortcut Polish
- **Theme Shop Selection Clarity**: Enhanced card selection borders, glow halos, and action button focus outlines for high visibility across all themes.
- **Consolidated Port Renaming**: Removed duplicate "Rename" button from Game Details when viewing ports, keeping it centralized under "Port options".
- **Shortcut Safety**: Audited `X` button actions across all screens to ensure strict non-overlapping behavior (`(X)` deletes empty pages, closes running games, or removes games from folders when applicable).
- **Localization**: Full translation updates across all 8 supported languages.

## Português (Brasil)

### Páginas Dinâmicas e Gerenciamento
- **Páginas Dinâmicas no Launcher**: Adicionada opção na Loja de Temas > Opções para ajustar a quantidade de páginas dinamicamente de acordo com os jogos e pastas instalados, em vez de manter 8 páginas fixas.
- **Animação Segurar para Criar**: Segurar `ZR` (ou segurar no botão `+` pelo toque) na última página da tela inicial ou de uma pasta preenche suavemente um anel de progresso; a nova página é criada quando a animação atinge 100%. Soltar antes cancela a ação.
- **Exclusão de Página Vazia**: Pressionar `(X) Excluir página` em qualquer página vazia a remove da grade inicial ou pasta com retorno sonoro e transição de tela.
- **Limpeza de Páginas em Pastas**: Adicionada ação "Excluir páginas vazias" nas Opções da Pasta (aba Gerenciamento).

### Pastas e Arrastar e Soltar
- **Arrastar e Soltar em Pastas**: Corrigida movimentação de ícones com `Y` para dentro e fora de pastas. A textura e proporções do ícone são mantidas nas transições, evitando o ponto fixo no canto superior esquerdo.
- **Dossiê Simplificado**: Removida a aba redundante "Adicionar à pasta" do dossiê de detalhes (`+`), mantendo a organização de pastas por arrastar e soltar.

### WaraWara Plaza e Desempenho
- **Cache Dedicado na Plaza**: Corrigido piscar de ícones e descarte de texturas na WaraWara Plaza desacoplando as texturas dos pedestais do streamer da grade inicial.
- **Carregamento de Ícones Grandes**: Aumentado o limite de leitura do cache de controle de 256 KB para 1 MB, permitindo que ícones grandes de homebrews e ports carreguem perfeitamente sem ícones de letras.
- **Miis Exclusivos na Plaza**: Removidos os Miis do fundo da tela inicial, mantendo-os exclusivamente na WaraWara Plaza.

### Interface e Atalhos
- **Clareza na Loja de Temas**: Maior contraste e bordas iluminadas nos cartões de temas e botões de ação para fácil visualização em qualquer tema.
- **Renomear Ports Centralizado**: Removido botão duplicado "Renomear" nos detalhes de ports, mantendo-o dentro de "Opções do port".
- **Auditoria de Atalhos**: Botão `X` revisado para comportamento não sobreposto (exclui páginas vazias, fecha jogos suspensos ou remove de pastas conforme o contexto).
- **Localização**: Traduções completas atualizadas para os 8 idiomas suportados.
