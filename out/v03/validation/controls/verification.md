# Vérification du moteur Astra CPS1 v0.3

Les deux suites ont exécuté le même ELF ARM, identifié par l’empreinte SHA-256 `B98085BA775520C09D881CFFE432966F1EFBE0D5702F208A4C7E1738A92B600D`. L’émulation ARM sur ordinateur permet ici de vérifier le fonctionnement. La vitesse d’affichage sur Old 3DS se mesure sur la console.

| Essai | Résultat observé |
| --- | --- |
| Street Fighter II, World Warrior du 22 mai 1991 | 3 050 trames depuis l’archive originale. Sans crédit inséré et avec le réglage Free Play, Ryu affronte Dhalsim. Les six lignes de boutons ont été actionnées individuellement. Le son stéréo atteint une amplitude de 18 104 sur 32 768. |
| Réglages de Final Fight et de Street Fighter II | Les 12 groupes de Final Fight et les 9 de Street Fighter II acceptent chacun leurs valeurs proposées. Le test vérifie les autres réglages après chaque sélection. Une impulsion de reset conserve la difficulté et le jeu gratuit sur les deux jeux, ainsi que les quatre vies choisies dans Final Fight. |
| Écran retourné de Street Fighter II | Deux parties de 2 100 trames ont reçu les mêmes commandes. Avec le DIP Flip activé, le jeu programme le bit 15 du registre de contrôle vidéo. Les 106 captures prélevées lorsque le bit était actif correspondent, pixel par pixel, à une rotation de 180° de l’image normale. |
| Catalogue | Les 56 pilotes fournissent au total 582 groupes et 2 314 valeurs de réglage. Le test a parcouru leurs tables et vérifié la sélection de chaque valeur. Les deux jeux ci-dessus disposaient d’archives locales pour les essais de jeu. |

Les images du combat et les rapports détaillés permettent de vérifier les résultats. Coup de pied de Ryu en jeu gratuit · Image normale · Image retournée · Mesures des commandes et des DIP · Mesures du retournement
