# Astra CPS1 pour Old Nintendo 3DS

**[Télécharger le ZIP public v0.6a](https://github.com/SerialZ61/Astra-CPS1/releases/download/v0.6a/Astra-CPS1-v0.6a-public.zip)** · [Toutes les versions](https://github.com/SerialZ61/Astra-CPS1/releases) · [English](README.md)

Ce homebrew permet de jouer aux jeux d’arcade CPS1 sur la première génération de Nintendo 3DS. La v0.6a est la version de référence essayée sur la console de SerialZ. L’archive regroupe le programme, le code source correspondant, les notices et le journal du développement.

## Installer sur la carte SD

1. Décompresser le dossier `3ds` à la racine de la carte SD.
2. Placer ses propres ZIP arcade dans `/3ds/AstraCPS1/roms/`. Pour un jeu dont les puces sont réparties dans plusieurs ZIP, placer le parent et le clone dans le même dossier.
3. Ouvrir **Astra CPS1** dans le Homebrew Launcher. Un seul ZIP démarre directement. S’il y en a plusieurs, choisir le jeu au stylet ou avec la croix et A.
4. SELECT insère une pièce. START commence la partie. **RÉGLER** donne accès aux touches et aux DIP. SELECT + START revient à la liste. L + R + START quitte l’application.

**CADENCE** permet de choisir Auto, 30 i/s, 1 sur 2 ou Toutes. **FORMAT** permet notamment de jouer console tournée à *1941*, avec les directions qui suivent l’écran. Toucher **SON CONTINU** permet de comparer le traitement adaptatif avec le son arcade. [Guide détaillé](LIRE-MOI.txt)

## Versions et compatibilité

Le banc de la v0.6a a lancé 136 archives pendant 420 trames chacune. La collection couvre des versions régionales et des clones. *Three Wonders* reste à confirmer sur console. Certaines variantes équipées d’un PIC sonore sont muettes. Les essais sur PC et les retours de SerialZ sur Old 3DS sont détaillés dans le journal. [Journal du projet](HISTORIQUE-ASTRA-CPS1.md) · [Résultats par ZIP](out/v06/card-collection.json)

Pour recompiler, installer devkitARM, libctru, citro3d, picasso, zlib 3DS et les outils 3DSX de devkitPro. Depuis la racine, lancer `.\build.ps1` dans PowerShell ou `make -j4` dans le shell devkitPro. Le programme est produit dans `out/AstraCPS1.3dsx`.

Projet et essais sur console par **SerialZ**. Portage, optimisations et bancs de test réalisés avec **Codex (OpenAI)**. Les auteurs des moteurs et des outils sont crédités dans les notices. [Crédits et conditions](notices/README.md)
