# SwitchU 2.5.1

Fixes for a title stuck showing its id instead of its name, four settings that changed the wrong thing, and theme folders left behind by an interrupted install. The daemon also starts logging the console's memory pools around a game launch, which is groundwork rather than a change you will see.

## English

### A title showing its id instead of its name

- A game whose control data carried no usable name had the hex title id written into the metadata cache **as its name**. Nothing could tell that apart from a real name afterwards: the daemon stopped asking for the title, and the artwork lookup searched SteamGridDB for the id, which can never match. Reported on a Zelda downgraded to an older build, where reloading games and shortcuts only wrote the same string back.
- The placeholder is not written any more, and a cache already carrying one repairs itself on the next catalogue build -- nothing has to be deleted by hand.
- When the usual source answers without a name, the daemon now also tries the two other control-data sources before giving up. A downgraded title can get its name back this way.

### Settings

- **TV resolution** listed Auto, 720p, 1080p against the values the system stores as 0 Auto, 1 1080p, 2 720p, 3 480p, so choosing 720p set the console to 1080p and the other way round. The list follows the system's order now, and 480p can be chosen.
- **Screen burn-in reduction** wrote the stored auto-brightness flag -- the setting the toggle above it already owns -- instead of the burn-in one. It writes the right flag now.
- **Brightness** and **auto-brightness** were applied but never saved, so a restart brought the old value back.
- **Default profile** appeared twice in the list.
- **DNS** printed a fixed "Auto (DHCP)" and read nothing; it shows the servers the console is actually using, which matters if you set them by hand.
- **USB 3.0** says that the change only takes effect after a restart, instead of looking like it did nothing.
- **Region** is a read-only row now, rather than a selector that moved and changed nothing.
- **Console nickname** can be edited, with the menu's own keyboard.

### Themes

- A theme install cut short by a crash or a power cut left its staging folders behind, and one of them still holds a theme.json -- so it came back in the list as a duplicate of the theme it was installing, with the same id. Deleting either entry then removed both from the list and one folder from the card. Those folders are skipped in the list and swept once when the menu starts.

### Under the hood

- The daemon logs the Application, Applet, System and SystemUnsafe memory pools at four points around a launch, including the instant before a game gets a process. "Launching a game on a loaded console can be unstable" has never been measured, and this is what the measurement needs.

---

## Português

Correções para um jogo preso mostrando o id no lugar do nome, quatro configurações que alteravam a coisa errada e pastas de tema deixadas por uma instalação interrompida. O daemon também passa a registrar os pools de memória do console em torno do lançamento de um jogo, o que é preparação e não algo que você vê na tela.

### Um jogo mostrando o id no lugar do nome

- Um jogo cujos dados de controle não traziam nome utilizável tinha o id em hexadecimal gravado no cache de metadados **como se fosse o nome**. Depois disso nada conseguia distinguir um do outro: o daemon parava de consultar aquele título, e a busca de capas procurava no SteamGridDB pelo id, o que nunca encontra nada. Relatado num Zelda rebaixado para uma versão antiga, em que recarregar jogos e atalhos só regravava a mesma string.
- O valor falso não é mais gravado, e um cache que já tenha um se conserta sozinho na próxima reconstrução do catálogo — nada precisa ser apagado à mão.
- Quando a fonte habitual responde sem nome, o daemon agora tenta as outras duas fontes de dados de controle antes de desistir. Um título rebaixado pode recuperar o nome assim.

### Configurações

- **Resolução da TV** listava Auto, 720p, 1080p contra os valores que o sistema guarda como 0 Auto, 1 1080p, 2 720p, 3 480p, então escolher 720p colocava o console em 1080p e vice-versa. A lista agora segue a ordem do sistema, e dá para escolher 480p.
- **Redução de burn-in** gravava o flag de brilho automático — a configuração que o interruptor logo acima já controla — em vez do de burn-in. Agora grava o correto.
- **Brilho** e **brilho automático** eram aplicados mas nunca salvos, então o reinício trazia o valor antigo de volta.
- **Perfil padrão** aparecia duas vezes na lista.
- **DNS** mostrava um fixo "Auto (DHCP)" sem ler nada; agora mostra os servidores realmente em uso, o que importa para quem os define à mão.
- **USB 3.0** avisa que a mudança só vale após reiniciar, em vez de parecer que nada aconteceu.
- **Região** virou uma linha somente leitura, em vez de um seletor que se movia e não mudava nada.
- **Apelido do console** pode ser editado, pelo teclado do próprio menu.

### Temas

- Uma instalação de tema interrompida por travamento ou queda de energia deixava as pastas de trabalho para trás, e uma delas ainda tem um theme.json — então ela voltava na lista como uma cópia do tema que estava sendo instalado, com o mesmo id. Apagar qualquer uma das duas entradas tirava as duas da lista e só uma pasta do cartão. Essas pastas passam a ser ignoradas na lista e varridas quando o menu abre.

### Por baixo

- O daemon registra os pools de memória Application, Applet, System e SystemUnsafe em quatro pontos ao redor de um lançamento, incluindo o instante antes de o jogo ganhar um processo. "Abrir um jogo num console carregado pode ser instável" nunca foi medido, e é disso que a medição precisa.
