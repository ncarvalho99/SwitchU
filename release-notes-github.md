# SwitchU 2.5.2

Everything here came from one console's logs, photos and a video. A launch that froze for half a minute, wrong icons after a catalogue reload, a game with no name, and logs that could not be copied off the card.

## English

### A launch that froze for thirty seconds

- Choosing a game left the launch animation stopped mid-expansion for about thirty seconds before the console handed over, and returning to a suspended game was slow the same way. The play time refresh -- added in 2.5.0 for the most-played view -- asked the system about every installed title, and each answer costs about a quarter of a second: 29 seconds on a console with 117 games, measured in its own log. The menu waits for its background work before giving the console to a game, so a launch during those seconds waited for the rest of the batch.
- It now asks only about titles that can have changed: the one just played, and any with no figure recorded yet. After the first run that is one query instead of a hundred and seventeen. The batch can also be stopped where it stands when a game is starting, so even a first run cannot hold a launch.

### Wrong icons after "reload games and shortcuts"

- The grid came back with the right play time badges and the wrong pictures, and stayed that way until the sort mode was changed and back. The reload replaced the grid's model with the sorted one without telling the icon loader, so every icon kept resolving to whatever title used to sit at its position. Only the hand-made order matched, which is why any sort showed it.

### Reloading is fast again

- "Reload games and shortcuts" cleared every cached name and icon and read them from the titles again -- about a second each, so roughly six minutes on a full console, with the grid on loading spinners the whole time. To pick up one new shortcut, which is what it is normally used for. It now asks only for what the catalogue is missing, and the expensive half moved to its own action, **Rebuild names and icons**, which says what it costs.

### Renaming a game

- **Rename**, in a game's dossier. Some titles have no name to give: a release downgraded to an older build can carry none at all, leaving the grid showing its title id. The chosen name holds in the grid, the title pill, the A-Z order, folders and the single-row view, and becomes the artwork search term when the title has no name of its own -- which is what lets SteamGridDB find it. An empty field restores the original.

### Play time in the dossier

- A game marked as a port carries two extra actions, which left room for two fact rows, and play time was the row dropped: its dossier showed none while the grid badge showed 37 hours. Mods gives up its row first now.

### Logs

- **Save logs for copying**, in System settings. Both logs are held open while the console runs, so copying them over MTP failed with "resource already in use" -- the two files worth asking for were the two that could not be read without rebooting first. This closes them and leaves copies nothing holds open.
- Every line in the daemon's log carried a 1970 date, and so did the names of its archived copies, which made them impossible to tell apart or to line up against the menu's log. The daemon was opening the application clock instead of the system one, so every read failed and the log fell back to counting from boot.

---

## Português

Tudo aqui saiu dos logs, fotos e um vídeo de um console. Um lançamento que congelava por meio minuto, ícones errados depois de recarregar o catálogo, um jogo sem nome e logs que não dava para copiar do cartão.

### Um lançamento que congelava por trinta segundos

- Escolher um jogo deixava a animação de lançamento parada no meio por cerca de trinta segundos antes de o console passar o controle, e voltar a um jogo suspenso demorava do mesmo jeito. A atualização de tempo jogado — adicionada na 2.5.0 para a ordenação por mais jogados — perguntava ao sistema sobre todos os títulos instalados, e cada resposta custa cerca de um quarto de segundo: 29 segundos num console com 117 jogos, medidos no próprio log dele. O menu espera o trabalho em segundo plano terminar antes de entregar o console ao jogo, então um lançamento durante esses segundos ficava esperando o resto do lote.
- Agora ele pergunta só sobre o que pode ter mudado: o título recém-jogado e os que ainda não têm valor registrado. Depois da primeira vez, é uma consulta em vez de cento e dezessete. O lote também pode ser interrompido quando um jogo está iniciando, então nem a primeira execução segura um lançamento.

### Ícones errados depois de "recarregar jogos e atalhos"

- A grade voltava com os tempos de jogo certos e as imagens erradas, e ficava assim até trocar a ordenação e voltar. O recarregamento substituía o modelo da grade pelo ordenado sem avisar o carregador de ícones, então cada ícone continuava resolvendo para o título que ocupava aquela posição antes. Só a ordem pessoal coincidia, por isso qualquer ordenação revelava o problema.

### Recarregar voltou a ser rápido

- "Recarregar jogos e atalhos" apagava todos os nomes e ícones em cache e lia tudo de novo dos títulos — cerca de um segundo cada, ou seja, uns seis minutos num console cheio, com a grade carregando o tempo todo. Para reconhecer um atalho novo, que é o uso normal. Agora ele busca só o que falta no catálogo, e a parte cara virou uma ação separada, **Reconstruir nomes e ícones**, que avisa quanto custa.

### Renomear um jogo

- **Renomear**, no painel do jogo. Alguns títulos não têm nome para informar: uma versão rebaixada pode não trazer nenhum, deixando a grade com o title id. O nome escolhido vale na grade, na legenda, na ordem A-Z, nas pastas e na linha única, e vira o termo de busca de capas quando o título não tem nome próprio — que é o que permite ao SteamGridDB encontrá-lo. Campo vazio restaura o original.

### Tempo jogado no painel

- Um jogo marcado como port ganha duas ações a mais, o que deixava espaço para duas linhas de fatos, e a descartada era o tempo jogado: o painel não mostrava nada enquanto a etiqueta na grade mostrava 37 horas. Agora "Mods" cede a vez primeiro.

### Logs

- **Salvar logs para cópia**, nas configurações de Sistema. Os dois logs ficam abertos enquanto o console roda, então copiá-los por MTP falhava com "resource already in use" — os dois arquivos que alguém pediria eram justamente os que não dava para ler sem reiniciar antes. A ação fecha os dois e deixa cópias que ninguém mantém abertas.
- Todas as linhas do log do daemon traziam data de 1970, e os nomes das cópias arquivadas também, o que tornava impossível distingui-las ou cruzá-las com o log do menu. O daemon abria o relógio de aplicativo em vez do de sistema, então toda leitura falhava e o log caía na contagem desde o boot.
