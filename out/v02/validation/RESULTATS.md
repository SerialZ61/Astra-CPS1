# Final Fight sur Old 3DS — bilan de la v0.2

Sur huit images de combat dessinées, le cœur de la v0.2 a exécuté **63,9 % d’instructions ARM en moins** que la v0.1 dans le test sur PC. Les deux versions ont produit les mêmes pixels et le même son pendant la partie de contrôle. **La cadence de la v0.2 sur la console physique reste à mesurer.**

## Ce que montre la photo de la console

Sur la photo de la **v0.1** fournie dans le chat, le jeu est en pause et le réglage d’affichage est sur **Auto**. L’écran indique **93,7 %** de la vitesse arcade, **55,9 images émulées/s** et **14,0 images affichées/s**. Les compteurs de tampons audio vides et pleins sont tous les deux à **0**. Cette lecture ponctuelle sert de repère pour le prochain essai sur la même Old 3DS.

## Comparaison sur 3 200 trames

Les deux exécutables ARM ont chargé le même `ffight.zip` et reçu les mêmes commandes de déplacement et de combat. Le test alterne les trames dessinées et celles où seuls la simulation et le son avancent.

| Travail mesuré dans le cœur | v0.1 | v0.2 | Réduction |
| --- | ---: | ---: | ---: |
| Huit trames de combat avec dessin, total | 29 505 578 | 10 643 551 | **63,9 %** |
| Huit trames de combat sans dessin, total | 10 332 509 | 5 890 107 | **43,0 %** |

Le compteur additionne la taille des blocs ARM exécutés, divisée par quatre. Il couvre le cœur CPS1. L’affichage par le GPU de la 3DS et la restitution du son sur la console se mesurent séparément.

Les CRC32 correspondent **à chaque trame** pour les pixels RGB565 lorsqu’une image est dessinée, le PCM stéréo, les 64 Kio de RAM de travail et les registres 68000 suivis. Les quatre empreintes SHA-256 cumulées correspondent également. Données v0.1 · Données v0.2 · [Scénario de test](../../../tests/compare_releases.py)

## Essai du 3DSX dans Azahar

Azahar a lancé le 3DSX de la v0.2 et permis de jouer avec le son. La capture montre les deux écrans et l’image de jeu au format arcade **4:3**. Vidéo de la partie avec le son · Capture fixe

Le fichier `AstraCPS1.3dsx` testé porte l’empreinte SHA-256 `426cb0ea00f916dd78d35542a2cc33374a64a328ca686bba94e1c473b32f4ab1`.
