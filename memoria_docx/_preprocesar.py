#!/usr/bin/env python3
"""
Preprocesador LaTeX -> LaTeX 'amigo-pandoc'.
Lee las fuentes en ../memoria/ y escribe versiones limpias en este directorio.
"""
import re, shutil, pathlib

SRC = pathlib.Path(__file__).parent.parent / "memoria"
DST = pathlib.Path(__file__).parent

# Tabla de tokens siunitx -> símbolo de unidad.
UNITS = {
    r"\milli\ampere": "mA",
    r"\milli\second": "ms",
    r"\milli\meter":  "mm",
    r"\centi\meter":  "cm",
    r"\kilo\hertz":   "kHz",
    r"\kilo\ohm":     "kΩ",
    r"\celsius":      "°C",
    r"\percent":      r"\%",
    r"\second":       "s",
    r"\hertz":        "Hz",
    r"\meter":        "m",
    r"\baud":         "Bd",
    "lux":            "lx",
}

def expand_unit(u):
    u = u.strip()
    if u in UNITS:
        return UNITS[u]
    # Fallback: quitar barras y devolver tal cual.
    return u.lstrip("\\").replace("\\", "")

def repl_SI(m):
    return f"{m.group(1)} {expand_unit(m.group(2))}"  # NNBSP entre número y unidad

def repl_SIrange(m):
    return f"{m.group(1)}–{m.group(2)} {expand_unit(m.group(3))}"

def preprocess(text):
    # \codein{X} -> \texttt{X}
    text = text.replace(r"\codein{", r"\texttt{")
    # \hrule height 0.8pt -> nada (es decoración del titlepage que pandoc no
    # entiende y deja como texto literal "height 0.8pt").
    text = re.sub(r"\\hrule\s+height\s+[\d.]+pt", "", text)
    # \SI{val}{unit}
    text = re.sub(r"\\SI\{([^{}]*)\}\{([^{}]*)\}", repl_SI, text)
    # \SIrange{a}{b}{unit}
    text = re.sub(r"\\SIrange\{([^{}]*)\}\{([^{}]*)\}\{([^{}]*)\}", repl_SIrange, text)
    # \TODO{...} -> ...
    text = re.sub(r"\\TODO\{([^{}]*)\}", r"[TODO: \1]", text)
    # Fragmentos inline-math sustituidos por su equivalente Unicode plano.
    # Se hace tras expandir SI/codein.
    MATH_REPL = {
        r"$-40$":                              "−40",
        r"$0$":                                "0",
        r"$100\,\%$":                          "100 %",
        r"$80\,^{\circ}$":                     "80°",
        r"$<15 \%$":                           r"< 15 \%",
        r"$>20000 lx$":                        "> 20 000 lx",
        r"$[0,\mathrm{NUM\_PLANTAS}{-}1]$":    "[0, NUM_PLANTAS−1]",
        r"$\approx6 s$":                       "≈ 6 s",
        r"$\equiv$":                           "≡",
        r"$\leftrightarrow$":                  "↔",
        r"$\leq 500 mA$":                      "≤ 500 mA",
        r"$\rightarrow$":                      "→",
        r"$\times$":                           "×",
    }
    for src, dst in MATH_REPL.items():
        text = text.replace(src, dst)
    return text

# Reemplazos específicos para los \IfFileExists (llaves anidadas: regex incómoda).
# Se sustituyen los bloques completos por la inclusión directa de la imagen.
IFFILEEXISTS_BLOCKS = [
    # 05_hardware.tex
    (
        r"""  \IfFileExists{img/wokwi_diagram.png}{%
    \includegraphics[width=0.85\textwidth]{wokwi_diagram.png}%
  }{%
    \fbox{\textit{[Pendiente: captura de pantalla del montaje Wokwi]}}%
  }""",
        r"  \includegraphics[width=0.85\textwidth]{wokwi_diagram.png}"
    ),
    # 07_autodiagnostico.tex
    (
        r"""  \IfFileExists{img/diag_pipeline.png}{%
    \includegraphics[width=0.88\textwidth]{diag_pipeline.png}%
  }{%
    \fbox{\textit{[Pendiente: diagrama de bloques del módulo
      \codein{diagnostics}]}}%
  }""",
        r"  \includegraphics[width=0.88\textwidth]{diag_pipeline.png}"
    ),
]

def unwrap_iffileexists(text):
    for src, dst in IFFILEEXISTS_BLOCKS:
        if src in text:
            text = text.replace(src, dst)
    return text

# --- procesa ficheros raíz ---
def process_file(src, dst):
    txt = src.read_text(encoding="utf-8")
    txt = unwrap_iffileexists(txt)
    txt = preprocess(txt)
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(txt, encoding="utf-8")
    print(f"  {src.relative_to(SRC)}  ->  {dst.relative_to(DST)}")

print("Preprocesando .tex ...")
process_file(SRC / "memoria.tex",    DST / "memoria.tex")
process_file(SRC / "preambulo.tex",  DST / "preambulo.tex")
for s in sorted((SRC / "secciones").glob("*.tex")):
    process_file(s, DST / "secciones" / s.name)

# Copiar bibliografía.
shutil.copy(SRC / "bibliografia.bib", DST / "bibliografia.bib")
print(f"  bibliografia.bib  ->  copiado")

# Copiar imágenes si las hay.
src_img = SRC / "img"
if src_img.exists():
    for img in src_img.glob("*"):
        shutil.copy(img, DST / "img" / img.name)
        print(f"  img/{img.name}  ->  copiado")

print("Hecho.")
