# Jeu de Tir à la Corde Multijoueur sur ESP32

Ce projet programme une carte ESP32 pour créer un jeu de "tir à la corde" numérique multijoueur. Deux joueurs disposent chacun d'une carte, d'un écran OLED et de deux boutons. Le but est d'appuyer le plus vite possible sur le bouton de tir pour ramener un curseur virtuel de son côté.

## La liaison de communication : ESP-NOW (Wi-Fi Direct)

La liaison entre les deux microcontrôleurs s'effectue via **ESP-NOW**, un protocole de communication sans fil développé spécifiquement pour les puces ESP. Il exploite le matériel Wi-Fi (ondes radio à 2,4 GHz) de la carte, mais fonctionne différemment du Wi-Fi domestique classique.

Contrairement à un appareil qui se connecte à un routeur, ESP-NOW relie les cartes directement entre elles en pair-à-pair (Point-to-Point). Les données sont envoyées via une adresse de diffusion universelle (`FF:FF:FF:FF:FF:FF`), ce qui permet aux deux cartes de s'échanger des informations instantanément, sans délai de connexion ni besoin de réseau local préexistant.

## Explication du code de A à Z

### 1. Les outils de base
Les instructions `#include` importent les bibliothèques nécessaires à l'exécution de tâches complexes :
*   `esp_now.h` et `WiFi.h` : Gèrent l'antenne radio et le protocole sans fil.
*   `Wire.h`, `Adafruit_GFX.h`, `Adafruit_SSD1306.h` : Gèrent la communication filaire interne (I2C) et l'affichage des formes sur l'écran OLED.

### 2. L'écran et le calibrage intelligent des boutons
Le code déclare un écran de 128 pixels de large par 64 pixels de haut.
La section "GESTION MATÉRIELLE" configure les boutons. Plutôt que d'imposer un câblage strict, le programme détecte automatiquement le branchement des boutons :
*   Il teste une liste de broches définie dans `BROCHES`.
*   La fonction `calibrerBouton` demande à l'utilisateur, via l'écran, d'appuyer sur le bouton "Tirer" puis sur "Reset".
*   Il enregistre la broche activée au moment de la pression pour associer physiquement le bouton à son action dans le jeu.

### 3. Les règles du jeu et la préparation des données
Plusieurs variables stockent l'état de la partie :
*   `myScore` et `opponentScore` enregistrent le nombre d'appuis de chaque joueur.
*   `scoreLimit = 25` définit la condition de victoire (25 clics d'avance sur l'adversaire).
*   `struct_message` définit la structure du paquet de données envoyé par Wi-Fi. Il contient le nombre de clics et un ordre de redémarrage (reset).
*   `OnDataRecv` est une fonction de rappel (callback) qui s'exécute automatiquement lors de la réception d'un message ESP-NOW valide. Elle lit le paquet et met à jour le score de l'adversaire.

### 4. La phase de démarrage : `setup()`
La fonction `setup` s'exécute une seule fois à la mise sous tension :
*   Initialisation de l'écran OLED.
*   Analyse de l'état électrique initial.
*   Lancement de la routine de calibration des boutons.
*   Configuration de l'antenne en mode "Station Wi-Fi" (mode d'émission/réception).
*   Activation du protocole ESP-NOW sur le canal 0 (sans chiffrement) et ajout de l'adresse universelle (broadcast) pour joindre la seconde carte.

### 5. La boucle principale : `loop()`
La fonction `loop` tourne en continu et contient la logique active :
*   **Vérification des actions :** Lecture de l'état physique des boutons. Si "Tirer" est pressé en cours de partie, le score augmente de 1. Le paquet de données est mis à jour et expédié instantanément via `esp_now_send`. Si "Reset" est pressé, les scores sont remis à 0 et l'ordre d'annulation est envoyé.
*   **Vérification du score :** Calcul de la différence (`diff`) entre les deux scores. Si l'écart atteint 25 ou -25, la partie passe en mode "Terminé" (`gameOver = true`).
*   **Dessin sur l'écran :** L'écran est effacé et actualisé à chaque cycle. En fin de partie, il affiche "VICTOIRE!" ou "PERDU...". En cours de jeu, il dessine un rectangle (la corde) et utilise la fonction `map()` pour convertir l'écart de points en pixels, déplaçant ainsi un marqueur blanc de gauche à droite en temps réel.
