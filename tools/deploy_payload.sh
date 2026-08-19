#!/bin/bash
set -e

echo "Descompactando temas temporariamente..."
mkdir -p /tmp/bc1-extracted
unzip -q -o /tmp/themes-bc1-all.zip -d /tmp/bc1-extracted/

echo "Distribuindo os temas para suas pastas e quebrando hardlinks antigos..."
cd /srv/themes/themes
for zipfile in /tmp/bc1-extracted/*.zip; do
    base=$(basename "$zipfile" .zip)
    if [ -d "$base" ]; then
        # Remove o hardlink antigo antes de mover o novo, preservando o arquivo original (theme-hash.zip) intacto
        rm -f "$base/theme.zip"
        mv "$zipfile" "$base/theme.zip"
    else
        echo "Aviso: Pasta para o tema $base não existe no servidor!"
    fi
done

echo "Limpando e arrumando permissões..."
rm -rf /tmp/bc1-extracted
chown -R root:root /srv/themes/themes/

echo "Re-indexando catalogo..."
/opt/switchu-themes/reindex.py

echo "Publicação finalizada com sucesso!"
