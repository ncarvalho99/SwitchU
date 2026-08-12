# SwitchU 1.1.0+fork.5

## English

This is the fifth release of the [ncarvalho99/SwitchU](https://github.com/ncarvalho99/SwitchU)
fork, based on [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU) 1.1.0. The
original project remains credited in the About screen.

### Highlights

- Animated themes are available from the **Animated Themes** tab. SwitchU downloads
  its HTTPS catalogue and theme packages directly from
  [themes.nclabs.dev](https://themes.nclabs.dev).
- Animated backgrounds use one definitive package format: 912×512 DDS **BC7** frames
  streamed to the GPU at runtime, with no real-time video decoding.
- Press **R** to switch between My order, A–Z and Recent. Manual layout changes are
  saved; Recent is based on actual game launches.
- Package installation validates free SD space and archive input, installs
  transactionally, reliably replaces an old package, and reports consistent progress.
- Repeated sorting no longer leaves stale icon textures or focus pointers, fixing the
  visible artifacts and crashes discovered during console testing.
- Background and preview memory handling, game-return loading, and daemon refresh
  throttling were improved to keep the launcher responsive.
- The themes website now uses separated HTML/CSS/JavaScript assets and strengthened
  browser-facing security headers.

### Validated on console

- Animated-theme download, installation, replacement and application.
- Repeated switching among all three game-order modes, including manual icon moves.
- Zelda: Tears of the Kingdom launch and return to the launcher after the refresh
  throttling fix.

---

## Português

Esta é a quinta versão da fork [ncarvalho99/SwitchU](https://github.com/ncarvalho99/SwitchU),
baseada no [PoloNX/SwitchU](https://github.com/PoloNX/SwitchU) 1.1.0. O projeto
original continua creditado na tela Sobre.

### Destaques

- Os temas animados estão disponíveis na aba **Temas Animados**. O SwitchU baixa o
  catálogo HTTPS e os pacotes diretamente de [themes.nclabs.dev](https://themes.nclabs.dev).
- Os fundos animados usam um único formato definitivo: quadros DDS em **BC7** a
  912×512 enviados em fluxo à GPU, sem reprodução de vídeo em tempo real.
- Pressione **R** para alternar entre Minha ordem, A–Z e Recentes. As mudanças
  manuais no layout são salvas; Recentes usa aberturas reais dos jogos.
- A instalação de pacotes valida espaço livre no cartão e o conteúdo do arquivo,
  instala de modo transacional, substitui corretamente um pacote anterior e informa
  progresso consistente.
- Alternar repetidamente a ordem dos jogos não deixa mais texturas de ícone ou
  ponteiros de foco obsoletos, corrigindo os artefatos e crashes encontrados no
  console.
- O uso de memória de fundos e prévias, o retorno de jogos e a atualização do daemon
  foram ajustados para manter o launcher responsivo.
- O site de temas agora usa arquivos HTML/CSS/JavaScript separados e cabeçalhos de
  segurança reforçados para o navegador.

### Validado no console

- Download, instalação, substituição e aplicação de tema animado.
- Alternância repetida entre os três modos de ordem, inclusive após mover ícones
  manualmente.
- Abertura de Zelda: Tears of the Kingdom e retorno ao launcher depois da correção
  de atualização do daemon.
