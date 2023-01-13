Algumas breves alterações face à sugestão de gestão de ficheiros do GS:

- o ficheiro de palavras está na diretoria corrente (e não numa diretoria específica).
IMPORTANTE: colocar os ficheiros de hint na diretoria corrente (e não numa diretoria específica).

- os ficheiros de jogo são armazenados no path: ./GAMES/PLID/PLID.txt pelo que, após término do jogo, são renomeados, permanecendo
nessa mesma diretoria. (de notar que, nesta situação, não foi seguida a sugestão do ficheiro se chamar "GAME_PLID.txt" mas sim
"PLID.txt").

-"number_games.txt" é um ficheiro criado no arranque do GS, com apenas uma linha, que guarda o número de 
palavras existentes no ficheiro word_eng.txt, bem como o index do número da linha do ficheiro word_eng.txt
 na qual está a palavra a ser atribuída ao próximo jogador.