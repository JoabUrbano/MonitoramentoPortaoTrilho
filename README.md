# MonitoramentoPortaoTrilho

<h1 id="usage" > 💻 Descrição </h1>

Projeto que visa monitorar um portão automatico de trilho com Esp32. É verificado se ele está aberto, fechado ou no meio do caminho. Esses dados são enviados via MQTT e são consultados por um raspberry com o Home Assistent, e então de lá o usuario é notificado via telegram, podendo assim enviar comando para o Esp32 abrir ou fechar o portão, sendo também possivel vizualizar os ultimos logs das interações de abrir/fechar o portão.

<h1 id="usage" > 🧰 Componentes utilizados </h1>
- Um Esp32 Dev Module<br>
- Jumpers<br>
- Três sensores magneticos de porta<br>
- Seis resistores de 10K Ohm<br>
- Um botão<br>
- Dois LEDs

<h1 id="usage" > 📚 Bibliotecas </h1>
- Wifi<br>
- WiFiClient<br>
- PubSubClient<br>
- NTPClient<br>
- SPIFFS<br>
- Fs<br>
- queue

<h2>Como rodar o projeto?</h2>

É necessario que a IDE do arduino e as bibliotecas necessarias estejam instalados na maquina. Após instalar tudo, você deve escolher o modelo do seu Esp32 e a porta para fazer a gravação.


<h3>Autores</h3>
<a href="https://github.com/JoabUrbano">Joab Urbano</a><br>
<a href="https://github.com/GabrielNSD">Gabriel Dantas</a><br>
<a href="">João Pedro</a>
