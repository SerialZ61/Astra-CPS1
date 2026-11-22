# Essai de Final Fight sur Astra CPS1 0.1

Le paquet SD contient le binaire et l’archive d’origine vérifiés par SHA-256.

| Fichier | SHA-256 |
| --- | --- |
| AstraCPS1.3dsx | `6dddc15f19ecb5d7e7ad19aa5f1414c8b0d693093e94618b8938615eba3e05cc` |
| AstraCPS1.elf | `71a72157771b3b676479bfdfa392299e886c935d0727953f540878d2f78bbf23` |
| ffight.zip | `a7cc8894488ee126851ee1f45eccb99e6da96f63b5bbd31689adaf08f7a37eab` |

## Partie dans Azahar

Azahar 2126.2 a démarré ce 3DSX avec le profil Old 3DS. Une séquence de commandes a inséré le crédit, lancé la partie et fait avancer, attaquer et sauter Guy dans Slum. La partie a atteint 7 000 points. La sortie audiovisuelle d’Azahar contient les deux écrans et le son stéréo. Le fichier Final-Fight-Old3DS-demo.mp4 en reprend neuf secondes. Sa capture fixe est également disponible.

## Comparaison du cœur 68000

Le même ELF a exécuté le même scénario de 1 900 trames avec l’accélération des tâches en attente active puis désactivée. Les images RGB565, le son PCM, les registres du 68000 et les 64 Kio de RAM de travail correspondent à chaque trame. Le chemin accéléré a traité 1 310 628 entrées en attente. Le scénario a produit 702 582 paires d’échantillons stéréo.

Une seconde paire de tests a dessiné une trame sur deux. L’audio et l’état du jeu correspondent également à ceux du test qui dessine toutes les trames. Les résultats détaillés sont dans full-fast/report.json, full-base/report.json, skip-fast/report.json et skip-base/report.json.

## Mesure sur console

La vitesse et la continuité du son restent à mesurer sur la Old 3DS physique. La vidéo de démonstration documente le fonctionnement dans Azahar. La vitesse d’enregistrement ou de lecture de ce fichier ne mesure pas les performances sur console.
