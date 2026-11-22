# Astra CPS1 v0.4 pour Old 3DS

La carte 3DS32GO contient la v0.4 et les deux ZIP d’origine. Le 3DSX et le fichier LIRE-MOI ont remplacé leurs versions précédentes. Après relecture des sept fichiers du paquet et vérification interne des deux ZIP, Windows a éjecté la carte.

## Street Fighter II expliqué simplement

- La borne a un processeur qui fait avancer le jeu et un autre qui s’occupe du son. Chacun passe parfois son temps à vérifier si quelque chose est prêt. Pour deux boucles précises de cette révision, l’émulateur peut calculer plusieurs tours d’attente d’un coup. Le temps écoulé dans le jeu reste le même.
- Quand les combattants bougent, la caméra décale le sol et le décor. Pour le plan de décor compatible, la 3DS garde en mémoire des bandes déjà dessinées et copie la portion qui arrive à l’écran. Si le jeu change un motif ou ses couleurs, elle le redessine. Un étage de dessin entièrement invisible peut aussi être sauté.
- Pour certains instruments du synthétiseur de la borne, la 3DS calcule plusieurs échantillons d’une traite. Les changements d’instrument et de volume gardent leur place. On a comparé directement les échantillons gauche et droit.
- Le choix se fait automatiquement au chargement de la révision World 910522. Final Fight garde son propre traitement. Les boutons et les DIP restent accessibles par RÉGLER et se sauvegardent par jeu.

## Mesures

Les deux moteurs ARM de la v0.3 et de la v0.4 ont exécuté les mêmes entrées sur les ZIP de la carte. Le compteur additionne les emplacements d’instructions dans les blocs ARM parcourus. La charge des autres logiciels du portable n’entre pas dans ce décompte. Les accès mémoire, le cache de la console et la présentation à l’écran influencent aussi la cadence physique.

| Scénario | Instants avec dessin | Variation du travail ARM avec dessin | Instants sans dessin | Variation sans dessin |
|---|---:|---:|---:|---:|
| Ryu contre Dhalsim, partie du scénario à partir de la trame 1500 | 5 | −45,83 % | 5 | −32,18 % |
| Dhalsim contre Chun-Li, partie du scénario à partir de la trame 1700 | 4 | −11,71 % | 4 | −13,71 % |
| Final Fight, scénario complet échantillonné | 8 | +0,10 % | 8 | +0,19 % |

Sur les huit couples d’instants du scénario complet, en incluant les écrans qui précèdent le combat, les écarts avec dessin sont de −34,57 % et −13,04 % respectivement pour les deux essais de Street Fighter II. La cadence affichée sur la Old 3DS physique reste à mesurer avec cette version.

## Contrôles de fidélité et de lancement

Les comparaisons couvrent 3 200 trames émulées par scénario, soit 9 600 trames au total. Les images demandées à une cadence variable, le flux audio stéréo, les registres suivis du 68000 et la RAM principale concordent avec la v0.3. Les registres suivis du Z80 et sa RAM concordent également sur les deux essais de Street Fighter II.

Le 3DSX livré a ensuite démarré dans Azahar. Un extrait de huit secondes montre les deux écrans, le combat de Ryu contre Dhalsim et contient sa piste stéréo. Le compteur visible appartient à cet essai sur PC.

- Extrait du combat
- Mesure Ryu contre Dhalsim
- Mesure Dhalsim contre Chun-Li
- Mesure Final Fight
- Empreintes relues sur la carte

SHA-256 du 3DSX installé : `136380a55ec429e90a997edea2b774d92d3201f038c13011a7b035102d81f55f`.
