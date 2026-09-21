# Orca Unificado

Slicer de escritorio **independiente**. No es [Orca Slicer](https://github.com/OrcaSlicer/OrcaSlicer) ni [Snapmaker Orca](https://github.com/Snapmaker/OrcaSlicer).

## Origen

Semilla: [Snapmaker/OrcaSlicer](https://github.com/Snapmaker/OrcaSlicer) tag **`v2.4.0`** (commit `b1831e5dcb464172de33783142425aafda834fbc`). Conserva Device U1, el botón de sync de filamentos `=`, MQTT/SSWCP y Full Spectrum de esa base.

`SLIC3R_APP_NAME` = `Orca Unificado` · `SLIC3R_APP_KEY` = `OrcaUnificado` (no pisa las carpetas de configuración de OrcaSlicer ni de Snapmaker_Orca).

## Objetivo

Añadir **Orca Cloud** (login `cloud.orcaslicer.com`, sync de presets entre PCs, carpeta `user/<UUID>/`) encima de SnOrca. Eso es el milestone M1; **esta semilla no lo incluye**. No se porta `OrcaCloudServiceAgent` aquí. No se toca `flutter_web`.

## Licencia

[AGPL-3.0](LICENSE.txt). Cadena: Slic3r → PrusaSlicer → Bambu Studio → Orca Slicer → Snapmaker Orca → este fork.

La pestaña Device incluye un bundle Flutter **precompilado** (`resources/web/flutter_web/`) heredado de SnOrca. Este repo **no** incluye fuente Dart de esa UI.

El plugin de red de Bambu Lab es **opcional y no libre**. No lo publicamos como propio; no hace falta para U1 ni para Orca Cloud.

## Marcas

Orca Slicer / SoftFever y Snapmaker Orca / Snapmaker son marcas de sus dueños. Este proyecto no está afiliado ni avalado por ellos.

## Compilar

Igual que SnOrca 2.4.0 (`build_release*`, `build_linux.sh`). Si hace falta: `git lfs pull` tras clonar.
