# Third-Party Notices

This file lists third-party software used by Lantern, what it is used for,
and whether it ships inside games or editors built with Lantern.

## Currently used

| Component | Version | License | Used for | Shipped? |
|---|---|---|---|---|
| [Catch2](https://github.com/catchorg/Catch2) | v3.16.1 | BSL-1.0 | Unit tests only | No — test builds only |

## Planned (not yet integrated)

These will be added to the table above, with their full license texts,
when they enter the build:

| Component | License | Notes |
|---|---|---|
| Runtime backend (Axmol v2.11.5 recommended, see ADR 0001) | MIT | Brings its own bundled libraries; notably **OpenAL Soft (LGPL-2.1)**, which must be dynamically linked or replaced in shipped games. |
| Dear ImGui (docking branch) | MIT | Editor UI only (Milestone 2). |
| nlohmann/json | MIT | Project, scene and content files (Milestone 3). |
