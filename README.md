# Switch Storage Analyzer 0.1.0

Analizador de espacio de la microSD (treemap, donut, explorador). Solo lectura.

## Controles
A abrir · B subir · X cambiar gráfico · Y ordenar · L/R saltar · + reescanear · - salir

## Compilar sin instalar nada (GitHub Actions)
1. Crea un repositorio en github.com y sube todo el contenido de esta carpeta (incluida `.github`).
2. Pestaña **Actions** > "Build NRO" > se ejecuta solo al subir (o "Run workflow").
3. Al terminar, descarga el artefacto `SwitchStorageAnalyzer-nro` (contiene el .nro).
4. Copia `SwitchStorageAnalyzer.nro` a `sd:/switch/` y ábrelo desde el Homebrew Menu.

## Compilar en local
Requiere devkitPro, devkitA64, libnx y `switch-sdl2`; luego `make`.
