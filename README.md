# MonitoramentoPortaoTrilho

<h1 id="usage" > 💻 Descrição </h1>

Projeto que visa monitorar um portão automatico de trilho. É verificado se ele está aberto, fechado ou no meio do caminho. Se o portão fica aberto por muito tempo o usuario é notificado e pode fechalo remotamente.

Também havera a feature de consultar os ultimos logs dos status do portão, com seu respectivo horario.

Componentes utilizados no projeto:
- Um Esp32 Dev Module
- Jumpers
- Três sensores magneticos de porta
- Seis resistores de 10K Ohm
- Um botão
- Dois LEDs

Bibliotecas necessarias para Esp32:
- Wifi
- WiFiClient
- PubSubClient
- NTPClient
- SPIFFS
- Fs
- queue

<h2>Como rodar o projeto?</h2>

É necessario que a IDE do arduino e as bibliotecas necessarias estejam instalados na maquina. Após instalar tudo, você deve escolher o modelo do seu Esp32 e a porta para fazer a gravação.


<h3>Autores</h3>
<a href="https://github.com/JoabUrbano">Joab Urbano</a><br>
<a href="https://github.com/GabrielNSD">Gabriel Dantas</a><br>
<a href="">João Pedro</a>
