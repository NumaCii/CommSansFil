#ifndef CARDMANAGER_H
#define CARDMANAGER_H

#include <QString>
#include <QObject>

// Librairie ODALID
extern "C" {
#include "MfErrNo.h"
#include "Core.h"
#include "Sw_Device.h"
#include "Sw_ISO14443A-3.h"
#include "Sw_Mf_Classic.h"
#include "Tools.h"
}

/*
 * Architecture de la carte (cf. TD section 2.2) :
 *
 *  Secteur 2 — Identité
 *    Block  8 : "Identite"      (nom de l'application)
 *    Block  9 : Prenom
 *    Block 10 : Nom
 *    Block 11 : trailer (KeyA + AccessBits + KeyB)
 *    Authentification : KeyA (index 2) en lecture, KeyB (index 2) en écriture
 *
 *  Secteur 3 — Porte-monnaie
 *    Block 12 : "Porte Monnaie"
 *    Block 13 : Backup compteur (value block)
 *    Block 14 : Compteur        (value block)
 *    Block 15 : trailer (KeyA + AccessBits + KeyB)
 *    Authentification : KeyA (index 3) lecture/décrément, KeyB (index 3) écriture/incrément
 */

class CardManager : public QObject
{
    Q_OBJECT

public:
    // Constantes d'architecture
    static constexpr uint8_t SECTOR_IDENTITE        = 2;
    static constexpr uint8_t BLOCK_APP_NAME_ID      = 8;   // "Identite"
    static constexpr uint8_t BLOCK_PRENOM           = 9;
    static constexpr uint8_t BLOCK_NOM              = 10;
    static constexpr uint8_t KEY_INDEX_IDENTITE     = 2;

    static constexpr uint8_t SECTOR_PORTE_MONNAIE   = 3;
    static constexpr uint8_t BLOCK_APP_NAME_PM      = 12;  // "Porte Monnaie"
    static constexpr uint8_t BLOCK_BACKUP           = 13;
    static constexpr uint8_t BLOCK_COMPTEUR         = 14;
    static constexpr uint8_t KEY_INDEX_PORTE_MONNAIE = 3;

    explicit CardManager(QObject *parent = nullptr);
    ~CardManager();

    // Gestion du lecteur
    int16_t connectReader();      // OpenCOM + Version + RF ON
    int16_t disconnectReader();   // RF OFF + CloseCOM
    bool    isConnected() const { return m_connected; }

    // Carte
    int16_t pollCard();           // ISO14443_3_A_PollCard + récupère UID/ATQ/SAK
    int16_t haltCard();

    // Identité (secteur 2)
    int16_t readIdentity(QString &nom, QString &prenom);
    int16_t writeIdentity(const QString &nom, const QString &prenom);

    // Porte-monnaie (secteur 3) — gère le backup
    int16_t readCounter(int32_t &value);
    int16_t pay(int32_t units);      // décrément + transfer compteur + restore vers backup
    int16_t charge(int32_t units);   // incrément + transfer compteur + restore vers backup
    int16_t initValueBlocks();       // à appeler une fois pour formater les blocs 13/14 en value blocks

    // Feedback visuel/sonore
    int16_t signalSuccess();         // LED verte + buzzer court
    int16_t signalFailure();         // LED rouge

    // Dernière erreur lisible
    QString lastError() const { return m_lastError; }
    QString uidHex() const { return m_uidHex; }

private:
    // Helpers internes
    int16_t writeTextBlock(uint8_t block, const QString &text, uint8_t keyIdx, bool keyA);
    int16_t readTextBlock(uint8_t block, QString &out, uint8_t keyIdx, bool keyA);
    void    setError(int16_t status, const char *context);

    ReaderName m_reader{};
    bool       m_connected = false;
    QString    m_lastError;
    QString    m_uidHex;
};

#endif // CARDMANAGER_H
