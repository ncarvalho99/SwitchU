# SwitchU 2.5.4

Settings now show where a loaded console's memory goes, uninstalling from the Storage tab stops freezing the menu, and the clock says it changed only once it has.

## English

### Memory and sysmodules

- The System tab ends with a new section: how full the System memory pool is, how much is free, and the sysmodules Atmosphère starts at boot, named from their toolbox.json when they ship one. 2.5.2's logs showed that pool at 221 of 232 MB on the console where launches were unstable; every sysmodule on the card draws from it, alongside the services a game needs to start, and nothing on the console showed it.
- Below 16 MB free, the section says to turn off the sysmodules you do not use. On firmware 21 and later it also says why the same card can be stable before a system update and not after it: Atmosphère can add only 7 MB to this pool there, against 40 MB before.

### Uninstalling from the Storage tab

- It had its own copy of the delete, running on the menu's main thread: the menu froze with no progress until it finished, a title that only lived on the card was reported as a failure even after its files were gone, and the title stayed filed in its folder. It now goes through the same path as deleting from a game's own panel, with the progress bar.

### Date and time

- "Date and time updated." and "Clock synchronized via Internet." appeared as soon as the command was sent, before the daemon had applied it or knew whether it worked. The daemon now answers, and the message waits for that answer. A failure shows the error, and the Internet sync toggle goes back to the state the console actually holds.
- Success also means the clock reads the time that was asked for, within two minutes. Before, it was enough for one of the console's three clocks to accept it.

---

## Português

As configurações agora mostram para onde vai a memória de um console carregado, desinstalar pela aba Armazenamento deixa de travar o menu, e o relógio só diz que mudou depois de mudar.

### Memória e sysmodules

- A aba Sistema termina com uma seção nova: quanto do pool de memória System está ocupado, quanto está livre e quais sysmodules o Atmosphère inicia no boot, com o nome do toolbox.json quando existe. Os logs da 2.5.2 mostraram esse pool em 221 de 232 MB no console onde a abertura de jogos era instável; todo sysmodule do cartão usa esse pool, junto com os serviços de que um jogo precisa para iniciar, e nada no console mostrava isso.
- Com menos de 16 MB livres, a seção sugere desativar os sysmodules que você não usa. No firmware 21 em diante, ela também explica por que o mesmo cartão pode ser estável antes de uma atualização do sistema e não depois: ali o Atmosphère só consegue acrescentar 7 MB a esse pool, contra 40 MB antes.

### Desinstalar pela aba Armazenamento

- A aba tinha a própria cópia da exclusão, rodando na linha principal do menu: o menu travava sem progresso até terminar, um título que só existia no cartão era dado como falha mesmo depois de os arquivos sumirem, e o título continuava na pasta onde estava. Agora usa o mesmo caminho da exclusão pelo painel do jogo, com barra de progresso.

### Data e hora

- "Data e hora atualizadas." e "Relógio sincronizado pela Internet." apareciam assim que o comando era enviado, antes de o daemon aplicar ou saber se tinha funcionado. Agora o daemon responde, e a mensagem espera essa resposta. Uma falha mostra o erro, e o botão de sincronização pela Internet volta ao estado que o console realmente tem.
- Sucesso também passa a significar que o relógio marca a hora pedida, com tolerância de dois minutos. Antes bastava um dos três relógios do console aceitar.
