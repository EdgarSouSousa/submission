# ESP32 Sensor to HTTP Starter

## Objetivo

Desenvolver e testar firmware para recolher dados de um sensor utilizando I2C, processá-los e transmiti-los a um servidor remoto via Wi-Fi, utilizando HTTP.

## Requisitos

- Suporte a plataforma dual-core (ESP32).
- Leitura de dados via protocolo físico (I2C).
- Transmissão de dados via protocolo wireless (HTTP sobre Wi-Fi).
- Separação do firmware em tarefas independentes (FreeRTOS tasks).

## Decisões de Protocolos/Hardware

![Esquema Completo](/imgs/hardwareProtocols.png "Esquema Completo")

### Plataforma: ESP32

- Arquitetura dual-core.
- Suporte robusto da comunidade.
- Diversidade de interfaces de comunicação físicas e wireless.

### Protocolo Físico: I2C

- Simples de implementar e robusto.
- Adequado para situações com baixas quantidades de sensores e dados.
- Desvantagens como o limite de dispositivos e distância entre microcontrolador e sensor não aplicável neste caso.

### Protocolo Wireless: HTTP sobre Wi-Fi

- Utilização quase universal, não são necessárias bibliotecas externas.
- Versatilidade permitindo controlo de erros e estruturação de dados via endpoints.
- Protocolos como MQTT podem ser considerados exessivos para cenários com apenas um produtor e consumidor.

### BME280

- Utilização do protocolo I2C
- Documentação robusta
- Acessibilidade

## Arquitetura do Projeto

![Esquema Tarefas e Memória](/imgs/taskAndStructure.png "Esquema Tarefas e Memória")

- **Task 1**: Leitura e processamento dos dados do sensor.
- **Task 2**: Envio assíncrono dos dados para o servidor via HTTP POST.
- Comunicação entre tarefas feita por uma **fila (Queue)**.

### jutificação da Arquitetura

- Separação entre operações síncronas (I2C) e assíncronas (HTTP).
- Uso de uma fila permite:
  - Buffer simples.
  - Acesso controlado entre tasks aos dados
- Cada mensagem inclui um timestamp local:
  - Garante validade temporal dos dados.
  - Facilita análise de mensagens de erro quando enviadas pelo microcontrolador.

## Estrutura do Projeto

```bash
├── CMakeLists.txt        //CMakeFile 
├── platformio.ini        //Configurações PlatformIO
├── readme.md
├── sdkconfig.esp32dev    //Configurações sdk 
├── server.py             //Código servidor
├── src
│   ├── BME280.c          // Funções Referentes ao sensor
│   ├── BME280.h
│   ├── httpComms.c       // Frunções referentes ao envio de pacotes HTTP
│   ├── httpComms.h
│   ├── httpMessages.h   
│   └── main.c            // Ficheiro main.
```

## Como Executar

```bash
git clone https://github.com/EdgarSouSousa/submission.git
```

- Editar definições no header do ficheiro main.c (Wi-fi SSID, Wi-fi Password, Max retrys, auth mode).
- Editar URL do servidor no ficheiro httpComms.h.
- Editar Configurações I2C do ficheiro main (numero dos GPIO's).
- Conectar BME280 ás portas designadas para I2C do ESP.
- Conectar ESP por porta série e editar permissões
- Executar servidor

```bash
python3 server.py
```

- Build e flash do projeto

```bash
idf.py build
idf.py flash
idf.py monitor
```
