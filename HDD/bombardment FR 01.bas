10 PRINT TAB(33);"BOMBARDMENT"
20 PRINT TAB(15);"CREATIVE COMPUTING  MORRISTOWN, NEW JERSEY"

100 PRINT "Vous êtes sur un champ de bataille avec 4 pelotons"
110 PRINT "Il y a 25 avant-postes ou ils peuvent etre places."
120 PRINT "Vous ne pouvez placer qu'un seul peloton par avant-poste"
130 PRINT "L'ordinateur fait de meme avec ses quatre pelotons."

140 PRINT "Le but du jeu est de tirer des missiles sur les avant-"
150 PRINT "postes de l'ordinateur. Il en fera autant contre vous."
160 PRINT "Le premier a détruire les quatre pelotons ennemis"
170 PRINT "remporte la victoire."

190 PRINT "Bonne chance, et dites-nous ou vous voulez qu'on envoie les corps !"

210 PRINT "Copiez la grille et utilisez-la pour cocher les numeros."
220 FOR R=1 TO 5: PRINT;: NEXT R
260 DIM M(100)
270 FOR R=1 TO 5
280 I=(R-1)*5+1
290 PRINT; I,I+1,I+2,I+3,I+4
300 NEXT R
350 FOR R=1 TO 10: PRINT;: NEXT R
380 C=INT(RND(1)*25)+1
390 D=INT(RND(1)*25)+1
400 E=INT(RND(1)*25)+1
410 F=INT(RND(1)*25)+1
420 IF C=D THEN 390
430 IF C=E THEN 400
440 IF C=F THEN 410
450 IF D=E THEN 400
460 IF D=F THEN 410
470 IF E=F THEN 410
480 PRINT "Quelles sont vos quatre positions ? x,x,x,x";
490 INPUT G,H,K,L
495 PRINT
500 PRINT "Ou souhaitez-vous tirer votre missile ?";
510 INPUT Y
520 IF Y=C THEN 710
530 IF Y=D THEN 710
540 IF Y=E THEN 710
550 IF Y=F THEN 710
560 GOTO 630
570 M=INT(RND(1)*25)+1
575 GOTO 1160
580 IF X=G THEN 920
590 IF X=H THEN 920
600 IF X=L THEN 920
610 IF X=K THEN 920
620 GOTO 670
630 PRINT "HA, HA Tu as rate ton coup. A mon tour maintenant :"
640 PRINT: PRINT: GOTO 570
670 PRINT "Tu m'as manque, sale rat. J'ai choisi...";M". A vous de jouer :"
680 PRINT: PRINT: GOTO 500
710 Q=Q+1
720 IF Q=4 THEN 890
730 PRINT "Tu as pris l'un de mes avant-postes.!"
740 IF Q=1 THEN 770
750 IF Q=2 THEN 810
760 IF Q=3 THEN 850
770 PRINT "Un de fait, trois a faire."
780 PRINT: PRINT: GOTO 570
810 PRINT "Deux de faits, deux a faire."
820 PRINT: PRINT: GOTO 570
850 PRINT "Trois de faits, plus qu'un."
860 PRINT: PRINT: GOTO 570
890 PRINT "Tu m'as eu, je vais vite... Mais je t'aurai quand..."
900 PRINT "Mes transistors auront recuperer"
910 GOTO 1235
920 Z=Z+1
930 IF Z=4 THEN 1110
940 PRINT "Je t'ai eu. Ton avant-poste";X;"est detruit."
950 IF Z=1 THEN 990
960 IF Z=2 THEN 1030
970 IF Z=3 THEN 1070
990 PRINT "Il ne vous reste que trois avant-postes."
1000 PRINT: PRINT: GOTO 500
1030 PRINT "Il ne vous reste que deux avant-postes."
1040 PRINT: PRINT: GOTO 500
1070 PRINT "Il ne vous reste qu'un seul avant-poste."
1080 PRINT: PRINT: GOTO 500
1110 PRINT "Vous etes mort. Votre dernier avant-poste etait a ";X;". HA, HA, HA."
1120 PRINT "Soyez meilleur la prochaine fois."
1150 GOTO 1235
1160 P=P+1
1170 N=P-1
1180 FOR T=1 TO N
1190 IF M=M(T) THEN 570
1200 NEXT T
1210 X=M
1220 M(P)=M
1230 GOTO 580
1235 END
