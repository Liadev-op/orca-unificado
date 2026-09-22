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

## Portable Windows

El zip de [release `m1-windows`](https://github.com/Liadev-op/orca-unificado/releases/tag/m1-windows) incluye una carpeta **`data_dir`** junto a `snapmaker-orca.exe`.

Ese es el mecanismo nativo de Orca/SnOrca (`GUI_App::init_app_config`, PR SoftFever/OrcaSlicer #6780): si esa carpeta existe al lado del binario, perfiles, cache, logs y presets de usuario viven **ahí**. No se usa `%APPDATA%\OrcaUnificado`, ni `Snapmaker_Orca`, ni `OrcaSlicer`.

1. Extrae la carpeta `OrcaUnificado`.
2. Ejecuta `snapmaker-orca.exe`.
3. Tras el primer arranque: `OrcaUnificado\data_dir\` (`user\`, `cache\`, `log\`, `OrcaUnificado.conf`, …).

Si usaste un zip anterior que escribía en APPDATA, copia `%APPDATA%\OrcaUnificado\*` dentro de `data_dir`. Fallback: `snapmaker-orca.exe --datadir "D:\ruta\absoluta"`. No borres `data_dir`: si falta, la app cae a APPDATA.

## Licencia

[AGPL-3.0](LICENSE.txt). Cadena: Slic3r → PrusaSlicer → Bambu Studio → Orca Slicer → Snapmaker Orca → este fork.

La pestaña Device incluye un bundle Flutter **precompilado** (`resources/web/flutter_web/`) heredado de SnOrca. Este repo **no** incluye fuente Dart de esa UI.

El plugin de red de Bambu Lab es **opcional y no libre**. No lo publicamos como propio; no hace falta para U1 ni para Orca Cloud.

## Marcas

Orca Slicer / SoftFever y Snapmaker Orca / Snapmaker son marcas de sus dueños. Este proyecto no está afiliado ni avalado por ellos.

## Compilar

Igual que SnOrca 2.4.0 (`build_release*`, `build_linux.sh`). Deps pesadas (wxWidgets, OpenGL, deps de Orca). Si hace falta: `git lfs pull` tras clonar.

El zip Windows se publica con `.github/workflows/windows-build.yml` (cache de deps + `build_release_vs2022.bat`). No hace falta instalar Visual Studio en el PC de Liadev.
