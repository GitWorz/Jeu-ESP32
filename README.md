# Jeu de Tir à la Corde Multijoueur sur ESP32

Ce projet programme une carte ESP32 pour créer un jeu de tir à la corde numérique multijoueur. Deux joueurs utilisent chacun une carte, un écran OLED et deux boutons. Le but est d'appuyer le plus vite possible sur le bouton de tir pour amener un curseur virtuel de son côté.

## Fonctionnalités Principales
*   **Communication sans fil** : Utilisation du protocole ESP-NOW (Wi-Fi Direct) pour une liaison pair-à-pair ultra-rapide.
*   **Calibrage dynamique** : Détection automatique des broches utilisées pour les boutons au démarrage.
*   **Anti-rebond logiciel** : Filtrage des faux contacts mécaniques pour un comptage précis des scores.
*   **Sécurisation réseau** : Filtrage des échos de paquets (ID unique) et gestion des paquets retardataires (ID de manche).
*   **Interface graphique OLED** : Affichage dynamique de la corde, du curseur et de l'écart de force en temps réel.

## Matériel Requis (par joueur)
*   1x Carte ESP32
*   1x Écran OLED SSD1306 (128x64) via interface I2C
*   2x Boutons poussoirs (Tirer, Reset)
*   Câbles de connexion

## La liaison de communication : ESP-NOW

La communication entre les microcontrôleurs utilise ESP-NOW, un protocole sans fil pair-à-pair développé pour les puces ESP. Il exploite l'antenne Wi-Fi (2,4 GHz) sans nécessiter de routeur ou de réseau local. Les données sont expédiées via une adresse de diffusion universelle (`FF:FF:FF:FF:FF:FF`), permettant un échange de données instantané entre les cartes.

## Architecture et Explication du Code

### 1. Les bibliothèques fondamentales
*   `esp_now.h` et `WiFi.h` : Gèrent le module radio et le protocole pair-à-pair.
*   `Wire.h`, `Adafruit_GFX.h`, `Adafruit_SSD1306.h` : Assurent le protocole de communication I2C et le contrôle matériel de l'écran OLED.

### 2. Gestion matérielle et calibrage dynamique
Le programme configure un écran de 128x64 pixels. Il n'impose aucun brochage fixe pour les boutons. Lors du démarrage, la carte identifie dynamiquement le câblage :
*   Une liste de broches potentielles est définie dans le tableau `BROCHES`.
*   La fonction `calibrerBouton` demande à l'utilisateur d'appuyer séquentiellement sur le bouton "Tirer" puis sur le bouton "Reset".
*   Le code détecte la modification de l'état électrique et assigne la broche correspondante à l'action.

### 3. Logique multijoueur et intégrité des paquets
L'état de la partie est géré par les variables de score (`myScore`, `opponentScore`) et une limite de victoire (`scoreLimit = 25`). Le paquet de données envoyé (`struct_message`) intègre deux sécurités pour garantir l'intégrité du réseau :
*   **Anti-écho (Identifiant unique `myId`)** : L'adresse de diffusion renvoie le message à toutes les cartes du réseau (y compris l'émetteur). Un ID généré aléatoirement au démarrage est inclus dans le paquet. La fonction de réception `OnDataRecv` ignore les paquets contenant le propre ID de la carte.
*   **Anti-latence (Identifiant de partie `gameId`)** : Empêche les paquets réseau mis en attente lors d'une manche précédente de valider un score dans la manche actuelle. Lors d'un "Reset", le `gameId` est incrémenté. Tout paquet reçu avec un ancien ID de partie est rejeté.

### 4. Phase d'initialisation (`setup`)
Exécutée une seule fois à la mise sous tension :
*   Initialisation du bus I2C et de l'écran OLED.
*   Analyse électrique et calibration utilisateur des boutons.
*   Génération de l'identifiant unique `myId`.
*   Passage de la puce radio en mode "Station Wi-Fi" (`WIFI_STA`).
*   Activation d'ESP-NOW, configuration du canal de diffusion non chiffré, et assignation de la fonction de réception (`OnDataRecv`).

### 5. Boucle principale (`loop`)
La fonction `loop` lit en continu les entrées physiques et applique les règles :
*   **Anti-rebond logiciel (Debounce)** : Lors d'un appui, un délai strict de 50 ms (`debounceDelay`) est imposé avant d'autoriser une nouvelle action, neutralisant les faux contacts mécaniques.
*   **Transmission** : Si "Tirer" est pressé et validé, le score s'incrémente et le paquet de données est diffusé via `esp_now_send`. Si "Reset" est pressé, le `gameId` est incrémenté, les scores tombent à zéro, et l'ordre de redémarrage est diffusé.
*   **Conditions de fin** : La différence stricte (`diff`) entre les scores est mesurée. Si elle atteint l'un des extrêmes (`25` ou `-25`), l'état de la partie passe en `gameOver = true`, verrouillant la prise en compte de nouveaux tirs.

### 6. Moteur de rendu graphique
L'écran est systématiquement effacé et redessiné à la fin de chaque cycle :
*   **En fin de partie** : Le programme calcule la largeur du texte via `getTextBounds` pour centrer mathématiquement la mention "VICTOIRE" ou "DEFAITE", ainsi que la consigne de redémarrage.
*   **En cours de jeu** : Dessin de l'environnement visuel (la corde en double ligne, les repères). La fonction `map()` convertit la valeur de l'écart de score (de -25 à 25) en position horizontale de pixels (de 15 à 113). Le curseur mobile est tracé à ces coordonnées, accompagné de l'affichage numérique de l'écart de force en temps réel.
