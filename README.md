# BPRandr

A small graphical tool to configure monitors (resolution, refresh rate, rotation and position) with `xrandr` on **Tiny Core Linux**, built with **FLTK 1.3**.

Una pequeña herramienta gráfica para configurar monitores (resolución, frecuencia, rotación y posición) con `xrandr` en **Tiny Core Linux**, construida con **FLTK 1.3**.

---

## English

### Description

BPRandr is a lightweight C++/FLTK 1.3 GUI for **Tiny Core Linux** that works as a front-end for `xrandr`. It writes and persists a startup script under `~/.X.d/`, so your monitor layout survives reboots on a system that is normally stored in RAM.

It is inspired by LXRandR, but it has no GTK dependency and is built to stay as small as possible:

- Detects connected monitors automatically, and runs `xrandr --auto` if no output is active.
- Lets you choose, for each monitor: on/off, **resolution** (shown with its aspect ratio), **refresh rate**, **rotation** and **position** (clone, right, above, left or below the primary monitor).
- Offers quick options when two monitors are connected: mirror, use only one of them, or extend to the right or above.
- Applies the changes and asks for confirmation within 15 seconds; if you don't confirm, the previous layout is restored automatically.
- Refuses to turn off every monitor, so you can't lock yourself out.
- Shows the error output of `xrandr` if a configuration can't be applied.
- Writes `~/.X.d/bprandr.sh` with the `xrandr` command for your layout (falling back to `xrandr --auto` if it fails) and sets its permissions to `755`.
- Registers the script in `/opt/.filetool.lst` if it isn't already covered, so it is included in Tiny Core's persistence, and runs `filetool.sh -b` to back it up immediately.
- Can run without a window: `bprandr --auto` executes `xrandr --auto` and exits, which is handy for scripts.

Tiny Core Linux runs the scripts placed in `~/.X.d` when X starts. If your setup doesn't, call the saved script from `~/.xsession`.

### Requirements

- Tiny Core Linux (x86 / 32-bit branch).
- An **Xorg** server with RandR support (Xvesa is not enough).
- The `xrandr` command.
- **FLTK 1.3** — the exact major/minor version matters; the UI code targets the FLTK 1.3 API.
- `filetool.sh` available on the system (included by default on Tiny Core).

### Building

1. Install the compiler toolchain and FLTK 1.3 (runtime + development headers). Use `-wil` instead of `-wi` so these build-only tools are downloaded and loaded for the current session **without** being added to `onboot.lst` — you don't need to load a full compiler toolchain on every boot if you're not compiling every day:

```sh
   tce-load -wil compiletc.tcz
   tce-load -wil fltk-1.3-dev.tcz
```

   `tce-load` will automatically pull in `fltk-1.3.tcz` and any required X11 dependencies.

2. Verify FLTK is available:

```sh
   fltk-config --version
```

3. Compile (quick build):

```sh
   g++ -O2 -o bprandr bprandr.cpp $(fltk-config --cxxflags --ldflags)
```

4. Run:

```sh
   ./bprandr
```

   To only let `xrandr` pick the outputs automatically, without opening the window:

```sh
   ./bprandr --auto
```

### Optimized, minimal-size build

Since this is meant to run on Tiny Core Linux, where every kilobyte in the base filesystem matters, you can build a much smaller binary by targeting the 32-bit baseline CPU, optimizing for size instead of speed, dropping the exception/RTTI runtime support and the unwind tables (the code relies on neither), discarding unused functions and data at link time, and linking with a trimmed-down linker script (`elf_i386.xbn`, part of `binutils`):

```sh
g++ $(fltk-config --cxxflags) -flto -march=i486 -mtune=i686 -Os -pipe -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-stack-protector -ffunction-sections -fdata-sections -fmerge-all-constants -o bprandr bprandr.cpp -Wl,--as-needed -Wl,--gc-sections -Wl,-O1 -s $(fltk-config --ldflags) -Wl,-T/usr/local/lib/ldscripts/elf_i386.xbn
```

If your `find /usr -iname elf_i386.xbn` reports a different path than the one above, adjust it accordingly.

Then strip the binary further with `sstrip` (from the `sstrip.tcz` extension, loaded the same on-demand way since it's only needed while building):

```sh
tce-load -wil sstrip.tcz
strip --strip-all bprandr
sstrip bprandr
ls -la bprandr
```

`strip --strip-all` removes symbol tables and debug information the running program doesn't need. `sstrip` goes further and removes the ELF section header table itself, which the Linux kernel doesn't need to execute the binary (only debuggers/analysis tools would).

### License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**.
See the [`LICENSE`](./LICENSE) file, or the official text at
<https://www.gnu.org/licenses/gpl-3.0.en.html>, for the full terms.

---

## Español

### Descripción

BPRandr es una interfaz gráfica liviana en C++/FLTK 1.3 para **Tiny Core Linux** que funciona como frontend de `xrandr`. Escribe y hace persistente un script de inicio en `~/.X.d/`, para que la disposición de tus monitores sobreviva a los reinicios en un sistema que normalmente vive en RAM.

Está inspirada en LXRandR, pero no depende de GTK y está pensada para ocupar lo mínimo posible:

- Detecta automáticamente los monitores conectados, y ejecuta `xrandr --auto` si ninguna salida está activa.
- Permite elegir, para cada monitor: encendido/apagado, **resolución** (mostrada con su relación de aspecto), **frecuencia de refresco**, **rotación** y **posición** (clon, a la derecha, arriba, a la izquierda o abajo del monitor principal).
- Ofrece opciones rápidas cuando hay dos monitores conectados: duplicar, usar solo uno de ellos, o extender a la derecha o arriba.
- Aplica los cambios y pide confirmación en 15 segundos; si no confirmás, se restaura automáticamente la disposición anterior.
- No permite apagar todos los monitores, para que no te quedes sin pantalla.
- Muestra la salida de error de `xrandr` si no se puede aplicar una configuración.
- Escribe `~/.X.d/bprandr.sh` con el comando `xrandr` de tu disposición (con `xrandr --auto` como respaldo si falla) y le ajusta los permisos a `755`.
- Registra el script en `/opt/.filetool.lst` si todavía no está cubierto, para que quede incluido en la persistencia de Tiny Core, y ejecuta `filetool.sh -b` para respaldarlo de inmediato.
- Puede ejecutarse sin ventana: `bprandr --auto` ejecuta `xrandr --auto` y sale, útil para scripts.

Tiny Core Linux ejecuta los scripts que están en `~/.X.d` al iniciar X. Si tu configuración no lo hace, llamá al script guardado desde `~/.xsession`.

### Requisitos

- Tiny Core Linux (rama x86 / 32 bits).
- Un servidor **Xorg** con soporte RandR (Xvesa no alcanza).
- El comando `xrandr`.
- **FLTK 1.3** — la versión exacta importa; el código de la interfaz usa la API de FLTK 1.3.
- `filetool.sh` disponible en el sistema (viene por defecto en Tiny Core).

### Compilación

1. Instalá las herramientas de compilación y FLTK 1.3 (runtime + headers de desarrollo). Usá `-wil` en vez de `-wi` para que estas herramientas, que solo hacen falta al compilar, se descarguen y carguen en la sesión actual **sin** agregarse a `onboot.lst` — no hace falta cargar todo un toolchain de compilación en cada arranque si no vas a compilar todos los días:

```sh
   tce-load -wil compiletc.tcz
   tce-load -wil fltk-1.3-dev.tcz
```

   `tce-load` va a traer automáticamente `fltk-1.3.tcz` y las dependencias de X11 necesarias.

2. Verificá que FLTK esté disponible:

```sh
   fltk-config --version
```

3. Compilá (build rápida):

```sh
   g++ -O2 -o bprandr bprandr.cpp $(fltk-config --cxxflags --ldflags)
```

4. Ejecutá:

```sh
   ./bprandr
```

   Para que `xrandr` elija las salidas automáticamente, sin abrir la ventana:

```sh
   ./bprandr --auto
```

### Build optimizada y de tamaño mínimo

Como esto está pensado para correr en Tiny Core Linux, donde cada kilobyte del sistema base importa, se puede lograr un binario bastante más chico apuntando a la CPU base de 32 bits, optimizando por tamaño en vez de velocidad, sacando el soporte de excepciones/RTTI y las tablas de unwind (el código no depende de ninguno), descartando en el enlazado las funciones y datos que no se usan, y linkeando con un linker script recortado (`elf_i386.xbn`, parte de `binutils`):

```sh
g++ $(fltk-config --cxxflags) -flto -march=i486 -mtune=i686 -Os -pipe -fno-exceptions -fno-rtti -fno-unwind-tables -fno-asynchronous-unwind-tables -fomit-frame-pointer -fno-stack-protector -ffunction-sections -fdata-sections -fmerge-all-constants -o bprandr bprandr.cpp -Wl,--as-needed -Wl,--gc-sections -Wl,-O1 -s $(fltk-config --ldflags) -Wl,-T/usr/local/lib/ldscripts/elf_i386.xbn
```

Si tu `find /usr -iname elf_i386.xbn` te devuelve una ruta distinta a la de arriba, ajustala.

Después, achicá el binario todavía más con `sstrip` (viene de la extensión `sstrip.tcz`, cargada de la misma forma "on-demand" ya que solo hace falta al compilar):

```sh
tce-load -wil sstrip.tcz
strip --strip-all bprandr
sstrip bprandr
ls -la bprandr
```

`strip --strip-all` elimina la tabla de símbolos y la información de debug que el programa no necesita para correr. `sstrip` va un paso más allá y elimina la tabla de section headers del ELF, que el kernel de Linux tampoco necesita para ejecutarlo (solo la usarían debuggers o herramientas de análisis).

### Licencia

Este proyecto está licenciado bajo la **GNU General Public License v3.0 (GPLv3)**.
Consultá el archivo [`LICENSE`](./LICENSE), o el texto oficial en
<https://www.gnu.org/licenses/gpl-3.0.en.html>, para conocer los términos completos.
