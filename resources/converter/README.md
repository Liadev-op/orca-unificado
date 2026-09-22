# Conversor Bambu / MakerWorld → Snapmaker U1

Port in-process (C++) de [josuanbn/bl2u1](https://github.com/josuanbn/bl2u1), **GPL-3.0**.

- Lógica original: `app.py` (`analyze` + `convert`).
- Este fork **no** llama a https://bl2u1.nbn.cat/ ni ejecuta Python.
- Licencia completa: `LICENSE-bl2u1.txt`.

## Plantillas

- `u1_template.3mf` — `Metadata/project_settings.config` de un proyecto U1 0.20 mm Standard (0.4 nozzle), sin soportes.
- `u1_template_supports.3mf` — igual, con `enable_support` y `tree(auto)`.
- `u1_filament_map.json` — tipo Bambu → `filament_settings_id` de Snapmaker Orca 2.4.0.

Las plantillas se regeneran cuando un tag nuevo de SnOrca cambie perfiles U1 (playbook §8).

El 3MF de salida vive en `OrcaUnificado/models/converted/<stem>-U1.3mf` y se abre como **proyecto** (máquina U1). Un 3MF Bambu sin convertir se importa como geometría (pinta colores, no cambia la impresora).
