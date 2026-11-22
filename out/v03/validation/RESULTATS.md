# Astra CPS1 v0.3 sur Old 3DS

## Résultat sur la console de test

La v0.2 sur la Old 3DS de l’utilisateur a atteint 59,6 trames émulées/s et 29,8 images affichées/s en Auto sur la première photo, avec 100 % de vitesse et les compteurs audio à zéro. L’utilisateur décrit des baisses ponctuelles vers 22 images/s dans une scène chargée en mode Toutes. La v0.3 permet de reprendre cette mesure au même endroit grâce aux compteurs Vitesse, Émulation, Dessinées et Affichage.

## Jeux et commandes

Les deux ZIP fournis, Final Fight (World) et Street Fighter II: The World Warrior (World 910522), ont été lancés depuis le sélecteur dans Azahar configuré en Old 3DS. Les essais ont couvert le crédit, le choix du personnage, des combats et le retour à l’autre jeu dans le même processus. Un essai de Street Fighter II avec les boutons réaffectés et le DIP de jeu gratuit a démarré avec la nouvelle touche de départ. Le lancement d’un nouveau processus a également permis de vérifier que les profils SF2 revenaient.

Le catalogue en source compte 56 sets, parmi lesquels plusieurs régions et révisions d’un même titre. Le test de tous les groupes DIP vérifie les changements de bits des pilotes. Les tests de jeu utilisent les deux archives disponibles. [Catalogue intégré](../../../engine/drivers.inc) · Contrôles et interrupteurs vérifiés

L’exécutable installé porte le SHA-256 `c36fc9f6a4fad99e91dc8738df858ee0276b48fd6d8d540b64cdceff8e55a286`. Il a relu les profils enregistrés, puis appliqué le DIP d’inversion et le retour à l’orientation normale après redémarrage de la borne dans Azahar. Seule l’image du jeu pivote. Les séquences de combat filmées proviennent de la compilation précédente, avant cet ajustement du DIP d’inversion. Image retournée · Image à l’endroit · Vidéo de Street Fighter II · Vidéo de Final Fight

## Dessin et son

Sur un banc Azahar avec les coûts CPU des photos, le début du calcul était décalé de 1, 4, 7, 10 ou 13 ms par rapport au rafraîchissement. Pour 90 trames en Auto, la v0.2 soumettait respectivement 75, 48, 45, 45 et 58 images. La v0.3 en soumet 90 à chacun des cinq décalages. C’est une mesure de la présentation avec une charge simulée. Une partie sur la console physique mesure aussi le coût de chaque décor, personnage et coup. Mesures v0.2 · Mesures v0.3

Sur une partie de Final Fight de 3 200 trames à cadence de dessin variable, les empreintes des pixels calculés, du son stéréo, de la mémoire du jeu et des registres 68000 suivis sont identiques entre v0.2 et v0.3. Mesures du binaire v0.3 · Référence v0.2

## Paquet personnel installé

Les sept fichiers de la distribution ont été relus depuis le volume FAT32 `3DS32GO`. La ROM Final Fight déjà présente et la copie ajoutée de Street Fighter II ont aussi passé le contrôle CRC de tous leurs membres ZIP. Le menu détecte les ZIP au démarrage et garde les profils de touches et de DIP séparés pour chaque set. Empreintes après installation · Scénarios ZIP · Tests des profils
