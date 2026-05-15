# Quickstart (5 minutos)

Guia rápido para colocar o `Klipper_Wifi_Display` no ar.

## 1. Pré-requisitos

- ESP-IDF 5.x instalado e exportado no shell.
- ESP32 conectado por USB.
- Moonraker ativo na rede local.
- (Opcional) Display OLED SSD1306 I2C ligado no ESP32.

## 2. Entrar na pasta do projeto

```bash
cd /home/luiz/Documents/EspressIf/Klipper_Wifi_Display
```

## 3. Configurar credenciais e Moonraker

```bash
idf.py menuconfig
```

Ajuste:

- `Configuracao Wi-Fi`
- `WIFI_SSID`: sua rede
- `WIFI_PASSWORD`: sua senha
- `Configuracao Moonraker`
- `MOONRAKER_IP`: IP do host com Klipper/Moonraker
- `MOONRAKER_PORT`: `7125` (padrão)
- `MOONRAKER_POLL_INTERVAL_MS`: `2000` (padrão)

Opcional (OLED):

- `Configuracao Display OLED`
- `DISPLAY_ENABLE`: habilitado
- `DISPLAY_I2C_SDA_GPIO`: `21` (padrão)
- `DISPLAY_I2C_SCL_GPIO`: `22` (padrão)
- `DISPLAY_I2C_ADDR`: `0x3C` (padrão)

## 4. Compilar e gravar

```bash
idf.py set-target esp32
idf.py -p /dev/ttyUSB0 build flash monitor
```

Se sua porta serial for outra, troque `/dev/ttyUSB0`.

## 5. Validar funcionamento

No monitor serial, confirme:

- `Wi-Fi conectado com sucesso.`
- logs periódicos do Moonraker com `progress`, `extruder` e `bed`

No OLED (se habilitado), confirme:

- linha de mensagem (`MSG`)
- progresso (`PRG`)
- temperatura extrusor (`EXT`)
- temperatura mesa (`BED`)

## 6. Se der erro

- `Falha conexao`:
  - revisar `WIFI_SSID` e `WIFI_PASSWORD`
- `Falha ao consultar Moonraker`:
  - revisar `MOONRAKER_IP`/`MOONRAKER_PORT`
  - testar no navegador: `http://IP:7125/printer/info`
- display sem imagem:
  - revisar SDA/SCL/GND/VCC
  - testar endereço `0x3C` ou `0x3D`

## 7. Antes de subir para Git

- não commitar valores reais de SSID/senha/IP
- revisar mudanças com:

```bash
git diff --staged
```
