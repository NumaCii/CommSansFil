#include "cardmanager.h"

#include <QDebug>

#include <cstring>

static constexpr BOOL KEY_A = TRUE;

static constexpr BOOL KEY_B = FALSE;

CardManager::CardManager(QObject *parent)

    : QObject(parent)

{

    std::memset(&m_reader, 0, sizeof(ReaderName));

    m_reader.Type   = ReaderCDC;

    m_reader.device = 0;

}

CardManager::~CardManager()

{

    if (m_connected) disconnectReader();

}

// --- Connect / Disconnect (diagrammes 2.3.1 et 2.3.6) -----------------------

int16_t CardManager::connectReader()

{

    int16_t status = OpenCOM(&m_reader);

    if (status != MI_OK) { setError(status, "OpenCOM"); return status; }

    status = Version(&m_reader);

    if (status != MI_OK) { setError(status, "Version"); CloseCOM(&m_reader); return status; }

    status = RF_Power_Control(&m_reader, TRUE, 0);

    if (status != MI_OK) { setError(status, "RF_Power_Control ON"); CloseCOM(&m_reader); return status; }

    LEDBuzzer(&m_reader, LED_YELLOW_ON);

    m_connected = true;

    return MI_OK;

}

int16_t CardManager::disconnectReader()

{

    if (!m_connected) return MI_OK;

    RF_Power_Control(&m_reader, FALSE, 0);

    int16_t status = CloseCOM(&m_reader);

    m_connected = false;

    return status;

}

// --- Sélection carte (diagramme 2.3.2) --------------------------------------

int16_t CardManager::pollCard()

{

    uint8_t  atq[2] = {0};

    uint8_t  sak[1] = {0};

    uint8_t  uid[12] = {0};

    uint16_t uidLen = 12;

    int16_t status = ISO14443_3_A_PollCard(&m_reader, atq, sak, uid, &uidLen);

    if (status != MI_OK) { setError(status, "PollCard"); return status; }

    if ((sak[0] & 0x1F) != 0x08) {

        m_lastError = QStringLiteral("Carte détectée mais ce n'est pas une MIFARE Classic 1k");

        return -1;

    }

    m_uidHex.clear();

    for (uint16_t i = 0; i < uidLen; ++i)

        m_uidHex += QString::asprintf("%02X", uid[i]);

    return MI_OK;

}

int16_t CardManager::haltCard()

{

    return ISO14443_3_A_Halt(&m_reader);

}

// --- Identité (secteur 2) ---------------------------------------------------

int16_t CardManager::readIdentity(QString &nom, QString &prenom)

{

    int16_t s = readTextBlock(BLOCK_NOM, nom, KEY_INDEX_IDENTITE, KEY_A);

    if (s != MI_OK) return s;

    return readTextBlock(BLOCK_PRENOM, prenom, KEY_INDEX_IDENTITE, KEY_A);

}

int16_t CardManager::writeIdentity(const QString &nom, const QString &prenom)

{

    int16_t s = writeTextBlock(BLOCK_NOM, nom, KEY_INDEX_IDENTITE, KEY_B);

    if (s != MI_OK) return s;

    return writeTextBlock(BLOCK_PRENOM, prenom, KEY_INDEX_IDENTITE, KEY_B);

}

// --- Porte-monnaie (secteur 3) ----------------------------------------------

int16_t CardManager::readCounter(int32_t &value)

{

    uint32_t v = 0;

    int16_t status = Mf_Classic_Read_Value(&m_reader, TRUE, BLOCK_COMPTEUR,
&v, KEY_A, KEY_INDEX_PORTE_MONNAIE);

    if (status != MI_OK) { setError(status, "Read_Value compteur"); return status; }

    value = static_cast<int32_t>(v);

    return MI_OK;

}

// Signature : (pName, Auth, Block, Value, trans_Block, AuthKey, KeyIndex)

int16_t CardManager::pay(int32_t units)

{

    int16_t status = Mf_Classic_Decrement_Value(&m_reader, TRUE,

                                                BLOCK_COMPTEUR,

                                                static_cast<uint32_t>(units),

                                                BLOCK_COMPTEUR,

                                                KEY_A, KEY_INDEX_PORTE_MONNAIE);

    if (status != MI_OK) { setError(status, "Decrement_Value"); return status; }

    status = Mf_Classic_Restore_Value(&m_reader, TRUE,

                                      BLOCK_COMPTEUR, BLOCK_BACKUP,

                                      KEY_B, KEY_INDEX_PORTE_MONNAIE);

    if (status != MI_OK) { setError(status, "Restore_Value backup"); return status; }

    return MI_OK;

}

int16_t CardManager::charge(int32_t units)

{

    int16_t status = Mf_Classic_Increment_Value(&m_reader, TRUE,

                                                BLOCK_COMPTEUR,

                                                static_cast<uint32_t>(units),

                                                BLOCK_COMPTEUR,

                                                KEY_B, KEY_INDEX_PORTE_MONNAIE);

    if (status != MI_OK) { setError(status, "Increment_Value"); return status; }

    status = Mf_Classic_Restore_Value(&m_reader, TRUE,

                                      BLOCK_COMPTEUR, BLOCK_BACKUP,

                                      KEY_B, KEY_INDEX_PORTE_MONNAIE);

    if (status != MI_OK) { setError(status, "Restore_Value backup"); return status; }

    return MI_OK;

}

// Formate les blocs 13 (backup) et 14 (compteur) en value blocks MIFARE.

// Format : valeur (4o LE) | ~valeur (4o) | valeur (4o LE) | addr | ~addr | addr | ~addr

int16_t CardManager::initValueBlocks()

{

    auto buildValueBlock = [](uint8_t out[16], int32_t value, uint8_t addr) {

        uint32_t v  = static_cast<uint32_t>(value);

        uint32_t nv = ~v;

        out[0]  = v  & 0xFF; out[1]  = (v  >> 8) & 0xFF; out[2]  = (v  >> 16) & 0xFF; out[3]  = (v  >> 24) & 0xFF;

        out[4]  = nv & 0xFF; out[5]  = (nv >> 8) & 0xFF; out[6]  = (nv >> 16) & 0xFF; out[7]  = (nv >> 24) & 0xFF;

        out[8]  = v  & 0xFF; out[9]  = (v  >> 8) & 0xFF; out[10] = (v  >> 16) & 0xFF; out[11] = (v  >> 24) & 0xFF;

        out[12] = addr;

        out[13] = static_cast<uint8_t>(~addr);

        out[14] = addr;

        out[15] = static_cast<uint8_t>(~addr);

    };

    uint8_t buf[16];

    buildValueBlock(buf, 0, BLOCK_COMPTEUR);

    int16_t s = Mf_Classic_Write_Block(&m_reader, TRUE, BLOCK_COMPTEUR, buf,

                                       KEY_B, KEY_INDEX_PORTE_MONNAIE);

    if (s != MI_OK) { setError(s, "init compteur"); return s; }

    buildValueBlock(buf, 0, BLOCK_BACKUP);

    s = Mf_Classic_Write_Block(&m_reader, TRUE, BLOCK_BACKUP, buf,

                               KEY_B, KEY_INDEX_PORTE_MONNAIE);

    if (s != MI_OK) { setError(s, "init backup"); return s; }

    return MI_OK;

}

// --- Feedback ---------------------------------------------------------------

int16_t CardManager::signalSuccess()

{

    int16_t s = LEDBuzzer(&m_reader, LED_GREEN_ON);

    DELAYS_MS(100);

    return s;

}

int16_t CardManager::signalFailure()

{

    int16_t s = LEDBuzzer(&m_reader, LED_RED_ON);

    DELAYS_MS(100);

    return s;

}

// --- Helpers texte ↔ bloc 16 octets -----------------------------------------

int16_t CardManager::writeTextBlock(uint8_t block, const QString &text,

                                    uint8_t keyIdx, bool keyA)

{

    uint8_t data[16] = {0};

    QByteArray utf8 = text.toUtf8();

    int n = qMin(utf8.size(), 16);

    std::memcpy(data, utf8.constData(), n);

    int16_t status = Mf_Classic_Write_Block(&m_reader, TRUE, block, data,

                                            keyA ? KEY_A : KEY_B, keyIdx);

    if (status != MI_OK) setError(status, "Write_Block");

    return status;

}

int16_t CardManager::readTextBlock(uint8_t block, QString &out,

                                   uint8_t keyIdx, bool keyA)

{

    uint8_t data[16] = {0};

    int16_t status = Mf_Classic_Read_Block(&m_reader, TRUE, block, data,

                                           keyA ? KEY_A : KEY_B, keyIdx);

    if (status != MI_OK) { setError(status, "Read_Block"); return status; }

    int len = 0;

    while (len < 16 && data[len] != 0x00) ++len;

    out = QString::fromUtf8(reinterpret_cast<const char*>(data), len);

    return MI_OK;

}

void CardManager::setError(int16_t status, const char *context)

{

    m_lastError = QString("%1 : %2 (code %3)")

                      .arg(context)

                      .arg(GetErrorMessage(status))

                      .arg(status);

    qWarning().noquote() << m_lastError;

}
