# Cloud build — Sharp68000Box360 v5.2b

The GitHub Actions workflow in `.github/workflows/xenon-toolchain-cloud.yml` prepares the Ubuntu runner, builds the Free60Project/libxenon cross-toolchain and libraries, verifies the Xenon compiler/linker, builds the upstream cube smoke test, then attempts the project's `xenon.elf` and `xenon.elf32`.

## Run it

1. Open the repository's **Actions** tab.
2. Select **Xenon toolchain cloud build**.
3. Choose **Run workflow** on branch `cloud/xenon-toolchain`, or push a commit to that branch.
4. Open the run and wait for the job to finish.
5. Download the artifact named `sharp68000box360-xenon-cloud-build`.

## Expected project artifacts

- `xenon.elf`: first-stage Xbox 360 ELF executable.
- `xenon.elf32`: ELF32 variant produced by the libXenon build rules.
- `README.md` in the artifact: runner, compiler availability, and build status.

Artifacts are only available when the workflow actually generates and uploads them. A successful upstream cube smoke test alone does not mean the Sharp68000Box360 project ELF built successfully.

## Current implementation boundary

`source/main.c` currently initializes the Xenos video subsystem and libXenon console, then prints a diagnostic banner. It is a boot/console bootstrap, not the full Sharp X68000 emulator. The Motorola 68000 CPU, RAM/ROM map, device emulation, storage/media loading, and gameplay compatibility still need to be integrated and tested on hardware.

## Packaging note

This workflow does not yet produce `default.xex`. First validate `xenon.elf` and the ELF32 artifact. XEX conversion should be a separate step only after a compatible converter is verified in the runner.
