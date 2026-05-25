#!/bin/bash
# Regenera memoria.docx desde las fuentes LaTeX en ../memoria/.
# Uso:
#   ./regenerar.sh
#
# Requisitos: pandoc, python3.
# Las imágenes (wokwi_diagram.png, diag_pipeline.png) deben estar en img/.

set -e
cd "$(dirname "$0")"

echo "1. Preprocesando .tex..."
python3 _preprocesar.py

echo "2. Convirtiendo a memoria.docx..."
pandoc memoria.tex -o memoria.docx \
    --reference-doc=reference.docx \
    --bibliography=bibliografia.bib \
    --citeproc \
    --csl=ieee.csl

echo "3. Listo: memoria.docx ($(du -h memoria.docx | cut -f1))"
