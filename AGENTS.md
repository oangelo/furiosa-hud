# AGENTS.md — Furiosa HUD

## Estrutura do Projeto

- Cada experimento em `experiments/NN-nome/` é independente com seu próprio `platformio.ini`
- PlatformIO compila **todos** os `.cpp` em `src/` — arquivos fora de `src/` são ignorados
- Para preservar código sem compilar, mover para a raiz do experimento (ex: `main_display_sim.cpp`)

## Build & Flash

```bash
pio run -t upload -d experiments/06-vesc-gauge --upload-port /dev/ttyUSB0
```

Se a porta estiver ocupada, usar `--upload-port` explicitamente ou `fuser -k /dev/ttyUSB0`.

## Convenções de Código

- **Namespaces** para módulos: `vesc_bt::`, `bt_screen::`, `screens::`, `gauge::`
- **`static`** para funções e variáveis internas de cada `.cpp`
- **Dirty flags** para controlar redraw — `screenDirty = true` ao trocar de estado/tela
- **Structs de dados** em headers compartilhados (`VescData` em `screens.h`)
- **Sem comentários** no código, a menos que solicitado

## Dependências

| Lib | Versão | Notas |
|---|---|---|
| SolidGeek/VescUart | ^1.0.0 | Requer patches manuais (ver abaixo) |
| lovyan03/LovyanGFX | ^1.1.16 | Display driver |
| BluetoothSerial | built-in | ESP32 Arduino framework |

## Bugs Conhecidos — VescUart 1.0.1

Requer patch manual em `.pio/libdeps/esp32dev/VescUart/` após `pio run`:

### 1. `VescUart.h` — campo `tempMotor` duplicado

O struct `dataPackage` tem `tempMotor` declarado duas vezes (linhas 14 e 27). Remover a segunda ocorrência.

### 2. `VescUart.cpp` — jump to case label

O `case COMM_GET_VALUES_SELECTIVE` declara variável sem chaves, causando erro com `default`. Envelopar em `{ }`.

## Lessons Learned

### Bluetooth

- **`discoverAsync()` não funciona** para encontrar devices BT Classic — usar `discover()` síncrono
- O scan síncrono **bloqueia** o loop — a tela de "Procurando..." congela durante o scan
- `bluetoothctl discoverable on` tem timeout de 180s — renovar se necessário
- **Debug de BT do ESP32**: tornar PC discoverable + capturar serial com pyserial

### Anti-Flickering (Dirty Flag Pattern)

- **Nunca** chamar `fillScreen()` no loop — só ao trocar de estado/tela
- Separar **draw estático** (1x por transição) de **update dinâmico** (só partes que mudam)
- Funções `draw*Static()` redesenham tudo, chamadas apenas quando `screenDirty = true`
- Funções de update dinâmico (ex: `updateScanningDots()`) só limpam e redesenham retângulos pequenos
- Fluxo: `enterState()` → `screenDirty = true` → loop detecta → chama `draw*Static()` → `screenDirty = false`
- Padrão já usado no `screens.cpp` do Step 1 com `screenDirty`

## Hardware — CYD (ESP32-2432S028R)

- **Display**: ST7789 240×320, SPI (VSPI)
- **Touch**: XPT2046 resistivo, SPI separado do display
- **Backlight**: GPIO 21 (HIGH = ligado)
- **Rotação**: 1 (landscape 320×240)
- Config completo dos pinos em `display.h` de cada experimento

## State Machine (06-vesc-gauge)

```
SCANNING → LIST → CONNECTING → CONNECTED
                                 ↓
                            RECONNECTING → (volta CONNECTED ou SCANNING)
```

| Estado | Descrição |
|---|---|
| SCANNING | `discover()` síncrono, bloqueia até completar. Tela "Procurando..." |
| LIST | Mostra devices na tela, espera touch. Auto-connect se achar `VESC_BT_NAME` |
| CONNECTING | `SerialBT.connect()`. Tela "Conectando..." |
| CONNECTED | `getVescValues()` a cada 250ms, imprime no Serial. Tela "Conectado!" |
| RECONNECTING | Tenta reconectar a cada 3s. Se sem endereço, volta ao scan |
