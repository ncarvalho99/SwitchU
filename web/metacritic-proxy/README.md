# SwitchU metadata bridge

Serviço de leitura mínima para o painel de detalhes de jogos. Ele expõe
metadados públicos do IGDB, MetaScore/User Score e suas contagens. Para a
interface `pt-BR`, traduz a sinopse uma única vez no servidor via Gemini e
armazena o resultado no cache por jogo+idioma; a chave nunca chega ao Switch.
O serviço não acessa busca, login, conta, reviews individuais ou qualquer área
desautorizada pelo `robots.txt` publicado.

## Contrato público

`GET /v1/scores?title=<título>&platform=nintendo-switch`

Resposta de sucesso:

```json
{
  "found": true,
  "title": "Super Mario Odyssey",
  "platform": "Nintendo Switch",
  "source": "Metacritic",
  "sourceUrl": "https://www.metacritic.com/game/super-mario-odyssey/?platform=nintendo-switch",
  "metascore": {"value": 97, "reviewCount": 113},
  "userScore": {"value": 8.9, "reviewCount": 0},
  "fetchedAt": "2026-08-13T00:00:00Z",
  "cached": false,
  "stale": false
}
```

O cliente só envia um título e a plataforma permitida. Não há parâmetro de URL,
portanto o endpoint não pode ser usado como proxy para outros destinos.

## Limites e cache

- Apenas `nintendo-switch` nesta primeira versão.
- Máximo de 20 chamadas por minuto por IP externo.
- Uma consulta nova ao Metacritic a cada dois segundos no serviço inteiro.
- Cache SQLite persistente: 30 dias para respostas encontradas e 24 horas para
  ausência de resultado; a última resposta válida ainda pode ser entregue por
  até 180 dias se a origem estiver indisponível (`stale: true`).
- O serviço tenta somente slugs derivados localmente do título e confirma o
  título e a plataforma no HTML recebido. Ele não usa a área `/search`, que é
  desautorizada pelo `robots.txt` da origem.

## Instalação no LXC

```sh
apt-get update
apt-get install -y python3-fastapi python3-uvicorn
useradd --system --home /nonexistent --shell /usr/sbin/nologin switchu-metacritic
install -d -o switchu-metacritic -g switchu-metacritic -m 0750 /opt/switchu-metacritic /var/lib/switchu-metacritic
install -o root -g root -m 0644 metacritic_proxy.py /opt/switchu-metacritic/
install -o root -g root -m 0644 switchu-metacritic.service /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now switchu-metacritic
```

O serviço fica na porta HTTP `8082`; ele deve ser publicado somente por um
Cloudflare Tunnel que alcance a rede privada permitida.

## Metadados IGDB

`GET /v1/metadata?title=<titulo>&platform=nintendo-switch` entrega sinopse,
história, data de lançamento, gêneros, temas, modos de jogo, desenvolvedora,
publicadora, capa, até oito capturas e estimativas de tempo rápido, principal e
completo. O arquivo
`/etc/switchu-metadata/igdb.env` contem `IGDB_CLIENT_ID` e
`IGDB_CLIENT_SECRET`; ele fica somente no LXC, como `root:root` e permissao
`0600`. As credenciais nunca entram no launcher ou neste repositorio.

## Tradução por idioma

`/etc/switchu-metadata/gemini.env` contém `GEMINI_API_KEY` e, opcionalmente,
`GEMINI_MODEL=gemini-3.5-flash`, também como `root:root`/`0600`. A rota aceita
`language=pt-BR`, `es-ES`, `fr-FR`, `de-DE`, `it-IT`, `nl-NL` e `ru-RU` usam
Gemini para localizar sinopse, história, gêneros, temas e modos de jogo. O
resultado é cacheado por jogo e idioma; `en-US` mantém o texto original. Em uma
falha da API, o cache válido continua disponível; na primeira consulta a
resposta mantém o original e informa `summaryLanguage: "en"`.
