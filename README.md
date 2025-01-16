# Monitoramento Portao de Trilho Elétrico

<h1 id="usage" > 💻 Descrição </h1>

Projeto que visa monitorar um portão elétrico de trilho com Esp32. É verificado se ele está aberto totalmente, fechado ou se ele está aberto parcialmente. Esses dados são enviados via MQTT e são consultados por um raspberry com o Home Assistent, e então de lá o usuario é notificado via telegram, podendo assim enviar um comando via chat para o Esp32 conectado a um ontrole para abrir ou fechar o portão, sendo também possivel vizualizar os ultimos logs das interações de abrir/fechar o portão.

<h1 id="usage" > 🧰 Componentes utilizados </h1>
- Um Esp32 Dev Module ou modelo similar<br>
- Jumpers<br>
- Três sensores magneticos de porta<br>
- Cinco resistores de 10K Ohm<br>
- Um controle de portão sintonizadocom seu motor<br>
- Um LED (Nó código utilizamos o pino 2 para o LED do Wifi, que é LED interno, por isso se não quiser utilizar um LED dedicado naõ precisa)

<h1 id="usage" > 📚 Bibliotecas </h1>
- Wifi<br>
- WiFiClient<br>
- <a href="https://www.arduinolibraries.info/libraries/pub-sub-client">PubSubClient</a><br>
- <a href="https://github.com/arduino-libraries/NTPClient">NTPClient</a><br>
- SPIFFS<br>
- Fs<br>
- queue

<h2>Como rodar o projeto?</h2>

- Ter a IDE do Arduino ou a extenção do VSCode ou outra forma de compilar o código para o microcontrolador Esp. 

- Ter as bibliotecas externas devidamente instaladas.

- Selecione o modelo de seu microcontrolador e grave o código.

<h2>Montagem</h2>

- Os pinos 4, 18 e 19 são os pinos bara os sensores em ordem da direção do motor do portão para a tranca do portão. O 4 é para o sensor totalmente aberto, o 18 para o no meio do percurso e o 19 para o sensor onde fica a tranca indicando que o portão está fechado.

- O LED é para indicar que a conexão com o Wifi foi bem sucedida.

- Finalmente os pinos 22 e 23 são pinos que estarão conectados diretamente no circuito de um controle sintonizado com o motor do portão. Dessa forma dará para fazer o controle do portão remotamente via Telegram.

<h3>Autores</h3>
<a href="https://github.com/JoabUrbano">Joab Urbano</a><br>
<a href="https://github.com/GabrielNSD">Gabriel Dantas</a><br>
<a href="">João Pedro</a>
