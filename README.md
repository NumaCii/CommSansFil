# TP — Porte-monnaie électronique sur carte MIFARE Classic 1k

**Polytech Dijon — 4A IT4 IoT & Cybersécurité — Systèmes de télécommunications sans-fil et applications**

**Auteurs :** Enzo Leone Provost & Numa Ciribino
**Encadrant :** Vincent Thivent (ODALID)
**Année :** 2025–2026

---

## Présentation

Ce projet implémente l'application **« Porte-monnaie ODALID »** demandée dans le sujet de TP : une interface graphique Qt qui dialogue avec une carte sans-contact NXP MIFARE Classic 1k via un coupleur USB CDC ODALID, et qui permet de gérer une carte multiservice étudiante de l'ESIREM (identité + porte-monnaie unités cafétéria/photocopies).

L'application reproduit fidèlement la maquette du sujet (page 6) et implémente les 6 séquences décrites dans les diagrammes 2.3.1 à 2.3.6 du TD :

- **Connect** : ouverture du port COM, récupération de la version, activation du champ RF
- **Sélectionner la carte** : polling ISO 14443-3 A, lecture de l'identité, lecture du compteur
- **Mise à jour** : écriture du nom et prénom dans la carte avec relecture de vérification
- **Payer** : décrément du compteur d'unités avec mise à jour du backup
- **Charger** : incrément du compteur d'unités avec mise à jour du backup
- **Disconnect** : coupure RF et fermeture du port COM

Un feedback visuel/sonore (LED + buzzer) est émis sur chaque transaction comme exigé page 7 du sujet.

---

## Architecture du projet

Le code est organisé en **deux couches** :

| Fichier | Rôle |
|---|---|
| `main.cpp` | Point d'entrée Qt minimal |
| `mainwindow.h/.cpp/.ui` | Interface graphique — slots des boutons, gestion de l'état UI, feedback utilisateur |
| `cardmanager.h/.cpp` | Encapsulation de toute la logique MIFARE — connaît la cartographie de la carte et les appels à la librairie ODALID |

Ce découpage évite tout mélange entre logique métier et UI : `CardManager` ne dépend pas de Qt Widgets et reste réutilisable hors interface.

---

## Architecture de la carte

Conformément au TD section 2.2 :

### Secteur 2 — Identité

| Bloc | Contenu | Authentification |
|------|---------|------------------|
| 8 | `"Identite"` (nom de l'application) | — |
| 9 | Prénom (UTF-8, max 16 octets) | KeyA index 2 (lecture) / KeyB index 2 (écriture) |
| 10 | Nom (UTF-8, max 16 octets) | idem |
| 11 | Trailer : KeyA + AccessBits + KeyB | — |

### Secteur 3 — Porte-monnaie

| Bloc | Contenu | Authentification |
|------|---------|------------------|
| 12 | `"Porte Monnaie"` (nom de l'application) | — |
| 13 | **Backup** du compteur (value block) | KeyB index 3 |
| 14 | Compteur (value block) | KeyA index 3 (lecture/décrément) / KeyB index 3 (écriture/incrément) |
| 15 | Trailer : KeyA + AccessBits + KeyB | — |

Les clés MIFARE par défaut (`FF FF FF FF FF FF`) sont utilisées, déjà pré-chargées dans le Secure Element du coupleur aux index 2 et 3.

### Gestion du backup

Comme demandé dans le sujet, le bloc 13 sert de **backup** du compteur (bloc 14). Après chaque opération `pay()` ou `charge()`, la nouvelle valeur du compteur est recopiée vers le backup via `Mf_Classic_Restore_Value` — cela protège contre une coupure RF en plein milieu d'une transaction (un éloignement de la carte du lecteur). Si le compteur principal devient corrompu, le backup contient encore la dernière valeur valide.

### Initialisation transparente

Lors de la toute première utilisation d'une carte vierge, les blocs 13 et 14 ne sont pas formatés en *value blocks* MIFARE (le format spécifique sur 16 octets requis par les commandes Increment/Decrement/Restore : valeur + complément + valeur + adresse + complément). L'application **détecte automatiquement** l'absence de ce format lors du clic sur « Sélectionner la carte » et formate silencieusement les deux blocs à zéro, sans intervention de l'utilisateur. Les sélections ultérieures liront directement la valeur stockée.

---

## Diagramme des classes

```
                       ┌──────────────┐
                       │   main.cpp   │
                       └──────┬───────┘
                              │ crée
                       ┌──────▼─────────┐
                       │   MainWindow   │
                       │   (Qt Widgets) │
                       └──────┬─────────┘
                              │ possède un
                       ┌──────▼─────────┐
                       │  CardManager   │
                       │  (QObject)     │
                       └──────┬─────────┘
                              │ appelle
                       ┌──────▼─────────┐
                       │  Librairie     │
                       │  ODALID_Edu    │
                       │  (.dll + .a)   │
                       └──────┬─────────┘
                              │ pilote
                       ┌──────▼─────────┐
                       │  Coupleur USB  │
                       │     CDC        │
                       └──────┬─────────┘
                              │ RF 13.56 MHz
                       ┌──────▼─────────┐
                       │ Carte MIFARE   │
                       │  Classic 1k    │
                       └────────────────┘
```

---

## Prérequis logiciels

| Outil | Version testée |
|---|---|
| **Qt** | 5.15.2 |
| **Compilateur** | MinGW 64-bit 8.1.0 (livré avec Qt 5.15.2) |
| **Standard C++** | C++17 |
| **Qt Creator** | toute version compatible Qt 5.15 |
| **OS testé** | Windows 10 / 11 |

> ⚠️ Le projet utilise la librairie `libODALID_Education.a` et `ODALID_Education.dll` qui sont **spécifiques à MinGW**. Une utilisation avec MSVC nécessiterait des versions .lib correspondantes.

---

## Mise en place

### 1. Structure des dossiers attendue

```
Librairie+sample/
├── ESIREM-TP/              ← ce projet
│   ├── TP-LectureCarteMIFARE.pro
│   ├── main.cpp
│   ├── cardmanager.h
│   ├── cardmanager.cpp
│   ├── mainwindow.h
│   ├── mainwindow.cpp
│   ├── mainwindow.ui
│   └── README.md
└── LIB/                    ← fourni par ODALID
    ├── Core.h
    ├── Sw_Device.h
    ├── Sw_ISO14443A-3.h
    ├── Sw_Mf_Classic.h
    ├── MfErrNo.h
    ├── Tools.h
    ├── libODALID_Education.a
    └── ODALID_Education.dll
```

Le fichier `.pro` cherche la lib en `../LIB/`. Si votre structure diffère, ajustez la ligne :
```pro
LIBS += -L$$PWD/../LIB/ -lODALID_Education
```

### 2. Ouverture dans Qt Creator

1. Lancer **Qt Creator**
2. *Fichier → Ouvrir un fichier ou un projet*
3. Sélectionner `TP-LectureCarteMIFARE.pro`
4. Configurer avec le kit **Desktop Qt 5.15.2 MinGW 64-bit**
5. *Build → Run qmake*
6. *Build → Build All* (ou Ctrl+B)

### 3. Déploiement de la DLL

Avant de lancer l'exécutable, **copier `ODALID_Education.dll`** dans le dossier de build (à côté de l'exécutable), sinon Windows refusera de le lancer :

```
build-TP-LectureCarteMIFARE-Desktop_Qt_5_15_2_MinGW_64_bit-Debug/
└── debug/
    ├── TP-LectureCarteMIFARE.exe
    └── ODALID_Education.dll   ← à copier ici
```

### 4. Connexion du coupleur

1. Brancher le coupleur USB CDC ODALID sur un port USB
2. Vérifier dans le Gestionnaire de périphériques Windows qu'un nouveau port COM est apparu
3. Lancer l'application

---

## Utilisation

### Workflow standard

1. **Connect** — connexion au coupleur (LED jaune)
2. Poser une carte MIFARE Classic 1k sur le coupleur
3. **Sélectionner la carte** — lit l'identité et le solde (LED verte + buzzer si OK)
4. Modifier nom/prénom puis **Mise à jour** pour écrire dans la carte
5. Saisir un nombre d'unités puis **Charger** pour créditer, ou **Payer** pour débiter
6. **Disconnect** en fin de session

### Codes de retour de la librairie

L'application affiche les messages d'erreur ODALID directement dans une boîte de dialogue. Les codes les plus courants :

| Code | Signification |
|------|---------------|
| 0 (MI_OK) | Opération réussie |
| -1 | Pas de réponse / carte muette / non présente |
| -18 | Lecture/écriture refusée par la carte (souvent format value block invalide ou clé incorrecte) |

---

## Choix techniques

- **Pattern Card Manager** : toute la logique MIFARE est regroupée dans une seule classe `CardManager` qui expose une API métier de haut niveau (`pay`, `charge`, `readIdentity`, etc.). Les slots de `MainWindow` se contentent d'appeler cette API et de mettre à jour l'UI — séparation claire des responsabilités.
- **Authentification par index** : les clés ne transitent jamais en clair dans le code applicatif. Elles sont stockées dans le Secure Element du coupleur (les "coffres-forts" index 2 et 3) et référencées par leur index — c'est la philosophie de sécurité recommandée par le TD section 1.1.
- **Format value block manuel** : la fonction `initValueBlocks()` construit elle-même le format value block officiel MIFARE (valeur LE / complément / valeur LE / adresse / ~adresse / adresse / ~adresse) via `Mf_Classic_Write_Block`. Cela garantit qu'une carte vierge est utilisable immédiatement sans préformatage externe.
- **Auto-init silencieuse** : la détection d'une carte non formatée se fait par échec de `Mf_Classic_Read_Value`. L'application formate alors transparente les blocs sans demande utilisateur — l'expérience reste identique sur carte vierge et sur carte déjà utilisée.
- **Feedback systématique** : `signalSuccess()` (LED verte) et `signalFailure()` (LED rouge) sont appelés sur chaque transaction (conformément aux cas d'utilisation page 7 où chaque action inclut `«include» Allumer LED` et `«include» Allumer Buzzer`).
- **Gestion d'état de l'UI** : la méthode `refreshUiState()` grise dynamiquement les boutons selon l'état (déconnecté / connecté / carte sélectionnée) pour empêcher des actions invalides.

---

## Limites et améliorations possibles

- Les **access bits** du sector trailer (blocs 11 et 15) ne sont pas customisés dans cette implémentation — ils restent en mode "transport" par défaut (toutes les opérations autorisées avec KeyA ou KeyB). Une version durcie devrait définir des conditions d'accès strictes pour ne permettre, par exemple, que le décrément avec KeyA et l'incrément avec KeyB. Cette opération n'a pas été réalisée car une mauvaise écriture du trailer bloque le secteur de façon **irréversible** (cf. datasheet MF1S50YYX §8.7).
- Les **clés A et B** restent aux valeurs par défaut `FF×6`. Un déploiement réel nécessiterait de diversifier les clés (par UID par exemple) et de les charger via `AutoReader_LoadKeyMifare`.
- Aucun **chiffrement applicatif** des données — l'identité est stockée en clair dans les blocs. Suffisant pour une carte cafétéria, insuffisant pour des données sensibles.

---

## Crédits

- Sujet et librairie : **Vincent Thivent, ODALID** (Dijon)
- Documentation MIFARE : NXP — datasheet MF1S50YYX
- Documentation librairie ODALID : <https://odalid.github.io/doc-librairie-carte-MIFARE/>
