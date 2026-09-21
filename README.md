# Orca Unificado

Slicer de escritorio **independiente**. No es [Orca Slicer](https://github.com/OrcaSlicer/OrcaSlicer) ni [Snapmaker Orca](https://github.com/Snapmaker/OrcaSlicer).

## Origen

Semilla: [Snapmaker/OrcaSlicer](https://github.com/Snapmaker/OrcaSlicer) tag **`v2.4.0`** (commit `b1831e5dcb464172de33783142425aafda834fbc`). Conserva Device U1, el botón de sync de filamentos `=`, MQTT/SSWCP y Full Spectrum de esa base.

`SLIC3R_APP_NAME` = `Orca Unificado` · `SLIC3R_APP_KEY` = `OrcaUnificado` (no pisa las carpetas de configuración de OrcaSlicer ni de Snapmaker_Orca).

## M1 — Orca Cloud

Login y sync de presets de **Orca Cloud** (`cloud.orcaslicer.com`) encima de SnOrca, portados desde [OrcaSlicer](https://github.com/OrcaSlicer/OrcaSlicer) tag **`v2.4.2`**.

- Menú Archivo → **Orca Cloud…** (cuenta `cloud.orcaslicer.com`, no Snapmaker).
- **Sync Presets** tira los presets de usuario al `user/<UUID>/` de esa sesión.
- Stealth mode (Preferencias) bloquea login y sync Cloud, igual que vanilla.
- `NetworkAgent` Bambu (`m_agent`) no se sustituye. Device Flutter y filamentsync `=` no se tocan.
- Hub de plugins de `main` 2.5.0-dev **no** está portado.

Desactiva Stealth si el login Cloud no abre. El User-Agent HTTP es `OrcaUnificado/2.4.0`; los tokens van a `OrcaUnificado/Auth`, no a la store de Orca Slicer oficial.

## Licencia

[AGPL-3.0](LICENSE.txt). Cadena: Slic3r → PrusaSlicer → Bambu Studio → Orca Slicer → Snapmaker Orca → este fork.

La pestaña Device incluye un bundle Flutter **precompilado** (`resources/web/flutter_web/`) heredado de SnOrca. Este repo **no** incluye fuente Dart de esa UI.

El plugin de red de Bambu Lab es **opcional y no libre**. No lo publicamos como propio; no hace falta para U1 ni para Orca Cloud.

## Marcas

Orca Slicer / SoftFever y Snapmaker Orca / Snapmaker son marcas de sus dueños. Este proyecto no está afiliado ni avalado por ellos.

## Compilar

Igual que SnOrca 2.4.0 (`build_release*`, `build_linux.sh`). Deps pesadas (wxWidgets, OpenGL, deps de Orca). Si hace falta: `git lfs pull` tras clonar.

En agentes cloud sin el árbol de deps completo **no se espera un binario**; el código queda cableado en CMake y GUI. Este entorno no tiene wxWidgets ni las deps de Orca, así que **M1 no se compiló aquí**.
