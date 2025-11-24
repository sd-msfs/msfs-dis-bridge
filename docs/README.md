# Documentation

This directory contains the Doxygen configuration for generating documentation from source code comments.

## Prerequisites

Install Doxygen:
- **Windows**: Download from [doxygen.org](https://www.doxygen.nl/download.html) or `winget install Doxygen.Doxygen`
- **macOS**: `brew install doxygen`
- **Linux**: `sudo apt install doxygen` or `sudo yum install doxygen`

Optional (for graphs):
- Install Graphviz for call graphs and class diagrams: [graphviz.org](https://graphviz.org/download/)

## Generating Documentation

From this directory (`/docs`), run:

```bash
doxygen Doxyfile
```

This generates:
- **HTML documentation** → `docs/html/index.html` (open this in your browser)
- **LaTeX documentation** → `docs/latex/` (for PDF generation)

## Viewing Documentation

After generation, open the HTML docs:

```bash
# Windows
start html/index.html

# macOS
open html/index.html

# Linux
xdg-open html/index.html
```

## Documentation Structure

The generated documentation includes:
- **Class hierarchy** - Inheritance diagrams for all classes
- **File list** - All source files with their documentation
- **Class list** - Complete API reference for all classes/structs
- **Namespace list** - Organized by namespace (DISBridge::Controllers, etc.)
- **Function index** - Searchable function reference
- **Call graphs** - Visual function call relationships (if Graphviz installed)

## Customizing Documentation

Edit `Doxyfile` to customize:
- `PROJECT_NAME` - Project title
- `PROJECT_NUMBER` - Version number
- `OUTPUT_DIRECTORY` - Where to generate docs
- `INPUT` - Source directories to document
- `EXTRACT_ALL` - Whether to document all entities
- `GENERATE_LATEX` - Enable/disable LaTeX output
- `HAVE_DOT` - Enable Graphviz diagrams

## CI/CD Integration

To auto-generate documentation in GitHub Actions:

```yaml
- name: Install Doxygen
  run: sudo apt-get install -y doxygen graphviz

- name: Generate Documentation
  run: |
    cd docs
    doxygen Doxyfile

- name: Deploy to GitHub Pages
  uses: peaceiris/actions-gh-pages@v3
  with:
    github_token: ${{ secrets.GITHUB_TOKEN }}
    publish_dir: ./docs/html
```

## Notes

- Generated files (`html/`, `latex/`) are **excluded from git** via `.gitignore`
- Only commit the `Doxyfile` and source code with Doxygen comments
- Regenerate docs after code changes to keep them current
- Use `doxygen -u Doxyfile` to upgrade config file to new Doxygen versions
