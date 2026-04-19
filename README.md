# furiosa-hud

Heads-up display de telemetria para o triciclo elétrico **Furiosa**, usando um **ESP32 Cheap Yellow Display** (ESP32-2432S028R, ST7789 240x320) conectado via **Bluetooth Classic SPP** ao módulo VESC BT. Sem fios entre display e controlador.

## Hardware

- **Display**: ESP32-2432S028R (Cheap Yellow Display) — ST7789 240×320, touch resistivo, SD slot
- **Controlador motor**: VESC com módulo VESC BT (Bluetooth Classic SPP)

## Roadmap

| # | Experimento | Descrição |
|---|---|---|
| 01 | [bt-connect](experiments/01-bt-connect/) | Estabelecer conexão Bluetooth Classic SPP com o módulo VESC BT |
| 02 | [display-hello](experiments/02-display-hello/) | Inicializar o display ILI9341 (LovyanGFX) e renderizar texto |
| 03 | [vesc-data-serial](experiments/03-vesc-data-serial/) | Ler dados do VESC via UART e imprimir no Serial Monitor |
| 04 | [dados-na-tela](experiments/04-dados-na-tela/) | Combinar BT + display: mostrar telemetria do VESC em tempo real |
| 05 | [gauge](experiments/05-gauge/) | Gauge semi-circular de velocidade (0–35 km/h) |
| 06 | [vesc-gauge](experiments/06-vesc-gauge/) | Gauge + BT SPP scan com seleção por touch e telemetria VescUart |

## Referências

### Projetos VESC + ESP32 Display

- [SimpleVescDisplay (ESP32-2432S028R)](https://github.com/NetworkDir/SimpleVescDisplay) — Display VESC para o mesmo CYD. Baseado no Gh0513d/SVD.
- [VESC_ESP32_Display](https://github.com/SimonRafferty/VESC_ESP32_Display) — Display simples de velocidade/bateria/potência com TTGO ESP32.
- [Super VESC Display](https://github.com/payalneg/Super_VESC_DIsplay) — Dashboard avançado com ESP32-S3 Touch LCD 4" via CAN bus.
- [VescBLEBridge](https://github.com/A-Emile/VescBLEBridge) — Bridge Bluetooth BLE para VESC com ESP32-C3.
- [VescBLEBridge-esp32](https://github.com/payalneg/VescBLEBridge-esp32) — Fork do VescBLEBridge para ESP32 padrão.
- [petersi123/VESC_ESP32](https://www.youtube.com/watch?v=r8KC5SG6EjY) — Display DIY para scooter elétrica com ESP32.
- [OpenSourceEBike](https://opensourceebike.github.io/) — Projeto open-source de e-bike/scooter com VESC + ESP32.

### Cases 3D para o CYD

- [CYD Modular Case](https://www.printables.com/model/1324890-cyd-cheap-yellow-display-modular-case) — Case modular com camadas para add-ons (Printables).
- [CYD Portable Case w/ Battery](https://makerworld.com/en/models/853122-cyd-cheap-yellow-display-portable-case-w-battery) — Case portátil com bateria (MakerWorld).
- [CYD Enclosure](https://cults3d.com/en/3d-model/gadget/cheap-yellow-display-cyd-enclosure) — Enclosure com acesso ao SD card (Cults3D).
- [3D Models no repo oficial CYD](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/tree/main/3dModels) — Vários modelos listados pelo witnessmenow.
- [CYD with a CYB](https://www.thingiverse.com/thing:6736285) — Case simples "Cheap Yellow Box" (Thingiverse).

## License

[MIT](LICENSE)
