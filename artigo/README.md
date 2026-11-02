# Compilação do Artigo IEEE em LaTeX

Este diretório contém a estrutura oficial do artigo no padrão **IEEE Conference (`IEEEtran`)** para ser editado diretamente no repositório.

## Arquivos:
- `artigo.tex`: Arquivo LaTeX principal estruturado com todas as seções exigidas pelo professor.
- `referencias.bib`: Banco de referências bibliográficas (Tanenbaum, Silberschatz, Amdahl, Belady).
- `IEEEtran.cls`: Arquivo de classe oficial da IEEE.
- `IEEEtran.bst`: Estilo de bibliografia oficial da IEEE.

## Como compilar para PDF:

### Opção 1: No próprio terminal (se tiver texlive instalado)
```bash
pdflatex artigo.tex
bibtex artigo
pdflatex artigo.tex
pdflatex artigo.tex
```

### Opção 2: No VS Code / IDE
Instale a extensão **LaTeX Workshop** no VS Code. Ao abrir `artigo.tex`, ela compila e visualiza o PDF automaticamente com `Ctrl+Alt+B`.

### Opção 3: No Overleaf (se quiserem sincronizar com a equipe)
Basta compactar o conteúdo desta pasta `artigo/` em um arquivo `.zip` e fazer o upload no Overleaf.

