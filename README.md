# Klipper WiFi Display

Display Wi-Fi com ESP32 para monitorar uma impressora 3D com **Klipper + Moonraker**.

Guia rápido: [`docs/QUICKSTART.md`](docs/QUICKSTART.md)

O firmware conecta no Wi-Fi, consulta periodicamente a API HTTP do Moonraker e mostra no OLED (SSD1306 I2C):
- mensagem do `display_status.message`
- progresso da impressão
- temperatura do extrusor
- temperatura da mesa

## Como funciona

Fluxo de execução:
1. Inicializa NVS.
2. Inicializa display OLED (se habilitado).
3. Conecta no Wi-Fi em modo STA.
4. Inicia task de polling do Moonraker.
5. Atualiza display em intervalo configurável.

Endpoint consultado:
- `GET /printer/objects/query?toolhead&extruder&heater_bed&print_stats&display_status`

## Requisitos

- ESP-IDF 5.x (projeto em CMake padrão do IDF)
- Placa ESP32 compatível com Wi-Fi
- Moonraker acessível na mesma rede local
- Display OLED SSD1306 via I2C (opcional)

Dependência de componente:
- `nixy4/u8g2` (declarada em `main/idf_component.yml`)

## Estrutura do projeto

- `main/main.c`: bootstrap da aplicação
- `main/wifi_manager.*`: conexão e reconexão Wi-Fi
- `main/moonraker_client.*`: consulta HTTP e parse de status
- `main/display_manager.*`: driver de exibição com u8g2
- `main/Kconfig.projbuild`: opções no `menuconfig`

## Configuração

Use `idf.py menuconfig` e ajuste os menus:

### Wi-Fi
- `WIFI_SSID`
- `WIFI_PASSWORD`
- `WIFI_MAXIMUM_RETRY`

### Moonraker
- `MOONRAKER_IP`
- `MOONRAKER_PORT` (padrão: `7125`)
- `MOONRAKER_POLL_INTERVAL_MS` (padrão: `2000`)

### Display OLED
- `DISPLAY_ENABLE`
- `DISPLAY_I2C_PORT` (padrão: `0`)
- `DISPLAY_I2C_SDA_GPIO` (padrão: `21`)
- `DISPLAY_I2C_SCL_GPIO` (padrão: `22`)
- `DISPLAY_I2C_FREQ_HZ` (padrão: `400000`)
- `DISPLAY_I2C_ADDR` (padrão: `0x3C`)

## Build, flash e monitor

```bash
idf.py set-target esp32
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

Se sua placa for outro target (ex.: `esp32s3`), ajuste no `set-target`.

## Segurança e versionamento (Git)

Este projeto já está preparado para não subir segredos por padrão:
- `sdkconfig` está no `.gitignore`
- `managed_components/` está no `.gitignore`
- artefatos de build estão no `.gitignore`

Boas práticas antes de `git push`:
1. Não commitar SSID, senha ou IP real em arquivos versionados.
2. Revisar `git diff --staged` para garantir que não há credenciais.
3. Usar valores placeholder em exemplos/documentação (`MEU_WIFI`, `MINHA_SENHA`, `192.168.x.x`).

## Troubleshooting

### Não conecta no Wi-Fi
- Confirme SSID/senha no `menuconfig`.
- Verifique alcance da rede e modo WPA/WPA2.
- Aumente `WIFI_MAXIMUM_RETRY`.

### Falha ao consultar Moonraker
- Verifique `MOONRAKER_IP` e `MOONRAKER_PORT`.
- Teste no PC/celular: `http://IP:PORT/printer/info`.
- Confirme se ESP32 e host Klipper estão na mesma rede.

### Display não mostra dados
- Verifique SDA/SCL, GND e VCC.
- Confirme endereço I2C (`0x3C` ou `0x3D`).
- Desabilite `DISPLAY_ENABLE` se quiser rodar sem OLED.

## Limitações atuais

- Consulta somente via HTTP local (sem TLS/autenticação).
- Parse JSON manual para campos específicos do Moonraker.
- Mostra apenas 4 linhas de status no OLED.

## Próximos passos sugeridos

- Adicionar indicação de estado da impressão (`printing`, `paused`, etc.).
- Implementar fallback para hostname mDNS do Moonraker.
- Evoluir para UI gráfica com ícones e barras no display.
