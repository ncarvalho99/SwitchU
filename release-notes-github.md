# SwitchU 2.5.3

Returning to the menu with HOME is quicker, the grid stops rebuilding itself over and over while names and icons are being read, and the daemon's log finally carries real dates. All of it measured on a console, from the logs 2.5.2 made collectable.

## English

### Returning to the menu

- Coming back with HOME takes about two seconds, and the console's own traces say where they go: 75 ms for the daemon to react, 628 ms for the system to start the menu process, 829 ms creating the menu, 251 ms to the first frame. A quarter of that last part was ours and recent: 2.5.1 had started asking the system for the recently-played widget's total play time while the menu was being created -- a figure that is stored and never drawn. It reads the cache instead.
- The rest of the menu's creation is now timed in the log, so the next round can say which step holds the remaining half second instead of guessing at it.

### Reading names and icons

- The daemon announced itself after every single title it cached, and each announcement makes the menu reload its app list and rebuild the whole grid. Rebuilding names and icons on a full console therefore spent minutes tearing the grid down and building it again about once a second. Announcements are held back while there is still a queue.

### The daemon's log

- Every line was dated 1970, and so were the names of the archived copies, which made them impossible to tell apart or to line up against the menu's log. libnx reads the clock once at startup and counts from there; the daemon has its own startup path and never did that read. It does now, and retries until the console's clock is actually set -- a system process starts before that happens.
- The memory snapshot also records how much of the daemon's heap is really in use. What it holds is what it reserved, which never changes; on a console whose System memory pool has ten megabytes free, against the twelve this daemon reserves, the real figure is what makes it safe to reserve less.

---

## Português

Voltar ao menu pelo HOME ficou mais rápido, a grade para de se reconstruir repetidamente enquanto nomes e ícones são lidos, e o log do daemon finalmente tem datas reais. Tudo medido num console, a partir dos logs que a 2.5.2 tornou coletáveis.

### Voltar ao menu

- Voltar pelo HOME leva cerca de dois segundos, e os registros do próprio console dizem onde eles vão: 75 ms para o daemon reagir, 628 ms para o sistema iniciar o processo do menu, 829 ms criando o menu e 251 ms até o primeiro quadro. Um quarto dessa última parte era nosso e recente: a 2.5.1 passou a perguntar ao sistema o tempo total do widget de jogo recente enquanto o menu era criado — um número que é guardado e nunca exibido. Agora ele lê do cache.
- O restante da criação do menu passa a ser cronometrado no log, para a próxima rodada dizer qual etapa segura o meio segundo que falta, em vez de chutarmos.

### Leitura de nomes e ícones

- O daemon se anunciava a cada título que colocava em cache, e cada aviso faz o menu recarregar a lista de aplicativos e reconstruir a grade inteira. Reconstruir nomes e ícones num console cheio passava minutos derrubando e remontando a grade cerca de uma vez por segundo. Os avisos agora são segurados enquanto ainda há fila.

### O log do daemon

- Todas as linhas tinham data de 1970, e os nomes das cópias arquivadas também, o que tornava impossível distingui-las ou cruzá-las com o log do menu. A libnx lê o relógio uma vez, no início, e conta a partir dali; o daemon tem seu próprio início e nunca fazia essa leitura. Agora faz, e repete até o console realmente ter o relógio ajustado — um processo de sistema sobe antes disso.
- O registro de memória também passa a anotar quanto do heap do daemon está de fato em uso. O que ele ocupa é o que reservou, e isso nunca muda; num console cujo pool System tem dez megabytes livres, contra os doze que este daemon reserva, o número real é o que permite reservar menos com segurança.
