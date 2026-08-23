# Firmware documentation

The project overview, safety contract, USB wire format, Flash layout, build
commands, CubeMX boundary, and hardware-validation checklist are in the
[root README](../README.md).

Documentation is grouped by purpose:

- [`architecture`](architecture/) describes source ownership, dependencies,
  request timing, operating modes, and board-independent firmware tests.
- [`hardware`](hardware/) contains local component references and datasheets.
- [`operations`](operations/) contains procedures used while operating or
  validating a board.
- [`protocol`](protocol/) points to the USB wire contract and its two
  implementations.
- [`archive`](archive/) retains superseded design notes for history only.

The startup path initializes the board directly into its guarded safe state.
High-power tests remain host-driven through the guarded USB command surface.

## API reference

Run Doxygen from the project root:

```powershell
doxygen Doxyfile
```

The input is restricted to user-owned `Application` code. Generated CubeMX,
USB, HAL, and middleware sources are deliberately excluded. Documentation
warnings are fatal so missing or invalid public API documentation fails the
command.

The generated entry page is `docs/doxygen/html/index.html`. The generated tree
and warning log are build artifacts and are intentionally ignored by Git.
