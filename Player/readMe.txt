Visto existir alguma liberdade para a implementação de pequenos detalhes do projeto, listam-se algumas observações inerentes
a decisões tomadas em situações que havia essa liberdade:

- Se o player der quit após o arranque da aplicação, o output é "Trying to quit a non existing game.\n".
- Se o player der hint após o arranque da aplicação, o output é "Trying to access hint without any ongoing game.\n".
- Se o player der state após o arranque da aplicação, o output é "Trying to access state without a plid.\n".
(nota: em nenhum dos casos existe envio de mensagem para o servidor)

- Se o player der exit e não existir nenhum jogo ativo, a aplicação é fechada, não havendo envio de mensagem para o servidor.



Para não incluir os timers, em UDP, comentar as seguintes linhas de código:
(linhas 86, 90 a 95)

n = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);

if (n < 0) {
    /* Timeout reached */
    printf("No server response was received within the timeout window (5 seconds). Please, try again.\n");
    n = 0;
}
else if (n==-1) exit(1);