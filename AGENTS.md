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

- **Namespaces** para módulos: `vesc_bt::`, `bt_screen::`, `screens::`, `gauge::`, `ble_transport::`
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
| ESP32 BLE Arduino | built-in | BLE GATT client |

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
SCANNING → LIST → CONNECTING → DASHBOARD
                                ↕
                           RECONNECTING → (volta DASHBOARD ou SCANNING)
```

| Estado | Descrição |
|---|---|
| SCANNING | `discover()` síncrono, bloqueia até completar. Tela "Procurando..." |
| LIST | Mostra devices na tela, espera touch. Auto-connect se achar `VESC_BT_NAME` |
| CONNECTING | `SerialBT.connect()`. Tela "Conectando..." |
| DASHBOARD | `getVescValues()` a cada 250ms, gauge + topbar + alerts no display. Touch troca tela detail |
| RECONNECTING | Dashboard visível com speed=0 e BT amarelo. Tenta reconectar a cada 3s |

## Modo Simulação

`SIMULATE_VESC` no `config.h` controla o comportamento:

| Valor | Comportamento |
|---|---|
| `1` | Pula BT, dados simulados no loop. Dashboard animado sem hardware |
| `0` | Fluxo BT real. Scan → connect → DASHBOARD com dados do VESC |

## Simulador VESC via Bluetooth

`tools/vesc_sim.py` — simula um VESC via BT SPP no PC.

### Dependências (Arch Linux)

```bash
sudo pacman -S python-pybluez dbus-python
```

### Uso

```bash
# 1. Registrar serviço SPP (requer root)
sudo sdptool add SP

# 2. Tornar PC visível e pareável
bluetoothctl discoverable on
bluetoothctl pairable on

# 3. Rodar simulador
python3 tools/vesc_sim.py
```

### Controles do simulador

| Tecla | Ação |
|---|---|
| `↑`/`w` | Aumentar velocidade (+2 km/h) |
| `↓`/`s` | Diminuir velocidade (-2 km/h) |
| `t` | Spike de temperatura por 5s |
| `f` | Ciclar fault codes (0-4) |
| `r` | Resetar todos os valores |
| `q` | Sair |

### Nota (Arch Linux)

O `sdptool` foi removido do `bluez-utils` nas versões recentes. Instalar via AUR (`bluez-deprecated-tools`) ou compilar do fonte. Sem o registro SDP, o ESP32 pareia mas não consegue encontrar o canal RFCOMM.

Com um **VESC real** (com módulo BT Classic embutido), nenhuma configuração de PC é necessária — o ESP32 conecta diretamente.

### Compatibilidade de Hardware

| Hardware | BT Classic SPP | BLE | Compatível |
|---|---|---|---|
| VESC + módulo BT Classic (Flipsky, etc.) | Sim | Não | **Sim** (Classic) |
| VESC Express (ESP32-C3) | Não | Sim | **Sim** (BLE) |

O HUD suporta **ambos os modos** — o usuário alterna via botão toggle na tela LIST.
ESP32 original (CYD) suporta Classic e BLE, mas **não simultaneamente**.

## BLE Transport

### Arquitetura

```
transport.h          → BtType enum + BtDevice struct
transport_ble.h/cpp  → ble_transport:: namespace (BLE GATT client + BLEStream)
vesc_bt.h/cpp        → vesc_bt:: namespace (dispatcher Classic/BLE)
```

### VESC Express BLE Protocol

| UUID | Tipo | Função |
|---|---|---|
| `6e400001-...` | Service | VESC UART Service |
| `6e400002-...` | Characteristic | **RX** — cliente escreve comandos |
| `6e400003-...` | Characteristic | **TX** — servidor envia notificações |

Protocolo idêntico ao UART — mesmo framing VESC, transportado sobre BLE GATT.

### BLEStream

`BLEStream` herda de `Stream` — buffer circular preenchido pelo callback de notify do TX characteristic.
O `VescUart.setSerialPort()` aceita o `Stream*` retornado por `ble_transport::getStream()`.

### Toggle Classic/BLE

Na tela LIST, dois botões na barra inferior:
- **[Classic]** / **[BLE]** — alterna o modo e re-escaneia
- **[Scan]** — re-escaneia no modo atual

`handleTouch()` retorna `-3` quando o toggle é pressionado. `main.cpp` troca `BtType`, chama `vesc_bt::setBtType()` e reinicia o scan.

## BtState — Indicador Bluetooth

| Estado | Cor | Significado |
|---|---|---|
| `BT_OFF` | Vermelho piscando | Desconectado |
| `BT_CONNECTED` | Verde sólido | Conectado, recebendo dados |
| `BT_RECONNECTING` | Amarelo sólido | Conexão perdida, tentando reconectar |
