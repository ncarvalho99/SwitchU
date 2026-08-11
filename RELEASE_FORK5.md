# SwitchU 1.1.0+fork.5

Descrição pronta para a release do GitHub.

Esta é a evolução direta da `1.1.0+fork.4`. A base continua sendo o
[SwitchU de PoloNX](https://github.com/PoloNX/SwitchU); esta fork preserva os
créditos do projeto original e concentra as melhorias no launcher, na loja de
temas e na estabilidade no console.

## Destaques

- Fundos animados sem reprodução de vídeo em tempo real: quadros DDS/BC1 são
  carregados gradualmente e exibidos a 30 fps.
- Nova aba **Temas Animados**, prévias em folha animada e pacotes de tema.
- Seleção de qualidade **Padrão/Alta** para temas cujo catálogo disponibiliza
  a versão HD de 1280×720.
- Instalação resiliente: verifica espaço no cartão e só substitui um tema já
  instalado depois que o pacote novo terminou de extrair.
- Menos uso de memória de GPU na navegação da loja e menos requisições quando o
  catálogo está indisponível.
- Correção de um polling agressivo no daemon que podia deixar o launcher com lag
  extremo depois de abrir ou fechar um jogo.

## Validado no console

- Instalação e aplicação de tema HD, com 300 quadros a 1280×720 e 30 fps.
- Retorno ao launcher com tema animado estabilizado em 60 fps.
- Zelda: Tears of the Kingdom abriu e voltou ao menu sem repetir a falha observada
  antes da correção do polling.

## Observações

- O catálogo HD ocupa significativamente mais espaço no cartão e na memória de
  imagens; a escolha fica com a pessoa usuária.
- A validação visual em TV continua pendente; os testes desta release foram feitos
  na tela do console.
