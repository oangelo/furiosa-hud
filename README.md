# furiosa-hud

Heads-up display de telemetria para o triciclo elétrico **Furiosa**, usando um **ESP32 Cheap Yellow Display** (ESP32-2432S028R, ILI9341 240x320) conectado via **Bluetooth Classic SPP** ao módulo VESC BT. Sem fios entre display e controlador.

## Hardware

- **Display**: ESP32-2432S028R (Cheap Yellow Display) — ILI9341 240×320, touch resistivo, SD slot
- **Controlador motor**: VESC com módulo VESC BT (Bluetooth Classic SPP)

## Roadmap

| # | Experimento | Descrição |
|---|---|---|
| 01 | [bt-connect](experiments/01-bt-connect/) | Estabelecer conexão Bluetooth Classic SPP com o módulo VESC BT |
| 02 | [display-hello](experiments/02-display-hello/) | Inicializar o display ILI9341 (LovyanGFX) e renderizar texto |
| 03 | [vesc-data-serial](experiments/03-vesc-data-serial/) | Ler dados do VESC via UART e imprimir no Serial Monitor |
| 04 | [dados-na-tela](experiments/04-dados-na-tela/) | Combinar BT + display: mostrar telemetria do VESC em tempo real |

## License

[MIT](LICENSE)
