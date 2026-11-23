# esp32-dataformat
Для добавления в проект в папке компонентов из командной строки запустить:    
```
git submodule add https://github.com/rbliznets/esp32-dataformat dataformat
```

## JSON stream framing

`CONFIG_DATAFORMAT_JSON_STRING_AWARE` (`menuconfig` -> `Data Format` ->
`Respect quoted strings in JSON stream framing`) is **disabled by default**.
When disabled, `CJsonReadStream` preserves legacy behavior: every `{`/`}` counts,
including string contents, and resync mode can treat `{123}` inside text as a CRC
marker. Enable the option (`CONFIG_DATAFORMAT_JSON_STRING_AWARE=y`) to ignore
braces and CRC markers inside quoted strings, handling escaped quotes/backslashes
and retaining string/escape state across input chunks. Overflowed frames are
still consumed and dropped; `free()` resets partial frame/string state.

This is a framing option, not full JSON validation; extraction remains object-based
(top-level arrays are not frames). An unterminated quoted string remains open until
its closing quote or an explicit `free()`; CRC-like text inside it does not resync.

### Isolated Windows host regressions

Run from the project root (MSVC Build Tools required):

```powershell
pwsh -File components/dataformat/test/host/run.ps1
# Or specify the toolchain initialization script:
pwsh -File components/dataformat/test/host/run.ps1 -VcVars64 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
```

The runner builds and executes both legacy (option absent) and string-aware modes
using only local ESP stubs. Outputs stay in `test/host/build/` inside this component;
no ESP-IDF build or root host-test files are used.
